// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0
//
// marinaMoji: UI strings for the Windows floating toolbar, Symbols Palette
// and Shortcuts viewer, in EN / FR / JA (see docs/LOCALIZATION.md).
//
// These windows live in marinamoji_renderer.exe rather than the TIP DLL, and
// are drawn with raw Win32 controls, so they can't use the TIP's
// tip_resource.rc STRINGTABLE. Strings are kept in a table in the .cc the
// same way Linux keeps them in unix/ibus/message_translator.cc, and use the
// same "MM.*" keys as mac's Resources/{en,fr,ja}.lproj/Localizable.strings so
// the three platforms' wordings stay in step.

#ifndef MOZC_RENDERER_WIN32_MARINA_LOCALIZED_STRING_H_
#define MOZC_RENDERER_WIN32_MARINA_LOCALIZED_STRING_H_

namespace mozc {
namespace renderer {
namespace win32 {

// Returns the UI string for |key| ("MM.Hiragana", ...) in the user's UI
// language, falling back to English for any other language and to |key|
// itself for an unknown key. |key| must be a string literal (or otherwise
// outlive the returned pointer), since that fallback returns it unchanged.
const wchar_t* MarinaLocalizedString(const wchar_t* key);

// Overrides the language used by MarinaLocalizedString(). Only for tests;
// production code lets the module read GetUserDefaultUILanguage() once.
enum class MarinaUiLanguage {
  kEnglish,
  kFrench,
  kJapanese,
};
void SetMarinaUiLanguageForTesting(MarinaUiLanguage language);

}  // namespace win32
}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_WIN32_MARINA_LOCALIZED_STRING_H_
