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

}  // namespace
}  // namespace composer_ids
}  // namespace mozc
