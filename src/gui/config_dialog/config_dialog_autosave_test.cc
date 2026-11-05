// Copyright 2026 marinaMoji contributors.
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

// Drives the real Preferences window on Qt's offscreen platform against a fake
// converter, to check that changes are autosaved (issue #103, items 1-3, 5-8,
// 10 and 18). The state machine itself is covered by
// autosave_controller_test; this checks that the dialog is wired to it.

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QEventLoop>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QString>
#include <QTimer>
#include <QWidget>
#include <memory>
#include <string>

#include "client/client.h"
#include "client/client_interface.h"
#include "client/client_mock.h"
#include "config/config_handler.h"
#include "gui/config_dialog/config_dialog.h"
#include "protocol/config.pb.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace gui {
namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

// Stands in for the converter process: holds the config and records saves.
struct FakeServer {
  config::Config config;
  bool fail_get = false;
  bool fail_set = false;
  int set_count = 0;
};

class FakeClientFactory : public client::ClientFactoryInterface {
 public:
  explicit FakeClientFactory(FakeServer* server) : server_(server) {}

  std::unique_ptr<client::ClientInterface> NewClient() override {
    auto client = std::make_unique<NiceMock<client::ClientMock>>();
    FakeServer* server = server_;
    ON_CALL(*client, CheckVersionOrRestartServer()).WillByDefault(Return(true));
    ON_CALL(*client, GetConfig(_))
        .WillByDefault(Invoke([server](config::Config* out) {
          if (server->fail_get) return false;
          *out = server->config;
          return true;
        }));
    ON_CALL(*client, SetConfig(_))
        .WillByDefault(Invoke([server](const config::Config& in) {
          if (server->fail_set) return false;
          server->config = in;
          ++server->set_count;
          return true;
        }));
    return client;
  }

 private:
  FakeServer* server_;
};

// Closes any modal box that appears (a modal would otherwise block the test
// forever) and counts them. Clicks |button| when the box has it.
class ModalDismisser {
 public:
  explicit ModalDismisser(QMessageBox::StandardButton button =
                              QMessageBox::Ok)
      : button_(button) {
    timer_.setInterval(10);
    QObject::connect(&timer_, &QTimer::timeout, [this]() {
      QWidget* modal = QApplication::activeModalWidget();
      if (modal == nullptr) return;
      ++count_;
      if (auto* box = qobject_cast<QMessageBox*>(modal)) {
        if (QAbstractButton* b = box->button(button_)) {
          b->click();
          return;
        }
      }
      modal->close();
    });
    timer_.start();
  }

  int count() const { return count_; }

 private:
  QMessageBox::StandardButton button_;
  QTimer timer_;
  int count_ = 0;
};

class ConfigDialogAutosaveTest : public testing::TestWithTempUserProfile {
 protected:
  static void SetUpTestSuite() {
    static int argc = 1;
    static char name[] = "config_dialog_autosave_test";
    static char* argv[] = {name, nullptr};
    app_ = new QApplication(argc, argv);
  }

  void SetUp() override {
    server_.config = config::ConfigHandler::DefaultConfig();
    server_.config.set_session_keymap(config::Config::MSIME);
    server_.config.set_auto_check_for_updates(false);
    factory_ = std::make_unique<FakeClientFactory>(&server_);
    client::ClientFactory::SetClientFactory(factory_.get());
  }

  void TearDown() override {
    client::ClientFactory::SetClientFactory(nullptr);
    qunsetenv("MARINAMOJI_PREFS_UI_PREVIEW");
  }

  std::unique_ptr<ConfigDialog> MakeDialog() {
    auto dialog = std::make_unique<ConfigDialog>();
    dialog->show();
    return dialog;
  }

  // Runs the event loop, so the debounce timer can fire.
  static void Wait(int ms) {
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
  }
  // Longer than the 200 ms debounce.
  static constexpr int kSettleMs = 600;

  template <typename T>
  static T* Find(QWidget* root, const char* name) {
    T* widget = root->findChild<T*>(QString::fromLatin1(name));
    EXPECT_NE(widget, nullptr) << name;
    return widget;
  }

  static QPushButton* ResetButton(QWidget* root) {
    for (QPushButton* b : root->findChildren<QPushButton*>()) {
      if (b->text() == QStringLiteral("Reset to defaults")) return b;
    }
    ADD_FAILURE() << "no Reset to defaults button";
    return nullptr;
  }

  static QLabel* Banner(QWidget* root) {
    return Find<QLabel>(root, "preferencesSaveBanner");
  }

  // The user picks |index| in a combo box (programmatic changes emit nothing
  // the dialog listens to; the dialog saves on `activated`).
  static void Pick(QComboBox* combo, int index) {
    combo->setCurrentIndex(index);
    Q_EMIT combo->activated(index);
  }

  FakeServer server_;
  std::unique_ptr<FakeClientFactory> factory_;
  static QApplication* app_;
};

QApplication* ConfigDialogAutosaveTest::app_ = nullptr;

TEST_F(ConfigDialogAutosaveTest, OpeningTheWindowSavesNothing) {
  auto dialog = MakeDialog();
  Wait(kSettleMs);
  EXPECT_EQ(server_.set_count, 0);
  EXPECT_TRUE(Banner(dialog.get())->isHidden());
}

TEST_F(ConfigDialogAutosaveTest, ChangeIsSavedAfterTheDebounceAndSilently) {
  ModalDismisser modals;
  auto dialog = MakeDialog();
  auto* box = Find<QCheckBox>(dialog.get(), "historySuggestCheckBox");
  ASSERT_TRUE(box->isChecked());

  box->click();
  Wait(50);
  EXPECT_EQ(server_.set_count, 0) << "saved before the debounce elapsed";

  Wait(kSettleMs);
  EXPECT_EQ(server_.set_count, 1);
  EXPECT_FALSE(server_.config.use_history_suggest());
  EXPECT_TRUE(Banner(dialog.get())->isHidden());
  EXPECT_EQ(modals.count(), 0);
}

TEST_F(ConfigDialogAutosaveTest, ChangePersistsWhenTheWindowIsReopened) {
  {
    auto dialog = MakeDialog();
    Find<QCheckBox>(dialog.get(), "historySuggestCheckBox")->click();
    Wait(kSettleMs);
  }
  auto reopened = MakeDialog();
  EXPECT_FALSE(
      Find<QCheckBox>(reopened.get(), "historySuggestCheckBox")->isChecked());
}

TEST_F(ConfigDialogAutosaveTest, ClosingImmediatelyStillSavesTheEdit) {
  ModalDismisser modals;
  auto dialog = MakeDialog();
  Find<QCheckBox>(dialog.get(), "historySuggestCheckBox")->click();

  dialog->reject();  // Esc / the close button / Ctrl+W, before the debounce.

  EXPECT_EQ(server_.set_count, 1);
  EXPECT_FALSE(server_.config.use_history_suggest());
  EXPECT_EQ(modals.count(), 0);
}

TEST_F(ConfigDialogAutosaveTest, RapidChangesAreOneSaveWithAllOfThem) {
  auto dialog = MakeDialog();
  Find<QCheckBox>(dialog.get(), "historySuggestCheckBox")->click();
  Find<QCheckBox>(dialog.get(), "dictionarySuggestCheckBox")->click();
  Find<QCheckBox>(dialog.get(), "realtimeConversionCheckBox")->click();
  Wait(kSettleMs);

  EXPECT_EQ(server_.set_count, 1);
  EXPECT_FALSE(server_.config.use_history_suggest());
  EXPECT_FALSE(server_.config.use_dictionary_suggest());
  EXPECT_FALSE(server_.config.use_realtime_conversion());
}

TEST_F(ConfigDialogAutosaveTest, EmptyCustomKeymapShowsBannerAndSavesNothing) {
  ModalDismisser modals;
  auto dialog = MakeDialog();
  auto* keymap = Find<QComboBox>(dialog.get(), "keymapSettingComboBox");
  const int previous = keymap->currentIndex();
  ASSERT_NE(previous, 0);  // Index 0 is "Custom keymap".

  Pick(keymap, 0);
  Wait(kSettleMs);

  QLabel* banner = Banner(dialog.get());
  EXPECT_FALSE(banner->isHidden());
  EXPECT_TRUE(banner->text().startsWith(QStringLiteral("Not saved yet")))
      << banner->text().toStdString();
  EXPECT_EQ(server_.set_count, 0);
  EXPECT_EQ(modals.count(), 0) << "a timer-driven autosave must not raise a "
                                  "dialog";

  // Fixing it clears the banner and saves.
  Pick(keymap, previous);
  Wait(kSettleMs);
  EXPECT_TRUE(banner->isHidden());
  EXPECT_EQ(server_.set_count, 1);
  EXPECT_EQ(server_.config.session_keymap(), config::Config::MSIME);
}

TEST_F(ConfigDialogAutosaveTest, ConverterDownShowsBannerThenRecovers) {
  ModalDismisser modals;
  auto dialog = MakeDialog();
  server_.fail_set = true;

  Find<QCheckBox>(dialog.get(), "historySuggestCheckBox")->click();
  Wait(kSettleMs);

  QLabel* banner = Banner(dialog.get());
  EXPECT_FALSE(banner->isHidden());
  EXPECT_TRUE(banner->text().startsWith(QStringLiteral("Could not save")))
      << banner->text().toStdString();
  EXPECT_EQ(server_.set_count, 0);

  // Another edit while it is still down: still no dialog per edit.
  Find<QCheckBox>(dialog.get(), "dictionarySuggestCheckBox")->click();
  Wait(kSettleMs);
  EXPECT_EQ(modals.count(), 0);

  // The converter comes back: the next edit retries, clears the banner and
  // saves everything that was pending.
  server_.fail_set = false;
  Find<QCheckBox>(dialog.get(), "realtimeConversionCheckBox")->click();
  Wait(kSettleMs);
  EXPECT_TRUE(banner->isHidden());
  EXPECT_EQ(server_.set_count, 1);
  EXPECT_FALSE(server_.config.use_history_suggest());
  EXPECT_FALSE(server_.config.use_dictionary_suggest());
  EXPECT_FALSE(server_.config.use_realtime_conversion());
}

TEST_F(ConfigDialogAutosaveTest, ClosingWithAProblemShowsOneDialog) {
  auto dialog = MakeDialog();
  server_.fail_set = true;
  Find<QCheckBox>(dialog.get(), "historySuggestCheckBox")->click();
  Wait(kSettleMs);
  ASSERT_FALSE(Banner(dialog.get())->isHidden());

  ModalDismisser modals;
  dialog->reject();
  EXPECT_EQ(modals.count(), 1);
}

TEST_F(ConfigDialogAutosaveTest, ResetToDefaultsAsksFirst) {
  server_.config.set_use_history_suggest(false);

  {  // Cancel changes nothing.
    auto dialog = MakeDialog();
    auto* box = Find<QCheckBox>(dialog.get(), "historySuggestCheckBox");
    ASSERT_FALSE(box->isChecked());
    ModalDismisser modals(QMessageBox::Cancel);
    ResetButton(dialog.get())->click();
    Wait(kSettleMs);
    EXPECT_EQ(modals.count(), 1);
    EXPECT_FALSE(box->isChecked());
    EXPECT_EQ(server_.set_count, 0);
  }
  {  // OK applies the defaults and saves them.
    auto dialog = MakeDialog();
    auto* box = Find<QCheckBox>(dialog.get(), "historySuggestCheckBox");
    ModalDismisser modals(QMessageBox::Ok);
    ResetButton(dialog.get())->click();
    Wait(kSettleMs);
    EXPECT_EQ(modals.count(), 1);
    EXPECT_TRUE(box->isChecked());
    EXPECT_EQ(server_.set_count, 1);
    EXPECT_TRUE(server_.config.use_history_suggest());
  }
}

TEST_F(ConfigDialogAutosaveTest, PreviewModeNeverSavesAndStaysQuiet) {
  qputenv("MARINAMOJI_PREFS_UI_PREVIEW", "1");
  server_.fail_get = true;  // No converter to talk to in a preview build.
  ModalDismisser modals;
  auto dialog = MakeDialog();
  Find<QCheckBox>(dialog.get(), "historySuggestCheckBox")->click();
  Wait(kSettleMs);
  dialog->reject();

  EXPECT_EQ(server_.set_count, 0);
  EXPECT_TRUE(Banner(dialog.get())->isHidden());
  EXPECT_EQ(modals.count(), 0);
}

TEST_F(ConfigDialogAutosaveTest, OtherPreviewValuesChangeNothing) {
  for (const char* value : {"", "0", "true", "yes", "11"}) {
    SCOPED_TRACE(value);
    server_.set_count = 0;
    server_.config.set_use_history_suggest(true);
    qputenv("MARINAMOJI_PREFS_UI_PREVIEW", value);
    auto dialog = MakeDialog();
    Find<QCheckBox>(dialog.get(), "historySuggestCheckBox")->click();
    Wait(kSettleMs);
    EXPECT_EQ(server_.set_count, 1);
    EXPECT_FALSE(server_.config.use_history_suggest());
  }
}

}  // namespace
}  // namespace gui
}  // namespace mozc
