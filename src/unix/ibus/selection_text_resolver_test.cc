// Copyright 2010-2021, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "unix/ibus/selection_text_resolver.h"

#include "protocol/commands.pb.h"
#include "testing/gunit.h"
#include "unix/ibus/surrounding_text_util.h"

namespace mozc {
namespace ibus {

TEST(SurroundingTextUtilSelectionTest, CharsBeforeCursor) {
  EXPECT_EQ(SurroundingTextUtil::CharsBeforeCursor("あいう", 3, 1), "う");
  EXPECT_EQ(SurroundingTextUtil::CharsBeforeCursor("あいう", 3, 2), "いう");
  EXPECT_EQ(SurroundingTextUtil::CharsBeforeCursor("あいう", 0, 1), "");
  EXPECT_EQ(SurroundingTextUtil::CharsBeforeCursor("abc", 2, 4), "ab");
}

TEST(SurroundingTextUtilSelectionTest, CharsAfterCursor) {
  EXPECT_EQ(SurroundingTextUtil::CharsAfterCursor("あいう", 0, 1), "あ");
  EXPECT_EQ(SurroundingTextUtil::CharsAfterCursor("あいう", 2, 2), "う");
  EXPECT_EQ(SurroundingTextUtil::CharsAfterCursor("あいう", 3, 1), "");
}

TEST(SurroundingTextUtilSelectionTest, WordBeforeCursor) {
  EXPECT_EQ(SurroundingTextUtil::WordBeforeCursor("foo bar", 7, 20), "bar");
  EXPECT_EQ(SurroundingTextUtil::WordBeforeCursor("foo 漢字", 6, 20), "漢字");
  EXPECT_EQ(SurroundingTextUtil::WordBeforeCursor("foo ", 4, 20), "");
}

TEST(SurroundingTextUtilSelectionTest, SelectionEndsAtCursor) {
  int32_t offset = 0;
  int32_t length = 0;
  EXPECT_TRUE(SurroundingTextUtil::SelectionEndsAtCursor("あいうえお", 3, "いう",
                                                         &offset, &length));
  EXPECT_EQ(offset, -2);
  EXPECT_EQ(length, 2);
  EXPECT_FALSE(SurroundingTextUtil::SelectionEndsAtCursor("あいうえお", 3, "あい",
                                                          &offset, &length));
}

TEST(SurroundingTextUtilSelectionTest, SelectionStartsAtCursor) {
  int32_t offset = 0;
  int32_t length = 0;
  EXPECT_TRUE(SurroundingTextUtil::SelectionStartsAtCursor("あいうえお", 1, "いう",
                                                           &offset, &length));
  EXPECT_EQ(offset, 0);
  EXPECT_EQ(length, 2);
  EXPECT_FALSE(SurroundingTextUtil::SelectionStartsAtCursor(
      "あいうえお", 1, "あい", &offset, &length));
}

TEST(SelectionTextResolverTest, PrefersSurroundingSelection) {
  SurroundingSnapshot snap;
  snap.available = true;
  snap.cursor_pos = 2;
  snap.anchor_pos = 1;
  snap.relative_selected_length = 1;
  snap.surrounding_text = "漢字";
  snap.selection_text = "字";

  const ResolvedSelection resolved = ResolveSelectionForCommandWithFallbacks(
      commands::SessionCommand::CONVERT_RELATED, snap, "漢", "漢");
  EXPECT_EQ(resolved.source, SelectionSource::kSurroundingSelection);
  EXPECT_EQ(resolved.text, "字");
  EXPECT_TRUE(resolved.can_delete);
  EXPECT_EQ(resolved.deletion_offset, -1);
}

TEST(SelectionTextResolverTest, PrimaryWhenSurroundingHasNoSelection) {
  SurroundingSnapshot snap;
  snap.available = true;
  snap.cursor_pos = 2;
  snap.anchor_pos = 2;
  snap.relative_selected_length = 0;
  snap.surrounding_text = "漢字";
  snap.selection_text = "";

  const ResolvedSelection resolved = ResolveSelectionForCommandWithFallbacks(
      commands::SessionCommand::CONVERT_RELATED, snap, "字", "");
  EXPECT_EQ(resolved.source, SelectionSource::kPrimary);
  EXPECT_EQ(resolved.text, "字");
  EXPECT_TRUE(resolved.can_delete);
  EXPECT_EQ(resolved.deletion_offset, -1);
}

// Wayland often collapses a mouse selection with the caret at the *start*
// of the highlight. Primary holds "字"; caret-before would wrongly pick "漢".
TEST(SelectionTextResolverTest, PrimaryWhenCaretAtStartOfSelection) {
  SurroundingSnapshot snap;
  snap.available = true;
  snap.cursor_pos = 1;
  snap.anchor_pos = 1;
  snap.relative_selected_length = 0;
  snap.surrounding_text = "漢字";
  snap.selection_text = "";

  const ResolvedSelection resolved = ResolveSelectionForCommandWithFallbacks(
      commands::SessionCommand::CONVERT_RELATED, snap, "字", "");
  EXPECT_EQ(resolved.source, SelectionSource::kPrimary);
  EXPECT_EQ(resolved.text, "字");
  EXPECT_TRUE(resolved.can_delete);
  EXPECT_EQ(resolved.deletion_offset, 0);
  EXPECT_EQ(resolved.deletion_length, 1);
}

// Without primary, related/components prefer the character AFTER the caret
// (Wayland mouse-select leaves the caret at the start of the highlight).
TEST(SelectionTextResolverTest, CaretAfterPreferredForRelated) {
  SurroundingSnapshot snap;
  snap.available = true;
  snap.cursor_pos = 1;
  snap.anchor_pos = 1;
  snap.surrounding_text = "漢字";
  snap.selection_text = "";

  const ResolvedSelection resolved = ResolveSelectionForCommandWithFallbacks(
      commands::SessionCommand::CONVERT_RELATED, snap, "", "");
  EXPECT_EQ(resolved.source, SelectionSource::kCaretFallback);
  EXPECT_EQ(resolved.text, "字");
  EXPECT_TRUE(resolved.can_delete);
  EXPECT_EQ(resolved.deletion_offset, 0);
}

TEST(SelectionTextResolverTest, CaretBeforeAtEndOfLine) {
  SurroundingSnapshot snap;
  snap.available = true;
  snap.cursor_pos = 2;
  snap.anchor_pos = 2;
  snap.surrounding_text = "漢字";
  snap.selection_text = "";

  const ResolvedSelection resolved = ResolveSelectionForCommandWithFallbacks(
      commands::SessionCommand::CONVERT_RELATED, snap, "", "");
  EXPECT_EQ(resolved.source, SelectionSource::kCaretFallback);
  EXPECT_EQ(resolved.text, "字");
  EXPECT_EQ(resolved.deletion_offset, -1);
}

TEST(SelectionTextResolverTest, RejectsStalePrimary) {
  SurroundingSnapshot snap;
  snap.available = true;
  snap.cursor_pos = 2;
  snap.anchor_pos = 2;
  snap.surrounding_text = "漢字";
  snap.selection_text = "";

  // Primary still holds an old selection that is not at the caret.
  const ResolvedSelection resolved = ResolveSelectionForCommandWithFallbacks(
      commands::SessionCommand::CONVERT_RELATED, snap, "別", "");
  EXPECT_EQ(resolved.source, SelectionSource::kCaretFallback);
  EXPECT_EQ(resolved.text, "字");
}

TEST(SelectionTextResolverTest, AtspiBeforeCaret) {
  SurroundingSnapshot snap;
  snap.available = true;
  snap.cursor_pos = 3;
  snap.anchor_pos = 3;
  snap.surrounding_text = "一二三";
  snap.selection_text = "";

  const ResolvedSelection resolved = ResolveSelectionForCommandWithFallbacks(
      commands::SessionCommand::CONVERT_COMPONENTS, snap, /*primary=*/"",
      /*atspi=*/"二三");
  EXPECT_EQ(resolved.source, SelectionSource::kAtspi);
  EXPECT_EQ(resolved.text, "二三");
  EXPECT_TRUE(resolved.can_delete);
  EXPECT_EQ(resolved.deletion_offset, -2);
}

TEST(SelectionTextResolverTest, CaretFallbackForRelated) {
  SurroundingSnapshot snap;
  snap.available = true;
  snap.cursor_pos = 1;
  snap.anchor_pos = 1;
  snap.surrounding_text = "龍";
  snap.selection_text = "";

  const ResolvedSelection resolved = ResolveSelectionForCommandWithFallbacks(
      commands::SessionCommand::CONVERT_RELATED, snap, "", "");
  EXPECT_EQ(resolved.source, SelectionSource::kCaretFallback);
  EXPECT_EQ(resolved.text, "龍");
  EXPECT_TRUE(resolved.can_delete);
  EXPECT_EQ(resolved.deletion_offset, -1);
}

TEST(SelectionTextResolverTest, CaretFallbackAfterCursorWhenAtStart) {
  SurroundingSnapshot snap;
  snap.available = true;
  snap.cursor_pos = 0;
  snap.anchor_pos = 0;
  snap.surrounding_text = "龍";
  snap.selection_text = "";

  // WithFallbacks: avoid the live ResolveSelectionForCommand path, which
  // walks AT-SPI and can abort the process when no accessibility bus is up.
  const ResolvedSelection resolved = ResolveSelectionForCommandWithFallbacks(
      commands::SessionCommand::CONVERT_RELATED, snap, "", "");
  EXPECT_EQ(resolved.source, SelectionSource::kCaretFallback);
  EXPECT_EQ(resolved.text, "龍");
  EXPECT_TRUE(resolved.can_delete);
  EXPECT_EQ(resolved.deletion_offset, 0);
}

TEST(SelectionTextResolverTest, LengthGates) {
  EXPECT_TRUE(IsAcceptableSelectionLength(
      commands::SessionCommand::CONVERT_RELATED, 1));
  EXPECT_FALSE(IsAcceptableSelectionLength(
      commands::SessionCommand::CONVERT_RELATED, 2));
  EXPECT_TRUE(IsAcceptableSelectionLength(
      commands::SessionCommand::CONVERT_COMPONENTS, 4));
  EXPECT_FALSE(IsAcceptableSelectionLength(
      commands::SessionCommand::CONVERT_COMPONENTS, 5));
}

}  // namespace ibus
}  // namespace mozc
