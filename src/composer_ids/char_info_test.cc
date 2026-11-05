// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "composer_ids/char_info.h"

#include "testing/gunit.h"

namespace mozc {
namespace composer_ids {
namespace {

TEST(CharInfoTableTest, ParsesJapaneseColumns) {
  constexpr char kTsv[] =
      "#char\tcp\tstrokes\tpinyin\tja_on\tja_kun\trs\tdefinition\tvariants\t"
      "ja_kana\told_form\tnew_form\n"
      "國\tU+570B\t11\tguó\tKOKU\tKUNI\t31.8\tnation\t国\tコク くに\t\t国\n";
  auto table = CharInfoTable::FromTsv(kTsv);
  ASSERT_TRUE(table.ok());
  const CharInfo *info = table->Find("國");
  ASSERT_NE(info, nullptr);
  EXPECT_EQ(info->ja_kana, "コク くに");
  EXPECT_EQ(info->old_form, "");
  EXPECT_EQ(info->new_form, "国");
}

TEST(CharInfoTableTest, OldFilesWithoutJapaneseColumnsStillParse) {
  auto table = CharInfoTable::FromTsv("謀\tU+8B00\t16\n");
  ASSERT_TRUE(table.ok());
  EXPECT_EQ(table->Find("謀")->ja_kana, "");
}

TEST(CharInfoTableTest, ParsesARow) {
  constexpr char kTsv[] =
      "#char\tcp\tstrokes\tpinyin\tja_on\tja_kun\trs\tdefinition\tvariants\n"
      "謀\tU+8B00\t16\tmóu\tBOU MU\tHAKARU\t149.9\tplan\t谋\n";
  auto table = CharInfoTable::FromTsv(kTsv);
  ASSERT_TRUE(table.ok());
  ASSERT_EQ(table->size(), 1);
  const CharInfo *info = table->Find("謀");
  ASSERT_NE(info, nullptr);
  EXPECT_EQ(info->cp, "U+8B00");
  EXPECT_EQ(info->strokes, "16");
  EXPECT_EQ(info->pinyin, "móu");
  EXPECT_EQ(info->ja_on, "BOU MU");
  EXPECT_EQ(info->ja_kun, "HAKARU");
  EXPECT_EQ(info->rs, "149.9");
  EXPECT_EQ(info->definition, "plan");
  EXPECT_EQ(info->variants, "谋");
  EXPECT_EQ(table->Find("木"), nullptr);
}

TEST(CharInfoTableTest, RelatedCandidatesOrderAndDedup) {
  constexpr char kTsv[] =
      "学\tU+5B66\t8\t\t\t\t\t\t學 斈 學\t\t學\t\n"
      "學\tU+5B78\t16\t\t\t\t\t\t学\t\t\t学\n"
      "的\tU+7684\t8\t\t\t\t\t\t\t\t\t\n";
  auto table = CharInfoTable::FromTsv(kTsv);
  ASSERT_TRUE(table.ok());

  const std::vector<std::string> from_gaku = table->RelatedCandidates("学");
  ASSERT_EQ(from_gaku.size(), 3);
  EXPECT_EQ(from_gaku[0], "学");
  EXPECT_EQ(from_gaku[1], "學");
  EXPECT_EQ(from_gaku[2], "斈");

  const std::vector<std::string> from_old = table->RelatedCandidates("學");
  ASSERT_EQ(from_old.size(), 2);
  EXPECT_EQ(from_old[0], "學");
  EXPECT_EQ(from_old[1], "学");

  EXPECT_TRUE(table->RelatedCandidates("的").empty());
  EXPECT_TRUE(table->RelatedCandidates("木").empty());
}

}  // namespace
}  // namespace composer_ids
}  // namespace mozc
