#include "gui/base/github_update_checker.h"

#include <cctype>
#include <string>

#include <QDesktopServices>
#include <QProcess>
#include <QUrl>

#include "absl/strings/str_cat.h"
#include "base/file_util.h"
#include "base/marina_curl_fetch.h"
#include "base/marina_github_releases.h"
#include "base/system_util.h"
#include "base/version.h"

namespace mozc::gui {
namespace {

constexpr char kReleasesApiUrl[] =
    "https://api.github.com/repos/marinaMoji/marinaMoji/releases?per_page=50";

}  // namespace

GitHubUpdateChecker::GitHubUpdateChecker(QObject* parent) : QObject(parent) {}

GitHubUpdateChecker::~GitHubUpdateChecker() {
  if (process_ != nullptr) {
    process_->kill();
    process_->deleteLater();
    process_ = nullptr;
  }
}

void GitHubUpdateChecker::CheckForUpdates(bool include_unstable) {
  if (busy_) {
    return;
  }
  include_unstable_ = include_unstable;
  busy_ = true;

  process_ = new QProcess(this);
  connect(process_,
          QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          &GitHubUpdateChecker::onProcessFinished);
  process_->start(
      QStringLiteral("curl"),
      {QStringLiteral("-fsSL"), QStringLiteral("-A"),
       QStringLiteral("marinaMoji-update-check"), QStringLiteral("-H"),
       QStringLiteral("Accept: application/vnd.github+json"),
       QString::fromUtf8(kReleasesApiUrl)});
  if (!process_->waitForStarted(5000)) {
    finishWithFailure(tr("Could not start curl to check for updates."));
  }
}

bool GitHubUpdateChecker::DownloadAndOpenInstaller(const QString& pkg_url,
                                                   const QString& tag_name,
                                                   QString* error_message) {
#if defined(__APPLE__)
  if (pkg_url.isEmpty()) {
    if (error_message != nullptr) {
      *error_message = tr("No installer package URL was found for this release.");
    }
    return false;
  }
  std::string safe_tag = tag_name.toStdString();
  for (char& c : safe_tag) {
    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-' ||
          c == '_')) {
      c = '_';
    }
  }
  const std::string dest = FileUtil::JoinPath(
      SystemUtil::GetUserProfileDirectory(),
      absl::StrCat("marinaMoji-update-", safe_tag, ".pkg"));
  const auto status =
      MarinaCurlDownload(pkg_url.toStdString(), dest);
  if (!status.ok()) {
    if (error_message != nullptr) {
      *error_message = tr("Could not download the installer.");
    }
    return false;
  }
  if (!MarinaOpenLocalPath(dest)) {
    if (error_message != nullptr) {
      *error_message = tr("Downloaded the installer but could not open it.");
    }
    return false;
  }
  return true;
#elif defined(_WIN32)
  if (pkg_url.isEmpty()) {
    if (error_message != nullptr) {
      *error_message = tr("No installer package URL was found for this release.");
    }
    return false;
  }
  std::string safe_tag = tag_name.toStdString();
  for (char& c : safe_tag) {
    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-' ||
          c == '_')) {
      c = '_';
    }
  }
  const QString dest = QString::fromStdString(FileUtil::JoinPath(
      SystemUtil::GetUserProfileDirectory(),
      absl::StrCat("marinaMoji-update-", safe_tag, ".msi")));

  // Same safe argv-list QProcess pattern as CheckForUpdates() above: no
  // shell is involved, so the URL (which comes from parsed GitHub JSON, not
  // a hardcoded constant) cannot inject extra curl arguments the way a
  // hand-built command-line string could.
  QProcess download;
  download.start(QStringLiteral("curl"),
                 {QStringLiteral("-fsSL"), QStringLiteral("-A"),
                  QStringLiteral("marinaMoji-update-check"),
                  QStringLiteral("-o"), dest, pkg_url});
  if (!download.waitForStarted(5000) || !download.waitForFinished(-1) ||
      download.exitStatus() != QProcess::NormalExit ||
      download.exitCode() != 0) {
    if (error_message != nullptr) {
      *error_message = tr("Could not download the installer.");
    }
    return false;
  }
  // QDesktopServices::openUrl on a file:// URL is Qt's cross-platform
  // "open with the OS default handler" -- for an .msi that is msiexec via
  // shell association, which raises the UAC elevation prompt just like the
  // macOS Installer.app password prompt above. The MSI is unsigned for now
  // (see CHANGELOG.md), so SmartScreen may also warn first; that is
  // expected and mirrors the initial install.
  if (!QDesktopServices::openUrl(QUrl::fromLocalFile(dest))) {
    if (error_message != nullptr) {
      *error_message = tr("Downloaded the installer but could not open it.");
    }
    return false;
  }
  return true;
#else
  (void)pkg_url;
  (void)tag_name;
  if (error_message != nullptr) {
    *error_message = tr("Installer download is only available on macOS and Windows.");
  }
  return false;
#endif
}

void GitHubUpdateChecker::finishWithFailure(const QString& message) {
  busy_ = false;
  if (process_ != nullptr) {
    process_->deleteLater();
    process_ = nullptr;
  }
  emit checkFailed(message);
}

void GitHubUpdateChecker::onProcessFinished(int exit_code) {
  if (process_ == nullptr) {
    return;
  }
  const QByteArray stdout_bytes = process_->readAllStandardOutput();
  const QByteArray stderr_bytes = process_->readAllStandardError();
  process_->deleteLater();
  process_ = nullptr;
  busy_ = false;

  if (exit_code != 0) {
    QString detail = QString::fromUtf8(stderr_bytes).trimmed();
    if (detail.isEmpty()) {
      detail = tr("Could not reach GitHub (is curl installed?)");
    }
    emit checkFailed(detail);
    return;
  }

  const std::string json(stdout_bytes.constData(),
                         static_cast<size_t>(stdout_bytes.size()));
  const auto releases = ParseMarinaGitHubReleasesJson(json);
  if (releases.empty()) {
    emit checkFailed(tr("No releases found on GitHub."));
    return;
  }

  const auto newer = SelectNewerMarinaRelease(
      releases, Version::GetProductVersion(), include_unstable_);
  if (!newer.has_value()) {
    emit upToDate();
    return;
  }

  QString pkg_url;
#if defined(__APPLE__)
  if (const auto url =
          FindMarinaPkgDownloadUrl(*newer, MarinaHostMacPkgArchToken());
      url.has_value()) {
    pkg_url = QString::fromStdString(*url);
  }
#elif defined(_WIN32)
  if (const auto url =
          FindMarinaMsiDownloadUrl(*newer, MarinaHostWindowsArchToken());
      url.has_value()) {
    pkg_url = QString::fromStdString(*url);
  }
#endif

  emit updateAvailable(QString::fromStdString(newer->tag_name),
                       QString::fromStdString(newer->html_url), pkg_url);
}

}  // namespace mozc::gui
