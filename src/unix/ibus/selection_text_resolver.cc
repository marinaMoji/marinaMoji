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

#include <dlfcn.h>
#include <glib.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "base/util.h"
#include "base/vlog.h"
#include "unix/ibus/surrounding_text_util.h"

namespace mozc {
namespace ibus {
namespace {

using ::mozc::commands::SessionCommand;

// Related needs exactly one character; components 1–4; reconvert a short
// phrase. Keep primary/AT-SPI caps tight so a stale clipboard cannot feed a
// whole paragraph into the converter.
constexpr size_t kMaxRelatedChars = 1;
constexpr size_t kMaxComponentChars = 4;
constexpr size_t kMaxReconvertChars = 64;
constexpr size_t kMaxCaretReconvertChars = 20;

size_t MaxCharsForCommand(SessionCommand::CommandType type) {
  switch (type) {
    case SessionCommand::CONVERT_RELATED:
      return kMaxRelatedChars;
    case SessionCommand::CONVERT_COMPONENTS:
      return kMaxComponentChars;
    case SessionCommand::CONVERT_REVERSE:
      return kMaxReconvertChars;
    default:
      return 0;
  }
}

size_t MinCharsForCommand(SessionCommand::CommandType type) {
  switch (type) {
    case SessionCommand::CONVERT_RELATED:
    case SessionCommand::CONVERT_COMPONENTS:
    case SessionCommand::CONVERT_REVERSE:
      return 1;
    default:
      return 0;
  }
}

std::string StripTrailingNewlines(std::string text) {
  while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
    text.pop_back();
  }
  return text;
}

bool TryAcceptCandidate(SessionCommand::CommandType type,
                        const SurroundingSnapshot& surrounding,
                        absl::string_view candidate, SelectionSource source,
                        ResolvedSelection* out) {
  if (candidate.empty() || out == nullptr) {
    return false;
  }
  if (!Util::IsValidUtf8(candidate)) {
    return false;
  }
  const size_t char_len = Util::CharsLen(candidate);
  if (!IsAcceptableSelectionLength(type, char_len)) {
    return false;
  }

  ResolvedSelection resolved;
  resolved.text = std::string(candidate);
  resolved.source = source;

  if (surrounding.available && !surrounding.surrounding_text.empty()) {
    int32_t offset = 0;
    int32_t length = 0;
    // Prefer "ends at caret" (caret after the highlight). Also accept
    // "starts at caret": on Wayland a mouse selection often leaves the
    // caret at the left edge of the range.
    if (SurroundingTextUtil::SelectionEndsAtCursor(
            surrounding.surrounding_text, surrounding.cursor_pos, candidate,
            &offset, &length) ||
        SurroundingTextUtil::SelectionStartsAtCursor(
            surrounding.surrounding_text, surrounding.cursor_pos, candidate,
            &offset, &length)) {
      resolved.can_delete = true;
      resolved.deletion_offset = offset;
      resolved.deletion_length = length;
      *out = std::move(resolved);
      return true;
    }
    // Surrounding text is present but does not contain this candidate at the
    // caret: treat as a stale primary/clipboard and skip.
    return false;
  }

  // No surrounding text to validate against (or capability missing). Use the
  // text but do not invent a deletion range.
  resolved.can_delete = false;
  *out = std::move(resolved);
  return true;
}

ResolvedSelection MakeCaretResolved(SessionCommand::CommandType type,
                                    const SurroundingSnapshot& surrounding,
                                    absl::string_view text, bool before_cursor) {
  ResolvedSelection empty;
  if (text.empty() || !IsAcceptableSelectionLength(type, Util::CharsLen(text))) {
    return empty;
  }
  ResolvedSelection resolved;
  resolved.text = std::string(text);
  resolved.source = SelectionSource::kCaretFallback;
  const size_t n = Util::CharsLen(text);
  resolved.can_delete = true;
  if (before_cursor) {
    // DeleteSurroundingText: negative offset = characters before the caret.
    resolved.deletion_offset = -static_cast<int32_t>(n);
  } else {
    // offset 0 = delete forward from the caret (see UpdateDeletionRange).
    resolved.deletion_offset = 0;
  }
  resolved.deletion_length = static_cast<int32_t>(n);
  (void)surrounding;
  return resolved;
}

ResolvedSelection CaretFallback(SessionCommand::CommandType type,
                                const SurroundingSnapshot& surrounding) {
  ResolvedSelection empty;
  if (!surrounding.available || surrounding.surrounding_text.empty()) {
    return empty;
  }

  size_t max_chars = 0;
  const bool want_word = (type == SessionCommand::CONVERT_REVERSE);
  switch (type) {
    case SessionCommand::CONVERT_RELATED:
      max_chars = kMaxRelatedChars;
      break;
    case SessionCommand::CONVERT_COMPONENTS:
      max_chars = kMaxComponentChars;
      break;
    case SessionCommand::CONVERT_REVERSE:
      max_chars = kMaxCaretReconvertChars;
      break;
    default:
      return empty;
  }

  const std::string before =
      want_word ? SurroundingTextUtil::WordBeforeCursor(
                      surrounding.surrounding_text, surrounding.cursor_pos,
                      max_chars)
                : SurroundingTextUtil::CharsBeforeCursor(
                      surrounding.surrounding_text, surrounding.cursor_pos,
                      max_chars);
  const std::string after =
      want_word ? SurroundingTextUtil::WordAfterCursor(
                      surrounding.surrounding_text, surrounding.cursor_pos,
                      max_chars)
                : SurroundingTextUtil::CharsAfterCursor(
                      surrounding.surrounding_text, surrounding.cursor_pos,
                      max_chars);

  // Related / components: prefer AFTER the caret. On Wayland a mouse
  // highlight often collapses with the caret at the *start* of the range
  // (select 際 → caret between 國 and 際); "before" then wrongly picks 國.
  // At end-of-line, "after" is empty and we fall through to "before", which
  // is why a trailing line-break made Ctrl+Shift+7/8 appear to work.
  // Reconvert keeps the older "word before caret" preference.
  if (!want_word) {
    if (ResolvedSelection resolved = MakeCaretResolved(
            type, surrounding, after, /*before_cursor=*/false);
        !resolved.text.empty()) {
      return resolved;
    }
    return MakeCaretResolved(type, surrounding, before,
                             /*before_cursor=*/true);
  }

  if (ResolvedSelection resolved =
          MakeCaretResolved(type, surrounding, before, /*before_cursor=*/true);
      !resolved.text.empty()) {
    return resolved;
  }
  return MakeCaretResolved(type, surrounding, after, /*before_cursor=*/false);
}

}  // namespace

bool IsAcceptableSelectionLength(SessionCommand::CommandType type,
                                 size_t char_len) {
  const size_t min_chars = MinCharsForCommand(type);
  const size_t max_chars = MaxCharsForCommand(type);
  return min_chars > 0 && char_len >= min_chars && char_len <= max_chars;
}

const char* SelectionSourceName(SelectionSource source) {
  switch (source) {
    case SelectionSource::kSurroundingSelection:
      return "surrounding";
    case SelectionSource::kPrimary:
      return "primary";
    case SelectionSource::kAtspi:
      return "atspi";
    case SelectionSource::kCaretFallback:
      return "caret";
    case SelectionSource::kNone:
    default:
      return "none";
  }
}

std::string ReadPrimarySelectionText() {
  // wl-paste -p reads the Wayland primary selection (mouse highlight).
  // -n omits a trailing newline that some compositors append.
  // Without a timeout, wl-paste blocks forever when nothing is offered, which
  // freezes the IBus engine on every related/components/reconvert keypress.
  gchar* stdout_str = nullptr;
  gchar* stderr_str = nullptr;
  gint exit_status = 0;
  GError* error = nullptr;
  const gboolean ok = g_spawn_command_line_sync(
      "timeout 0.2s wl-paste -n -p", &stdout_str, &stderr_str, &exit_status,
      &error);
  g_free(stderr_str);
  if (error != nullptr) {
    // Common when wl-clipboard / coreutils timeout is missing.
    MOZC_VLOG(1) << "wl-paste unavailable: " << error->message;
    g_error_free(error);
    g_free(stdout_str);
    return "";
  }
  if (!ok || !g_spawn_check_wait_status(exit_status, nullptr) ||
      stdout_str == nullptr) {
    g_free(stdout_str);
    return "";
  }
  std::string text = StripTrailingNewlines(stdout_str);
  g_free(stdout_str);
  // Guard against huge pastes.
  if (Util::CharsLen(text) > kMaxReconvertChars) {
    return "";
  }
  return text;
}

std::string ReadAtspiSelectedText() {
  void* lib = dlopen("libatspi.so.0", RTLD_LAZY);
  if (lib == nullptr) {
    return "";
  }

  auto atspi_init = reinterpret_cast<int (*)()>(dlsym(lib, "atspi_init"));
  auto get_desktop =
      reinterpret_cast<void* (*)(int)>(dlsym(lib, "atspi_get_desktop"));
  auto get_child_count = reinterpret_cast<int (*)(void*, void**)>(
      dlsym(lib, "atspi_accessible_get_child_count"));
  auto get_child = reinterpret_cast<void* (*)(void*, int, void**)>(
      dlsym(lib, "atspi_accessible_get_child_at_index"));
  auto get_state_set =
      reinterpret_cast<void* (*)(void*)>(dlsym(lib, "atspi_accessible_get_state_set"));
  auto state_contains = reinterpret_cast<int (*)(void*, int)>(
      dlsym(lib, "atspi_state_set_contains"));
  auto get_text_iface = reinterpret_cast<void* (*)(void*)>(
      dlsym(lib, "atspi_accessible_get_text"));
  if (get_text_iface == nullptr) {
    get_text_iface = reinterpret_cast<void* (*)(void*)>(
        dlsym(lib, "atspi_accessible_get_text_iface"));
  }
  auto text_n_selections = reinterpret_cast<int (*)(void*, void**)>(
      dlsym(lib, "atspi_text_get_n_selections"));
  // AtspiRange* atspi_text_get_selection(AtspiText*, gint, GError**)
  auto text_get_selection = reinterpret_cast<void* (*)(void*, int, void**)>(
      dlsym(lib, "atspi_text_get_selection"));
  auto text_get_text =
      reinterpret_cast<char* (*)(void*, int, int, void**)>(
          dlsym(lib, "atspi_text_get_text"));
  auto gobj_unref =
      reinterpret_cast<void (*)(void*)>(dlsym(lib, "g_object_unref"));

  if (!get_desktop || !get_child_count || !get_child || !get_state_set ||
      !state_contains || !get_text_iface || !text_n_selections ||
      !text_get_selection || !text_get_text) {
    dlclose(lib);
    return "";
  }
  if (atspi_init) {
    atspi_init();
  }

  void* desktop = get_desktop(0);
  if (desktop == nullptr) {
    dlclose(lib);
    return "";
  }

  // ATSPI_STATE_FOCUSED — see atspi-constants.h / toolbar comment.
  constexpr int kStateFocused = 12;

  struct RangeOffsets {
    int start_offset;
    int end_offset;
  };

  std::string result;
  std::vector<void*> stack;
  stack.push_back(desktop);
  // Bound the walk: accessibility trees can be huge.
  constexpr size_t kMaxNodes = 400;
  for (size_t i = 0; i < stack.size() && i < kMaxNodes && result.empty(); ++i) {
    void* node = stack[i];
    void* err = nullptr;

    void* states = get_state_set(node);
    const bool focused =
        states != nullptr && state_contains(states, kStateFocused);
    if (states != nullptr && gobj_unref) {
      gobj_unref(states);
    }

    if (focused) {
      void* text_iface = get_text_iface(node);
      if (text_iface != nullptr) {
        err = nullptr;
        const int nsel = text_n_selections(text_iface, &err);
        if (nsel > 0) {
          err = nullptr;
          void* range = text_get_selection(text_iface, 0, &err);
          if (range != nullptr) {
            const auto* offsets = reinterpret_cast<RangeOffsets*>(range);
            const int start = std::min(offsets->start_offset, offsets->end_offset);
            const int end = std::max(offsets->start_offset, offsets->end_offset);
            // g_free is correct for AtspiRange from atspi_text_get_selection.
            g_free(range);
            if (end > start) {
              err = nullptr;
              char* selected =
                  text_get_text(text_iface, start, end, &err);
              if (selected != nullptr) {
                result = StripTrailingNewlines(selected);
                g_free(selected);
              }
            }
          }
        }
        if (gobj_unref) {
          gobj_unref(text_iface);
        }
      }
    }

    err = nullptr;
    const int n = get_child_count(node, &err);
    for (int c = 0; c < n && stack.size() < kMaxNodes; ++c) {
      err = nullptr;
      void* child = get_child(node, c, &err);
      if (child != nullptr) {
        stack.push_back(child);
      }
    }
  }

  for (void* node : stack) {
    if (node != desktop && gobj_unref) {
      gobj_unref(node);
    }
  }
  if (desktop != nullptr && gobj_unref) {
    gobj_unref(desktop);
  }
  dlclose(lib);

  if (Util::CharsLen(result) > kMaxReconvertChars) {
    return "";
  }
  return result;
}

ResolvedSelection ResolveSelectionForCommandWithFallbacks(
    SessionCommand::CommandType type, const SurroundingSnapshot& surrounding,
    absl::string_view primary_text, absl::string_view atspi_text) {
  ResolvedSelection resolved;

  if (surrounding.available && !surrounding.selection_text.empty() &&
      IsAcceptableSelectionLength(type,
                                  Util::CharsLen(surrounding.selection_text))) {
    resolved.text = surrounding.selection_text;
    resolved.source = SelectionSource::kSurroundingSelection;
    // Same delete convention as the historical ExecuteCallback path.
    if (surrounding.relative_selected_length > 0) {
      resolved.can_delete = true;
      resolved.deletion_offset = -surrounding.relative_selected_length;
      resolved.deletion_length = surrounding.relative_selected_length;
    } else if (surrounding.relative_selected_length < 0) {
      // Selection extends forward from the caret: delete with offset 0.
      resolved.can_delete = true;
      resolved.deletion_offset = 0;
      resolved.deletion_length = -surrounding.relative_selected_length;
    }
    return resolved;
  }

  if (TryAcceptCandidate(type, surrounding, primary_text,
                         SelectionSource::kPrimary, &resolved)) {
    return resolved;
  }
  if (TryAcceptCandidate(type, surrounding, atspi_text, SelectionSource::kAtspi,
                         &resolved)) {
    return resolved;
  }
  return CaretFallback(type, surrounding);
}

ResolvedSelection ResolveSelectionForCommand(
    SessionCommand::CommandType type, const SurroundingSnapshot& surrounding) {
  // Short-circuit: real surrounding selection needs no process spawn / a11y.
  if (surrounding.available && !surrounding.selection_text.empty() &&
      IsAcceptableSelectionLength(type,
                                  Util::CharsLen(surrounding.selection_text))) {
    return ResolveSelectionForCommandWithFallbacks(type, surrounding, "", "");
  }

  // Primary (and AT-SPI) before the caret fallback. On Wayland a mouse
  // selection often leaves cursor==anchor at the *start* of the range; the
  // caret-before heuristic then picks the neighbour to the left. wl-paste is
  // timeout-bounded, so this no longer hangs the engine.
  const std::string primary = ReadPrimarySelectionText();
  ResolvedSelection from_primary;
  if (TryAcceptCandidate(type, surrounding, primary, SelectionSource::kPrimary,
                         &from_primary)) {
    return from_primary;
  }

  const std::string atspi = ReadAtspiSelectedText();
  return ResolveSelectionForCommandWithFallbacks(type, surrounding, primary,
                                                 atspi);
}

}  // namespace ibus
}  // namespace mozc
