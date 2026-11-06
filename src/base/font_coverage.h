// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// "Can any installed font draw this?" for the converter process, which has no
// toolkit of its own. The candidate-window renderers colour a character amber
// when the answer is no; this lets the session leave those characters out of
// the Ctrl+1 / Ctrl+2 palettes instead (see
// Config.show_unrenderable_palette_candidates).
//
// Backends: CoreText on macOS, DirectWrite on Windows, fontconfig (loaded at
// run time, so it is not a build dependency) on Linux. Elsewhere, and whenever
// a backend is unavailable, the answer is "yes", so a character is never hidden on a guess.

#ifndef MOZC_BASE_FONT_COVERAGE_H_
#define MOZC_BASE_FONT_COVERAGE_H_

#include <cstdint>

#include "absl/strings/string_view.h"

namespace mozc {

// True if some installed font (or the system's fallback chain) has a glyph for
// |code_point|. Results are cached for the life of the process.
bool CanRenderCodePoint(char32_t code_point);

// True if every code point of the UTF-8 |text| can be rendered. Invalid UTF-8
// and the empty string count as renderable (nothing to hide).
bool CanRenderText(absl::string_view text);

namespace internal {
// Uncached backend. Implemented per platform; see font_coverage.cc.
bool CanRenderCodePointUncached(char32_t code_point);
}  // namespace internal

}  // namespace mozc

#endif  // MOZC_BASE_FONT_COVERAGE_H_
