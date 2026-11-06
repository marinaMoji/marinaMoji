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

#ifndef MOZC_UNIX_IBUS_SURROUNDING_TEXT_UTIL_H_
#define MOZC_UNIX_IBUS_SURROUNDING_TEXT_UTIL_H_

#include <cstdint>
#include <string>

#include "absl/strings/string_view.h"
#include "unix/ibus/ibus_header.h"

namespace mozc {
namespace ibus {

class SurroundingTextUtil {
 public:
  SurroundingTextUtil() = delete;
  SurroundingTextUtil(const SurroundingTextUtil&) = delete;
  SurroundingTextUtil& operator=(const SurroundingTextUtil&) = delete;

  // Calculates |from| - |to| and stores the result into |delta| with
  // checking integer overflow.
  // Returns true when neither |abs(delta)| nor |-delta| does not cause
  // integer overflow, that is, |delta| is in a safe range.
  // Returns false otherwise.
  static bool GetSafeDelta(uint from, uint to, int32_t* delta);

  // Returns true if
  // 1. |surrounding_text| contains |selected_text|
  //    from |cursor_pos| to |*anchor_pos|.
  // or,
  // 2. |surrounding_text| contains |selected_text|
  //    from |*anchor_pos| to |cursor_pos|.
  // with calculating |*anchor_pos|,
  // where |cursor_pos| and |*anchor_pos| are counts of Unicode characters.
  // When both 1) and 2) are satisfied, this function calculates
  // |*anchor_pos| for case 1).
  // Otherwise returns false.
  static bool GetAnchorPosFromSelection(absl::string_view surrounding_text,
                                        absl::string_view selected_text,
                                        uint cursor_pos, uint* anchor_pos);

  // Returns up to |max_chars| Unicode characters immediately before
  // |cursor_pos| in |surrounding_text|. |cursor_pos| is a character offset.
  static std::string CharsBeforeCursor(absl::string_view surrounding_text,
                                       size_t cursor_pos, size_t max_chars);

  // Returns up to |max_chars| Unicode characters immediately after
  // |cursor_pos|. Useful when Wayland collapses a selection and leaves the
  // caret at the start of the former range.
  static std::string CharsAfterCursor(absl::string_view surrounding_text,
                                      size_t cursor_pos, size_t max_chars);

  // Trailing non-whitespace run before |cursor_pos|, capped at |max_chars|.
  // Used as a reconvert caret fallback when Wayland hides the selection.
  static std::string WordBeforeCursor(absl::string_view surrounding_text,
                                      size_t cursor_pos, size_t max_chars);

  // Leading non-whitespace run after |cursor_pos|, capped at |max_chars|.
  static std::string WordAfterCursor(absl::string_view surrounding_text,
                                     size_t cursor_pos, size_t max_chars);

  // True when |needle| is the character slice of |surrounding_text| that ends
  // at |cursor_pos|. On success fills a DeleteSurroundingText-compatible range
  // (negative offset, positive length).
  static bool SelectionEndsAtCursor(absl::string_view surrounding_text,
                                    size_t cursor_pos,
                                    absl::string_view needle,
                                    int32_t* deletion_offset,
                                    int32_t* deletion_length);

  // True when |needle| begins at |cursor_pos| (caret at the start of the
  // selection — common on Wayland after a mouse highlight). Deletion uses
  // offset 0 (forward from the caret).
  static bool SelectionStartsAtCursor(absl::string_view surrounding_text,
                                      size_t cursor_pos,
                                      absl::string_view needle,
                                      int32_t* deletion_offset,
                                      int32_t* deletion_length);
};

}  // namespace ibus
}  // namespace mozc

#endif  // MOZC_UNIX_IBUS_SURROUNDING_TEXT_UTIL_H_
