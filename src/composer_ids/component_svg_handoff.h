// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// On commit of an UNENCODED_SVG Ctrl+2 candidate: render a drawing of the
// selected components and hand it off the same way the character composer
// does (profile SVG file + ONLYOFFICE plugin script). Does not insert the
// IDS string as Unicode text.

#ifndef MOZC_COMPOSER_IDS_COMPONENT_SVG_HANDOFF_H_
#define MOZC_COMPOSER_IDS_COMPONENT_SVG_HANDOFF_H_

#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/types/span.h"

namespace mozc {
namespace composer_ids {

// Builds an SVG for |op| + |parts| (each part one character). Uses a text
// layout matching the composer's cell geometry when KAGE data is unavailable
// from this process (the IME server stays BSD and does not link //kage).
std::string RenderComponentSelectionSvg(std::string_view op,
                                        absl::Span<const std::string> parts,
                                        int view_box = 1000);

// Writes the SVG to DefaultSvgResultFilePath() and the ONLYOFFICE hand-off
// script. |op| defaults to ⿰ when empty and |parts| has size 2.
absl::Status HandOffComponentSvg(std::string_view op,
                                 absl::Span<const std::string> parts);

// Convenience: split |selected| into characters and hand off with |op|.
absl::Status HandOffComponentSvgFromSelection(std::string_view op,
                                              std::string_view selected);

}  // namespace composer_ids
}  // namespace mozc

#endif  // MOZC_COMPOSER_IDS_COMPONENT_SVG_HANDOFF_H_
