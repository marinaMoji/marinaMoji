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

}  // namespace
}  // namespace composer_ids
}  // namespace mozc
