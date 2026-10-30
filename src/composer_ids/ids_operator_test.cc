// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "composer_ids/ids_operator.h"

#include "testing/gunit.h"

namespace mozc {
namespace composer_ids {
namespace {

TEST(IdsOperatorTest, HasAllTwelveStandardOperators) {
  EXPECT_EQ(IdsOperators().size(), 12);
  for (const IdsOperatorInfo &info : IdsOperators()) {
    EXPECT_TRUE(IsIdsOperator(info.op)) << info.name;
  }
}

TEST(IdsOperatorTest, OnlyTheTripleOperatorsTakeThreeParts) {
  for (const IdsOperatorInfo &info : IdsOperators()) {
    const bool triple = info.op == "⿲" || info.op == "⿳";
    EXPECT_EQ(PartCountFor(info.op), triple ? 3 : 2) << info.name;
  }
}

TEST(IdsOperatorTest, NonOperatorsAreRejected) {
  EXPECT_FALSE(IsIdsOperator("木"));
  EXPECT_FALSE(IsIdsOperator(""));
  EXPECT_FALSE(IsIdsOperator("⿰木"));
  EXPECT_FALSE(IsIdsOperator("\xE2\xBF\xBC"));  // U+2FFC, not one of the twelve
  EXPECT_EQ(PartCountFor("木"), 2);
}

TEST(IdsOperatorTest, ExpandIdsAbbreviationsUsesZiToolsCodes) {
  EXPECT_EQ(ExpandIdsAbbreviations("LR言某"), "⿰言某");
  EXPECT_EQ(ExpandIdsAbbreviations("lr日月"), "⿰日月");
  EXPECT_EQ(ExpandIdsAbbreviations("UD木口"), "⿱木口");
  EXPECT_EQ(ExpandIdsAbbreviations("LL木木木"), "⿲木木木");
  EXPECT_EQ(ExpandIdsAbbreviations("XX中"), "⿻中");
  // Already an operator: leave it alone.
  EXPECT_EQ(ExpandIdsAbbreviations("⿰言某"), "⿰言某");
  // Nested codes.
  EXPECT_EQ(ExpandIdsAbbreviations("LR木UD日月"), "⿰木⿱日月");
  // Unknown letters stay.
  EXPECT_EQ(ExpandIdsAbbreviations("AB木"), "AB木");
}

TEST(IdsOperatorTest, EveryOperatorHasATwoLetterAbbr) {
  for (const IdsOperatorInfo &info : IdsOperators()) {
    ASSERT_EQ(info.abbr.size(), 2) << info.name;
    EXPECT_EQ(ExpandIdsAbbreviations(std::string(info.abbr)), info.op)
        << info.name;
  }
}

}  // namespace
}  // namespace composer_ids
}  // namespace mozc
