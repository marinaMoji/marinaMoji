// Copyright 2010-2021, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Qt component of configure dialog for Mozc
#include "gui/config_dialog/config_dialog.h"

#include "gui/config_dialog/config_dialog_shortcuts_tab.h"
#include "gui/config_dialog/config_dialog_sync_tab.h"
#include "gui/config_dialog/config_dialog_theme.h"
#include "gui/config_dialog/preferences_explanations.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QToolButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QTableWidgetItem>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
#include <QStyleHints>
#include <QTabBar>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <istream>
#include <map>
#include <memory>
#include <sstream>
#include <string>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "base/config_file_stream.h"
#include "base/file_util.h"
#include "base/process.h"
#include "client/client.h"
#include "config/config_handler.h"
#include "gui/base/stats_config_util.h"
#include "gui/base/util.h"
#include "gui/config_dialog/keymap_editor.h"
#include "gui/config_dialog/kaeriten_table_editor.h"
#include "gui/config_dialog/roman_table_editor.h"
#include "protocol/config.pb.h"
#include "request/dictionary_pack_ids.h"
#include "session/keymap.h"

#if defined(__ANDROID__) || defined(__wasm__)
#error "This platform is not supported."
#endif  // __ANDROID__ || __wasm__

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <QGuiApplication>
// clang-format on

#include "base/run_level.h"
#include "base/system_util.h"
#include "gui/base/win_util.h"
#endif  // _WIN32

#if defined(__APPLE__) || defined(_WIN32)
#include "base/marina_update_throttle.h"
#include "base/version.h"
#endif  // __APPLE__ || _WIN32

#ifdef __APPLE__
#include "base/mac/mac_util.h"
#endif  // __APPLE__

#if defined(__linux__)
#include "base/file_util.h"
#include "base/protobuf/text_format.h"
#include "base/system_util.h"
#include "unix/ibus/ibus_config.pb.h"
#include "unix/ibus/path_util.h"
#endif  // __linux__

namespace {

template <typename T>
void Connect(const QList<T*>& objects, const char* signal,
             const QObject* receiver, const char* slot) {
  for (typename QList<T*>::const_iterator itr = objects.begin();
       itr != objects.end(); ++itr) {
    QObject::connect(*itr, signal, receiver, slot);
  }
}
#if defined(__APPLE__) || defined(__linux__) || defined(_WIN32)
std::string UserSymbolsPath() {
#if defined(__APPLE__)
  return mozc::FileUtil::JoinPath(mozc::MacUtil::GetApplicationSupportDirectory(),
                                 "user_symbols.txt");
#elif defined(_WIN32)
  // Must match what the TIP reads in win32/tip/tip_ui_handler_conventional.cc
  // (LoadUserSymbolsFromFile), which is where the Symbols Palette gets its
  // user entries from on Windows.
  return mozc::FileUtil::JoinPath(mozc::SystemUtil::GetUserProfileDirectory(),
                                  "user_symbols.txt");
#else
  return mozc::FileUtil::JoinPath(mozc::ibus::GetUserDataDirectory(),
                                  "user_symbols.txt");
#endif
}

// FileUtil rather than std::fstream: on Windows the narrow path handed to
// fstream is interpreted in the active code page, not UTF-8, so a profile
// directory with non-ASCII characters (C:\Users\Zoe...) would silently fail to
// open. FileUtil widens the path first, and is also what the TIP side uses to
// read this same file.
std::vector<std::string> LoadUserSymbolsFromFile() {
  std::vector<std::string> values;
  const auto contents = mozc::FileUtil::GetContents(UserSymbolsPath());
  if (!contents.ok()) {
    return values;
  }
  const std::string &text = *contents;
  size_t pos = 0;
  while (pos < text.size()) {
    const size_t eol = text.find('\n', pos);
    std::string line = text.substr(
        pos, eol == std::string::npos ? std::string::npos : eol - pos);
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (!line.empty()) {
      values.push_back(std::move(line));
    }
    if (eol == std::string::npos) {
      break;
    }
    pos = eol + 1;
  }
  return values;
}

void SaveUserSymbolsToFile(const std::vector<std::string> &values) {
  const std::string path = UserSymbolsPath();
  const std::string dir = mozc::FileUtil::Dirname(path);
  if (!dir.empty()) {
    (void)mozc::FileUtil::CreateDirectory(dir);
  }
  std::string serialized;
  for (const std::string &value : values) {
    if (!value.empty()) {
      serialized.append(value);
      serialized.push_back('\n');
    }
  }
  if (!mozc::FileUtil::SetContents(path, serialized).ok()) {
    LOG(ERROR) << "Failed to write " << path;
  }
}
#endif  // __APPLE__ || __linux__ || _WIN32
}  // namespace

namespace mozc {
namespace gui {

namespace {
// Preferences may trigger a slow server-side config write; use the same
// generous IPC timeout as dictionary_tool (100s), not the 30s client default.
constexpr absl::Duration kConfigDialogTimeout = absl::Milliseconds(100000);
}  // namespace

#if defined(MARINAMOJI) && defined(__APPLE__)
namespace {

// Runs `marinaMoji --silent_update <mode>` (see SilentUpdateCommand in
// mac/main.mm for why it must be that binary) and returns its one-word result:
// on / off / approval / missing. Returns an empty string and fills |error| if
// it could not be run or reported a failure.
QString RunSilentUpdate(const QString &mode, QString *error) {
  // Preferences is marinaMoji.app/Contents/Resources/ConfigDialog.app.
  QString exe = QDir::cleanPath(QCoreApplication::applicationDirPath() +
                                QStringLiteral("/../../../../MacOS/marinaMoji"));
  if (!QFileInfo::exists(exe)) {
    exe = QStringLiteral(
        "/Library/Input Methods/marinaMoji.app/Contents/MacOS/marinaMoji");
  }
  QProcess process;
  process.start(exe, {QStringLiteral("--silent_update"), mode});
  if (!process.waitForStarted(5000) || !process.waitForFinished(30000)) {
    *error = QStringLiteral("could not run %1").arg(exe);
    return QString();
  }
  if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
    *error = QString::fromUtf8(process.readAllStandardError()).trimmed();
    if (error->isEmpty()) {
      *error = QStringLiteral("exit status %1").arg(process.exitCode());
    }
    return QString();
  }
  return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

}  // namespace
#endif  // MARINAMOJI && __APPLE__

ConfigDialog::ConfigDialog()
    : client_(client::ClientFactory::NewClient()) {
  client_->set_timeout(kConfigDialogTimeout);
  // Avoid spawning ErrorMessageDialog.app on IPC errors during shutdown.
  client_->set_suppress_error_dialog(true);
  setupUi(this);

  // Remove the context help button (question mark button) from the window.
  Qt::WindowFlags flags = windowFlags();
  flags &= ~Qt::WindowContextHelpButtonHint;
  setWindowFlags(flags);

  setWindowModality(Qt::NonModal);

#ifdef _WIN32
  miscStartupWidget->setVisible(false);
#endif  // _WIN32

#ifdef __APPLE__
  miscDefaultIMEWidget->setVisible(false);
  miscAdministrationWidget->setVisible(false);
  setWindowTitle(tr("%1 Preferences").arg(GuiUtil::ProductName()));
  candidateWindowFontSizeSpinBox->setRange(14, 36);
#else
  appearanceHeader->setVisible(false);
  appearanceGroup->setVisible(false);
#endif  // __APPLE__

#if defined(__linux__)
  miscDefaultIMEWidget->setVisible(false);
  miscAdministrationWidget->setVisible(false);
  miscStartupWidget->setVisible(false);
  // Candidate window: IBus (default) or Mozc. Only relevant on Linux/IBus.
  candidateWindowComboBox->addItem(tr("IBus"), true);   // use_ibus_candidate_window
  candidateWindowComboBox->addItem(tr("Mozc"), false);
  LoadIbusCandidateWindowFromFile();
#else
  miscCandidateWindowWidget->setVisible(false);
#endif  // __linux__

#if defined(__linux__) || defined(__APPLE__)
  startKanjiModeComboBox->addItem(tr("Shin (modern)"));
  startKanjiModeComboBox->addItem(tr("Kyu (traditional)"));
#else
  // Shin/kyu start-up style is Linux/macOS only; the conversion mode below is
  // offered everywhere.
  widget_start_kanji->setVisible(false);
#endif
  startLiveModeComboBox->addItem(tr("Automatic (live)"));
  startLiveModeComboBox->addItem(tr("Manual (Space to convert)"));

#ifdef NDEBUG
  // disable logging options
  miscLoggingWidget->setVisible(false);

#if defined(__linux__)
  // Keep the Misc tab on Linux so users can set sub-segment candidate limit.
  // (Other Misc options are hidden on Linux.)
#endif  // __linux__
#endif  // NDEBUG

  // Sub-segment candidate limit: 0 = no limit, else max candidates per prefix.
  suggestionsSizeSpinBox->setRange(1, 9);

  punctuationsSettingComboBox->addItem(QString::fromUtf8("、。"));
  punctuationsSettingComboBox->addItem(QString::fromUtf8("，．"));
  punctuationsSettingComboBox->addItem(QString::fromUtf8("、．"));
  punctuationsSettingComboBox->addItem(QString::fromUtf8("，。"));

  symbolsSettingComboBox->addItem(QString::fromUtf8("「」・"));
  symbolsSettingComboBox->addItem(QString::fromUtf8("[]／"));
  symbolsSettingComboBox->addItem(QString::fromUtf8("「」／"));
  symbolsSettingComboBox->addItem(QString::fromUtf8("[]・"));

  keymapSettingComboBox->addItem(tr("Custom keymap"));
  keymapSettingComboBox->addItem(tr("ATOK"));
  keymapSettingComboBox->addItem(tr("MS-IME"));
  keymapSettingComboBox->addItem(tr("Kotoeri"));

  keymapname_sessionkeymap_map_[tr("ATOK")] = config::Config::ATOK;
  keymapname_sessionkeymap_map_[tr("MS-IME")] = config::Config::MSIME;
  keymapname_sessionkeymap_map_[tr("Kotoeri")] = config::Config::KOTOERI;

  inputModeComboBox->addItem(tr("Romaji"));
  inputModeComboBox->addItem(tr("Kana"));
#ifdef _WIN32
  // These options changing the preedit method by a hot key are only
  // supported by Windows.
  inputModeComboBox->addItem(tr("Romaji (switchable)"));
  inputModeComboBox->addItem(tr("Kana (switchable)"));
#endif  // _WIN32

  spaceCharacterFormComboBox->addItem(tr("Follow input mode"));
  spaceCharacterFormComboBox->addItem(tr("Fullwidth"));
  spaceCharacterFormComboBox->addItem(tr("Halfwidth"));

  selectionShortcutModeComboBox->addItem(tr("No shortcut"));
  selectionShortcutModeComboBox->addItem(tr("1 -- 9"));
  selectionShortcutModeComboBox->addItem(tr("A -- L"));

  historyLearningLevelComboBox->addItem(tr("Yes"));
  historyLearningLevelComboBox->addItem(tr("Yes (don't record new data)"));
  historyLearningLevelComboBox->addItem(tr("No"));

  shiftKeyModeSwitchComboBox->addItem(tr("Off"));
  shiftKeyModeSwitchComboBox->addItem(tr("Alphanumeric"));
  shiftKeyModeSwitchComboBox->addItem(tr("Katakana"));

  numpadCharacterFormComboBox->addItem(tr("Follow input mode"));
  numpadCharacterFormComboBox->addItem(tr("Fullwidth"));
  numpadCharacterFormComboBox->addItem(tr("Halfwidth"));
  numpadCharacterFormComboBox->addItem(tr("Direct input"));

  verboseLevelComboBox->addItem(tr("0"));
  verboseLevelComboBox->addItem(tr("1"));
  verboseLevelComboBox->addItem(tr("2"));

  yenSignComboBox->addItem(tr("Yen Sign ¥"));
  yenSignComboBox->addItem(tr("Backslash \\"));

  // Order must exactly match the MarinaKeyboardLayout enum in config.proto,
  // since the combobox index is cast directly to the enum value.
  keyboardLayoutComboBox->addItem(tr("System default (current OS layout)"));
  keyboardLayoutComboBox->addItem(tr("US QWERTY"));
  keyboardLayoutComboBox->addItem(tr("UK QWERTY"));
  keyboardLayoutComboBox->addItem(tr("French AZERTY"));
  keyboardLayoutComboBox->addItem(tr("German QWERTZ"));
  keyboardLayoutComboBox->addItem(tr("Spanish"));
  keyboardLayoutComboBox->addItem(tr("Italian"));
  keyboardLayoutComboBox->addItem(tr("Dutch"));
  keyboardLayoutComboBox->addItem(tr("Dvorak"));
  keyboardLayoutComboBox->addItem(tr("BÉPO"));
  keyboardLayoutComboBox->addItem(tr("Japanese JIS"));

#ifndef _WIN32
  // Fixed romaji keyboard layout selection is only implemented on Windows.
  keyboardLayoutLabel->hide();
  keyboardLayoutComboBox->hide();
#endif  // !_WIN32

#ifndef __APPLE__
  // On Windows/Linux, yenSignCombBox can be hidden.
  yenSignLabel->hide();
  yenSignComboBox->hide();
  // On Windows/Linux, useJapaneseLayout checkbox should be invisible.
  useJapaneseLayout->hide();
#endif  // !__APPLE__

#ifndef _WIN32
  // Mode indicator is available only on Windows.
  useModeIndicator->hide();
#endif  // !_WIN32

  // Preferences auto-saves; the stock button box is hidden by the shell.
  configDialogButtonBox->hide();

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  // Preview-only: force dark chrome without changing system appearance.
  if (qEnvironmentVariableIsSet("MARINAMOJI_PREFS_UI_PREVIEW_DARK")) {
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
  }
#endif

  autosave_timer_ = new QTimer(this);
  autosave_timer_->setSingleShot(true);
  autosave_timer_->setInterval(200);
  connect(autosave_timer_, &QTimer::timeout, this, [this]() {
    RunAutoSave(/*quiet_validation=*/true);
  });

  // signal/slot
  QObject::connect(clearUserHistoryButton, SIGNAL(clicked()), this,
                   SLOT(ClearUserHistory()));
  QObject::connect(editUserDictionaryButton, SIGNAL(clicked()), this,
                   SLOT(EditUserDictionary()));
  QObject::connect(editUserSymbolsButton, SIGNAL(clicked()), this,
                   SLOT(EditUserSymbols()));
  QObject::connect(editKeymapButton, SIGNAL(clicked()), this,
                   SLOT(EditKeymap()));
  QObject::connect(resetToDefaultsButton, SIGNAL(clicked()), this,
                   SLOT(ResetToDefaults()));
  QObject::connect(editRomanTableButton, SIGNAL(clicked()), this,
                   SLOT(EditRomanTable()));
  QObject::connect(inputModeComboBox, SIGNAL(currentIndexChanged(int)), this,
                   SLOT(SelectInputModeSetting(int)));
  QObject::connect(useAutoConversion, SIGNAL(stateChanged(int)), this,
                   SLOT(SelectAutoConversionSetting(int)));
  QObject::connect(historySuggestCheckBox, SIGNAL(stateChanged(int)), this,
                   SLOT(SelectSuggestionSetting(int)));
  QObject::connect(dictionarySuggestCheckBox, SIGNAL(stateChanged(int)), this,
                   SLOT(SelectSuggestionSetting(int)));
  QObject::connect(realtimeConversionCheckBox, SIGNAL(stateChanged(int)), this,
                   SLOT(SelectSuggestionSetting(int)));
  QObject::connect(launchAdministrationDialogButton, SIGNAL(clicked()), this,
                   SLOT(LaunchAdministrationDialog()));
  QObject::connect(launchAdministrationDialogButtonForUsageStats,
                   SIGNAL(clicked()), this, SLOT(LaunchAdministrationDialog()));

  // Auto-save on committed control changes. Push-button tools (Edit…, Clear…)
  // are excluded: only those that mutate staged tables call EnableApplyButton
  // explicitly after a successful edit.
  Connect(findChildren<QCheckBox*>(), SIGNAL(clicked()), this,
          SLOT(EnableApplyButton()));
  Connect(findChildren<QComboBox*>(), SIGNAL(activated(int)), this,
          SLOT(EnableApplyButton()));
  Connect(findChildren<QSpinBox*>(), SIGNAL(editingFinished()), this,
          SLOT(EnableApplyButton()));
  if (characterFormEditor != nullptr) {
    connect(characterFormEditor, &QTableWidget::itemChanged, this,
            [this](QTableWidgetItem*) { EnableApplyButton(); });
  }

  // When clicking these messages, CheckBoxs corresponding
  // to them should be toggled.
  // We cannot use connect/slot as QLabel doesn't define
  // clicked slot by default.
  usageStatsMessage->installEventFilter(this);
  incognitoModeMessage->installEventFilter(this);

#ifndef _WIN32
  checkDefaultCheckBox->setVisible(false);
  checkDefaultLine->setVisible(false);
  checkDefaultLabel->setVisible(false);
#endif  // !_WIN32

#ifdef _WIN32
  launchAdministrationDialogButton->setEnabled(true);
  // if the current application is not elevated by UAC,
  // add a shield icon
  if (!mozc::RunLevel::IsElevatedByUAC()) {
    const QIcon& vista_shield_icon =
        QApplication::style()->standardIcon(QStyle::SP_VistaShield);
    launchAdministrationDialogButton->setIcon(vista_shield_icon);
    launchAdministrationDialogButtonForUsageStats->setIcon(vista_shield_icon);
  }
#else   // _WIN32
  launchAdministrationDialogButton->setEnabled(false);
  launchAdministrationDialogButton->setVisible(false);
  launchAdministrationDialogButtonForUsageStats->setEnabled(false);
  launchAdministrationDialogButtonForUsageStats->setVisible(false);
  administrationLine->setVisible(false);
  administrationLabel->setVisible(false);
  dictionaryPreloadingAndUACLabel->setVisible(false);
#endif  // _WIN32

#ifdef GOOGLE_JAPANESE_INPUT_BUILD
#ifdef _WIN32
  // On Windows, usage_stats is managed by administration_dialog.
  usageStatsCheckBox->setDisabled(true);
  usageStatsCheckBox->setVisible(false);
  usageStatsMessage->setDisabled(true);
  usageStatsMessage->setVisible(false);
#else   // _WIN32
  launchAdministrationDialogButtonForUsageStats->setEnabled(false);
  launchAdministrationDialogButtonForUsageStats->setVisible(false);
#endif  // _WIN32
#ifdef __linux__
  // On Linux, disable all fields for UsageStats
  usageStatsLabel->setEnabled(false);
  usageStatsLabel->setVisible(false);
  usageStatsLine->setEnabled(false);
  usageStatsLine->setVisible(false);
  usageStatsMessage->setEnabled(false);
  usageStatsMessage->setVisible(false);
  usageStatsCheckBox->setEnabled(false);
  usageStatsCheckBox->setVisible(false);
#endif  // __linux__
#else   // GOOGLE_JAPANESE_INPUT_BUILD
  // marinaMoji / OSS: show the section but permanently disabled with an
  // explicit privacy statement (all platforms).
  launchAdministrationDialogButtonForUsageStats->setEnabled(false);
  launchAdministrationDialogButtonForUsageStats->setVisible(false);
  usageStatsButtonGroup->setVisible(false);
#endif  // GOOGLE_JAPANESE_INPUT_BUILD

  GuiUtil::ReplaceWidgetLabels(this);

#ifndef GOOGLE_JAPANESE_INPUT_BUILD
  usageStatsCheckBox->setChecked(false);
  usageStatsCheckBox->setEnabled(false);
  usageStatsCheckBox->setVisible(false);
  usageStatsLabel->setVisible(true);
  usageStatsLabel->setEnabled(true);
  usageStatsLine->setVisible(true);
  usageStatsMessage->setVisible(true);
  usageStatsMessage->setEnabled(true);
  usageStatsMessage->setText(
      tr("%1 does not send usage statistics or crash reports to Google or "
         "to the marinaMoji developers. Upstream Mozc includes an optional "
         "telemetry setting; that setting is permanently disabled here.")
          .arg(GuiUtil::ProductName()));
  usageStatsMessage->setWordWrap(true);
  usageStatsMessage->setContentsMargins(0, 2, 0, 6);
#endif  // GOOGLE_JAPANESE_INPUT_BUILD

  shortcuts_tab_ = std::make_unique<ConfigDialogShortcutsTab>(this);
  shortcuts_tab_->AddToTabWidget(configDialogTabWidget);
  shortcuts_tab_->ConnectApplyButton(this, SLOT(EnableApplyButton()));
  shortcuts_tab_->ConnectEditKaeritenButton(this, SLOT(EditKaeritenTable()));

  sync_tab_ = std::make_unique<ConfigDialogSyncTab>(
      static_cast<client::Client*>(client_.get()), this);
  sync_tab_->AddToTabWidget(configDialogTabWidget);

#if defined(MARINAMOJI) && (defined(_WIN32) || defined(__APPLE__))
  {
    auto *updates_widget = new QWidget(miscTab);
    updates_widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto *updates_layout = new QVBoxLayout(updates_widget);
    updates_layout->setContentsMargins(0, 0, 5, 10);
    updates_layout->setSpacing(8);

    // The installed version, so it can be compared with the latest release
    // without hunting for the About dialog (which on Windows is only in the
    // language bar menu, easy to miss on Windows 11).
    auto *version_label = new QLabel(
        tr("Current version: %1")
            .arg(QString::fromStdString(Version::GetProductVersion())),
        updates_widget);
    version_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    updates_layout->addWidget(version_label);

    auto_check_for_updates_checkbox_ = new QCheckBox(
        tr("Automatically check for updates (once a day)"), updates_widget);
    updates_layout->addWidget(auto_check_for_updates_checkbox_);
    connect(auto_check_for_updates_checkbox_, &QCheckBox::toggled, this,
            &ConfigDialog::EnableApplyButton);

    include_unstable_updates_checkbox_ = new QCheckBox(
        tr("Include unstable (rc) releases when checking for updates"),
        updates_widget);
    updates_layout->addWidget(include_unstable_updates_checkbox_);

    check_for_updates_button_ =
        new QPushButton(tr("Check for updates…"), updates_widget);
    updates_layout->addWidget(check_for_updates_button_);

    miscTabLayout->insertWidget(0, updates_widget);

    connect(include_unstable_updates_checkbox_, &QCheckBox::toggled, this,
            &ConfigDialog::EnableApplyButton);
    connect(check_for_updates_button_, &QPushButton::clicked, this,
            &ConfigDialog::CheckForUpdates);

#if defined(__APPLE__)
    // Opt-in background installer (the UpdateHelper daemon). Acts immediately
    // rather than on Apply, because it registers or removes a system item.
    silent_update_checkbox_ = new QCheckBox(
        tr("Install updates automatically in the background"), updates_widget);
    silent_update_checkbox_->setToolTip(
        tr("macOS asks you to approve this once in System Settings. Stable "
           "releases only. The input method restarts after an update."));
    updates_layout->addWidget(silent_update_checkbox_);
    {
      QString error;
      silent_update_checkbox_->setChecked(
          RunSilentUpdate(QStringLiteral("status"), &error) ==
          QStringLiteral("on"));
    }
    connect(silent_update_checkbox_, &QCheckBox::clicked, this,
            [this](bool checked) {
              QString error;
              const QString result = RunSilentUpdate(
                  checked ? QStringLiteral("enable")
                          : QStringLiteral("disable"),
                  &error);
              if (result.isEmpty()) {
                silent_update_checkbox_->setChecked(!checked);
                QMessageBox::warning(
                    this, windowTitle(),
                    tr("Could not change automatic updates:\n%1").arg(error));
                return;
              }
              silent_update_checkbox_->setChecked(result ==
                                                  QStringLiteral("on"));
              if (checked && result == QStringLiteral("on")) {
                QMessageBox::information(
                    this, windowTitle(),
                    tr("Automatic updates are on. marinaMoji checks once a "
                       "day and installs new stable releases in the "
                       "background."));
              } else if (!checked && result == QStringLiteral("off")) {
                QMessageBox::information(this, windowTitle(),
                                         tr("Automatic updates are off."));
              }
              if (checked && result == QStringLiteral("approval")) {
                QMessageBox::information(
                    this, windowTitle(),
                    tr("macOS needs your approval first. In System Settings > "
                       "General > Login Items & Extensions, switch on "
                       "marinaMoji under \"Allow in the Background\" "
                       "(you may be asked for your password), then tick this "
                       "box again."));
              }
            });
#endif  // __APPLE__

    update_checker_ = std::make_unique<GitHubUpdateChecker>(this);
    connect(update_checker_.get(), &GitHubUpdateChecker::updateAvailable, this,
            &ConfigDialog::OnUpdateAvailable);
    connect(update_checker_.get(), &GitHubUpdateChecker::upToDate, this,
            &ConfigDialog::OnUpdateUpToDate);
    connect(update_checker_.get(), &GitHubUpdateChecker::checkFailed, this,
            &ConfigDialog::OnUpdateCheckFailed);
  }
#endif  // MARINAMOJI && (_WIN32 || __APPLE__)

  // marinaMoji: one checkbox per optional supplementary dictionary pack
  // (mozc::kDictionaryPackIds), built at runtime rather than in the .ui file
  // so new packs need no GUI code change -- see dictionary_pack_checkboxes_.
  // Same dialog on every platform (no separate Windows settings panel
  // exists), so this covers all of them.
  {
    auto *packs_widget = new QWidget(miscTab);
    packs_widget->setObjectName(QStringLiteral("dictionaryPacksWidget"));
    packs_widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto *packs_layout = new QVBoxLayout(packs_widget);
    packs_layout->setContentsMargins(0, 0, 5, 10);
    packs_layout->setSpacing(8);

    for (const DictionaryPackId &pack : kDictionaryPackIds) {
      QString label;
      if (pack.id == "experimental") {
        label = tr("Experimental vocabulary (archaic words, era names, court "
                   "titles)");
      } else {
        label = QString::fromUtf8(pack.display_name.data(),
                                  pack.display_name.size());
      }
      auto *checkbox = new QCheckBox(label, packs_widget);
      packs_layout->addWidget(checkbox);
      connect(checkbox, &QCheckBox::toggled, this,
              &ConfigDialog::EnableApplyButton);
      dictionary_pack_checkboxes_[std::string(pack.id)] = checkbox;
    }

    miscTabLayout->insertWidget(0, packs_widget);
  }

  // Reorganise into the six architecture pages before the visual shell reads
  // tab titles for the sidebar.
  explanation_controller_ =
      std::make_unique<PreferencesExplanationController>(this);
  MigratePreferencesInformationArchitecture();

  // Before Reload(). A failed config read opens a modal message, which shows
  // this window immediately. The shell has to be in place before that.
  InstallPreferencesShell();

  autosave_suppressed_ = true;
  Reload();
#ifdef _WIN32
  IMEHotKeyDisabledCheckBox->setChecked(WinUtil::GetIMEHotKeyDisabled());
#else   // _WIN32
  IMEHotKeyDisabledCheckBox->setVisible(false);
#endif  // _WIN32
  autosave_suppressed_ = false;

#if defined(GOOGLE_JAPANESE_INPUT_BUILD) && defined(CHANNEL_DEV)
  usageStatsCheckBox->setEnabled(false);
#endif  // GOOGLE_JAPANESE_INPUT_BUILD && CHANNEL_DEV
}

namespace {

void TakeFromLayout(QWidget* widget) {
  if (widget == nullptr) {
    return;
  }
  if (QWidget* parent = widget->parentWidget()) {
    if (QLayout* layout = parent->layout()) {
      layout->removeWidget(widget);
    }
  }
}

void FlattenGroupMargins(QWidget* widget) {
  if (widget == nullptr) {
    return;
  }
  if (QLayout* layout = widget->layout()) {
    layout->setContentsMargins(0, 2, 0, 2);
    layout->setSpacing(std::min(layout->spacing(), 8));
  }
  widget->setContentsMargins(0, 0, 0, 0);
}

void AddToPage(QVBoxLayout* page_layout, QWidget* widget) {
  if (page_layout == nullptr || widget == nullptr) {
    return;
  }
  TakeFromLayout(widget);
  if (QWidget* page = page_layout->parentWidget()) {
    widget->setParent(page);
  }
  FlattenGroupMargins(widget);
  widget->show();
  // Pages use spacing 0 so section helpers own the rhythm. Separate consecutive
  // controls with the shared row gap, but do not stack another gap onto the
  // spacer already inserted after a section heading.
  if (page_layout->count() > 0) {
    QLayoutItem* last = page_layout->itemAt(page_layout->count() - 1);
    if (last == nullptr || last->spacerItem() == nullptr) {
      page_layout->addSpacing(config_dialog_theme::Spacing::row);
    }
  }
  page_layout->addWidget(widget);
}

// Tab pages are owned by QTabWidget, not a normal layout. removeTab before
// reparenting, otherwise ClearTabWidget can hide or strand the page.
void DetachTabPage(QTabWidget* tabs, QWidget* page) {
  if (tabs == nullptr || page == nullptr) {
    return;
  }
  const int index = tabs->indexOf(page);
  if (index >= 0) {
    tabs->removeTab(index);
  }
}

// Keep a former tab page intact as one widget. Pulling its children out of
// their layout (as we tried earlier) breaks size hints — tables and later
// sections then paint on top of each other.
void EmbedFormerTabPage(QVBoxLayout* dest, QTabWidget* tabs, QWidget* page) {
  if (dest == nullptr || page == nullptr) {
    return;
  }
  DetachTabPage(tabs, page);
  if (auto* src = page->layout()) {
    src->setContentsMargins(0, 0, 0, 0);
    src->setSpacing(std::min(src->spacing(), 8));
    for (int i = src->count() - 1; i >= 0; --i) {
      QLayoutItem* item = src->itemAt(i);
      if (item == nullptr || item->spacerItem() == nullptr) {
        continue;
      }
      QSpacerItem* spacer = item->spacerItem();
      if (spacer->sizePolicy().verticalPolicy() == QSizePolicy::Expanding) {
        delete src->takeAt(i);
      }
    }
  }
  page->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  if (QWidget* parent = dest->parentWidget()) {
    page->setParent(parent);
  }
  page->show();
  dest->addWidget(page);
}

// Official wordmark, authored as SVG and shipped as a high-DPI raster (this
// dialog does not yet bundle QtSvg). Scaled with right-side clearance in the
// sidebar.
QPixmap LoadPreferencesBrandLogo(bool dark, int logical_width, qreal dpr) {
  const QPixmap src(dark ? QStringLiteral(":/preferences_header_logo_dark.png")
                         : QStringLiteral(":/preferences_header_logo.png"));
  if (src.isNull() || logical_width <= 0) {
    return QPixmap();
  }
  const qreal ratio = dpr > 0 ? dpr : 1.0;
  const int pixel_w = std::max(1, qRound(logical_width * ratio));
  QPixmap scaled = src.scaledToWidth(pixel_w, Qt::SmoothTransformation);
  scaled.setDevicePixelRatio(ratio);
  return scaled;
}

// Pull glyph ink flush to the layout edge. Large/serif capitals often have a
// positive left bearing, which made page/section titles look indented past
// body labels and checkbox indicators.
void FlushLeftInk(QLabel* label) {
  if (label == nullptr) {
    return;
  }
  label->setIndent(0);
  label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  const QString text = label->text();
  if (text.isEmpty()) {
    label->setContentsMargins(0, 0, 0, 0);
    return;
  }
  const QFontMetrics metrics(label->font());
  const QChar ch = text.at(0);
  const int bearing = metrics.leftBearing(ch);
  const int ink = metrics.tightBoundingRect(QString(ch)).x();
  label->setContentsMargins(-std::max({0, bearing, ink}), 0, 0, 0);
}

void AddSectionGap(QVBoxLayout* page_layout, bool first_on_page) {
  if (page_layout == nullptr || first_on_page) {
    return;
  }
  page_layout->addSpacing(config_dialog_theme::Spacing::before_section);
}

QFrame* MakeIaSectionHeader(const QString& title, QWidget* parent) {
  auto* header = new QFrame(parent);
  header->setProperty("class", QStringLiteral("setting-group-header"));
  header->setFrameShape(QFrame::NoFrame);
  header->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
  auto* layout = new QHBoxLayout(header);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(config_dialog_theme::Spacing::heading_rule_gap);
  auto* label = new QLabel(title, header);
  label->setFont(config_dialog_theme::SectionFont());
  config_dialog_theme::EnsureHeadingMetrics(label);
  FlushLeftInk(label);
  layout->addWidget(label, 0, Qt::AlignLeft | Qt::AlignVCenter);
  auto* line = new QFrame(header);
  line->setProperty("class", QStringLiteral("setting-group-line"));
  line->setFrameShape(QFrame::HLine);
  line->setFixedHeight(1);
  layout->addWidget(line, 1, Qt::AlignVCenter);
  return header;
}

// Level 3: no rule, semibold — subordinate to Level 2, stronger than labels.
QLabel* MakeIaSectionSubhead(const QString& title, QWidget* parent) {
  auto* label = new QLabel(title, parent);
  label->setObjectName(QStringLiteral("preferencesSubhead"));
  label->setFont(config_dialog_theme::SubheadFont());
  config_dialog_theme::EnsureHeadingMetrics(label);
  FlushLeftInk(label);
  return label;
}

void AddMajorSection(QVBoxLayout* page_layout, const QString& title,
                     QWidget* page, bool* first_section) {
  AddSectionGap(page_layout, first_section != nullptr && *first_section);
  if (first_section != nullptr) {
    *first_section = false;
  }
  page_layout->addWidget(MakeIaSectionHeader(title, page));
  page_layout->addSpacing(config_dialog_theme::Spacing::section_to_control);
}

void AddMinorSubhead(QVBoxLayout* page_layout, const QString& title,
                     QWidget* page) {
  page_layout->addSpacing(config_dialog_theme::Spacing::before_subhead);
  page_layout->addWidget(MakeIaSectionSubhead(title, page));
  page_layout->addSpacing(config_dialog_theme::Spacing::subhead_to_control);
}

// Label (or checkbox) + ⓘ as one compact typographic unit — never a
// right-edge layout citizen. The slot width is reserved even when the
// symbol is hidden so toggling explanations does not shift controls.
QWidget* MakeInlineAnnotatedLabel(
    QWidget* label, PreferencesExplanationController* explanations,
    const char* explanation_id, QWidget* parent) {
  auto* group = new QWidget(parent);
  group->setObjectName(QStringLiteral("preferencesAnnotatedLabel"));
  group->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
  auto* layout = new QHBoxLayout(group);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(config_dialog_theme::Spacing::info_gap);
  if (label != nullptr) {
    TakeFromLayout(label);
    if (auto* text = qobject_cast<QLabel*>(label)) {
      text->setFont(config_dialog_theme::LabelFont());
      FlushLeftInk(text);
      text->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    } else if (auto* box = qobject_cast<QCheckBox*>(label)) {
      box->setContentsMargins(0, 0, 0, 0);
      box->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    }
    layout->addWidget(label, 0, Qt::AlignLeft | Qt::AlignVCenter);
  }
  if (explanations != nullptr) {
    layout->addWidget(explanations->MakeInfoSlot(explanation_id, group), 0,
                      Qt::AlignVCenter);
  }
  return group;
}

QWidget* MakeLabelControlRow(QWidget* label, QWidget* control, QWidget* parent,
                             PreferencesExplanationController* explanations,
                             const char* explanation_id = nullptr) {
  auto* row = new QWidget(parent);
  row->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  auto* layout = new QHBoxLayout(row);
  layout->setContentsMargins(0, config_dialog_theme::Spacing::unit / 2, 0,
                             config_dialog_theme::Spacing::unit / 2);
  layout->setSpacing(12);
  if (label != nullptr) {
    layout->addWidget(
        MakeInlineAnnotatedLabel(label, explanations, explanation_id, row), 0,
        Qt::AlignLeft | Qt::AlignVCenter);
  } else if (explanations != nullptr) {
    // Keep a reserved slot so control columns stay aligned.
    layout->addWidget(explanations->MakeInfoSlot(nullptr, row), 0,
                      Qt::AlignVCenter);
  }
  layout->addStretch(1);
  if (control != nullptr) {
    TakeFromLayout(control);
    layout->addWidget(control, 0, Qt::AlignRight | Qt::AlignVCenter);
  }
  return row;
}

// Content-sized control (typically a checkbox) with ⓘ immediately after its
// text. Do not use this for full-width groups or tables.
QWidget* MakeExplainedControlRow(QWidget* control, QWidget* parent,
                                 PreferencesExplanationController* explanations,
                                 const char* explanation_id) {
  return MakeInlineAnnotatedLabel(control, explanations, explanation_id,
                                  parent);
}

bool AttachExplanationInPlace(QWidget* widget,
                              PreferencesExplanationController* explanations,
                              const char* explanation_id) {
  if (widget == nullptr || explanations == nullptr ||
      explanation_id == nullptr ||
      FindPreferencesExplanation(explanation_id) == nullptr) {
    return false;
  }
  QWidget* parent = widget->parentWidget();
  QLayout* host = parent != nullptr ? parent->layout() : nullptr;
  if (host == nullptr) {
    return false;
  }

  int row = 0;
  int col = 0;
  int row_span = 1;
  int col_span = 1;
  int box_index = -1;
  auto* grid = qobject_cast<QGridLayout*>(host);
  auto* box = qobject_cast<QBoxLayout*>(host);
  if (grid != nullptr) {
    const int index = grid->indexOf(widget);
    if (index < 0) {
      return false;
    }
    grid->getItemPosition(index, &row, &col, &row_span, &col_span);
    grid->removeWidget(widget);
  } else if (box != nullptr) {
    box_index = box->indexOf(widget);
    if (box_index < 0) {
      return false;
    }
    box->removeWidget(widget);
  } else {
    return false;
  }

  auto* wrap = new QWidget(parent);
  wrap->setObjectName(QStringLiteral("preferencesAnnotatedLabel"));
  wrap->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  auto* layout = new QHBoxLayout(wrap);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(config_dialog_theme::Spacing::info_gap);
  widget->setParent(wrap);
  if (auto* check = qobject_cast<QCheckBox*>(widget)) {
    check->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
  }
  layout->addWidget(widget, 0, Qt::AlignLeft | Qt::AlignVCenter);
  layout->addWidget(explanations->MakeInfoSlot(explanation_id, wrap), 0,
                    Qt::AlignVCenter);
  layout->addStretch(1);

  if (grid != nullptr) {
    grid->addWidget(wrap, row, col, row_span, col_span);
  } else if (box != nullptr) {
    box->insertWidget(box_index, wrap);
  }
  return true;
}

QWidget* MakeNewPreferencesPage(QWidget* parent) {
  auto* page = new QWidget(parent);
  auto* layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 0, config_dialog_theme::Spacing::page_right_gutter,
                             config_dialog_theme::Spacing::unit * 2);
  // Rhythm comes from AddMajorSection / AddMinorSubhead / AddToPage gaps.
  layout->setSpacing(0);
  return page;
}

void ClearTabWidget(QTabWidget* tabs) {
  if (tabs == nullptr) {
    return;
  }
  while (tabs->count() > 0) {
    // Detach without deleting; callers reparent the interesting children.
    tabs->removeTab(0);
  }
}

}  // namespace

void ConfigDialog::MigratePreferencesInformationArchitecture() {
  QTabWidget* tabs = configDialogTabWidget;
  if (tabs == nullptr) {
    return;
  }

  PreferencesExplanationController* explanations =
      explanation_controller_.get();
  QWidget* shortcuts_page =
      shortcuts_tab_ != nullptr ? shortcuts_tab_->page() : nullptr;
  QWidget* sync_page = sync_tab_ != nullptr ? sync_tab_->page() : nullptr;
  QWidget* packs_widget =
      findChild<QWidget*>(QStringLiteral("dictionaryPacksWidget"));

  // Capture update controls that were inserted into Misc at runtime.
  QWidget* updates_host = nullptr;
  if (auto_check_for_updates_checkbox_ != nullptr) {
    updates_host = auto_check_for_updates_checkbox_->parentWidget();
  }

  auto* writing = MakeNewPreferencesPage(tabs);
  auto* dictionaries = MakeNewPreferencesPage(tabs);
  auto* keyboard = MakeNewPreferencesPage(tabs);
  auto* suggestions = MakeNewPreferencesPage(tabs);
  auto* privacy_sync = MakeNewPreferencesPage(tabs);
  auto* application = MakeNewPreferencesPage(tabs);

  auto* writing_layout = qobject_cast<QVBoxLayout*>(writing->layout());
  auto* dictionaries_layout =
      qobject_cast<QVBoxLayout*>(dictionaries->layout());
  auto* keyboard_layout = qobject_cast<QVBoxLayout*>(keyboard->layout());
  auto* suggestions_layout =
      qobject_cast<QVBoxLayout*>(suggestions->layout());
  auto* privacy_layout = qobject_cast<QVBoxLayout*>(privacy_sync->layout());
  auto* application_layout =
      qobject_cast<QVBoxLayout*>(application->layout());

  // --- Writing ---
  // Major sections keep a rule; Input/Conversion host quieter subheads so the
  // page does not read as a stack of equally loud headings.
  bool writing_first = true;
  AddMajorSection(writing_layout, tr("Input"), writing, &writing_first);
  AddToPage(writing_layout,
            MakeLabelControlRow(inputModeLabel, inputModeComboBox, writing,
                                explanations, "writing.input_mode"));

  AddMinorSubhead(writing_layout, tr("Punctuation & symbols"), writing);
  AddToPage(writing_layout,
            MakeLabelControlRow(PunctuationsSettingLabel,
                                punctuationsSettingComboBox, writing,
                                explanations, "writing.punctuation_style"));
  AddToPage(writing_layout,
            MakeLabelControlRow(symbolsSettingLabel, symbolsSettingComboBox,
                                writing, explanations, "writing.symbol_style"));

  AddMinorSubhead(writing_layout, tr("Spacing & widths"), writing);
  AddToPage(writing_layout,
            MakeLabelControlRow(spaceInputSettingLabel,
                                spaceCharacterFormComboBox, writing,
                                explanations, "writing.space_style"));
  AddToPage(writing_layout,
            MakeLabelControlRow(numpadCharacterFormLabel,
                                numpadCharacterFormComboBox, writing,
                                explanations, nullptr));
  AddToPage(writing_layout,
            MakeExplainedControlRow(autoSwitchCompositionMode, writing,
                                    explanations, "writing.auto_halfwidth"));

  AddMajorSection(writing_layout, tr("Conversion"), writing, &writing_first);
  {
    // Same left–right pattern as other settings rows: master checkbox on the
    // left, punctuation toggles grouped on the right.
    TakeFromLayout(useAutoConversion);
    TakeFromLayout(kutenCheckBox);
    TakeFromLayout(toutenCheckBox);
    TakeFromLayout(questionMarkCheckBox);
    TakeFromLayout(exclamationMarkCheckBox);
    auto* punct_controls = new QWidget(writing);
    punct_controls->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    auto* punct_layout = new QHBoxLayout(punct_controls);
    punct_layout->setContentsMargins(0, 0, 0, 0);
    punct_layout->setSpacing(10);
    for (QCheckBox* box :
         {kutenCheckBox, toutenCheckBox, questionMarkCheckBox,
          exclamationMarkCheckBox}) {
      // UI file capped these for a dense grid cell; free them for a row.
      box->setMaximumWidth(QWIDGETSIZE_MAX);
      punct_layout->addWidget(box, 0, Qt::AlignVCenter);
    }
    AddToPage(writing_layout,
              MakeLabelControlRow(useAutoConversion, punct_controls, writing,
                                  explanations, "writing.auto_conversion"));
  }
  if (miscStartKanjiModeWidget != nullptr) {
    // Live/manual conversion lives in the same Misc block as shin/kyū.
    TakeFromLayout(startLiveModeLabel);
    TakeFromLayout(startLiveModeComboBox);
    AddToPage(writing_layout,
              MakeLabelControlRow(startLiveModeLabel, startLiveModeComboBox,
                                  writing, explanations,
                                  "writing.initial_conversion_mode"));
  }
  AddToPage(writing_layout,
            MakeExplainedControlRow(twoStepCommitCheckBox, writing,
                                    explanations, "writing.two_step_enter"));

  AddMinorSubhead(writing_layout, tr("Kanji forms"), writing);
  if (widget_start_kanji != nullptr) {
    // Prefer the labelled shin/kyū row when present.
    TakeFromLayout(startKanjiModeLabel);
    TakeFromLayout(startKanjiModeComboBox);
    AddToPage(writing_layout,
              MakeLabelControlRow(startKanjiModeLabel, startKanjiModeComboBox,
                                  writing, explanations, "writing.kanji_style"));
  } else if (miscStartKanjiModeWidget != nullptr) {
    AddToPage(writing_layout, miscStartKanjiModeWidget);
  }

  AddMajorSection(writing_layout, tr("Detailed width rules"), writing,
                  &writing_first);
  if (explanations != nullptr) {
    auto* width_hint = new QWidget(writing);
    auto* width_hint_layout = new QHBoxLayout(width_hint);
    width_hint_layout->setContentsMargins(0, 0, 0, 0);
    width_hint_layout->setSpacing(8);
    auto* width_label = new QLabel(
        tr("Per-group fullwidth and halfwidth defaults"), width_hint);
    width_label->setObjectName(QStringLiteral("preferencesSecondary"));
    width_label->setFont(config_dialog_theme::UiFont(
        config_dialog_theme::Type::secondary));
    width_hint_layout->addWidget(width_label, 0, Qt::AlignVCenter);
    width_hint_layout->addWidget(
        explanations->MakeInfoSlot("writing.width_rules", width_hint), 0,
        Qt::AlignVCenter);
    width_hint_layout->addStretch(1);
    AddToPage(writing_layout, width_hint);
  }
  AddToPage(writing_layout, fullwidthHalfwidthGroup);

  AddMajorSection(writing_layout, tr("Candidate appearance"), writing,
                  &writing_first);
  AddToPage(writing_layout, appearanceGroup);
  AddToPage(writing_layout, miscCandidateWindowWidget);
  writing_layout->addSpacing(config_dialog_theme::Spacing::unit * 2);

  // --- Dictionaries ---
  // Rebuild Learning as a compact label/control row (the old grid left a tall
  // spacer cell that read as extra padding under the section header).
  bool dictionaries_first = true;
  AddMajorSection(dictionaries_layout, tr("Learning"), dictionaries,
                  &dictionaries_first);
  {
    TakeFromLayout(label_7);
    TakeFromLayout(historyLearningLevelComboBox);
    TakeFromLayout(clearUserHistoryButton);
    label_7->setWordWrap(false);
    AddToPage(dictionaries_layout,
              MakeLabelControlRow(label_7, historyLearningLevelComboBox,
                                  dictionaries, explanations,
                                  "dictionaries.learning"));
    auto* clear_row = new QWidget(dictionaries);
    auto* clear_layout = new QHBoxLayout(clear_row);
    clear_layout->setContentsMargins(0, 0, 0, 2);
    clear_layout->addStretch(1);
    clear_layout->addWidget(clearUserHistoryButton);
    dictionaries_layout->addWidget(clear_row);
    if (personalizationGroup != nullptr) {
      personalizationGroup->hide();
    }
  }

  AddMajorSection(dictionaries_layout, tr("Personal dictionary"), dictionaries,
                  &dictionaries_first);
  AddToPage(dictionaries_layout, userDictionaryGroup);

  AddMajorSection(dictionaries_layout, tr("Dictionary sources"), dictionaries,
                  &dictionaries_first);
  AddToPage(dictionaries_layout, usageDictionaryGroup);
  AttachExplanationInPlace(localUsageDictionaryCheckBox, explanations,
                           "dictionaries.usage");

  AddMinorSubhead(dictionaries_layout, tr("Scholarly vocabulary"),
                  dictionaries);
  if (packs_widget != nullptr) {
    if (auto* packs_layout = packs_widget->layout()) {
      packs_layout->setContentsMargins(0, 2, 0, 2);
    }
    AddToPage(dictionaries_layout, packs_widget);
    const QList<QCheckBox*> pack_boxes =
        packs_widget->findChildren<QCheckBox*>();
    if (!pack_boxes.isEmpty()) {
      AttachExplanationInPlace(pack_boxes.first(), explanations,
                               "dictionaries.scholarly");
    }
  }

  AddMinorSubhead(dictionaries_layout, tr("Special conversions"), dictionaries);
  AddToPage(dictionaries_layout, specialConversionsGroup);
  AttachExplanationInPlace(emojiConversionCheckBox, explanations,
                           "dictionaries.special");
  dictionaries_layout->addSpacing(config_dialog_theme::Spacing::unit * 2);

  // --- Keyboard ---
  bool keyboard_first = true;
  AddMajorSection(keyboard_layout, tr("Keymap"), keyboard, &keyboard_first);
  AddToPage(keyboard_layout, keymapGroup);

  AddMinorSubhead(keyboard_layout, tr("Key behaviour"), keyboard);
  AddToPage(keyboard_layout,
            MakeLabelControlRow(yenSignLabel, yenSignComboBox, keyboard,
                                explanations, nullptr));
  AddToPage(keyboard_layout,
            MakeLabelControlRow(keyboardLayoutLabel, keyboardLayoutComboBox,
                                keyboard, explanations, nullptr));
  AddToPage(keyboard_layout, useJapaneseLayout);

  AddMinorSubhead(keyboard_layout, tr("Candidate selection"), keyboard);
  AddToPage(keyboard_layout,
            MakeLabelControlRow(selectionShortcutModeLabel,
                                selectionShortcutModeComboBox, keyboard,
                                explanations, nullptr));

  // Mode switching: page-level controls, then the Shortcuts block without
  // repeating its own Mode switching heading.
  AddMinorSubhead(keyboard_layout, tr("Mode switching"), keyboard);
  AddToPage(keyboard_layout,
            MakeLabelControlRow(autoCorrectionLable_3,
                                shiftKeyModeSwitchComboBox, keyboard,
                                explanations, "keyboard.shift_mode_switch"));
  AddToPage(keyboard_layout,
            MakeExplainedControlRow(useModeIndicator, keyboard, explanations,
                                    "keyboard.mode_indicator"));
  if (shortcuts_page != nullptr) {
    // Keep Shortcuts as one intact block (Custom / Kaeriten). Do not
    // disassemble its layout — that caused overlapping text.
    keyboard_layout->addSpacing(
        config_dialog_theme::Spacing::before_subhead);
    EmbedFormerTabPage(keyboard_layout, tabs, shortcuts_page);
    for (QLabel* label : shortcuts_page->findChildren<QLabel*>()) {
      if (label->text() == tr("Mode switching")) {
        if (QWidget* header = label->parentWidget()) {
          header->hide();
        }
        break;
      }
    }
  }
  keyboard_layout->addSpacing(config_dialog_theme::Spacing::unit * 2);

  // --- Suggestions ---
  bool suggestions_first = true;
  auto header_title = [](QFrame* header, const QString& fallback) {
    if (header != nullptr) {
      for (QLabel* label : header->findChildren<QLabel*>()) {
        if (!label->text().isEmpty()) {
          return label->text();
        }
      }
    }
    return fallback;
  };
  if (suggestSourceHeader != nullptr) {
    suggestSourceHeader->hide();
  }
  AddMajorSection(suggestions_layout,
                  header_title(suggestSourceHeader, tr("Suggestion sources")),
                  suggestions, &suggestions_first);
  AddToPage(suggestions_layout, suggestSourceGroup);
  AttachExplanationInPlace(historySuggestCheckBox, explanations,
                           "suggestions.history");
  AttachExplanationInPlace(dictionarySuggestCheckBox, explanations,
                           "suggestions.system_dictionary");
  AttachExplanationInPlace(realtimeConversionCheckBox, explanations,
                           "suggestions.realtime");
  if (suggestOtherHeader != nullptr) {
    suggestOtherHeader->hide();
  }
  AddMajorSection(suggestions_layout,
                  header_title(suggestOtherHeader, tr("Other settings")),
                  suggestions, &suggestions_first);
  AddToPage(suggestions_layout, suggestOtherGroup);
  suggestions_layout->addSpacing(config_dialog_theme::Spacing::unit * 2);

  // --- Privacy & Sync ---
  // Rebuild each block flush-left. The old UI paired an empty checkbox with a
  // separate label (Secret mode) and left default layout margins on headers,
  // which read as a staggered indent next to normal checkboxes.
  bool privacy_first = true;
  if (incognitoModeHeader != nullptr) {
    incognitoModeHeader->hide();
  }
  AddMajorSection(privacy_layout, tr("Secret mode"), privacy_sync,
                  &privacy_first);
  if (incognitoModeCheckBox != nullptr) {
    TakeFromLayout(incognitoModeCheckBox);
    if (incognitoModeMessage != nullptr) {
      incognitoModeCheckBox->setText(incognitoModeMessage->text());
      TakeFromLayout(incognitoModeMessage);
      incognitoModeMessage->hide();
    }
    incognitoModeCheckBox->setSizePolicy(QSizePolicy::Preferred,
                                         QSizePolicy::Preferred);
    AddToPage(privacy_layout,
              MakeExplainedControlRow(incognitoModeCheckBox, privacy_sync,
                                      explanations, "privacy.secret_mode"));
  }
  if (incognitoModeGroup != nullptr) {
    incognitoModeGroup->hide();
  }

  if (presentationModeHeader != nullptr) {
    presentationModeHeader->hide();
  }
  AddMajorSection(privacy_layout, tr("Presentation mode"), privacy_sync,
                  &privacy_first);
  if (presentationModeCheckBox != nullptr) {
    TakeFromLayout(presentationModeCheckBox);
    AddToPage(privacy_layout,
              MakeExplainedControlRow(presentationModeCheckBox, privacy_sync,
                                      explanations,
                                      "privacy.presentation_mode"));
  }
  if (presentationModeGroup != nullptr) {
    presentationModeGroup->hide();
  }

  if (usageStatsHeader != nullptr) {
    usageStatsHeader->hide();
  }
  AddMajorSection(privacy_layout, tr("Usage statistics and crash reports"),
                  privacy_sync, &privacy_first);
  if (usageStatsMessage != nullptr) {
    TakeFromLayout(usageStatsMessage);
    usageStatsMessage->setContentsMargins(
        0, config_dialog_theme::Spacing::label_to_help / 2, 0,
        config_dialog_theme::Spacing::label_to_help);
    AddToPage(privacy_layout, usageStatsMessage);
  }
  if (usageStatsMessageGroup != nullptr) {
    usageStatsMessageGroup->hide();
  }
  if (usageStatsButtonGroup != nullptr) {
    AddToPage(privacy_layout, usageStatsButtonGroup);
  }

  AddMajorSection(privacy_layout, tr("Synchronisation"), privacy_sync,
                  &privacy_first);
  if (explanations != nullptr) {
    auto* sync_hint = new QWidget(privacy_sync);
    auto* sync_hint_layout = new QHBoxLayout(sync_hint);
    sync_hint_layout->setContentsMargins(0, 0, 0, 0);
    sync_hint_layout->setSpacing(8);
    auto* sync_label = new QLabel(
        tr("Encrypted dictionary and history sync"), sync_hint);
    sync_label->setObjectName(QStringLiteral("preferencesSecondary"));
    sync_label->setFont(config_dialog_theme::UiFont(
        config_dialog_theme::Type::secondary));
    sync_hint_layout->addWidget(sync_label, 0, Qt::AlignVCenter);
    sync_hint_layout->addWidget(
        explanations->MakeInfoSlot("privacy.sync", sync_hint), 0,
        Qt::AlignVCenter);
    sync_hint_layout->addStretch(1);
    AddToPage(privacy_layout, sync_hint);
  }
  if (sync_page != nullptr) {
    EmbedFormerTabPage(privacy_layout, tabs, sync_page);
  }
  privacy_layout->addSpacing(config_dialog_theme::Spacing::unit * 2);

  // --- Application ---
  bool application_first = true;
  AddMajorSection(application_layout, tr("Updates"), application,
                  &application_first);
  if (updates_host != nullptr) {
    if (auto* updates_layout = updates_host->layout()) {
      updates_layout->setContentsMargins(0, 2, 0, 2);
    }
    AddToPage(application_layout, updates_host);
  }
  AddMajorSection(application_layout, tr("Startup"), application,
                  &application_first);
  // Prefer the checkbox group alone so the nested "Startup" header is not
  // duplicated under our section title.
  if (startupGroup != nullptr) {
    if (startupHeader != nullptr) {
      startupHeader->hide();
    }
    AddToPage(application_layout, startupGroup);
    if (miscStartupWidget != nullptr) {
      miscStartupWidget->hide();
    }
  } else {
    AddToPage(application_layout, miscStartupWidget);
  }
  // System hosts platform-specific blocks; skip the section when all are hidden
  // (e.g. macOS release builds hide IME, admin, and NDEBUG logging).
  const bool show_system =
      (miscDefaultIMEWidget != nullptr && miscDefaultIMEWidget->isVisible()) ||
      (miscAdministrationWidget != nullptr &&
       miscAdministrationWidget->isVisible()) ||
      (miscLoggingWidget != nullptr && miscLoggingWidget->isVisible());
  if (show_system) {
    AddMajorSection(application_layout, tr("System"), application,
                    &application_first);
    AddToPage(application_layout, miscDefaultIMEWidget);
    AddToPage(application_layout, miscAdministrationWidget);
    // Keep the nested "Logging" subheading under System; flatten margins only.
    AddToPage(application_layout, miscLoggingWidget);
  }
  application_layout->addSpacing(config_dialog_theme::Spacing::unit * 2);

  // Drop empty/old tab shells; widgets have been reparented above.
  ClearTabWidget(tabs);
  // Keep orphaned shells hidden. Do not deleteLater: any missed reparent
  // would destroy live controls once the event loop starts.
  for (QWidget* old :
       {static_cast<QWidget*>(basicTab), static_cast<QWidget*>(dictionaryTab),
        static_cast<QWidget*>(inputSupportTab),
        static_cast<QWidget*>(suggestionsTab),
        static_cast<QWidget*>(privacyTab), static_cast<QWidget*>(miscTab)}) {
    if (old != nullptr) {
      old->hide();
      old->setParent(this);
    }
  }

  tabs->addTab(writing, tr("Writing"));
  tabs->addTab(dictionaries, tr("Dictionaries"));
  tabs->addTab(keyboard, tr("Keyboard"));
  tabs->addTab(suggestions, tr("Suggestions"));
  tabs->addTab(privacy_sync, tr("Privacy & Sync"));
  tabs->addTab(application, tr("Application"));
  tabs->setCurrentIndex(0);
}

void ConfigDialog::InstallPreferencesShell() {
  QTabWidget* tabs = configDialogTabWidget;

  using Spacing = config_dialog_theme::Spacing;
  constexpr int kSidebarWidth = Spacing::sidebar_width;
  constexpr int kSidebarPadL = Spacing::sidebar_pad_l;
  constexpr int kSidebarPadR = Spacing::sidebar_pad_r;
  auto* sidebar = new QWidget(this);
  sidebar->setObjectName(QStringLiteral("preferencesSidebar"));
  sidebar->setFixedWidth(kSidebarWidth);
  sidebar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  sidebar->setAutoFillBackground(true);

  auto* side_layout = new QVBoxLayout(sidebar);
  side_layout->setContentsMargins(kSidebarPadL, Spacing::sidebar_pad_t,
                                  kSidebarPadR, Spacing::sidebar_pad_b);
  side_layout->setSpacing(0);

  auto* brand = new QLabel(sidebar);
  brand->setObjectName(QStringLiteral("preferencesBrand"));
  brand->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  brand->setScaledContents(false);
  {
    const bool dark = config_dialog_theme::IsDark();
    // ~9% smaller than full sidebar inner width; keeps ≥22 px right clearance.
    const int sidebar_inner = kSidebarWidth - kSidebarPadL - kSidebarPadR;
    const int logo_w =
        sidebar_inner - Spacing::logo_right_clearance;
    const QPixmap logo =
        LoadPreferencesBrandLogo(dark, logo_w, brand->devicePixelRatioF());
    if (!logo.isNull()) {
      brand->setPixmap(logo);
      brand->setFixedHeight(qRound(logo.height() / logo.devicePixelRatioF()));
      brand->setFixedWidth(logo_w);
    } else {
      brand->setText(GuiUtil::ProductName());
      brand->setFont(config_dialog_theme::DisplayFont(17));
    }
  }
  auto* brand_sub = new QLabel(tr("Preferences"), sidebar);
  brand_sub->setObjectName(QStringLiteral("preferencesBrandSub"));
  brand_sub->setFont(config_dialog_theme::UiFont(
      config_dialog_theme::Type::brand_sub, QFont::Normal));
  side_layout->addWidget(brand);
  side_layout->addSpacing(Spacing::logo_to_prefs);
  side_layout->addWidget(brand_sub);
  side_layout->addSpacing(Spacing::prefs_to_nav);

  auto* nav = new QListWidget(sidebar);
  nav->setObjectName(QStringLiteral("preferencesNav"));
  nav->setFrameShape(QFrame::NoFrame);
  nav->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  nav->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  nav->setFocusPolicy(Qt::StrongFocus);
  nav->setWordWrap(true);
  nav->setSpacing(0);
  nav->setUniformItemSizes(true);
  nav->setAccessibleName(tr("Preferences"));
  nav->setFont(config_dialog_theme::UiFont(config_dialog_theme::Type::nav));
  nav->blockSignals(true);
  for (int i = 0; i < tabs->count(); ++i) {
    auto* item = new QListWidgetItem(tabs->tabText(i));
    item->setSizeHint(QSize(0, Spacing::nav_row));
    nav->addItem(item);
  }
  nav->setCurrentRow(tabs->currentIndex());
  nav->blockSignals(false);
  side_layout->addWidget(nav, 1);

  // Utilities: bottom-anchored via nav stretch; quieter than primary nav.
  auto* utility_rule = new QFrame(sidebar);
  utility_rule->setObjectName(QStringLiteral("preferencesUtilityRule"));
  utility_rule->setFrameShape(QFrame::NoFrame);
  utility_rule->setFixedHeight(1);
  side_layout->addWidget(utility_rule);
  side_layout->addSpacing(Spacing::utility_rule_gap);

  auto make_utility_button = [&](const QString& text) {
    auto* button = new QPushButton(text, sidebar);
    button->setObjectName(QStringLiteral("preferencesUtilityButton"));
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::StrongFocus);
    button->setFlat(true);
    button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    button->setFixedHeight(Spacing::utility_row);
    button->setMinimumWidth(kSidebarWidth - kSidebarPadL - kSidebarPadR);
    button->setFont(
        config_dialog_theme::UiFont(config_dialog_theme::Type::utility));
    return button;
  };

  auto* explanations_button = make_utility_button(tr("Show explanations"));
  auto* reset_button = make_utility_button(tr("Reset to defaults"));

  auto sync_explanations_button = [explanations_button, this]() {
    const bool on = explanation_controller_ != nullptr &&
                    explanation_controller_->enabled();
    explanations_button->setText(on ? tr("Hide explanations")
                                    : tr("Show explanations"));
    explanations_button->setProperty("active", on);
    explanations_button->style()->unpolish(explanations_button);
    explanations_button->style()->polish(explanations_button);
  };
  sync_explanations_button();
  if (explanation_controller_ != nullptr) {
    connect(explanation_controller_.get(),
            &PreferencesExplanationController::enabledChanged, this,
            [sync_explanations_button](bool) { sync_explanations_button(); });
  }
  connect(explanations_button, &QPushButton::clicked, this, [this]() {
    if (explanation_controller_ != nullptr) {
      explanation_controller_->ToggleEnabled();
    }
  });

  // Move the existing Reset control into the utility region (same slot).
  TakeFromLayout(resetToDefaultsButton);
  resetToDefaultsButton->hide();
  connect(reset_button, &QPushButton::clicked, this,
          &ConfigDialog::ResetToDefaults);

  side_layout->addWidget(explanations_button, 0, Qt::AlignLeft);
  side_layout->addSpacing(Spacing::utility_gap);
  side_layout->addWidget(reset_button, 0, Qt::AlignLeft);

  auto* rule = new QFrame(this);
  rule->setObjectName(QStringLiteral("preferencesSidebarRule"));
  rule->setFrameShape(QFrame::NoFrame);
  rule->setFixedWidth(1);

  auto* content = new QWidget(this);
  content->setObjectName(QStringLiteral("preferencesContent"));
  content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  content->setAutoFillBackground(true);
  auto* content_layout = new QVBoxLayout(content);
  // One shared content inset: page title, section headers, labels, and
  // checkbox indicators all share this left edge.
  content_layout->setContentsMargins(
      Spacing::content_inset_l, Spacing::content_inset_t,
      Spacing::content_inset_r, Spacing::content_inset_b);
  content_layout->setSpacing(0);

  auto* title = new QLabel(tabs->tabText(tabs->currentIndex()), content);
  title->setObjectName(QStringLiteral("preferencesPageTitle"));
  title->setFont(config_dialog_theme::PageTitleFont());
  // Serif display faces need an explicit height; otherwise the first paint on
  // macOS often clips the ascenders until something forces a relayout.
  {
    const QFontMetrics title_metrics(title->font());
    title->setMinimumHeight(title_metrics.height() + 8);
    title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  }
  FlushLeftInk(title);
  content_layout->addWidget(title, 0, Qt::AlignLeft);
  content_layout->addSpacing(Spacing::title_to_rule);

  auto* title_rule = new QFrame(content);
  title_rule->setObjectName(QStringLiteral("preferencesTitleRule"));
  title_rule->setFrameShape(QFrame::NoFrame);
  title_rule->setFixedHeight(1);
  content_layout->addWidget(title_rule);
  content_layout->addSpacing(Spacing::rule_to_section);

  // setupUi builds these as vertical boxes. Narrow to QVBoxLayout for insert
  // and stretch APIs that the base QLayout type does not expose.
  auto* root = qobject_cast<QVBoxLayout*>(layout());
  auto* upper =
      root == nullptr || root->count() == 0 || root->itemAt(0) == nullptr
          ? nullptr
          : qobject_cast<QVBoxLayout*>(root->itemAt(0)->layout());
  if (upper == nullptr || root == nullptr) {
    sidebar->hide();
    rule->hide();
    content->hide();
    ApplyPreferencesChrome();
    return;
  }

  if (QTabBar* tab_bar = tabs->tabBar()) {
    tab_bar->hide();
    tab_bar->setFixedHeight(0);
  }
  tabs->setDocumentMode(true);

  upper->removeWidget(tabs);
  upper->setContentsMargins(0, 0, 0, 0);
  upper->setSpacing(0);
  auto* body = new QWidget(this);
  body->setObjectName(QStringLiteral("preferencesBody"));
  body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  auto* row = new QHBoxLayout(body);
  row->setContentsMargins(0, 0, 0, 0);
  row->setSpacing(0);
  row->addWidget(sidebar);
  row->addWidget(rule);
  row->addWidget(content, 1);
  upper->addWidget(body);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);
  root->setStretch(0, 1);

  // Drop Cancel · Apply · OK and the entire footer region so the sidebar and
  // content extend to the window's bottom corners (macOS settings style).
  if (auto* footer = qobject_cast<QHBoxLayout*>(
          root->count() >= 2 && root->itemAt(1) != nullptr
              ? root->itemAt(1)->layout()
              : nullptr)) {
    while (footer->count() > 0) {
      QLayoutItem* item = footer->takeAt(0);
      if (item == nullptr) {
        continue;
      }
      if (QWidget* widget = item->widget()) {
        widget->hide();
        if (widget == configDialogButtonBox || widget == resetToDefaultsButton) {
          // reset lives in the sidebar; button box is unused with autosave.
        }
      }
      delete item;
    }
    footer->setContentsMargins(0, 0, 0, 0);
    footer->setSpacing(0);
    if (QLayoutItem* footer_item = root->takeAt(1)) {
      delete footer_item;
    }
  }
  configDialogButtonBox->hide();

  // QTabWidget forces every page to the tallest page's height. Short pages
  // then stretch and open huge vertical gaps. Move pages into a stack that
  // keeps each page at its natural height inside the scroll area.
  auto* stack = new QStackedWidget(content);
  stack->setObjectName(QStringLiteral("preferencesStack"));
  stack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
  QStringList page_titles;
  while (tabs->count() > 0) {
    page_titles.append(tabs->tabText(0));
    QWidget* page = tabs->widget(0);
    tabs->removeTab(0);
    if (page == nullptr) {
      continue;
    }
    page->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    if (auto* page_layout = qobject_cast<QVBoxLayout*>(page->layout())) {
      // Comfortable bottom inset so the last control is not flush to the edge
      // once the former footer is gone (~28–36 logical px).
      page_layout->setContentsMargins(0, 0, Spacing::page_right_gutter,
                                      Spacing::page_bottom_pad);
      page_layout->setSpacing(0);
      for (int item = page_layout->count() - 1; item >= 0; --item) {
        QLayoutItem* layout_item = page_layout->itemAt(item);
        if (layout_item == nullptr || layout_item->spacerItem() == nullptr) {
          continue;
        }
        if (layout_item->spacerItem()->sizePolicy().verticalPolicy() ==
            QSizePolicy::Expanding) {
          delete page_layout->takeAt(item);
        }
      }
    }
    // Prevent setting groups from expanding into empty viewport space (that
    // was the large gap between section headers and their controls).
    for (QWidget* child : page->findChildren<QWidget*>()) {
      const QSizePolicy policy = child->sizePolicy();
      if (policy.verticalPolicy() == QSizePolicy::Expanding) {
        child->setSizePolicy(policy.horizontalPolicy(), QSizePolicy::Maximum);
      }
    }
    stack->addWidget(page);
  }
  tabs->hide();

  auto* scroll_host = new QWidget(content);
  scroll_host->setObjectName(QStringLiteral("preferencesScrollHost"));
  scroll_host->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
  auto* scroll_host_layout = new QVBoxLayout(scroll_host);
  scroll_host_layout->setContentsMargins(0, 0, 0, 0);
  scroll_host_layout->setSpacing(0);
  // AlignLeft|AlignTop: AlignTop alone can centre the stack horizontally and
  // leave a large empty left margin on short pages.
  scroll_host_layout->addWidget(stack, 0, Qt::AlignTop | Qt::AlignLeft);
  scroll_host_layout->addStretch(1);

  auto* scroll = new QScrollArea(content);
  scroll->setObjectName(QStringLiteral("preferencesScroll"));
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setWidgetResizable(true);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  scroll->setWidget(scroll_host);
  content_layout->addWidget(scroll, 1);

  auto sync_page = [this, nav, title, stack, page_titles](int index) {
    if (index < 0 || index >= stack->count()) {
      return;
    }
    if (stack->currentIndex() != index) {
      stack->setCurrentIndex(index);
    }
    if (nav->currentRow() != index) {
      nav->setCurrentRow(index);
    }
    if (index < page_titles.size()) {
      title->setText(page_titles.at(index));
      FlushLeftInk(title);
    }
    if (explanation_controller_ != nullptr) {
      explanation_controller_->OnPreferencesPageChanged();
    }
    UpdatePreferencesScrollPolicy();
  };

  connect(nav, &QListWidget::currentRowChanged, this,
          [sync_page](int row) { sync_page(row); });
  connect(stack, &QStackedWidget::currentChanged, this,
          [sync_page](int index) { sync_page(index); });
  setTabOrder(nav, scroll);
  if (stack->count() > 0) {
    sync_page(0);
  }

  const QList<QFrame*> frames = findChildren<QFrame*>();
  for (QFrame* frame : frames) {
    if (frame->property("class").toString() !=
        QLatin1String("setting-group-header")) {
      continue;
    }
    const QList<QLabel*> labels = frame->findChildren<QLabel*>();
    for (QLabel* label : labels) {
      label->setFont(config_dialog_theme::SectionFont());
      config_dialog_theme::EnsureHeadingMetrics(label);
      FlushLeftInk(label);
    }
  }

  // Keep checkbox widgets flush with the shared content column (indicator at
  // the same left edge as labels and section headers).
  for (QCheckBox* box : findChildren<QCheckBox*>()) {
    if (box == nullptr) {
      continue;
    }
    bool inside_table = false;
    for (QWidget* ancestor = box; ancestor != nullptr;
         ancestor = ancestor->parentWidget()) {
      if (qobject_cast<QTableWidget*>(ancestor) != nullptr) {
        inside_table = true;
        break;
      }
    }
    if (inside_table) {
      continue;
    }
    box->setContentsMargins(0, 0, 0, 0);
  }

  auto mark = [](QPushButton* button, const char* role) {
    if (button == nullptr) {
      return;
    }
    button->setProperty("role", QString::fromLatin1(role));
    button->style()->unpolish(button);
    button->style()->polish(button);
  };
  mark(editKeymapButton, "secondary");
  mark(editRomanTableButton, "secondary");
  mark(editUserDictionaryButton, "secondary");
  mark(editUserSymbolsButton, "secondary");
  mark(clearUserHistoryButton, "quiet");

  resize(1020, 700);
  setMinimumSize(880, 560);

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
          [this](Qt::ColorScheme) {
            ApplyPreferencesChrome();
            RefinePreferencesControls();
          });
#endif
  // Stylesheet first: combo sizeHint must include QSS padding/chevron, or
  // labels such as "Follow input mode" get clipped.
  ApplyPreferencesChrome();
  RefinePreferencesControls();
  QTimer::singleShot(0, this, &ConfigDialog::UpdatePreferencesScrollPolicy);
}

void ConfigDialog::RefinePreferencesControls() {
  // Content-driven combo widths from Qt's sizeHint (after stylesheet polish).
  // Skip combos inside tables — fixed widths there clip under the scrollbar.
  auto is_inside_table = [](QWidget* widget) {
    for (QWidget* ancestor = widget; ancestor != nullptr;
         ancestor = ancestor->parentWidget()) {
      if (qobject_cast<QTableWidget*>(ancestor) != nullptr) {
        return true;
      }
    }
    return false;
  };
  const QList<QComboBox*> combos = findChildren<QComboBox*>();
  for (QComboBox* combo : combos) {
    if (combo == nullptr || is_inside_table(combo)) {
      continue;
    }
    combo->setMaximumWidth(QWIDGETSIZE_MAX);
    combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    combo->style()->unpolish(combo);
    combo->style()->polish(combo);
    // Prefer the wider of Qt's sizeHint and an explicit text+QSS measure.
    // QSS uses padding 6px 28px 6px 11px plus a 22px drop-down slot.
    const QFontMetrics metrics(combo->font());
    int text_w = metrics.horizontalAdvance(QStringLiteral("MMMMMMMM"));
    for (int i = 0; i < combo->count(); ++i) {
      text_w =
          std::max(text_w, metrics.horizontalAdvance(combo->itemText(i)));
    }
    constexpr int kQssChrome = 11 + 28 + 2 + 22;  // pads + borders + arrow
    constexpr int kMin = 128;
    constexpr int kMax = 420;
    const int from_text = text_w + kQssChrome;
    const int from_hint = combo->sizeHint().width() + 12;
    const int width =
        std::clamp(std::max(from_text, from_hint), kMin, kMax);
    combo->setFixedWidth(width);
    combo->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  }

  const QList<QSpinBox*> spins = findChildren<QSpinBox*>();
  for (QSpinBox* spin : spins) {
    if (spin == nullptr || is_inside_table(spin)) {
      continue;
    }
    spin->setFixedWidth(96);
    spin->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  }

  // Dictionary: label+ⓘ left, editor buttons stacked and right-aligned.
  if (auto* grid = qobject_cast<QGridLayout*>(userDictionaryGroup->layout())) {
    while (grid->count() > 0) {
      QLayoutItem* item = grid->takeAt(0);
      if (item->spacerItem() != nullptr) {
        delete item;
      } else {
        // Widgets stay alive; re-added below.
        delete item;
      }
    }
    auto* personal_label =
        new QLabel(tr("Personal entries"), userDictionaryGroup);
    personal_label->setObjectName(QStringLiteral("preferencesSecondary"));
    personal_label->setFont(config_dialog_theme::LabelFont());
    QWidget* label_cell = personal_label;
    if (explanation_controller_ != nullptr) {
      label_cell = MakeInlineAnnotatedLabel(personal_label,
                                            explanation_controller_.get(),
                                            "dictionaries.personal",
                                            userDictionaryGroup);
    }
    auto* buttons = new QWidget(userDictionaryGroup);
    auto* buttons_layout = new QVBoxLayout(buttons);
    buttons_layout->setContentsMargins(0, 0, 0, 0);
    buttons_layout->setSpacing(6);
    buttons_layout->setAlignment(Qt::AlignRight);
    TakeFromLayout(editUserDictionaryButton);
    TakeFromLayout(editUserSymbolsButton);
    buttons_layout->addWidget(editUserDictionaryButton, 0, Qt::AlignRight);
    buttons_layout->addWidget(editUserSymbolsButton, 0, Qt::AlignRight);
    editUserDictionaryButton->setSizePolicy(QSizePolicy::Maximum,
                                            QSizePolicy::Fixed);
    editUserSymbolsButton->setSizePolicy(QSizePolicy::Maximum,
                                         QSizePolicy::Fixed);
    grid->addWidget(label_cell, 0, 0, Qt::AlignLeft | Qt::AlignTop);
    grid->addWidget(buttons, 0, 1, 2, 1, Qt::AlignRight | Qt::AlignTop);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 0);
  }

  // Keymap Customize… buttons should hug their label, not float wide.
  editKeymapButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  editRomanTableButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);

  // Help prose uses the same left edge as other controls (no narrow column).
  auto constrain_help = [](QLabel* label) {
    if (label == nullptr) {
      return;
    }
    label->setObjectName(QStringLiteral("preferencesSecondary"));
    label->setWordWrap(true);
    label->setMaximumWidth(QWIDGETSIZE_MAX);
    label->setMinimumWidth(0);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  };
  constrain_help(usageStatsMessage);
  const QList<QFrame*> groups = {userDictionaryGroup, usageDictionaryGroup,
                                 specialConversionsGroup, startupGroup,
                                 loggingGroup};
  for (QFrame* group : groups) {
    FlattenGroupMargins(group);
  }

  const QList<QTableWidget*> tables = findChildren<QTableWidget*>();
  for (QTableWidget* table : tables) {
    if (table == nullptr) {
      continue;
    }
    const int row_h = QFontMetrics(table->font()).height() + 18;
    table->verticalHeader()->setDefaultSectionSize(row_h);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    table->horizontalHeader()->setStretchLastSection(true);
    if (table->columnCount() >= 3) {
      // Keep Action flexible; size Modifier/Key to contents so the Key menu
      // is not pushed past the scrollbar.
      table->horizontalHeader()->setSectionResizeMode(
          0, QHeaderView::Stretch);
      table->horizontalHeader()->setSectionResizeMode(
          1, QHeaderView::ResizeToContents);
      table->horizontalHeader()->setSectionResizeMode(
          2, QHeaderView::ResizeToContents);
    }
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  }
}

void ConfigDialog::UpdatePreferencesScrollPolicy() {
  auto* scroll = findChild<QScrollArea*>(QStringLiteral("preferencesScroll"));
  auto* stack = findChild<QStackedWidget*>(QStringLiteral("preferencesStack"));
  auto* host = findChild<QWidget*>(QStringLiteral("preferencesScrollHost"));
  if (scroll == nullptr || stack == nullptr) {
    return;
  }
  QWidget* page = stack->currentWidget();
  if (page == nullptr) {
    return;
  }
  // Leave room for the scrollbar so table columns are not clipped under it.
  const int width =
      std::max(1, scroll->viewport()->width() - 4);
  page->setMaximumHeight(QWIDGETSIZE_MAX);
  page->setMinimumHeight(0);
  page->setFixedWidth(width);
  stack->setFixedWidth(width);
  if (QLayout* page_layout = page->layout()) {
    page_layout->activate();
  }
  page->adjustSize();
  int need = page->sizeHint().height();
  if (QLayout* page_layout = page->layout()) {
    need = std::max(need, page_layout->totalSizeHint().height());
  }
  need = std::max(need, page->minimumSizeHint().height());
  // Do not clamp with setMaximumHeight until after measuring — an
  // underestimated max was clipping Keyboard and stacking Kaeriten on the table.
  page->setFixedHeight(need);
  stack->setFixedHeight(need);
  if (host != nullptr) {
    host->setMinimumHeight(need);
  }
  const int have = scroll->viewport()->height();
  scroll->setVerticalScrollBarPolicy(need > have + 2
                                         ? Qt::ScrollBarAsNeeded
                                         : Qt::ScrollBarAlwaysOff);
}

void ConfigDialog::ApplyPreferencesChrome() {
  const bool dark = config_dialog_theme::IsDark();
  const config_dialog_theme::Palette palette =
      config_dialog_theme::Colors(dark);

  setAttribute(Qt::WA_StyledBackground, true);
  QString qss = config_dialog_theme::Stylesheet(dark);
#if defined(__linux__)
  // GNOME's compositor does not implement xdg-decoration, so on Wayland Qt
  // draws its own decorations and the window gets no shadow. A hairline is
  // the substitute. X11, macOS, and Windows already have a real frame.
  if (QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
    qss += QStringLiteral(
        "\nQDialog#ConfigDialog { border: 1px solid @@RULE@@; }");
    qss.replace(QLatin1String("@@RULE@@"),
                config_dialog_theme::Hex(palette.rule));
  }
#endif  // __linux__
  setStyleSheet(qss);

  QPalette window_palette = this->palette();
  window_palette.setColor(QPalette::Window, palette.window);
  window_palette.setColor(QPalette::WindowText, palette.text);
  window_palette.setColor(QPalette::Base, palette.window);
  window_palette.setColor(QPalette::Text, palette.text);
  window_palette.setColor(QPalette::ButtonText, palette.text);
  window_palette.setColor(QPalette::Highlight, palette.accent);
  window_palette.setColor(QPalette::HighlightedText, palette.on_accent);
  setPalette(window_palette);

  if (QWidget* sidebar =
          findChild<QWidget*>(QStringLiteral("preferencesSidebar"))) {
    QPalette side = sidebar->palette();
    side.setColor(QPalette::Window, palette.sidebar);
    side.setColor(QPalette::Base, palette.sidebar);
    sidebar->setPalette(side);
  }
  if (auto* brand =
          findChild<QLabel*>(QStringLiteral("preferencesBrand"))) {
    using Spacing = config_dialog_theme::Spacing;
    const int logo_w = Spacing::sidebar_width - Spacing::sidebar_pad_l -
                       Spacing::sidebar_pad_r -
                       Spacing::logo_right_clearance;
    const QPixmap logo = LoadPreferencesBrandLogo(
        dark, logo_w, brand->devicePixelRatioF());
    if (!logo.isNull()) {
      brand->setPixmap(logo);
      brand->setFixedHeight(qRound(logo.height() / logo.devicePixelRatioF()));
      brand->setFixedWidth(logo_w);
      brand->setText(QString());
    }
  }
  if (QWidget* content =
          findChild<QWidget*>(QStringLiteral("preferencesContent"))) {
    QPalette page = content->palette();
    page.setColor(QPalette::Window, palette.window);
    content->setPalette(page);
  }

  UpdatePreferencesScrollPolicy();
}

bool ConfigDialog::SetConfig(const config::Config& config) {
  if (!client_->CheckVersionOrRestartServer()) {
    LOG(ERROR) << "CheckVersionOrRestartServer failed";
    return false;
  }

  if (!client_->SetConfig(config)) {
    LOG(ERROR) << "SetConfig failed";
    return false;
  }

  return true;
}

bool ConfigDialog::GetConfig(config::Config* config) {
  if (!client_->CheckVersionOrRestartServer()) {
    LOG(ERROR) << "CheckVersionOrRestartServer failed";
    return false;
  }

  if (!client_->GetConfig(config)) {
    LOG(ERROR) << "GetConfig failed";
    return false;
  }

  return true;
}

void ConfigDialog::Reload() {
  config::Config config;
  if (!GetConfig(&config)) {
    // Standalone UI preview builds cannot talk to the installed converter.
    // Skip the modal so the window remains inspectable.
    if (!qEnvironmentVariableIsSet("MARINAMOJI_PREFS_UI_PREVIEW")) {
      QString message = tr("Failed to get current config values.");
#if defined(__APPLE__)
      message += QStringLiteral("\n\n") +
                 tr("If marinaMoji was just installed or updated, open System "
                    "Settings > General > Login Items & Extensions and switch "
                    "on marinaMoji under \"Allow in the Background\", then "
                    "reopen Preferences.");
#endif  // __APPLE__
      QMessageBox::critical(this, windowTitle(), message);
    }
  }
  ConvertFromProto(config);

#if defined(__linux__)
  LoadIbusCandidateWindowFromFile();
#endif  // __linux__

  SelectAutoConversionSetting(static_cast<int>(config.use_auto_conversion()));

  if (sync_tab_) {
    sync_tab_->LoadFromServer();
  }
}

bool ConfigDialog::Update(bool quiet_validation) {
  config::Config config;
  ConvertToProto(&config);

  if (config.session_keymap() == config::Config::CUSTOM &&
      config.custom_keymap_table().empty()) {
    if (!quiet_validation) {
      QMessageBox::warning(this, windowTitle(),
                           tr("The current custom keymap table is empty. "
                              "When custom keymap is selected, "
                              "you must customize it."));
    }
    return false;
  }

  if (shortcuts_tab_) {
    QString shortcut_error;
    if (!shortcuts_tab_->ValidateBeforeApply(&shortcut_error)) {
      if (!quiet_validation) {
        QMessageBox::warning(
            this, windowTitle(),
            shortcut_error.isEmpty()
                ? tr("Number-row shortcut bindings are invalid.")
                : shortcut_error);
      }
      return false;
    }
  }

  // Note: Romaji/Kana input style, the fixed romaji keyboard layout, and the
  // mode indicator used to require restarting each application ("enabled from
  // new applications"). The Windows client now refreshes its config snapshot
  // on thread-focus events (see win32/base/config_snapshot.cc), so these
  // settings take effect in running applications as soon as they regain
  // focus. No message box is needed anymore.

  if (!SetConfig(config)) {
    QMessageBox::critical(this, windowTitle(), tr("Failed to update config"));
    return false;
  }

#if defined(__linux__)
  SaveIbusCandidateWindowToFile();
#endif  // __linux__

#ifdef _WIN32
  if (!WinUtil::SetIMEHotKeyDisabled(IMEHotKeyDisabledCheckBox->isChecked())) {
    // Do not show any dialog here, since this operation will not fail
    // in almost all cases.
    // TODO(taku): better to show dialog?
    LOG(ERROR) << "Failed to update IME HotKey status";
  }
#endif  // _WIN32

#ifdef __APPLE__
  if (startupCheckBox->isChecked()) {
    if (!MacUtil::CheckPrelauncherLoginItemStatus()) {
      MacUtil::AddPrelauncherLoginItem();
    }
  } else {
    if (MacUtil::CheckPrelauncherLoginItemStatus()) {
      MacUtil::RemovePrelauncherLoginItem();
    }
  }
#endif  // __APPLE__

  if (sync_tab_) {
    sync_tab_->SaveToServer();
  }

  return true;
}

void ConfigDialog::SetSendStatsCheckBox() {
#ifndef GOOGLE_JAPANESE_INPUT_BUILD
  usageStatsCheckBox->setChecked(false);
  return;
#endif  // GOOGLE_JAPANESE_INPUT_BUILD
  // On windows, usage_stats flag is managed by
  // administration_dialog. http://b/2889759
#ifndef _WIN32
  const bool val = StatsConfigUtil::IsEnabled();
  usageStatsCheckBox->setChecked(val);
#endif  // _WIN32
}

void ConfigDialog::GetSendStatsCheckBox() const {
#ifndef GOOGLE_JAPANESE_INPUT_BUILD
  return;
#endif  // GOOGLE_JAPANESE_INPUT_BUILD
  // On windows, usage_stats flag is managed by
  // administration_dialog. http://b/2889759
#ifndef _WIN32
  const bool val = usageStatsCheckBox->isChecked();
  StatsConfigUtil::SetEnabled(val);
#endif  // _WIN32
}

#if defined(__linux__)
void ConfigDialog::LoadIbusCandidateWindowFromFile() {
  const std::string path = FileUtil::JoinPath(
      SystemUtil::GetUserProfileDirectory(), "ibus_config.textproto");
  absl::StatusOr<std::string> content = FileUtil::GetContents(path);
  if (!content.ok()) {
    candidateWindowComboBox->setCurrentIndex(0);  // Default: IBus
    return;
  }
  ibus::Config config;
  if (!mozc::protobuf::TextFormat::ParseFromString(*content, &config)) {
    candidateWindowComboBox->setCurrentIndex(0);
    return;
  }
  const bool use_ibus = config.has_mozc_renderer()
                            ? config.mozc_renderer().use_ibus_candidate_window()
                            : true;
  const int idx = candidateWindowComboBox->findData(use_ibus);
  candidateWindowComboBox->setCurrentIndex(idx >= 0 ? idx : 0);
}

void ConfigDialog::SaveIbusCandidateWindowToFile() const {
  const std::string path = FileUtil::JoinPath(
      SystemUtil::GetUserProfileDirectory(), "ibus_config.textproto");
  absl::StatusOr<std::string> content = FileUtil::GetContents(path);
  ibus::Config config;
  if (content.ok()) {
    mozc::protobuf::TextFormat::ParseFromString(*content, &config);
  }
  if (!config.has_mozc_renderer()) {
    config.mutable_mozc_renderer();
  }
  const bool use_ibus =
      candidateWindowComboBox->currentData().toBool();
  config.mutable_mozc_renderer()->set_use_ibus_candidate_window(use_ibus);
  std::string out;
  if (!mozc::protobuf::TextFormat::PrintToString(config, &out)) {
    return;
  }
  FileUtil::SetContents(path, out);
}
#endif  // __linux__

#define SET_COMBOBOX(combobox, enumname, field)                    \
  do {                                                             \
    (combobox)->setCurrentIndex(static_cast<int>(config.field())); \
  } while (0)

#define SET_CHECKBOX(checkbox, field)       \
  do {                                      \
    (checkbox)->setChecked(config.field()); \
  } while (0)

#define GET_COMBOBOX(combobox, enumname, field)                              \
  do {                                                                       \
    config->set_##field(                                                     \
        static_cast<config::Config_##enumname>((combobox)->currentIndex())); \
  } while (0)

#define GET_CHECKBOX(checkbox, field)             \
  do {                                            \
    config->set_##field((checkbox)->isChecked()); \
  } while (0)

namespace {
static constexpr int kPreeditMethodSize = 2;
#ifdef __APPLE__
static constexpr int kMinCandidateWindowFontSize = 14;
static constexpr int kMaxCandidateWindowFontSize = 36;
#endif  // __APPLE__

void SetComboboxForPreeditMethod(const config::Config& config,
                                 QComboBox* combobox) {
  int index = static_cast<int>(config.preedit_method());
#ifdef _WIN32
  if (config.use_keyboard_to_change_preedit_method()) {
    index += kPreeditMethodSize;
  }
#endif  // _WIN32
  combobox->setCurrentIndex(index);
}

void GetComboboxForPreeditMethod(const QComboBox* combobox,
                                 config::Config* config) {
  int index = combobox->currentIndex();
  if (index >= kPreeditMethodSize) {
    // |use_keyboard_to_change_preedit_method| should be true and
    // |index| should be adjusted to smaller than kPreeditMethodSize.
    config->set_preedit_method(
        static_cast<config::Config_PreeditMethod>(index - kPreeditMethodSize));
    config->set_use_keyboard_to_change_preedit_method(true);
  } else {
    config->set_preedit_method(
        static_cast<config::Config_PreeditMethod>(index));
    config->set_use_keyboard_to_change_preedit_method(false);
  }
}

// MarinaKeyboardLayout is a top-level enum (not nested in Config), so it
// cannot use the config::Config_##enumname pattern that SET_COMBOBOX/
// GET_COMBOBOX rely on.
void SetComboboxForKeyboardLayout(const config::Config &config,
                                  QComboBox *combobox) {
  combobox->setCurrentIndex(
      static_cast<int>(config.marina_keyboard_layout()));
}

void GetComboboxForKeyboardLayout(const QComboBox *combobox,
                                  config::Config *config) {
  config->set_marina_keyboard_layout(
      static_cast<config::MarinaKeyboardLayout>(combobox->currentIndex()));
}
}  // namespace

// TODO(taku)
// Actually ConvertFromProto and ConvertToProto are almost the same.
// The difference only SET_ and GET_. We would like to unify the twos.
void ConfigDialog::ConvertFromProto(const config::Config& config) {
  base_config_ = config;
  // tab1
  SetComboboxForPreeditMethod(config, inputModeComboBox);
  SetComboboxForKeyboardLayout(config, keyboardLayoutComboBox);
  SET_COMBOBOX(punctuationsSettingComboBox, PunctuationMethod,
               punctuation_method);
  SET_COMBOBOX(symbolsSettingComboBox, SymbolMethod, symbol_method);
  SET_COMBOBOX(spaceCharacterFormComboBox, FundamentalCharacterForm,
               space_character_form);
  SET_COMBOBOX(selectionShortcutModeComboBox, SelectionShortcut,
               selection_shortcut);
  SET_COMBOBOX(numpadCharacterFormComboBox, NumpadCharacterForm,
               numpad_character_form);
  SET_COMBOBOX(keymapSettingComboBox, SessionKeymap, session_keymap);

#ifdef __APPLE__
  candidateWindowFontSizeSpinBox->setValue(std::clamp<int>(
      static_cast<int>(config.candidate_window_font_size()),
      kMinCandidateWindowFontSize, kMaxCandidateWindowFontSize));
#endif  // __APPLE__

  custom_keymap_table_ = config.custom_keymap_table();
  custom_roman_table_ = config.custom_roman_table();
  custom_kaeriten_table_ = config.custom_kaeriten_table();

  // tab2
  SET_COMBOBOX(historyLearningLevelComboBox, HistoryLearningLevel,
               history_learning_level);
  SET_CHECKBOX(singleKanjiConversionCheckBox, use_single_kanji_conversion);
  SET_CHECKBOX(symbolConversionCheckBox, use_symbol_conversion);
  SET_CHECKBOX(emoticonConversionCheckBox, use_emoticon_conversion);
  SET_CHECKBOX(dateConversionCheckBox, use_date_conversion);
  SET_CHECKBOX(emojiConversionCheckBox, use_emoji_conversion);
  SET_CHECKBOX(numberConversionCheckBox, use_number_conversion);
  SET_CHECKBOX(calculatorCheckBox, use_calculator);
  SET_CHECKBOX(t13nConversionCheckBox, use_t13n_conversion);
  SET_CHECKBOX(zipcodeConversionCheckBox, use_zip_code_conversion);
  SET_CHECKBOX(spellingCorrectionCheckBox, use_spelling_correction);

  // InfoListConfig
  localUsageDictionaryCheckBox->setChecked(
      config.information_list_config().use_local_usage_dictionary());

  // tab3
  SET_CHECKBOX(autoSwitchCompositionMode, auto_switch_composition_mode);

  SET_CHECKBOX(useAutoConversion, use_auto_conversion);
  kutenCheckBox->setChecked(config.auto_conversion_key() &
                            config::Config::AUTO_CONVERSION_KUTEN);
  toutenCheckBox->setChecked(config.auto_conversion_key() &
                             config::Config::AUTO_CONVERSION_TOUTEN);
  questionMarkCheckBox->setChecked(
      config.auto_conversion_key() &
      config::Config::AUTO_CONVERSION_QUESTION_MARK);
  exclamationMarkCheckBox->setChecked(
      config.auto_conversion_key() &
      config::Config::AUTO_CONVERSION_EXCLAMATION_MARK);

  SET_COMBOBOX(shiftKeyModeSwitchComboBox, ShiftKeyModeSwitch,
               shift_key_mode_switch);

  SET_CHECKBOX(useJapaneseLayout, use_japanese_layout);

  SET_CHECKBOX(useModeIndicator, use_mode_indicator);

  // tab4
  SET_CHECKBOX(historySuggestCheckBox, use_history_suggest);
  SET_CHECKBOX(dictionarySuggestCheckBox, use_dictionary_suggest);
  SET_CHECKBOX(realtimeConversionCheckBox, use_realtime_conversion);

  suggestionsSizeSpinBox->setValue(
      std::clamp<int>(config.suggestions_size(), 1, 9));

  // tab5
  SetSendStatsCheckBox();
  SET_CHECKBOX(incognitoModeCheckBox, incognito_mode);
  SET_CHECKBOX(presentationModeCheckBox, presentation_mode);

  // tab6 (Misc)
  SET_COMBOBOX(verboseLevelComboBox, int, verbose_level);
  SET_CHECKBOX(checkDefaultCheckBox, check_default);
  SET_COMBOBOX(yenSignComboBox, YenSignCharacter, yen_sign_character);
#if defined(__linux__) || defined(__APPLE__)
  startKanjiModeComboBox->setCurrentIndex(config.use_traditional_kanji() ? 1 : 0);
  startLiveModeComboBox->setCurrentIndex(config.use_live_conversion() ? 0 : 1);
#endif
  SET_CHECKBOX(twoStepCommitCheckBox, use_two_step_commit);
  if (include_unstable_updates_checkbox_ != nullptr) {
    include_unstable_updates_checkbox_->setChecked(
        config.include_unstable_updates());
  }
  if (auto_check_for_updates_checkbox_ != nullptr) {
    auto_check_for_updates_checkbox_->setChecked(
        config.auto_check_for_updates());
  }

  // marinaMoji: dictionary packs. While the user has never touched these
  // settings (dictionary_packs_configured() == false), each pack's own
  // default_enabled wins -- an empty enabled_dictionary_packs list can't by
  // itself distinguish "never configured" from "user disabled everything."
  // See protocol/config.proto. Same logic, independently, in
  // request/conversion_request.h's ComputeEnabledDictionaryPacksMask --
  // update both if this changes.
  {
    const bool configured = config.dictionary_packs_configured();
    for (const DictionaryPackId &pack : kDictionaryPackIds) {
      auto it = dictionary_pack_checkboxes_.find(std::string(pack.id));
      if (it == dictionary_pack_checkboxes_.end()) {
        continue;
      }
      bool enabled = pack.default_enabled;
      if (configured) {
        enabled = false;
        for (const std::string &enabled_id : config.enabled_dictionary_packs()) {
          if (enabled_id == pack.id) {
            enabled = true;
            break;
          }
        }
      }
      it->second->setChecked(enabled);
    }
  }

  characterFormEditor->Load(config);

  if (shortcuts_tab_) {
    shortcuts_tab_->LoadFromConfig(config);
  }

#ifdef __APPLE__
  startupCheckBox->setChecked(MacUtil::CheckPrelauncherLoginItemStatus());
#endif  // __APPLE__
}

void ConfigDialog::ConvertToProto(config::Config* config) const {
  *config = base_config_;

  // tab1
  GetComboboxForPreeditMethod(inputModeComboBox, config);
  GetComboboxForKeyboardLayout(keyboardLayoutComboBox, config);
  GET_COMBOBOX(punctuationsSettingComboBox, PunctuationMethod,
               punctuation_method);
  GET_COMBOBOX(symbolsSettingComboBox, SymbolMethod, symbol_method);
  GET_COMBOBOX(spaceCharacterFormComboBox, FundamentalCharacterForm,
               space_character_form);
  GET_COMBOBOX(selectionShortcutModeComboBox, SelectionShortcut,
               selection_shortcut);
  GET_COMBOBOX(numpadCharacterFormComboBox, NumpadCharacterForm,
               numpad_character_form);
  GET_COMBOBOX(keymapSettingComboBox, SessionKeymap, session_keymap);

#ifdef __APPLE__
  config->set_candidate_window_font_size(static_cast<uint32_t>(std::clamp<int>(
      candidateWindowFontSizeSpinBox->value(), kMinCandidateWindowFontSize,
      kMaxCandidateWindowFontSize)));
#endif  // __APPLE__

  config->set_custom_keymap_table(custom_keymap_table_);

  config->clear_custom_roman_table();
  if (!custom_roman_table_.empty()) {
    config->set_custom_roman_table(custom_roman_table_);
  }

  config->clear_custom_kaeriten_table();
  if (!custom_kaeriten_table_.empty()) {
    config->set_custom_kaeriten_table(custom_kaeriten_table_);
  }

  // tab2
  GET_COMBOBOX(historyLearningLevelComboBox, HistoryLearningLevel,
               history_learning_level);
  GET_CHECKBOX(singleKanjiConversionCheckBox, use_single_kanji_conversion);
  GET_CHECKBOX(symbolConversionCheckBox, use_symbol_conversion);
  GET_CHECKBOX(emoticonConversionCheckBox, use_emoticon_conversion);
  GET_CHECKBOX(dateConversionCheckBox, use_date_conversion);
  GET_CHECKBOX(emojiConversionCheckBox, use_emoji_conversion);
  GET_CHECKBOX(numberConversionCheckBox, use_number_conversion);
  GET_CHECKBOX(calculatorCheckBox, use_calculator);
  GET_CHECKBOX(t13nConversionCheckBox, use_t13n_conversion);
  GET_CHECKBOX(zipcodeConversionCheckBox, use_zip_code_conversion);
  GET_CHECKBOX(spellingCorrectionCheckBox, use_spelling_correction);

  // InformationListConfig
  config->mutable_information_list_config()->set_use_local_usage_dictionary(
      localUsageDictionaryCheckBox->isChecked());

  // tab3
  GET_CHECKBOX(autoSwitchCompositionMode, auto_switch_composition_mode);

  GET_CHECKBOX(useAutoConversion, use_auto_conversion);

  GET_CHECKBOX(useJapaneseLayout, use_japanese_layout);

  GET_CHECKBOX(useModeIndicator, use_mode_indicator);

  uint32_t auto_conversion_key = 0;
  if (kutenCheckBox->isChecked()) {
    auto_conversion_key |= config::Config::AUTO_CONVERSION_KUTEN;
  }
  if (toutenCheckBox->isChecked()) {
    auto_conversion_key |= config::Config::AUTO_CONVERSION_TOUTEN;
  }
  if (questionMarkCheckBox->isChecked()) {
    auto_conversion_key |= config::Config::AUTO_CONVERSION_QUESTION_MARK;
  }
  if (exclamationMarkCheckBox->isChecked()) {
    auto_conversion_key |= config::Config::AUTO_CONVERSION_EXCLAMATION_MARK;
  }
  config->set_auto_conversion_key(auto_conversion_key);

  GET_COMBOBOX(shiftKeyModeSwitchComboBox, ShiftKeyModeSwitch,
               shift_key_mode_switch);

  // tab4
  GET_CHECKBOX(historySuggestCheckBox, use_history_suggest);
  GET_CHECKBOX(dictionarySuggestCheckBox, use_dictionary_suggest);
  GET_CHECKBOX(realtimeConversionCheckBox, use_realtime_conversion);

  config->set_suggestions_size(
      static_cast<uint32_t>(suggestionsSizeSpinBox->value()));

  // tab5
  GetSendStatsCheckBox();
  GET_CHECKBOX(incognitoModeCheckBox, incognito_mode);
  GET_CHECKBOX(presentationModeCheckBox, presentation_mode);

  // marinaMoji: dictionary packs. Any Apply touches this dialog's checkbox
  // state at least once, so from here on dictionary_packs_configured() is
  // true and the saved list -- not each pack's default_enabled -- is
  // authoritative. See ConvertFromProto above.
  if (!dictionary_pack_checkboxes_.empty()) {
    config->set_dictionary_packs_configured(true);
    config->clear_enabled_dictionary_packs();
    for (const DictionaryPackId &pack : kDictionaryPackIds) {
      auto it = dictionary_pack_checkboxes_.find(std::string(pack.id));
      if (it != dictionary_pack_checkboxes_.end() && it->second->isChecked()) {
        config->add_enabled_dictionary_packs(std::string(pack.id));
      }
    }
  }

  // tab6 (Misc)
  config->set_verbose_level(verboseLevelComboBox->currentIndex());
  GET_CHECKBOX(checkDefaultCheckBox, check_default);
  GET_COMBOBOX(yenSignComboBox, YenSignCharacter, yen_sign_character);
#if defined(__linux__) || defined(__APPLE__)
  config->set_use_traditional_kanji(startKanjiModeComboBox->currentIndex() == 1);
  config->set_use_live_conversion(startLiveModeComboBox->currentIndex() == 0);
#endif
  GET_CHECKBOX(twoStepCommitCheckBox, use_two_step_commit);
  if (include_unstable_updates_checkbox_ != nullptr) {
    config->set_include_unstable_updates(
        include_unstable_updates_checkbox_->isChecked());
  }
  if (auto_check_for_updates_checkbox_ != nullptr) {
    config->set_auto_check_for_updates(
        auto_check_for_updates_checkbox_->isChecked());
  }

  characterFormEditor->Save(config);

  if (shortcuts_tab_) {
    shortcuts_tab_->ApplyToConfig(config);
  }
}

#undef SET_COMBOBOX
#undef SET_CHECKBOX
#undef GET_COMBOBOX
#undef GET_CHECKBOX

void ConfigDialog::clicked(QAbstractButton* button) {
  // Cancel · Apply · OK were removed; Preferences auto-saves. Kept so the
  // moc-generated slot remains linked if a platform still emits it.
  Q_UNUSED(button);
}

void ConfigDialog::ClearUserHistory() {
  if (QMessageBox::Ok !=
      QMessageBox::question(
          this, windowTitle(), tr("Do you want to clear all history data?"),
          QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel)) {
    return;
  }

  client_->CheckVersionOrRestartServer();

  if (!client_->ClearUserHistory() || !client_->ClearUserPrediction()) {
    QMessageBox::critical(this, windowTitle(),
                          tr("%1 Converter is not running. "
                             "Settings were not saved.")
                              .arg(GuiUtil::ProductName()));
  }
}

void ConfigDialog::EditUserDictionary() {
  client_->LaunchTool("dictionary_tool", "");
}

void ConfigDialog::EditUserSymbols() {
#if defined(__APPLE__) || defined(__linux__) || defined(_WIN32)
  const std::vector<std::string> current_values = LoadUserSymbolsFromFile();
  QStringList lines;
  for (const std::string &value : current_values) {
    if (!value.empty()) {
      lines << QString::fromStdString(value);
    }
  }
  bool ok = false;
  const QString initial_text = lines.join("\n");
  const QString edited = QInputDialog::getMultiLineText(
      this, windowTitle(),
      tr("User symbols for Symbols Palette (one entry per line):"),
      initial_text, &ok);
  if (!ok) {
    return;
  }

  std::vector<std::string> updated_values;
  const QStringList split = edited.split('\n');
  for (const QString &line : split) {
    const QString trimmed = line.trimmed();
    if (!trimmed.isEmpty()) {
      updated_values.push_back(trimmed.toStdString());
    }
  }
  SaveUserSymbolsToFile(updated_values);
#else
  QMessageBox::information(
      this, windowTitle(),
      tr("User symbols editor is not available on this platform."));
#endif
}

void ConfigDialog::EditKeymap() {
  std::string current_keymap_table = "";
  const QString keymap_name = keymapSettingComboBox->currentText();
  const std::map<QString, config::Config::SessionKeymap>::const_iterator itr =
      keymapname_sessionkeymap_map_.find(keymap_name);
  if (itr != keymapname_sessionkeymap_map_.end()) {
    // Load from predefined mapping file.
    const char* keymap_file =
        keymap::KeyMapManager::GetKeyMapFileName(itr->second);
    std::unique_ptr<std::istream> ifs(
        ConfigFileStream::LegacyOpen(keymap_file));
    CHECK(ifs.get() != nullptr);  // should never happen
    std::stringstream buffer;
    buffer << ifs->rdbuf();
    current_keymap_table = buffer.str();
  } else {
    current_keymap_table = custom_keymap_table_;
  }
  std::string output;
  if (gui::KeyMapEditorDialog::Show(this, current_keymap_table, &output)) {
    custom_keymap_table_ = output;
    // set keymapSettingComboBox to "Custom keymap"
    keymapSettingComboBox->setCurrentIndex(0);
    FlushAutoSave();
  }
}

void ConfigDialog::EditRomanTable() {
  std::string output;
  if (gui::RomanTableEditorDialog::Show(this, custom_roman_table_, &output)) {
    custom_roman_table_ = output;
    FlushAutoSave();
  }
}

void ConfigDialog::EditKaeritenTable() {
  std::string output;
  if (gui::KaeritenTableEditorDialog::Show(this, custom_kaeriten_table_,
                                           &output)) {
    custom_kaeriten_table_ = output;
    FlushAutoSave();
  }
}

void ConfigDialog::SelectInputModeSetting(int index) {
  // enable "EDIT" button if roman mode is selected
  editRomanTableButton->setEnabled((index == 0));
}

void ConfigDialog::SelectAutoConversionSetting(int state) {
  kutenCheckBox->setEnabled(static_cast<bool>(state));
  toutenCheckBox->setEnabled(static_cast<bool>(state));
  questionMarkCheckBox->setEnabled(static_cast<bool>(state));
  exclamationMarkCheckBox->setEnabled(static_cast<bool>(state));
}

void ConfigDialog::SelectSuggestionSetting(int state) {
  if (historySuggestCheckBox->isChecked() ||
      dictionarySuggestCheckBox->isChecked() ||
      realtimeConversionCheckBox->isChecked()) {
    presentationModeCheckBox->setEnabled(true);
  } else {
    presentationModeCheckBox->setEnabled(false);
  }
}

void ConfigDialog::ResetToDefaults() {
  const QString message =
      tr("When you reset %1 settings, any changes "
         "you've made will be reverted to the default settings. "
         "Do you want to reset settings? "
         "The following items are not reset with this operation.\n"
         " - Personalization data\n"
         " - Input history\n"
         " - Usage statistics and crash reports\n"
         " - Administrator settings")
          .arg(GuiUtil::ProductName());
  if (QMessageBox::Ok ==
      QMessageBox::question(this, windowTitle(), message,
                            QMessageBox::Ok | QMessageBox::Cancel,
                            QMessageBox::Cancel)) {
    // TODO(taku): remove the dependency to config::ConfigHandler
    // nice to have GET_DEFAULT_CONFIG command
    autosave_suppressed_ = true;
    ConvertFromProto(config::ConfigHandler::DefaultConfig());
    autosave_suppressed_ = false;
    FlushAutoSave();
  }
}

void ConfigDialog::LaunchAdministrationDialog() {
#ifdef _WIN32
  client_->LaunchTool("administration_dialog", "");
#endif  // _WIN32
}

void ConfigDialog::EnableApplyButton() {
  ScheduleAutoSave();
}

void ConfigDialog::ScheduleAutoSave() {
  if (autosave_suppressed_) {
    return;
  }
  // A change arrived while SetConfig (or an error dialog) is still running:
  // mark dirty so RunAutoSave repeats with the latest UI after it returns.
  if (autosave_in_progress_) {
    autosave_dirty_ = true;
    return;
  }
  if (autosave_timer_ == nullptr) {
    return;
  }
  autosave_timer_->start();
}

void ConfigDialog::RunAutoSave(bool quiet_validation) {
  if (autosave_suppressed_) {
    return;
  }
  if (autosave_in_progress_) {
    autosave_dirty_ = true;
    return;
  }
  autosave_in_progress_ = true;
  do {
    autosave_dirty_ = false;
    // Always read ConvertToProto at the start of each pass so a newer UI
    // state cannot be overwritten by an older in-flight snapshot.
    Update(quiet_validation);
  } while (autosave_dirty_);
  autosave_in_progress_ = false;
}

void ConfigDialog::FlushAutoSave() {
  if (autosave_timer_ != nullptr) {
    autosave_timer_->stop();
  }
  // Closing and editor commits must surface validation problems.
  RunAutoSave(/*quiet_validation=*/false);
}

void ConfigDialog::closeEvent(QCloseEvent* event) {
  FlushAutoSave();
  QDialog::closeEvent(event);
}

ConfigDialog::~ConfigDialog() = default;

void ConfigDialog::CheckForUpdates() {
  if (update_checker_ == nullptr) {
    return;
  }
  update_check_silent_ = false;
  if (check_for_updates_button_ != nullptr) {
    check_for_updates_button_->setEnabled(false);
    check_for_updates_button_->setText(tr("Checking…"));
  }
  const bool include_unstable =
      include_unstable_updates_checkbox_ != nullptr &&
      include_unstable_updates_checkbox_->isChecked();
  update_checker_->CheckForUpdates(include_unstable);
}

void ConfigDialog::MaybeAutoCheckForUpdates() {
#if defined(__APPLE__) || defined(_WIN32)
  if (update_checker_ == nullptr || auto_update_check_started_) {
    return;
  }
  const bool enabled =
      auto_check_for_updates_checkbox_ != nullptr
          ? auto_check_for_updates_checkbox_->isChecked()
          : base_config_.auto_check_for_updates();
  if (!enabled || !ShouldRunMarinaAutoUpdateCheck()) {
    return;
  }
  auto_update_check_started_ = true;
  update_check_silent_ = true;
  MarkMarinaAutoUpdateCheckRan();
  const bool include_unstable =
      include_unstable_updates_checkbox_ != nullptr &&
      include_unstable_updates_checkbox_->isChecked();
  update_checker_->CheckForUpdates(include_unstable);
#endif  // __APPLE__ || _WIN32
}

void ConfigDialog::showEvent(QShowEvent *event) {
  QDialog::showEvent(event);
  if (QLayout* root_layout = layout()) {
    root_layout->activate();
  }
  // macOS sometimes restores a smaller geometry from the process before our
  // shell resize sticks. Re-assert the preferred size on first show.
  if (width() < 1000 || height() < 660) {
    resize(1020, 700);
  }
  if (auto* title =
          findChild<QLabel*>(QStringLiteral("preferencesPageTitle"))) {
    const QFontMetrics title_metrics(title->font());
    title->setMinimumHeight(title_metrics.height() + 6);
    title->updateGeometry();
  }
  UpdatePreferencesScrollPolicy();
  // Second pass after the window is on-screen: first paint often clips the
  // serif page title until a system UI event forces a relayout (Spotlight).
  QTimer::singleShot(0, this, [this]() {
    if (auto* title =
            findChild<QLabel*>(QStringLiteral("preferencesPageTitle"))) {
      title->updateGeometry();
      title->update();
    }
    if (QLayout* root_layout = layout()) {
      root_layout->activate();
    }
    UpdatePreferencesScrollPolicy();
  });
  MaybeAutoCheckForUpdates();
}

void ConfigDialog::OnUpdateAvailable(const QString &tag_name,
                                     const QString &html_url,
                                     const QString &pkg_url,
                                     const QString &pkg_sha256) {
  if (check_for_updates_button_ != nullptr) {
    check_for_updates_button_->setEnabled(true);
    check_for_updates_button_->setText(tr("Check for updates…"));
  }
  // An automatic check (Preferences just opened) follows the same rule as the
  // background checkers: the same release is offered at most once a day. A
  // check the user started by clicking the button always shows its result.
  const bool automatic_check = update_check_silent_;
  update_check_silent_ = false;
#if defined(MARINAMOJI) && (defined(_WIN32) || defined(__APPLE__))
  if (automatic_check) {
    const std::string tag = tag_name.toStdString();
    if (!ShouldOfferMarinaUpdate(tag)) {
      return;
    }
    MarkMarinaUpdateOffered(tag);
  }
#else
  (void)automatic_check;
#endif  // MARINAMOJI && (_WIN32 || __APPLE__)

#if defined(__APPLE__) || defined(_WIN32)
  if (!pkg_url.isEmpty()) {
    QMessageBox box(this);
    box.setWindowTitle(windowTitle());
    box.setIcon(QMessageBox::Information);
    box.setText(tr("A newer release is available: %1").arg(tag_name));
#if defined(__APPLE__)
    box.setInformativeText(
        tr("Download the notarized .pkg and open the macOS Installer? "
           "You will still need to approve the install."));
#else  // _WIN32
    box.setInformativeText(
        tr("Download the installer (.msi) and run it now? You will still "
           "need to approve the install. The installer is signed, but "
           "Windows SmartScreen may still warn first while the certificate "
           "builds reputation; choose \"More info\" → \"Run anyway\"."));
#endif  // _WIN32
    QPushButton *install_button =
        box.addButton(tr("Download & Install…"), QMessageBox::AcceptRole);
    QPushButton *page_button =
        box.addButton(tr("Open release page"), QMessageBox::ActionRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(install_button);
    box.exec();
    if (box.clickedButton() == install_button) {
      QString error;
      if (!GitHubUpdateChecker::DownloadAndOpenInstaller(
              pkg_url, tag_name, pkg_sha256, &error)) {
        QMessageBox::warning(this, windowTitle(),
                             tr("The update could not be downloaded or started:\n%1").arg(error));
      }
    } else if (box.clickedButton() == page_button) {
      Process::OpenBrowser(html_url.toStdString());
    }
    return;
  }
#endif  // __APPLE__ || _WIN32

  const QString message =
      tr("A newer release is available: %1\n\nOpen the download page?")
          .arg(tag_name);
  if (QMessageBox::Ok ==
      QMessageBox::information(this, windowTitle(), message,
                               QMessageBox::Ok | QMessageBox::Cancel,
                               QMessageBox::Ok)) {
    Process::OpenBrowser(html_url.toStdString());
  }
}

void ConfigDialog::OnUpdateUpToDate() {
  if (check_for_updates_button_ != nullptr) {
    check_for_updates_button_->setEnabled(true);
    check_for_updates_button_->setText(tr("Check for updates…"));
  }
  if (update_check_silent_) {
    update_check_silent_ = false;
    return;
  }
  QMessageBox::information(
      this, windowTitle(),
      tr("You are using the latest available release for your update channel."));
}

void ConfigDialog::OnUpdateCheckFailed(const QString &message) {
  if (check_for_updates_button_ != nullptr) {
    check_for_updates_button_->setEnabled(true);
    check_for_updates_button_->setText(tr("Check for updates…"));
  }
  if (update_check_silent_) {
    update_check_silent_ = false;
    return;
  }
  QMessageBox::warning(this, windowTitle(),
                       tr("Update check failed:\n%1").arg(message));
}

// Catch MouseButtonRelease event to toggle the CheckBoxes
bool ConfigDialog::eventFilter(QObject* obj, QEvent* event) {
  if (event->type() == QEvent::MouseButtonRelease) {
    if (obj == usageStatsMessage) {
#if defined(GOOGLE_JAPANESE_INPUT_BUILD) && !defined(CHANNEL_DEV)
      usageStatsCheckBox->toggle();
#endif  // GOOGLE_JAPANESE_INPUT_BUILD && !CHANNEL_DEV
    } else if (obj == incognitoModeMessage) {
      incognitoModeCheckBox->toggle();
    }
  }
  return QObject::eventFilter(obj, event);
}

}  // namespace gui
}  // namespace mozc
