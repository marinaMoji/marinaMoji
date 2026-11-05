// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "composer_ids/component_candidates.h"

#include <string>
#include <utility>
#include <vector>

#include "base/file_util.h"
#include "composer_ids/ids_table.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace composer_ids {
namespace {

IdsTable LoadFixture() {
  const std::string ids = testing::GetSourceFileOrDie(
      {"composer_ids", "testdata", "ids_table.tsv"});
  const std::string dir = FileUtil::Dirname(ids);
  auto table = IdsTable::LoadFromDirectory(dir);
  EXPECT_TRUE(table.ok()) << table.status();
  return *std::move(table);
}

TEST(ComponentCandidatesTest, DictionaryFirstThenIdsExact) {
  const IdsTable table = LoadFixture();
  // 言某 encodes to 謀; dictionary value "海" should appear first.
  const std::vector<std::string> dict = {"海"};
  const ComponentSearchResult result =
      BuildComponentCandidates(table, "言某", dict);
  ASSERT_GE(result.candidates.size(), 2u);
  EXPECT_EQ(result.candidates[0].value, "海");
  EXPECT_EQ(result.candidates[0].specimen_kind, 0);
  EXPECT_EQ(result.candidates[1].value, "謀");
  EXPECT_TRUE(result.has_exact_encode);
}

TEST(ComponentCandidatesTest, FindContainingPositiveFilterAndRadicals) {
  const IdsTable table = LoadFixture();
  // 氵/水 are the same family; FindContaining("水") finds 一 and 二 from fixture.
  const ComponentSearchResult result =
      BuildComponentCandidates(table, "氵", /*dictionary_values=*/{});
  ASSERT_FALSE(result.candidates.empty());
  bool found_one = false;
  bool found_two = false;
  for (const ComponentCandidate &c : result.candidates) {
    if (c.value == "一") found_one = true;
    if (c.value == "二") found_two = true;
  }
  EXPECT_TRUE(found_one);
  EXPECT_TRUE(found_two);
}

TEST(ComponentCandidatesTest, ExcludesSelectionItselfFromContaining) {
  const IdsTable table = LoadFixture();
  const ComponentSearchResult result =
      BuildComponentCandidates(table, "某", /*dictionary_values=*/{});
  for (const ComponentCandidate &c : result.candidates) {
    EXPECT_NE(c.value, "某");
  }
}

TEST(ComponentCandidatesTest, SvgFallbackWhenUnencoded) {
  const IdsTable table = LoadFixture();
  // 牙齒 has no encode in the fixture (only ⿰言齒 -> 𪘙).
  const ComponentSearchResult result =
      BuildComponentCandidates(table, "牙齒", /*dictionary_values=*/{});
  EXPECT_FALSE(result.has_exact_encode);
  ASSERT_FALSE(result.candidates.empty());
  const ComponentCandidate &last = result.candidates.back();
  EXPECT_EQ(last.specimen_kind, 2);
  EXPECT_FALSE(last.value.empty());
  EXPECT_EQ(result.default_op, "⿰");
}

TEST(ComponentCandidatesTest, CapAtFourAndEmpty) {
  const IdsTable table = LoadFixture();
  EXPECT_TRUE(
      BuildComponentCandidates(table, "一二三四五", {}).candidates.empty());
  EXPECT_TRUE(BuildComponentCandidates(table, "", {}).candidates.empty());
}

TEST(ComponentCandidatesTest, NoSvgWhenExactEncodeExists) {
  const IdsTable table = LoadFixture();
  const ComponentSearchResult result =
      BuildComponentCandidates(table, "木目", /*dictionary_values=*/{});
  EXPECT_TRUE(result.has_exact_encode);
  for (const ComponentCandidate &c : result.candidates) {
    EXPECT_NE(c.specimen_kind, 2);
  }
}

}  // namespace
}  // namespace composer_ids
}  // namespace mozc
