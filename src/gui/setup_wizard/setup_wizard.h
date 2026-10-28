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

// Frameless first-run setup. Opened on its own (SetupWizard.app or
// mozc_tool --mode=setup_wizard). Settings are written only on the last page.

#ifndef MOZC_GUI_SETUP_WIZARD_SETUP_WIZARD_H_
#define MOZC_GUI_SETUP_WIZARD_SETUP_WIZARD_H_

#include <memory>

#include <QDialog>

#include "client/client_interface.h"
#include "protocol/config.pb.h"

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QFrame;
class QLabel;
class QMouseEvent;
class QPaintEvent;
class QPushButton;
class QRadioButton;
class QShowEvent;
class QStackedWidget;
class QStyle;
class QVBoxLayout;

namespace mozc {
namespace gui {

class SetupWizard : public QDialog {
  Q_OBJECT;

 public:
  SetupWizard();
  ~SetupWizard() override;

 protected:
  void paintEvent(QPaintEvent* event) override;
  void showEvent(QShowEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;

 private slots:
  void OnBack();
  void OnNext();

 private:
  enum PageKind {
    kWelcome,
    kKeyboard,
    kConversion,
    kPreferences,
    kReady,
  };

  // Combo-box values that are not Config::SessionKeymap entries.
  enum KeymapChoice {
    kKeymapUnknown = -2,
    kKeymapKeepCustom = -3,
  };

  bool GetConfig(config::Config* config);
  bool SetConfig(const config::Config& config);
  void LoadChoices(const config::Config& config, bool from_server);
  void ShowPage(int index);
  bool Save();
  bool CanDragFrom(QWidget* widget) const;

  QWidget* MakeWelcomePage();
  QWidget* MakeKeyboardPage();
  QWidget* MakeConversionPage();
  QWidget* MakePreferencesPage();
  QWidget* MakeReadyPage();

  QWidget* MakeScrollPage(QVBoxLayout** layout);
  QLabel* AddTitle(QVBoxLayout* layout, QWidget* parent, const QString& text,
                   const QFont& font);
  QLabel* AddMuted(QVBoxLayout* layout, QWidget* parent, const QString& text);
  void AddSection(QVBoxLayout* layout, QWidget* parent, const QString& title,
                  const QString& description);
  QRadioButton* AddChoice(QVBoxLayout* layout, QWidget* parent,
                          QButtonGroup* group, const QString& title,
                          const QString& detail);

  std::unique_ptr<client::ClientInterface> client_;
  QStyle* control_style_ = nullptr;
  bool config_loaded_ = false;
  bool sized_ = false;

  QPushButton* close_button_ = nullptr;
  QWidget* chrome_mark_ = nullptr;
  QLabel* progress_ = nullptr;
  QFrame* chrome_line_ = nullptr;
  QStackedWidget* stack_ = nullptr;
  QPushButton* back_ = nullptr;
  QPushButton* next_ = nullptr;

  int page_index_ = 0;

#ifdef __APPLE__
  QRadioButton* follow_layout_ = nullptr;
  QRadioButton* qwerty_layout_ = nullptr;
  QCheckBox* login_checkbox_ = nullptr;
#endif  // __APPLE__
#ifdef _WIN32
  QComboBox* layout_combo_ = nullptr;
#endif  // _WIN32

  QComboBox* keymap_combo_ = nullptr;
  QRadioButton* live_off_ = nullptr;
  QRadioButton* live_on_ = nullptr;
  QRadioButton* two_step_off_ = nullptr;
  QRadioButton* two_step_on_ = nullptr;
};

}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_SETUP_WIZARD_SETUP_WIZARD_H_
