#include "gui/base/github_update_checker.h"

#include <QProcess>

#include "base/marina_github_releases.h"
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

  emit updateAvailable(QString::fromStdString(newer->tag_name),
                       QString::fromStdString(newer->html_url));
}

}  // namespace mozc::gui
