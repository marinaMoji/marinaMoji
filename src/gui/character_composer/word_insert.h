// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// macOS and Windows: put a composed drawing straight into Microsoft Word at its
// cursor (AppleScript on macOS, Word COM through PowerShell on Windows), so the
// user need not go through the Word add-in's pane.
// The picture is sized and lowered to match the text (the same rule as the
// add-in's Render) and tagged so Render can resize it later.

#ifndef MOZC_GUI_CHARACTER_COMPOSER_WORD_INSERT_H_
#define MOZC_GUI_CHARACTER_COMPOSER_WORD_INSERT_H_

#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"

namespace mozc {
namespace gui {

// The picture is this fraction of the font size, lowered by this fraction of it
// (a drawing fills its square; a font's ink is smaller and hangs below the
// baseline). Same values as the ONLYOFFICE and Word plugins.
inline constexpr double kWordInkScale = 1.0;
inline constexpr double kWordDropEm = 0.19;

// The AppleScript that inserts the SVG file |svg_path| at Word's cursor and
// tags it |ids| (e.g. "⿰木巻"). Pure; exposed for testing.
std::string WordInsertScript(std::string_view svg_path, std::string_view ids);

// The PowerShell that does the same through Word's COM object model on
// Windows. Pure; exposed for testing.
std::string WordInsertPowerShell(std::string_view svg_path, std::string_view ids);

// |text| (UTF-8) as the base64 of its UTF-16LE bytes, the form of
// `powershell -EncodedCommand`, which keeps non-ASCII ids intact.
std::string PowerShellEncodedCommand(std::string_view text);

// Windows: remember whether the foreground window is Word's (class "OpusApp").
// Call before the composer shows its own window. No-op elsewhere.
void NoteForegroundApp();

// The bundle identifier in `lsappinfo info -only bundleid` output, or "".
std::string ParseBundleId(std::string_view lsappinfo_output);

// The ids label in an SVG's aria-label (see RenderComposerSvg), or "composed".
std::string IdsFromSvg(std::string_view svg);

// The ASNs in `lsappinfo visibleProcessList` output, front to back
// (`ASN:0x0-0x1b01b-"Claude": ASN:0x0-0x1c01c-"Terminal":` -> "ASN:0x0-0x1b01b",
// "ASN:0x0-0x1c01c").
std::vector<std::string> ParseVisibleAsns(std::string_view output);

// True if the app that was in front before the composer opened is Word. On
// macOS the composer itself stays frontmost until it exits, so this looks one
// place back in the front-to-back list instead of waiting for Word to take
// focus. On Windows it is what NoteForegroundApp saw. False elsewhere.
bool WordWasInFront();

// Runs the script. Not OK if Word refuses (no document, automation not
// allowed, protected view). Unimplemented off macOS and Windows.
absl::Status InsertSvgIntoWord(std::string_view svg_path, std::string_view ids);

}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_CHARACTER_COMPOSER_WORD_INSERT_H_
