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

  // Downloads |pkg_url| to a temp file and opens it with the OS installer.
  // Only implemented for macOS .pkg URLs; returns false on failure.
  static bool DownloadAndOpenInstaller(const QString& pkg_url,
                                       const QString& tag_name,
                                       QString* error_message);

 signals:
  // |pkg_url| is non-empty when a matching macOS .pkg asset was found.
  void updateAvailable(const QString& tag_name, const QString& html_url,
                       const QString& pkg_url);
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
