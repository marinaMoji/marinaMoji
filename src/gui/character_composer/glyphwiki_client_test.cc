// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "gui/character_composer/glyphwiki_client.h"

#include "base/file_util.h"
#include "base/system_util.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace gui {
namespace {

class GlyphWikiClientTest : public testing::TestWithTempUserProfile {};

TEST_F(GlyphWikiClientTest, ParseGlyphWikiApiDataReadsDataField) {
  constexpr char kJson[] =
      R"({"name":"u4e00","related":"U+4E00","version":46,"data":"99:0:0:0:0:200:200:u4e00-j"})";
  const auto data = ParseGlyphWikiApiData(kJson);
  ASSERT_TRUE(data.has_value());
  EXPECT_EQ(*data, "99:0:0:0:0:200:200:u4e00-j");
}

TEST_F(GlyphWikiClientTest, ParseGlyphWikiApiDataRejectsEmptyObject) {
  EXPECT_FALSE(ParseGlyphWikiApiData("{}").has_value());
  EXPECT_FALSE(ParseGlyphWikiApiData("{\"data\":\"\"}").has_value());
}

TEST_F(GlyphWikiClientTest, CacheRoundTrip) {
  ASSERT_TRUE(StoreGlyphWikiCache("u4e00-test", "1:0:0:0:0:200:200"));
  const auto loaded = LoadGlyphWikiCache("u4e00-test");
  ASSERT_TRUE(loaded.has_value());
  EXPECT_EQ(*loaded, "1:0:0:0:0:200:200");
  EXPECT_TRUE(FileUtil::DirectoryExists(GlyphWikiCacheDirectory()).ok());
}

TEST_F(GlyphWikiClientTest, CacheRejectsUnsafeNames) {
  EXPECT_FALSE(StoreGlyphWikiCache("../evil", "data"));
  EXPECT_FALSE(LoadGlyphWikiCache("u4e00/../x").has_value());
}

TEST_F(GlyphWikiClientTest, EnsureUsesCacheWithoutNetwork) {
  ASSERT_TRUE(StoreGlyphWikiCache("u-test-leaf", "1:0:2:10:10:20:20"));
  kage::Buhin scratch;
  EXPECT_TRUE(EnsureGlyphWikiGlyph("u-test-leaf", &scratch,
                                    /*allow_network=*/false));
  EXPECT_EQ(scratch.Search("u-test-leaf"), "1:0:2:10:10:20:20");
  EXPECT_FALSE(EnsureGlyphWikiGlyph("u-missing-xyz", &scratch,
                                    /*allow_network=*/false));
}

}  // namespace
}  // namespace gui
}  // namespace mozc
