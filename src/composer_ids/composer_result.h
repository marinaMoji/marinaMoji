// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// How the character composer hands its result to the input method when it is
// a separate process that cannot print to a pipe (Windows and macOS; on Linux
// the toolbar reads the tool's stdout instead).
//
// The tool writes the chosen character to a file just before it exits; the
// input method waits for the tool to exit and then reads the file. An absent,
// empty or unreadable file means the user closed the dialog without choosing.

#ifndef MOZC_COMPOSER_IDS_COMPOSER_RESULT_H_
#define MOZC_COMPOSER_IDS_COMPOSER_RESULT_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "absl/status/status.h"

namespace mozc {
namespace composer_ids {

// Longest result accepted, in bytes of UTF-8. One character is at most four;
// this leaves room for a combining sequence. The text is inserted straight into
// whatever the user is typing into, so anything longer is refused.
inline constexpr size_t kMaxComposerResultBytes = 32;

// <user profile directory>/character_composer_result.txt. Used when the tool is
// started without --result_file, i.e. as a macOS app bundle, which cannot be
// given arguments.
std::string DefaultResultFilePath();

// Sibling of DefaultResultFilePath() with a .svg suffix — unencoded drawings.
std::string DefaultSvgResultFilePath();

// Writes |text| (UTF-8, one character) to |path|, replacing any file there.
absl::Status WriteComposerResult(std::string_view path, std::string_view text);

// Writes an SVG document (UTF-8) for a composed-but-unencoded character.
absl::Status WriteComposerSvgResult(std::string_view path, std::string_view svg);

// ONLYOFFICE has no input-method channel for a drawing, so the marinaMoji
// ONLYOFFICE plugin polls a script file next to its own index.html and inserts
// a fresh result at the cursor.
//
// The JavaScript the plugin loads: assigns window.marinaMojiComposerResult =
// {id, svg?, text?}. |id| is the creation time in milliseconds since the Unix
// epoch; the plugin uses it both to skip results it has seen and to ignore
// stale ones. Pass an empty |svg| or |text| to omit that field.
std::string OnlyOfficeHandoffScript(std::string_view svg, int64_t id_ms,
                                    std::string_view text = "");

// The ONLYOFFICE Desktop Editors folder the marinaMoji plugin is installed in
// (macOS Application Support, or Linux XDG data), or an empty string when that
// host is not wired up yet (e.g. Windows).
std::string OnlyOfficePluginDirectory();

// Writes composer_result.js (see OnlyOfficeHandoffScript) into |directory|.
// Does nothing and returns OK when |directory| is empty or does not exist, i.e.
// the plugin is not installed. |svg| and/or |text| may be set (Unicode Insert
// uses text only; unencoded drawings use svg only).
absl::Status WriteOnlyOfficeHandoff(std::string_view directory,
                                    std::string_view svg, int64_t id_ms,
                                    std::string_view text = "");

// The result stored in |path|: surrounding whitespace removed, and nullopt if
// the file is missing, empty, longer than kMaxComposerResultBytes, or not valid
// UTF-8.
std::optional<std::string> ReadComposerResult(std::string_view path);

// Full SVG text from |path|, or nullopt if missing/empty/unreadable.
std::optional<std::string> ReadComposerSvgResult(std::string_view path);

}  // namespace composer_ids
}  // namespace mozc

#endif  // MOZC_COMPOSER_IDS_COMPOSER_RESULT_H_
