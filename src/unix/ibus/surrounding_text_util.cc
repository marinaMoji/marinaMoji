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

#include "unix/ibus/surrounding_text_util.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "base/util.h"

namespace mozc {
namespace ibus {

bool SurroundingTextUtil::GetSafeDelta(uint from, uint to, int32_t* delta) {
  DCHECK(delta);

  static_assert(sizeof(int64_t) >= sizeof(uint),
                "int64_t must be sufficient to store a uint value.");
  static_assert(sizeof(int64_t) == sizeof(llabs(0)),
                "|llabs(0)| must returns a 64-bit integer.");
  const int64_t kInt32AbsMax =
      llabs(static_cast<int64_t>(std::numeric_limits<int32_t>::max()));
  const int64_t kInt32AbsMin =
      llabs(static_cast<int64_t>(std::numeric_limits<int32_t>::min()));
  const int64_t kInt32SafeAbsMax = std::min(kInt32AbsMax, kInt32AbsMin);

  const int64_t diff = static_cast<int64_t>(from) - static_cast<int64_t>(to);
  if (llabs(diff) > kInt32SafeAbsMax) {
    return false;
  }

  *delta = static_cast<int32_t>(diff);
  return true;
}

namespace {

// Moves |iter| with |skip_count| characters.
// Returns false if |iter| reaches to the end before skipping
// |skip_count| characters.
bool Skip(ConstChar32Iterator* iter, size_t skip_count) {
  for (size_t i = 0; i < skip_count; ++i) {
    if (iter->Done()) {
      return false;
    }
    iter->Next();
  }
  return true;
}

// Returns true if |prefix_iter| is the prefix of |iter|.
// Returns false if |prefix_iter| is an empty sequence.
// Otherwise returns false.
// This function receives ConstChar32Iterator as pointer because
// ConstChar32Iterator is defined as non-copyable.
bool StartsWith(ConstChar32Iterator* iter, ConstChar32Iterator* prefix_iter) {
  if (iter->Done() || prefix_iter->Done()) {
    return false;
  }

  while (true) {
    if (iter->Get() != prefix_iter->Get()) {
      return false;
    }
    prefix_iter->Next();
    if (prefix_iter->Done()) {
      return true;
    }
    iter->Next();
    if (iter->Done()) {
      return false;
    }
  }
}

// Returns true if |surrounding_text| contains |selected_text|
// from |cursor_pos| to |*anchor_pos|.
// Otherwise returns false.
bool SearchAnchorPosForward(absl::string_view surrounding_text,
                            absl::string_view selected_text,
                            size_t selected_chars_len, uint cursor_pos,
                            uint* anchor_pos) {
  ConstChar32Iterator iter(surrounding_text);
  // Move |iter| to cursor pos.
  if (!Skip(&iter, cursor_pos)) {
    return false;
  }

  ConstChar32Iterator sel_iter(selected_text);
  if (!StartsWith(&iter, &sel_iter)) {
    return false;
  }
  *anchor_pos = cursor_pos + selected_chars_len;
  return true;
}

// Returns true if |surrounding_text| contains |selected_text|
// from |*anchor_pos| to |cursor_pos|.
// Otherwise returns false.
bool SearchAnchorPosBackward(absl::string_view surrounding_text,
                             absl::string_view selected_text,
                             size_t selected_chars_len, uint cursor_pos,
                             uint* anchor_pos) {
  if (cursor_pos < selected_chars_len) {
    return false;
  }

  ConstChar32Iterator iter(surrounding_text);
  // Skip |iter| to (potential) anchor pos.
  const uint skip_count = cursor_pos - selected_chars_len;
  DCHECK_LE(skip_count, cursor_pos);
  if (!Skip(&iter, skip_count)) {
    return false;
  }

  ConstChar32Iterator sel_iter(selected_text);
  if (!StartsWith(&iter, &sel_iter)) {
    return false;
  }
  *anchor_pos = cursor_pos - selected_chars_len;
  return true;
}

}  // namespace

bool SurroundingTextUtil::GetAnchorPosFromSelection(
    absl::string_view surrounding_text, absl::string_view selected_text,
    uint cursor_pos, uint* anchor_pos) {
  DCHECK(anchor_pos);

  if (surrounding_text.empty()) {
    return false;
  }

  if (selected_text.empty()) {
    return false;
  }

  const size_t selected_chars_len = Util::CharsLen(selected_text);

  if (SearchAnchorPosForward(surrounding_text, selected_text,
                             selected_chars_len, cursor_pos, anchor_pos)) {
    return true;
  }

  return SearchAnchorPosBackward(surrounding_text, selected_text,
                                 selected_chars_len, cursor_pos, anchor_pos);
}

namespace {

bool IsWordBreakingSpace(char32_t cp) {
  return cp == ' ' || cp == u'\u3000' || cp == '\n' || cp == '\r' || cp == '\t';
}

}  // namespace

std::string SurroundingTextUtil::CharsBeforeCursor(
    absl::string_view surrounding_text, size_t cursor_pos, size_t max_chars) {
  if (cursor_pos == 0 || max_chars == 0) {
    return "";
  }
  const size_t surrounding_chars = Util::CharsLen(surrounding_text);
  if (cursor_pos > surrounding_chars) {
    return "";
  }
  const size_t take = std::min(cursor_pos, max_chars);
  std::string out;
  Util::Utf8SubString(surrounding_text, cursor_pos - take, take, &out);
  return out;
}

std::string SurroundingTextUtil::CharsAfterCursor(
    absl::string_view surrounding_text, size_t cursor_pos, size_t max_chars) {
  if (max_chars == 0) {
    return "";
  }
  const size_t surrounding_chars = Util::CharsLen(surrounding_text);
  if (cursor_pos >= surrounding_chars) {
    return "";
  }
  const size_t take = std::min(surrounding_chars - cursor_pos, max_chars);
  std::string out;
  Util::Utf8SubString(surrounding_text, cursor_pos, take, &out);
  return out;
}

std::string SurroundingTextUtil::WordBeforeCursor(
    absl::string_view surrounding_text, size_t cursor_pos, size_t max_chars) {
  const std::string before =
      CharsBeforeCursor(surrounding_text, cursor_pos, max_chars);
  if (before.empty()) {
    return "";
  }
  // Walk code points from the end; stop at the first whitespace.
  std::vector<std::string> chars;
  for (ConstChar32Iterator iter(before); !iter.Done(); iter.Next()) {
    chars.emplace_back(Util::CodepointToUtf8(iter.Get()));
  }
  std::string word;
  for (auto it = chars.rbegin(); it != chars.rend(); ++it) {
    const char32_t cp = Util::Utf8ToCodepoint(*it);
    if (IsWordBreakingSpace(cp)) {
      break;
    }
    word.insert(0, *it);
  }
  return word;
}

std::string SurroundingTextUtil::WordAfterCursor(
    absl::string_view surrounding_text, size_t cursor_pos, size_t max_chars) {
  const std::string after =
      CharsAfterCursor(surrounding_text, cursor_pos, max_chars);
  if (after.empty()) {
    return "";
  }
  std::string word;
  for (ConstChar32Iterator iter(after); !iter.Done(); iter.Next()) {
    const char32_t cp = iter.Get();
    if (IsWordBreakingSpace(cp)) {
      break;
    }
    Util::CodepointToUtf8Append(cp, &word);
  }
  return word;
}

bool SurroundingTextUtil::SelectionEndsAtCursor(
    absl::string_view surrounding_text, size_t cursor_pos,
    absl::string_view needle, int32_t* deletion_offset,
    int32_t* deletion_length) {
  DCHECK(deletion_offset);
  DCHECK(deletion_length);
  const size_t needle_len = Util::CharsLen(needle);
  if (needle_len == 0 || cursor_pos < needle_len) {
    return false;
  }
  if (cursor_pos > Util::CharsLen(surrounding_text)) {
    return false;
  }
  std::string slice;
  Util::Utf8SubString(surrounding_text, cursor_pos - needle_len, needle_len,
                      &slice);
  if (slice != needle) {
    return false;
  }
  *deletion_offset = -static_cast<int32_t>(needle_len);
  *deletion_length = static_cast<int32_t>(needle_len);
  return true;
}

}  // namespace ibus
}  // namespace mozc
