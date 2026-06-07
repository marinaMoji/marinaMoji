#include "gui/config_dialog/config_dialog_sync_tab.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QThread>
#include <QVBoxLayout>

#include "sync/sync_config.h"
#include "sync/sync_key.h"
#include "sync/sync_status.h"
#include "sync/sync_util.h"

namespace mozc {
namespace gui {

ConfigDialogSyncTab::ConfigDialogSyncTab(client::Client* client,
                                         QWidget* parent)
    : client_(client), tab_(new QWidget(parent)) {
  auto* layout = new QVBoxLayout(tab_);

  enabled_ = new QCheckBox(
      QObject::tr("Enable encrypted sync (settings, dictionary, history)"),
      tab_);
  layout->addWidget(enabled_);

  auto* file_row = new QHBoxLayout();
  file_path_ = new QLineEdit(tab_);
  file_path_->setPlaceholderText(
      QObject::tr("Path to sync file (e.g. in your Nextcloud folder)"));
  browse_ = new QPushButton(QObject::tr("Browse..."), tab_);
  file_row->addWidget(file_path_);
  file_row->addWidget(browse_);
  layout->addLayout(file_row);

  auto* key_row = new QHBoxLayout();
  generate_key_ = new QPushButton(QObject::tr("Generate sync key"), tab_);
  enter_key_ = new QPushButton(QObject::tr("Enter sync key"), tab_);
  key_row->addWidget(generate_key_);
  key_row->addWidget(enter_key_);
  layout->addLayout(key_row);

  auto* what_group = new QGroupBox(QObject::tr("What to sync"), tab_);
  auto* what_layout = new QVBoxLayout(what_group);
  sync_settings_ = new QCheckBox(QObject::tr("Settings"), what_group);
  sync_dictionary_ = new QCheckBox(QObject::tr("User dictionary"), what_group);
  sync_history_ =
      new QCheckBox(QObject::tr("Commit / learning history"), what_group);
  sync_settings_->setChecked(true);
  sync_dictionary_->setChecked(true);
  sync_history_->setChecked(true);
  what_layout->addWidget(sync_settings_);
  what_layout->addWidget(sync_dictionary_);
  what_layout->addWidget(sync_history_);
  layout->addWidget(what_group);

  direction_ = new QComboBox(tab_);
  direction_->addItem(QObject::tr("Bidirectional"),
                      commands::UserSyncConfig::BIDIRECTIONAL);
  direction_->addItem(QObject::tr("Upload only"),
                      commands::UserSyncConfig::UPLOAD);
  direction_->addItem(QObject::tr("Download only"),
                      commands::UserSyncConfig::DOWNLOAD);

  auto_sync_ = new QComboBox(tab_);
  auto_sync_->addItem(QObject::tr("Never"), commands::UserSyncConfig::NEVER);
  auto_sync_->addItem(QObject::tr("Manual only"),
                      commands::UserSyncConfig::MANUAL);
  auto_sync_->addItem(QObject::tr("Every N minutes"),
                      commands::UserSyncConfig::EVERY_N_MINUTES);
  auto_sync_->addItem(QObject::tr("On shutdown"),
                      commands::UserSyncConfig::ON_SHUTDOWN);

  interval_ = new QSpinBox(tab_);
  interval_->setRange(1, 1440);
  interval_->setValue(30);
  interval_->setSuffix(QObject::tr(" min"));

  cooldown_ = new QSpinBox(tab_);
  cooldown_->setRange(0, 3600);
  cooldown_->setValue(60);
  cooldown_->setSuffix(QObject::tr(" sec"));

  auto* opts = new QFormLayout();
  opts->addRow(QObject::tr("Direction"), direction_);
  opts->addRow(QObject::tr("Auto-sync"), auto_sync_);
  opts->addRow(QObject::tr("Interval"), interval_);
  opts->addRow(QObject::tr("Cooldown after typing"), cooldown_);
  layout->addLayout(opts);

  sync_now_ = new QPushButton(QObject::tr("Sync now"), tab_);
  layout->addWidget(sync_now_);
  status_ = new QLabel(tab_);
  status_->setWordWrap(true);
  layout->addWidget(status_);
  layout->addStretch();

  QObject::connect(browse_, &QPushButton::clicked, [this]() { OnBrowseFile(); });
  QObject::connect(generate_key_, &QPushButton::clicked,
                   [this]() { OnGenerateKey(); });
  QObject::connect(enter_key_, &QPushButton::clicked,
                   [this]() { OnEnterOrShowKey(); });
  QObject::connect(sync_now_, &QPushButton::clicked, [this]() { OnSyncNow(); });
  QObject::connect(enabled_, &QCheckBox::toggled, [this]() { UpdateUiState(); });
  QObject::connect(file_path_, &QLineEdit::textChanged,
                   [this]() { UpdateUiState(); });
  QObject::connect(auto_sync_, &QComboBox::currentIndexChanged,
                   [this](int) { UpdateUiState(); });

  UpdateUiState();
}

void ConfigDialogSyncTab::AddToTabWidget(QTabWidget* tab_widget) {
  tab_widget->addTab(tab_, QObject::tr("Sync"));
}

void ConfigDialogSyncTab::LoadFromServer() {
  const auto config_or = sync::LoadSyncConfig();
  if (!config_or.ok()) {
    return;
  }
  config_ = *config_or;
  enabled_->setChecked(config_.enabled());
  file_path_->setText(QString::fromStdString(config_.sync_file_path()));
  sync_settings_->setChecked(config_.sync_settings());
  sync_dictionary_->setChecked(config_.sync_dictionary());
  sync_history_->setChecked(config_.sync_history());
  direction_->setCurrentIndex(direction_->findData(config_.direction()));
  auto_sync_->setCurrentIndex(auto_sync_->findData(config_.auto_sync_mode()));
  interval_->setValue(config_.auto_sync_interval_minutes());
  cooldown_->setValue(config_.sync_cooldown_seconds());
  const QString status = QObject::tr("Last sync: %1 — %2")
                             .arg(QString::fromStdString(config_.last_sync_time()))
                             .arg(QString::fromStdString(config_.last_sync_status()));
  status_->setText(status);
  UpdateUiState();
}

bool ConfigDialogSyncTab::ShouldOfferShowSyncKey() const {
  if (!sync::HasStoredSyncKey()) {
    return false;
  }
  const std::string path = file_path_->text().trimmed().toStdString();
  if (path.empty()) {
    return false;
  }
  if (config_.last_sync_status() != "OK") {
    return false;
  }
  return path == config_.sync_file_path();
}

void ConfigDialogSyncTab::OnEnterOrShowKey() {
  if (ShouldOfferShowSyncKey()) {
    OnShowKey();
  } else {
    OnEnterKey();
  }
}

void ConfigDialogSyncTab::OnShowKey() {
  const auto key_or = sync::LoadSyncKey();
  if (!key_or.ok()) {
    QMessageBox::information(
        tab_, QObject::tr("Sync"),
        QObject::tr("No sync key is stored on this device yet. Generate one "
                    "or enter a key from your other device."));
    return;
  }
  QMessageBox box(tab_);
  box.setIcon(QMessageBox::Information);
  box.setWindowTitle(QObject::tr("Your sync key"));
  box.setText(QObject::tr(
      "Copy this key to your other device(s). marinaMoji stores it only on "
      "this computer, not in the cloud."));
  box.setInformativeText(QString::fromStdString(*key_or));
  box.setTextInteractionFlags(Qt::TextSelectableByMouse);
  box.setStandardButtons(QMessageBox::Ok);
  box.exec();
}

bool ConfigDialogSyncTab::SaveToServer() {
  config_.set_enabled(enabled_->isChecked());
  config_.set_sync_file_path(file_path_->text().toStdString());
  config_.set_sync_settings(sync_settings_->isChecked());
  config_.set_sync_dictionary(sync_dictionary_->isChecked());
  config_.set_sync_history(sync_history_->isChecked());
  config_.set_direction(static_cast<commands::UserSyncConfig::Direction>(
      direction_->currentData().toInt()));
  config_.set_auto_sync_mode(static_cast<commands::UserSyncConfig::AutoSyncMode>(
      auto_sync_->currentData().toInt()));
  config_.set_auto_sync_interval_minutes(interval_->value());
  config_.set_sync_cooldown_seconds(cooldown_->value());
  config_.set_has_sync_key(sync::HasStoredSyncKey());
  return sync::SaveSyncConfig(config_).ok();
}

void ConfigDialogSyncTab::UpdateUiState() {
  const bool ready =
      enabled_->isChecked() && !file_path_->text().trimmed().isEmpty();
  if (ShouldOfferShowSyncKey()) {
    enter_key_->setText(QObject::tr("Show sync key"));
    enter_key_->setToolTip(QObject::tr(
        "Display the sync key stored on this device (after a successful sync)."));
  } else {
    enter_key_->setText(QObject::tr("Enter sync key"));
    enter_key_->setToolTip(QObject::tr(
        "Paste the sync key from your other device, or after changing the "
        "sync file path."));
  }
  sync_settings_->setEnabled(ready);
  sync_dictionary_->setEnabled(ready);
  sync_history_->setEnabled(ready);
  direction_->setEnabled(ready);
  auto_sync_->setEnabled(ready);
  interval_->setEnabled(ready);
  cooldown_->setEnabled(ready);
  sync_now_->setEnabled(ready);
}

void ConfigDialogSyncTab::OnBrowseFile() {
  // Save dialog so users can pick a new filename on the primary device; do not
  // confirm overwrite — choosing a path only updates sync.conf, not the file.
  const QString path = QFileDialog::getSaveFileName(
      tab_, QObject::tr("Choose sync file"),
      file_path_->text().isEmpty()
          ? QStringLiteral("marinamoji_sync.mmz.enc")
          : file_path_->text(),
      QObject::tr("Encrypted sync file (*.mmz.enc *.enc);;All files (*)"),
      nullptr, QFileDialog::DontConfirmOverwrite);
  if (!path.isEmpty()) {
    file_path_->setText(path);
  }
}

void ConfigDialogSyncTab::OnGenerateKey() {
  if (sync::HasStoredSyncKey()) {
    const auto answer = QMessageBox::question(
        tab_, QObject::tr("Replace sync key?"),
        QObject::tr(
            "Generating a new key replaces the one stored on this device. "
            "Enter the new key on your other devices, then sync again from "
            "this device so the shared file uses the new key.\n\nContinue?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
      return;
    }
  }
  const std::string key = sync::GenerateSyncKey();
  if (key.empty() ||
      !sync::StoreSyncKey(key).ok()) {
    QMessageBox::warning(tab_, QObject::tr("Sync"),
                         QObject::tr("Could not generate sync key."));
    return;
  }
  config_.set_has_sync_key(true);
  config_.set_last_sync_status("");
  config_.set_last_sync_message("");
  UpdateUiState();
  SaveToServer();
  QMessageBox box(tab_);
  box.setIcon(QMessageBox::Information);
  box.setWindowTitle(QObject::tr("Your sync key"));
  box.setText(QObject::tr(
      "Copy this key to your other device(s). marinaMoji stores it only on "
      "this computer, not in the cloud."));
  box.setInformativeText(QString::fromStdString(key));
  box.setTextInteractionFlags(Qt::TextSelectableByMouse);
  box.setStandardButtons(QMessageBox::Ok);
  box.exec();
}

void ConfigDialogSyncTab::OnEnterKey() {
  bool ok = false;
  const QString key = QInputDialog::getText(
      tab_, QObject::tr("Enter sync key"),
      QObject::tr("Paste the sync key from your other device:"), QLineEdit::Normal,
      QString(), &ok);
  if (!ok || key.isEmpty()) {
    return;
  }
  if (!sync::StoreSyncKey(key.trimmed().toStdString()).ok()) {
    QMessageBox::warning(tab_, QObject::tr("Sync"),
                         QObject::tr("Could not save sync key."));
    return;
  }
  config_.set_has_sync_key(true);
  config_.set_last_sync_status("");
  config_.set_last_sync_message("");
  UpdateUiState();
  SaveToServer();
}

void ConfigDialogSyncTab::OnSyncNow() {
  if (!enabled_->isChecked()) {
    QMessageBox::warning(
        tab_, QObject::tr("Sync"),
        QObject::tr("Sync is disabled. Check \"Enable encrypted sync\" and "
                    "save, then try again."));
    return;
  }
  if (!SaveToServer()) {
    QMessageBox::warning(tab_, QObject::tr("Sync"),
                         QObject::tr("Could not save sync settings."));
    return;
  }
  if (!sync::SpawnSyncNow(/*force=*/true)) {
    QMessageBox::warning(tab_, QObject::tr("Sync"),
                         QObject::tr("Could not start sync process."));
    return;
  }
  status_->setText(QObject::tr("Synchronising…"));
  QString last_state;
  QString last_message;
  for (int i = 0; i < 600; ++i) {
    QThread::msleep(500);
    const auto status_or = sync::ReadSyncStatus();
    if (!status_or.ok()) {
      continue;
    }
    last_state = QString::fromStdString(status_or->state);
    last_message = QString::fromStdString(status_or->message);
    if (status_or->state == "running") {
      status_->setText(last_message.isEmpty()
                           ? QObject::tr("Synchronising…")
                           : last_message);
      continue;
    }
    if (status_or->state == "done") {
      LoadFromServer();
      QMessageBox::information(tab_, QObject::tr("Sync"),
                               QObject::tr("Sync completed."));
      return;
    }
    if (status_or->state == "error") {
      QMessageBox::warning(
          tab_, QObject::tr("Sync"),
          QString::fromStdString(status_or->message));
      return;
    }
  }
  QMessageBox::warning(
      tab_, QObject::tr("Sync"),
      QObject::tr("Sync timed out (last state: %1).\n%2\n\nTry running "
                  "marinaMojiSync --now --force in Terminal and check "
                  "sync.status.json.")
          .arg(last_state.isEmpty() ? QStringLiteral("idle") : last_state)
          .arg(last_message));
}

}  // namespace gui
}  // namespace mozc
