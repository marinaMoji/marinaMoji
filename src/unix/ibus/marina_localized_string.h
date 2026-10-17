// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0
//
// marinaMoji: UI strings for the Linux (GTK) floating toolbar, Symbols Palette
// and Shortcuts viewer, in EN / FR / JA (see docs/LOCALIZATION.md).
//
// Uses the same "MM.*" keys and wordings as mac's
// Resources/{en,fr,ja}.lproj/Localizable.strings and Windows'
// renderer/win32/marina_localized_string.cc, so the three platforms read the
// same. This module has no GTK/GLib dependency so it can be unit-tested.

#ifndef MOZC_UNIX_IBUS_MARINA_LOCALIZED_STRING_H_
#define MOZC_UNIX_IBUS_MARINA_LOCALIZED_STRING_H_

#include <string>
#include <vector>

namespace mozc {
namespace ibus {

enum class MarinaUiLanguage {
  kEnglish,
  kFrench,
  kJapanese,
};

// Returns the UI string for |key| ("MM.Hiragana", ...) in the user's language,
// falling back to English for any other language and to |key| itself for an
// unknown key (so |key| must outlive the result; use string literals).
const char* MarinaLocalizedString(const char* key);

// Picks the language from an ordered list of locale names ("fr_FR.UTF-8",
// "ja", "en_US", ...), most preferred first. The first name starting with
// "ja", "fr" or "en" decides; "C"/"POSIX" and anything else are skipped, and
// an empty or all-unknown list yields English.
MarinaUiLanguage MarinaUiLanguageFromLocaleNames(
    const std::vector<std::string>& names);

// Overrides the language used by MarinaLocalizedString(). Only for tests;
// production code detects the language once from LANGUAGE / LC_ALL /
// LC_MESSAGES / LANG.
void SetMarinaUiLanguageForTesting(MarinaUiLanguage language);

}  // namespace ibus
}  // namespace mozc

#endif  // MOZC_UNIX_IBUS_MARINA_LOCALIZED_STRING_H_
