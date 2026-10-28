// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "gui/character_composer/word_insert.h"

#include <string>

#include "testing/gunit.h"

namespace mozc {
namespace gui {
namespace {

TEST(WordInsertTest, ParsesTheBundleId) {
  EXPECT_EQ(ParseBundleId("\"bundleid\"=\"com.microsoft.Word\"\n"),
            "com.microsoft.Word");
  EXPECT_EQ(ParseBundleId("\"bundleid\"=[ NULL ]"), "");
  EXPECT_EQ(ParseBundleId(""), "");
}

TEST(WordInsertTest, ParsesTheVisibleAsnsFrontToBack) {
  const auto asns = ParseVisibleAsns(
      "ASN:0x0-0x1b01b-\"Claude\": ASN:0x0-0x26d26d-\"Microsoft_Word\": "
      "ASN:0x0-0x15015-\"R\u00e9glages\":");
  ASSERT_EQ(asns.size(), 3);
  EXPECT_EQ(asns[0], "ASN:0x0-0x1b01b");
  EXPECT_EQ(asns[1], "ASN:0x0-0x26d26d");
  EXPECT_TRUE(ParseVisibleAsns("").empty());
}

TEST(WordInsertTest, ReadsTheIdsFromTheSvg) {
  EXPECT_EQ(IdsFromSvg("<svg aria-label=\"⿰木巻\"><title>x</title></svg>"), "⿰木巻");
  EXPECT_EQ(IdsFromSvg("<svg aria-label=\"a&amp;b\">"), "a&b");
  EXPECT_EQ(IdsFromSvg("<svg>"), "composed");
}

TEST(WordInsertTest, ScriptInsertsSizesAndTags) {
  const std::string script = WordInsertScript("/Users/a b/\"x\".svg", "⿰木巻");
  EXPECT_NE(script.find("make new inline picture at r"), std::string::npos);
  EXPECT_NE(script.find("file name:\"/Users/a b/\\\"x\\\".svg\""), std::string::npos);
  EXPECT_NE(script.find("MARINAMOJI:ids=⿰木巻"), std::string::npos);
  EXPECT_NE(script.find("pt * 1\n"), std::string::npos);
  EXPECT_NE(script.find("(round (pt * 0.19))"), std::string::npos);
  EXPECT_NE(script.find("set font position"), std::string::npos);
}

}  // namespace
}  // namespace gui
}  // namespace mozc
