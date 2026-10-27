// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "composer_ids/composer_result.h"

#include <string>

#include "base/file_util.h"
#include "base/system_util.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace composer_ids {
namespace {

// Each test gets its own temporary user profile directory.
class ComposerResultTest : public testing::TestWithTempUserProfile {
 protected:
  std::string Path(const char *name) {
    return FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(), name);
  }
};

TEST_F(ComposerResultTest, RoundTrip) {
  const std::string path = Path("result.txt");
  ASSERT_TRUE(WriteComposerResult(path, "謀").ok());
  EXPECT_EQ(ReadComposerResult(path), "謀");
}

TEST_F(ComposerResultTest, SupplementaryPlaneCharacter) {
  const std::string path = Path("result.txt");
  ASSERT_TRUE(WriteComposerResult(path, "𠟪").ok());
  EXPECT_EQ(ReadComposerResult(path), "𠟪");
}

TEST_F(ComposerResultTest, MissingOrEmptyFileMeansNoResult) {
  EXPECT_EQ(ReadComposerResult(Path("absent.txt")), std::nullopt);
  const std::string path = Path("empty.txt");
  ASSERT_TRUE(FileUtil::SetContents(path, " \r\n").ok());
  EXPECT_EQ(ReadComposerResult(path), std::nullopt);
}

TEST_F(ComposerResultTest, SurroundingWhitespaceIsRemoved) {
  const std::string path = Path("result.txt");
  ASSERT_TRUE(FileUtil::SetContents(path, "  謀\r\n").ok());
  EXPECT_EQ(ReadComposerResult(path), "謀");
}

TEST_F(ComposerResultTest, OversizeOrInvalidUtf8IsRefused) {
  const std::string path = Path("result.txt");
  ASSERT_TRUE(FileUtil::SetContents(path, std::string(33, 'a')).ok());
  EXPECT_EQ(ReadComposerResult(path), std::nullopt);
  ASSERT_TRUE(FileUtil::SetContents(path, "\xFF\xFE").ok());
  EXPECT_EQ(ReadComposerResult(path), std::nullopt);
}

TEST_F(ComposerResultTest, DefaultPathIsInTheUserProfile) {
  EXPECT_EQ(DefaultResultFilePath(), Path("character_composer_result.txt"));
}

TEST_F(ComposerResultTest, OnlyOfficeScriptEscapesTheSvg) {
  EXPECT_EQ(OnlyOfficeHandoffScript("<svg a=\"1\">\n\\</svg>", 1234),
            "window.marinaMojiComposerResult={id:1234,"
            "svg:\"<svg a=\\\"1\\\">\\n\\\\</svg>\"};\n");
  // Non-ASCII text passes through; line separators and control characters are
  // escaped so the literal stays valid JavaScript.
  EXPECT_EQ(OnlyOfficeHandoffScript("字\xE2\x80\xA8\x01", 5),
            "window.marinaMojiComposerResult={id:5,"
            "svg:\"字\\u2028\\u0001\"};\n");
}

TEST_F(ComposerResultTest, OnlyOfficeHandoffNeedsTheInstalledPlugin) {
  const std::string missing = Path("no_such_plugin_dir");
  EXPECT_TRUE(WriteOnlyOfficeHandoff(missing, "<svg/>", 1).ok());
  EXPECT_FALSE(FileUtil::FileExists(
                   FileUtil::JoinPath(missing, "composer_result.js")).ok());
  EXPECT_TRUE(WriteOnlyOfficeHandoff("", "<svg/>", 1).ok());
}

TEST_F(ComposerResultTest, OnlyOfficeHandoffWritesNextToThePlugin) {
  const std::string dir = Path("plugin");
  ASSERT_TRUE(FileUtil::CreateDirectory(dir).ok());
  ASSERT_TRUE(WriteOnlyOfficeHandoff(dir, "<svg/>", 42).ok());
  const auto contents =
      FileUtil::GetContents(FileUtil::JoinPath(dir, "composer_result.js"));
  ASSERT_TRUE(contents.ok());
  EXPECT_EQ(*contents,
            "window.marinaMojiComposerResult={id:42,svg:\"<svg/>\"};\n");
}

}  // namespace
}  // namespace composer_ids
}  // namespace mozc
