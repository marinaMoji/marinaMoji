// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0

#include "renderer/win32/marina_localized_string.h"

#include <string>

#include "testing/gunit.h"

namespace mozc {
namespace renderer {
namespace win32 {
namespace {

// A representative sample of "MM.*" keys spread across the toolbar, mode
// menu, Symbols Palette and Shortcuts viewer tables in
// marina_localized_string.cc -- not the full set, since hardcoding every key
// here would just duplicate (and drift from) that file. The point of this
// test is to catch a key that resolves to nothing in one language rather
// than to pin exact wording.
constexpr const wchar_t* kSampleKeys[] = {
    L"MM.Mode",         L"MM.Hiragana",         L"MM.KatakanaManyoshu",
    L"MM.Toolbar",      L"MM.SymbolsPalette",   L"MM.KeyboardShortcuts",
    L"MM.Odoriji",      L"MM.Script",
};

class MarinaLocalizedStringTest : public ::testing::Test {
 protected:
  void TearDown() override {
    // Other tests in this binary must not see a language override left
    // behind by this one.
    SetMarinaUiLanguageForTesting(MarinaUiLanguage::kEnglish);
  }
};

TEST_F(MarinaLocalizedStringTest, ResolvesInEveryLanguage) {
  for (const wchar_t* key : kSampleKeys) {
    for (const MarinaUiLanguage language :
        {MarinaUiLanguage::kEnglish, MarinaUiLanguage::kFrench,
         MarinaUiLanguage::kJapanese}) {
      SetMarinaUiLanguageForTesting(language);
      const wchar_t* result = MarinaLocalizedString(key);
      ASSERT_NE(result, nullptr) << key;
      EXPECT_NE(result[0], L'\0')
          << "key " << key << " resolved to an empty string";
      // A key that falls through to the "unknown key" path returns the key
      // itself unchanged -- every key in kSampleKeys is known, so that must
      // not happen in any of the three languages.
      EXPECT_STRNE(result, key)
          << "key " << key << " was not translated (echoed back verbatim)";
    }
  }
}

TEST_F(MarinaLocalizedStringTest, JapaneseDiffersFromEnglish) {
  for (const wchar_t* key : kSampleKeys) {
    SetMarinaUiLanguageForTesting(MarinaUiLanguage::kEnglish);
    const std::wstring en = MarinaLocalizedString(key);
    SetMarinaUiLanguageForTesting(MarinaUiLanguage::kJapanese);
    const std::wstring ja = MarinaLocalizedString(key);
    EXPECT_NE(en, ja) << "key " << key
                      << " has identical English and Japanese wording";
  }
}

TEST_F(MarinaLocalizedStringTest, UnknownKeyFallsBackToKeyItself) {
  SetMarinaUiLanguageForTesting(MarinaUiLanguage::kEnglish);
  constexpr wchar_t kBogusKey[] = L"MM.ThisKeyDoesNotExist";
  EXPECT_STREQ(MarinaLocalizedString(kBogusKey), kBogusKey);
}

TEST_F(MarinaLocalizedStringTest, NullKeyReturnsEmptyString) {
  EXPECT_STREQ(MarinaLocalizedString(nullptr), L"");
}

}  // namespace
}  // namespace win32
}  // namespace renderer
}  // namespace mozc
