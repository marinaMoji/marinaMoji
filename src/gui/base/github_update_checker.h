#ifndef MOZC_GUI_BASE_GITHUB_UPDATE_CHECKER_H_
#define MOZC_GUI_BASE_GITHUB_UPDATE_CHECKER_H_

#include <QObject>
#include <QString>

class QProcess;

namespace mozc::gui {

// Fetches GitHub releases via curl and compares them to the product version.
// Used by the config dialog on Windows and macOS.
class GitHubUpdateChecker : public QObject {
  Q_OBJECT

 public:
  explicit GitHubUpdateChecker(QObject* parent = nullptr);
  ~GitHubUpdateChecker() override;

  void CheckForUpdates(bool include_unstable);

  // Downloads |pkg_url| to a temp file and opens it with the OS installer
  // (macOS Installer.app for a .pkg, msiexec via shell association for a
  // .msi on Windows). Not implemented on other platforms; returns false on
  // failure.
  //
  // |expected_sha256| is the digest GitHub published for the asset (see
  // updateAvailable). If it is non-empty and the download does not match, the
  // file is deleted, never opened, and false is returned.
  static bool DownloadAndOpenInstaller(const QString& pkg_url,
                                       const QString& tag_name,
                                       const QString& expected_sha256,
                                       QString* error_message);

  // True if the SHA-256 of the file at |path| equals |expected_sha256|
  // (lowercase or uppercase hex). An empty |expected_sha256| means GitHub
  // published no digest (it only started doing so in 2024), so there is
  // nothing to compare and this returns true, like the background updaters.
  // An unreadable file never matches a non-empty digest.
  static bool FileMatchesSha256(const QString& path,
                                const QString& expected_sha256);

 signals:
  // |pkg_url| is non-empty when a matching installer asset was found for
  // the current platform (a .pkg on macOS, a .msi on Windows).
  // |pkg_sha256| is the lowercase hex SHA-256 GitHub published for that
  // asset, or empty if it published none.
  void updateAvailable(const QString& tag_name, const QString& html_url,
                       const QString& pkg_url, const QString& pkg_sha256);
  void upToDate();
  void checkFailed(const QString& message);

 private slots:
  void onProcessFinished(int exit_code);

 private:
  void finishWithFailure(const QString& message);

  QProcess* process_ = nullptr;
  bool include_unstable_ = false;
  bool busy_ = false;
};

}  // namespace mozc::gui

#endif  // MOZC_GUI_BASE_GITHUB_UPDATE_CHECKER_H_
