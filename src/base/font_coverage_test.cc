// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "base/font_coverage.h"

#include "testing/gunit.h"

namespace mozc {
namespace {

// Which CJK characters are drawable depends on the fonts installed, so only
// the machine-independent cases are asserted.
TEST(FontCoverageTest, AsciiAndEmptyAreAlwaysRenderable) {
  EXPECT_TRUE(CanRenderCodePoint(U'a'));
  EXPECT_TRUE(CanRenderText(""));
  EXPECT_TRUE(CanRenderText("abc"));
}

TEST(FontCoverageTest, InvalidUtf8IsNotHidden) {
  EXPECT_TRUE(CanRenderText("\xff\xfe"));
}

TEST(FontCoverageTest, AnswerIsStable) {
  EXPECT_EQ(CanRenderCodePoint(0x6728), CanRenderCodePoint(0x6728));
}

#if defined(__APPLE__)
// macOS always ships fonts for common kanji, and nothing draws a noncharacter.
TEST(FontCoverageTest, MacOsKnownAnswers) {
  EXPECT_TRUE(CanRenderCodePoint(0x6728));    // 木
  EXPECT_FALSE(CanRenderCodePoint(0x10FFFF));  // noncharacter
}
#endif

}  // namespace
}  // namespace mozc
