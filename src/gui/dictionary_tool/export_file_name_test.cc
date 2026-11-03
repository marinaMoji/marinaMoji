// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "gui/dictionary_tool/export_file_name.h"

#include "testing/gunit.h"

namespace mozc {
namespace gui {
namespace {

TEST(AddDefaultExportExtensionTest, AppendsTxtWhenThereIsNoExtension) {
  EXPECT_EQ(AddDefaultExportExtension("words"), "words.txt");
  EXPECT_EQ(AddDefaultExportExtension("/home/m/words"), "/home/m/words.txt");
  EXPECT_EQ(AddDefaultExportExtension("C:\\Users\\m\\words"),
            "C:\\Users\\m\\words.txt");
  EXPECT_EQ(AddDefaultExportExtension("/home/m/単語"), "/home/m/単語.txt");
}

TEST(AddDefaultExportExtensionTest, CompletesATrailingDot) {
  EXPECT_EQ(AddDefaultExportExtension("/home/m/words."), "/home/m/words.txt");
}

TEST(AddDefaultExportExtensionTest, KeepsAnExistingExtension) {
  EXPECT_EQ(AddDefaultExportExtension("/home/m/words.txt"),
            "/home/m/words.txt");
  EXPECT_EQ(AddDefaultExportExtension("/home/m/words.tsv"),
            "/home/m/words.tsv");
  EXPECT_EQ(AddDefaultExportExtension("/home/m/words.csv"),
            "/home/m/words.csv");
  EXPECT_EQ(AddDefaultExportExtension("/home/m/my.words.txt"),
            "/home/m/my.words.txt");
}

TEST(AddDefaultExportExtensionTest, IgnoresDotsInDirectoryNames) {
  EXPECT_EQ(AddDefaultExportExtension("/home/m/dict.d/words"),
            "/home/m/dict.d/words.txt");
  EXPECT_EQ(AddDefaultExportExtension("C:\\dict.d\\words"),
            "C:\\dict.d\\words.txt");
}

TEST(AddDefaultExportExtensionTest, LeadingDotIsNotAnExtension) {
  EXPECT_EQ(AddDefaultExportExtension("/home/m/.words"),
            "/home/m/.words.txt");
}

TEST(AddDefaultExportExtensionTest, LeavesDirectoriesAlone) {
  EXPECT_EQ(AddDefaultExportExtension("/home/m/"), "/home/m/");
  EXPECT_EQ(AddDefaultExportExtension(""), "");
}

TEST(AddDefaultExportExtensionTest, DefaultExtensionIsImportable) {
  // The import dialog's "Text Files" filter lists *.txt; keep them in step.
  EXPECT_EQ(kDictionaryExportExtension, "txt");
}

}  // namespace
}  // namespace gui
}  // namespace mozc
