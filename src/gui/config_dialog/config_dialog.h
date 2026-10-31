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

#ifndef MOZC_GUI_CONFIG_DIALOG_CONFIG_DIALOG_H_
#define MOZC_GUI_CONFIG_DIALOG_CONFIG_DIALOG_H_

#include <QCloseEvent>
#include <QObject>
#include <QTimer>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "client/client_interface.h"
#include "gui/base/github_update_checker.h"
#include "gui/config_dialog/config_dialog_shortcuts_tab.h"
#include "gui/config_dialog/config_dialog_sync_tab.h"
#include "gui/config_dialog/preferences_explanations.h"
#include "gui/config_dialog/ui_config_dialog.h"
#include "protocol/config.pb.h"
#include "protocol/commands.pb.h"

class QCheckBox;
class QPushButton;

namespace mozc {
namespace gui {

class ConfigDialog : public QDialog, private Ui::ConfigDialog {
  Q_OBJECT;

 public:
  ConfigDialog();
  ~ConfigDialog() override;

  // Methods defined in the 'slots' section (Qt's extension) will be processed
  // by Qt's moc tool (moc.exe on Windows). Unfortunately, preprocessor macros
  // defined for C/C++ are not automatically passed into the moc tool.
  // For example, you need to call the moc tool with '-D' option as
  // 'moc -DENABLE_FOOBER ...' to make the moc tool aware of the ENABLE_FOOBER
  // macro. http://developer.qt.nokia.com/doc/qt-4.8/moc.html
  // So basically we must not use any #ifdef macro in slot declarations.
  // Otherwise, methods enclosed by "ifdef ENABLE_FOOBER" will be simply ignored
  // by the moc tool and |QObject::connect| against these methods results in
  // failure. See b/5935351 about how we found this issue.
 protected slots:
  virtual void clicked(QAbstractButton* button);
  virtual void ClearUserHistory();
  virtual void EditUserDictionary();
  virtual void EditUserSymbols();
  virtual void EditKeymap();
  virtual void EditRomanTable();
  virtual void EditKaeritenTable();
  virtual void ResetToDefaults();
  virtual void SelectInputModeSetting(int index);
  virtual void SelectAutoConversionSetting(int state);
  virtual void SelectSuggestionSetting(int state);
  virtual void LaunchAdministrationDialog();
  // Slot formerly named EnableApplyButton: ordinary control changes schedule
  // an automatic persist through the existing SetConfig path.
  virtual void EnableApplyButton();
  virtual void CheckForUpdates();
  virtual void OnUpdateAvailable(const QString& tag_name, const QString& html_url,
                                 const QString& pkg_url,
                                 const QString& pkg_sha256);
  virtual void OnUpdateUpToDate();
  virtual void OnUpdateCheckFailed(const QString& message);

 protected:
  bool eventFilter(QObject* obj, QEvent* event) override;
  void showEvent(QShowEvent* event) override;
  void closeEvent(QCloseEvent* event) override;

 private:
  void MaybeAutoCheckForUpdates();
  bool GetConfig(config::Config* config);
  bool SetConfig(const config::Config& config);
  // Set/GetSendStatsChechBox read/write registry or file directly
  // instead of config protobuf.
  void SetSendStatsCheckBox();
  void GetSendStatsCheckBox() const;
  void ConvertToProto(config::Config* config) const;
  void ConvertFromProto(const config::Config& config);
  // Persist UI → config via the existing client SetConfig path (and platform
  // side effects that Apply/OK used to run). When quiet_validation is true,
  // invalid intermediate states (e.g. unfinished shortcut chords) skip the
  // modal warning so autosave stays quiet while the user is still editing.
  bool Update(bool quiet_validation = false);
  void Reload();
  void ScheduleAutoSave();
  void FlushAutoSave();
  // Runs Update() on the GUI thread; serializes overlapping requests so a
  // nested/re-entrant save cannot persist an older snapshot over a newer one.
  void RunAutoSave(bool quiet_validation);
  // Linux/IBus only: load/save candidate window choice from ibus_config.textproto.
  void LoadIbusCandidateWindowFromFile();
  void SaveIbusCandidateWindowToFile() const;
  // Reorganise tabs into Writing / Dictionaries / Keyboard / Suggestions /
  // Privacy & Sync / Application. Does not change config keys or defaults.
  void MigratePreferencesInformationArchitecture();
  // Sidebar + page chrome. Does not move settings.
  void InstallPreferencesShell();
  void ApplyPreferencesChrome();
  void RefinePreferencesControls();
  void UpdatePreferencesScrollPolicy();

  std::unique_ptr<client::ClientInterface> client_;
  std::string custom_keymap_table_;
  std::string custom_roman_table_;
  std::string custom_kaeriten_table_;
  // base_config_ keeps the original config imported from the file including
  // unconfigurable options with the GUI (e.g. composing_timeout_threshold_msec)
  config::Config base_config_;
  std::map<QString, config::Config::SessionKeymap>
      keymapname_sessionkeymap_map_;

  std::unique_ptr<ConfigDialogShortcutsTab> shortcuts_tab_;
  std::unique_ptr<ConfigDialogSyncTab> sync_tab_;
  std::unique_ptr<PreferencesExplanationController> explanation_controller_;

  QCheckBox *include_unstable_updates_checkbox_ = nullptr;
  QCheckBox *auto_check_for_updates_checkbox_ = nullptr;
  QPushButton *check_for_updates_button_ = nullptr;
  QCheckBox *silent_update_checkbox_ = nullptr;
  std::unique_ptr<GitHubUpdateChecker> update_checker_;
  bool update_check_silent_ = false;
  bool auto_update_check_started_ = false;

  // Blocks ScheduleAutoSave while ConvertFromProto/Reload fills controls.
  bool autosave_suppressed_ = false;
  // True while RunAutoSave is inside Update/SetConfig (may re-enter the
  // event loop during IPC or modal error dialogs).
  bool autosave_in_progress_ = false;
  // Set when ScheduleAutoSave runs during autosave_in_progress_; causes
  // RunAutoSave to repeat with the latest UI state before returning.
  bool autosave_dirty_ = false;
  QTimer* autosave_timer_ = nullptr;

  // marinaMoji: one checkbox per mozc::kDictionaryPackIds entry, built at
  // runtime in the constructor rather than in the .ui file so new packs
  // (e.g. a future Kyōgen pack) need no GUI code change. Keyed by pack id.
  // See ConvertFromProto/ConvertToProto and request/dictionary_pack_ids.h.
  std::map<std::string, QCheckBox *> dictionary_pack_checkboxes_;
};

}  // namespace gui
}  // namespace mozc
#endif  // MOZC_GUI_CONFIG_DIALOG_CONFIG_DIALOG_H_
