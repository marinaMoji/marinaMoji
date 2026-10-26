// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// Runs RunCharacterComposer() for real on Qt's offscreen platform: a timer
// plays the user (types into the boxes, presses Insert) and the test checks the
// file the input method would read.

#include <QApplication>
#include <QCoreApplication>
#include <QPushButton>
#include <QTimer>
#include <chrono>
#include <cstdlib>
#include <thread>
#include <string>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "base/file_util.h"
#include "base/system_util.h"
#include "composer_ids/composer_result.h"
#include "gui/character_composer/character_composer_dialog.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

ABSL_DECLARE_FLAG(std::string, result_file);
int RunCharacterComposer(int argc, char *argv[]);

namespace mozc {
namespace gui {
namespace {

// Fills the open dialog like a user would and presses |button|. RunCharacterComposer
// creates the QApplication and then blocks in exec(), so this waits for the
// application on another thread and runs on the GUI thread once it is up.
void Drive(const char *first, const char *second, const QString &button) {
  std::thread([=]() {
    while (QCoreApplication::instance() == nullptr) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    QTimer::singleShot(0, QCoreApplication::instance(), [=]() {
      QTimer::singleShot(300, QCoreApplication::instance(), [=]() {
    CharacterComposerDialog *dialog = nullptr;
    for (QWidget *w : QApplication::topLevelWidgets()) {
      if (auto *d = qobject_cast<CharacterComposerDialog *>(w)) dialog = d;
    }
    ASSERT_NE(dialog, nullptr);
    auto edits = dialog->findChildren<SlotEdit *>();
    if (edits.size() == 2) {
      edits[0]->setFocus();
      edits[0]->setText(QString::fromUtf8(first));
      emit edits[0]->textEdited(edits[0]->text());
      edits[1]->setFocus();
      edits[1]->setText(QString::fromUtf8(second));
      emit edits[1]->textEdited(edits[1]->text());
    }
    for (QPushButton *b : dialog->findChildren<QPushButton *>()) {
      if (b->text() == button) b->click();
    }
      });
    });
  }).detach();
}

class CharacterComposerLibmainTest : public testing::TestWithTempUserProfile {};

TEST_F(CharacterComposerLibmainTest, InsertWritesTheResultFile) {
  const std::string dir = FileUtil::Dirname(testing::GetSourceFileOrDie(
      {"composer_ids", "testdata", "ids_table.tsv"}));
  setenv("MARINAMOJI_CHARACTER_COMPOSER_DIR", dir.c_str(), 1);
  const std::string result =
      FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(), "r.txt");
  absl::SetFlag(&FLAGS_result_file, result);

  static int argc = 1;
  static char name[] = "character_composer_libmain_test";
  static char *argv[] = {name, nullptr};
  Drive("言", "某", "Insert");
  EXPECT_EQ(RunCharacterComposer(argc, argv), 0);
  EXPECT_EQ(composer_ids::ReadComposerResult(result), "謀");
}

}  // namespace
}  // namespace gui
}  // namespace mozc
