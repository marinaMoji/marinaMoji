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

#ifndef MOZC_UNIX_IBUS_SELECTION_TEXT_RESOLVER_H_
#define MOZC_UNIX_IBUS_SELECTION_TEXT_RESOLVER_H_

#include <cstdint>
#include <string>

#include "absl/strings/string_view.h"
#include "protocol/commands.pb.h"

namespace mozc {
namespace ibus {

// Snapshot of IBus surrounding text used to resolve a selection for
// CONVERT_RELATED / CONVERT_COMPONENTS / CONVERT_REVERSE. On GNOME Wayland the
// selection (cursor != anchor) is often missing even when the surrounding
// string itself is present; see ibus/ibus#2760.
struct SurroundingSnapshot {
  bool available = false;
  uint cursor_pos = 0;
  uint anchor_pos = 0;
  int32_t relative_selected_length = 0;
  std::string surrounding_text;
  std::string selection_text;
};

enum class SelectionSource {
  kNone = 0,
  kSurroundingSelection,
  kPrimary,
  kAtspi,
  kCaretFallback,
};

struct ResolvedSelection {
  std::string text;
  SelectionSource source = SelectionSource::kNone;
  // Set when DeleteSurroundingText can remove |text|. Offset is negative
  // when the text ends at the caret, or 0 when it starts at the caret
  // (forward delete). Positive offsets are not used.
  bool can_delete = false;
  int32_t deletion_offset = 0;
  int32_t deletion_length = 0;
};

// Resolves the text to convert, trying in order:
//   1. surrounding-text selection (cursor != anchor)
//   2. Wayland primary selection (timeout-bounded wl-paste -p)
//   3. AT-SPI focused-widget selection
//   4. characters / word beside the caret
// Primary and AT-SPI results are accepted only when they match the surrounding
// text at the caret — ending *or* starting there (Wayland often leaves the
// caret at the left of a mouse highlight) — or when no surrounding text is
// available at all (then deletion is skipped). Caret fallback runs last; for
// related/components it prefers text *after* the caret (Wayland mouse
// highlight) and only then text before (end-of-line / caret-after UX).
ResolvedSelection ResolveSelectionForCommand(
    commands::SessionCommand::CommandType type,
    const SurroundingSnapshot& surrounding);

// Test seams / lower-level helpers.
bool IsAcceptableSelectionLength(commands::SessionCommand::CommandType type,
                                 size_t char_len);

ResolvedSelection ResolveSelectionForCommandWithFallbacks(
    commands::SessionCommand::CommandType type,
    const SurroundingSnapshot& surrounding, absl::string_view primary_text,
    absl::string_view atspi_text);

std::string ReadPrimarySelectionText();
std::string ReadAtspiSelectedText();

const char* SelectionSourceName(SelectionSource source);

}  // namespace ibus
}  // namespace mozc

#endif  // MOZC_UNIX_IBUS_SELECTION_TEXT_RESOLVER_H_
