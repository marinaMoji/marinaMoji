// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// Drives the dialog the way a user would, on Qt's offscreen platform, with the
// small fixture tables (not the GPL data).

#include "gui/character_composer/character_composer_dialog.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QToolButton>
#include <memory>
#include <string>
#include <vector>

#include "base/file_util.h"
#include "composer_ids/ids_table.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace gui {
namespace {

class CharacterComposerDialogTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    static int argc = 1;
    static char name[] = "character_composer_dialog_test";
    static char *argv[] = {name, nullptr};
    app_ = new QApplication(argc, argv);
  }

  std::unique_ptr<CharacterComposerDialog> MakeDialog() {
    const std::string dir = FileUtil::Dirname(testing::GetSourceFileOrDie(
        {"composer_ids", "testdata", "ids_table.tsv"}));
    auto table = composer_ids::IdsTable::LoadFromDirectory(dir);
    EXPECT_TRUE(table.ok());
    auto dialog = std::make_unique<CharacterComposerDialog>(
        std::make_unique<composer_ids::IdsTable>(*std::move(table)),
        QString::fromStdString(dir));
    dialog->show();
    return dialog;
  }

  static std::vector<SlotEdit *> Edits(QWidget *w) {
    // A rebuilt layout leaves the old widgets queued for deletion.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    std::vector<SlotEdit *> edits;
    for (SlotEdit *e : w->findChildren<SlotEdit *>()) edits.push_back(e);
    return edits;
  }

  // Types |text| into the edit at |path| the way a keystroke does.
  static void Type(SlotEdit *edit, const QString &text) {
    edit->setFocus();
    edit->setText(text);
    emit edit->textEdited(text);
  }

  static QToolButton *PaletteButton(QWidget *w, const QString &op) {
    for (QToolButton *b : w->findChildren<QToolButton *>()) {
      if (b->toolTip().startsWith(op)) return b;
    }
    return nullptr;
  }

  static QPushButton *Button(QWidget *w, const QString &text) {
    for (QPushButton *b : w->findChildren<QPushButton *>()) {
      if (b->text() == text) return b;
    }
    return nullptr;
  }

  static QApplication *app_;
};

QApplication *CharacterComposerDialogTest::app_ = nullptr;

TEST_F(CharacterComposerDialogTest, StartsWithTwoEmptyBoxesAndInsertDisabled) {
  auto dialog = MakeDialog();
  EXPECT_EQ(Edits(dialog.get()).size(), 2);
  EXPECT_FALSE(Button(dialog.get(), "Insert")->isEnabled());
}

TEST_F(CharacterComposerDialogTest, FillingBothBoxesFindsTheEncodedCharacter) {
  auto dialog = MakeDialog();
  auto edits = Edits(dialog.get());
  Type(edits[0], QStringLiteral("言"));
  Type(edits[1], QStringLiteral("某"));
  EXPECT_TRUE(Button(dialog.get(), "Insert")->isEnabled());
  Button(dialog.get(), "Insert")->click();
  EXPECT_EQ(dialog->result(), "謀");
}

TEST_F(CharacterComposerDialogTest, UnrecordedStructureInsertsSvg) {
  auto dialog = MakeDialog();
  auto edits = Edits(dialog.get());
  Type(edits[0], QStringLiteral("牙"));
  Type(edits[1], QStringLiteral("齒"));
  EXPECT_TRUE(Button(dialog.get(), "Insert")->isEnabled());
  Button(dialog.get(), "Insert")->click();
  EXPECT_TRUE(dialog->result().empty());
  EXPECT_NE(dialog->result_svg().find("<svg"), std::string::npos);
  EXPECT_NE(dialog->result_svg().find("牙"), std::string::npos);
  EXPECT_NE(dialog->result_svg().find("齒"), std::string::npos);
}

TEST_F(CharacterComposerDialogTest, PaletteChangesOuterLayoutAndKeepsText) {
  auto dialog = MakeDialog();
  Type(Edits(dialog.get())[0], QStringLiteral("艹"));
  Type(Edits(dialog.get())[1], QStringLiteral("干"));
  // ⿰艹干 is unrecorded, but Insert is still offered (SVG drawing).
  EXPECT_TRUE(Button(dialog.get(), "Insert")->isEnabled());
  // Palette: the outer square is selected after the layout was rebuilt? Select
  // it explicitly, as a click on the padding does.
  dialog->findChild<LayoutEditor *>()->Select({});
  PaletteButton(dialog.get(), QStringLiteral("⿱"))->click();
  EXPECT_EQ(Edits(dialog.get()).size(), 2);
  EXPECT_TRUE(Button(dialog.get(), "Insert")->isEnabled());  // ⿱艹干 -> ⺸
  Button(dialog.get(), "Insert")->click();
  EXPECT_EQ(dialog->result(), "⺸");
  EXPECT_TRUE(dialog->result_svg().empty());
}

TEST_F(CharacterComposerDialogTest, ThreePartOperatorAddsAThirdBox) {
  auto dialog = MakeDialog();
  PaletteButton(dialog.get(), QStringLiteral("⿲"))->click();
  EXPECT_EQ(Edits(dialog.get()).size(), 3);
}

TEST_F(CharacterComposerDialogTest, SelectedBoxBecomesANestedLayout) {
  auto dialog = MakeDialog();
  auto *editor = dialog->findChild<LayoutEditor *>();
  Type(Edits(dialog.get())[0], QStringLiteral("言"));
  editor->Select({1});
  PaletteButton(dialog.get(), QStringLiteral("⿱"))->click();
  // Two top-level boxes became one box plus a layout of two.
  EXPECT_EQ(Edits(dialog.get()).size(), 3);
  EXPECT_EQ(editor->op(), "⿰");
  ASSERT_EQ(editor->parts().size(), 2);
  EXPECT_EQ(editor->parts()[1].kind, composer_ids::ComposerSlot::Kind::kNested);
  EXPECT_EQ(editor->parts()[1].op, "⿱");
}

TEST_F(CharacterComposerDialogTest, RemovingAFilledNestedLayoutIsRefused) {
  auto dialog = MakeDialog();
  auto *editor = dialog->findChild<LayoutEditor *>();
  editor->Select({1});
  PaletteButton(dialog.get(), QStringLiteral("⿱"))->click();
  Type(Edits(dialog.get())[1], QStringLiteral("日"));
  Type(Edits(dialog.get())[2], QStringLiteral("月"));
  EXPECT_FALSE(editor->Flatten({1}));
  EXPECT_EQ(editor->parts()[1].kind, composer_ids::ComposerSlot::Kind::kNested);
}

TEST_F(CharacterComposerDialogTest, BreakApartLoadsTheDecomposition) {
  auto dialog = MakeDialog();
  auto *edit = dialog->findChild<QLineEdit *>(QString(), Qt::FindChildrenRecursively);
  // The break-apart field is the only plain QLineEdit (box fields are SlotEdit).
  QLineEdit *break_apart = nullptr;
  for (QLineEdit *e : dialog->findChildren<QLineEdit *>()) {
    if (!dynamic_cast<SlotEdit *>(e)) break_apart = e;
  }
  ASSERT_NE(break_apart, nullptr);
  (void)edit;
  break_apart->setText(QStringLiteral("謀"));
  Button(dialog.get(), "Break apart")->click();
  auto *editor = dialog->findChild<LayoutEditor *>();
  EXPECT_EQ(editor->op(), "⿰");
  EXPECT_EQ(editor->parts()[0].input, "言");
  EXPECT_EQ(editor->parts()[1].input, "某");
  EXPECT_TRUE(Button(dialog.get(), "Insert")->isEnabled());
}

TEST_F(CharacterComposerDialogTest, WorksWithoutDataFiles) {
  CharacterComposerDialog dialog(nullptr, QStringLiteral("/nowhere"));
  dialog.show();
  auto edits = Edits(&dialog);
  Type(edits[0], QStringLiteral("言"));
  Type(edits[1], QStringLiteral("某"));
  EXPECT_FALSE(Button(&dialog, "Insert")->isEnabled());
}

}  // namespace
}  // namespace gui
}  // namespace mozc
