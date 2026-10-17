// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0

#include "unix/ibus/marina_localized_string.h"

#include <string>
#include <vector>

#include "testing/gunit.h"

namespace mozc {
namespace ibus {
namespace {

// Every key the Linux toolbar uses (see mozc_toolbar.cc).
constexpr const char* kKeys[] = {
    "MM.DirectInput",   "MM.Hiragana",         "MM.Katakana",
    "MM.Latin",         "MM.WideLatin",        "MM.HalfWidthKatakana",
    "MM.LiveConversion", "MM.CharacterComposer", "MM.SymbolsPalette",
    "MM.PinPalette",    "MM.OdorijiTab",       "MM.Kaeriten",
    "MM.Symbols",       "MM.User",             "MM.OdorijiHint",
    "MM.KaeritenHint",  "MM.UserSymbolsHint",  "MM.KeyboardShortcuts",
    "MM.Script",        "MM.Composition",      "MM.Close",
};

class MarinaLocalizedStringTest : public ::testing::Test {
 protected:
  void TearDown() override {
    SetMarinaUiLanguageForTesting(MarinaUiLanguage::kEnglish);
  }
};

TEST_F(MarinaLocalizedStringTest, LanguageFromLocaleNames) {
  using L = MarinaUiLanguage;
  EXPECT_EQ(MarinaUiLanguageFromLocaleNames({"ja_JP.UTF-8"}), L::kJapanese);
  EXPECT_EQ(MarinaUiLanguageFromLocaleNames({"fr_FR.UTF-8"}), L::kFrench);
  EXPECT_EQ(MarinaUiLanguageFromLocaleNames({"en_US.UTF-8"}), L::kEnglish);
  // The first recognised name wins, in preference order.
  EXPECT_EQ(MarinaUiLanguageFromLocaleNames({"fr", "ja"}), L::kFrench);
  EXPECT_EQ(MarinaUiLanguageFromLocaleNames({"C", "ja"}), L::kJapanese);
  // English fallback.
  EXPECT_EQ(MarinaUiLanguageFromLocaleNames({}), L::kEnglish);
  EXPECT_EQ(MarinaUiLanguageFromLocaleNames({"C"}), L::kEnglish);
  EXPECT_EQ(MarinaUiLanguageFromLocaleNames({"de_DE.UTF-8"}), L::kEnglish);
}

TEST_F(MarinaLocalizedStringTest, EveryKeyResolvesInEveryLanguage) {
  for (const char* key : kKeys) {
    for (const MarinaUiLanguage language :
         {MarinaUiLanguage::kEnglish, MarinaUiLanguage::kFrench,
          MarinaUiLanguage::kJapanese}) {
      SetMarinaUiLanguageForTesting(language);
      const char* result = MarinaLocalizedString(key);
      ASSERT_NE(result, nullptr) << key;
      EXPECT_STRNE(result, "") << key;
      EXPECT_STRNE(result, key) << key << " is not in the table";
    }
  }
}

TEST_F(MarinaLocalizedStringTest, JapaneseDiffersFromEnglish) {
  for (const char* key : kKeys) {
    SetMarinaUiLanguageForTesting(MarinaUiLanguage::kEnglish);
    const std::string en = MarinaLocalizedString(key);
    SetMarinaUiLanguageForTesting(MarinaUiLanguage::kJapanese);
    EXPECT_NE(en, MarinaLocalizedString(key)) << key;
  }
}

TEST_F(MarinaLocalizedStringTest, PicksLanguageWording) {
  SetMarinaUiLanguageForTesting(MarinaUiLanguage::kFrench);
  EXPECT_STREQ(MarinaLocalizedString("MM.LiveConversion"),
               "Conversion en direct");
  SetMarinaUiLanguageForTesting(MarinaUiLanguage::kJapanese);
  EXPECT_STREQ(MarinaLocalizedString("MM.LiveConversion"), "ライブ変換");
}

TEST_F(MarinaLocalizedStringTest, UnknownKeyFallsBackToKeyItself) {
  constexpr char kBogus[] = "MM.ThisKeyDoesNotExist";
  EXPECT_STREQ(MarinaLocalizedString(kBogus), kBogus);
  EXPECT_STREQ(MarinaLocalizedString(nullptr), "");
}

}  // namespace
}  // namespace ibus
}  // namespace mozc
