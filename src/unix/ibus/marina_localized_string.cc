// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0

#include "unix/ibus/marina_localized_string.h"

#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include "base/environ.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace ibus {

namespace {

struct LocalizedEntry {
  const char* key;
  const char* en;
  const char* fr;
  const char* ja;
};

// Keys and wordings mirror src/mac/Resources/{en,fr,ja}.lproj/
// Localizable.strings. Entries marked "Linux only" have no mac or Windows
// counterpart yet.
constexpr LocalizedEntry kEntries[] = {
    // Mode menu (the mode-indicator popup).
    {"MM.DirectInput", "Direct input", "Saisie directe", "直接入力"},
    {"MM.Hiragana", "Hiragana", "Hiragana", "ひらがな"},
    {"MM.Katakana", "Katakana", "Katakana", "カタカナ"},
    {"MM.Latin", "Latin", "Alphanumérique demi-chasse", "半角英数"},
    {"MM.WideLatin", "Wide Latin", "Alphanumérique pleine chasse", "全角英数"},
    {"MM.HalfWidthKatakana", "Half width katakana", "Katakana demi-chasse",
     "半角カタカナ"},

    // Toolbar button tooltips.
    {"MM.LiveConversion", "Live conversion", "Conversion en direct",
     "ライブ変換"},
    {"MM.CharacterComposer", "Character composer", "Compositeur de caractères",
     "文字合成"},

    // Symbols Palette.
    {"MM.SymbolsPalette", "Symbols Palette", "Palette de symboles",
     "記号パレット"},
    {"MM.PinPalette", "Pin palette", "Épingler la palette", "パレットを固定"},
    // Linux only: the short tab label ("MM.Odoriji" is the long menu form).
    {"MM.OdorijiTab", "Odoriji", "Odoriji", "踊り字"},
    {"MM.Kaeriten", "Kaeriten", "Kaeriten", "返り点"},
    {"MM.Symbols", "Symbols", "Symboles", "記号"},
    {"MM.User", "User", "Utilisateur", "ユーザー"},
    {"MM.OdorijiHint",
     "Clicking an odoriji inserts it and sets it as your default (same "
     "behavior as the main Odoriji palette). Shortcuts are available in your "
     "keymap (Ctrl+Shift+1/2 on common layouts).",
     "Un clic sur un odoriji l'insère et le définit par défaut (comme la "
     "palette Odoriji principale). Les raccourcis sont disponibles dans votre "
     "table de touches (Ctrl+Maj+1/2 sur les dispositions courantes).",
     "踊り字をクリックすると挿入され、既定の踊り字として設定されます"
     "（メインの踊り字パレットと同じ動作）。ショートカットはキーマップで"
     "設定できます（一般的な配列では Ctrl+Shift+1/2）。"},
    {"MM.KaeritenHint", "Shortcuts are available in your keymap.",
     "Les raccourcis sont disponibles dans votre table de touches.",
     "ショートカットはキーマップで設定できます。"},
    {"MM.UserSymbolsHint", "Edit user symbols in Preferences.",
     "Modifiez les symboles utilisateur dans les Préférences.",
     "ユーザー記号は環境設定で編集できます。"},

    // Keyboard Shortcuts viewer.
    {"MM.KeyboardShortcuts", "Keyboard Shortcuts", "Raccourcis clavier",
     "キーボードショートカット"},
    {"MM.Script", "Script", "Script", "スクリプト"},
    {"MM.Composition", "Composition", "Composition", "変換"},
    // Linux only.
    {"MM.Close", "Close", "Fermer", "閉じる"},
};

std::optional<MarinaUiLanguage>& OverrideLanguage() {
  static std::optional<MarinaUiLanguage> override_language;
  return override_language;
}

// The locale names GLib's g_get_language_names() would consider, without the
// GLib dependency: LANGUAGE (a colon-separated list, ignored under the "C"
// locale as in gettext), then the first set of LC_ALL / LC_MESSAGES / LANG.
std::vector<std::string> LocaleNamesFromEnvironment() {
  std::string locale;
  for (const char* var : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
    locale = Environ::GetEnv(var);
    if (!locale.empty()) break;
  }
  std::vector<std::string> names;
  if (locale != "C" && locale != "POSIX") {
    for (absl::string_view lang :
         absl::StrSplit(Environ::GetEnv("LANGUAGE"), ':', absl::SkipEmpty())) {
      names.emplace_back(lang);
    }
  }
  if (!locale.empty()) names.push_back(locale);
  return names;
}

MarinaUiLanguage CurrentUiLanguage() {
  if (OverrideLanguage().has_value()) return *OverrideLanguage();
  static const MarinaUiLanguage detected =
      MarinaUiLanguageFromLocaleNames(LocaleNamesFromEnvironment());
  return detected;
}

}  // namespace

MarinaUiLanguage MarinaUiLanguageFromLocaleNames(
    const std::vector<std::string>& names) {
  for (const std::string& name : names) {
    if (name.rfind("ja", 0) == 0) return MarinaUiLanguage::kJapanese;
    if (name.rfind("fr", 0) == 0) return MarinaUiLanguage::kFrench;
    if (name.rfind("en", 0) == 0) return MarinaUiLanguage::kEnglish;
  }
  return MarinaUiLanguage::kEnglish;
}

const char* MarinaLocalizedString(const char* key) {
  if (key == nullptr) return "";
  for (const LocalizedEntry& entry : kEntries) {
    if (std::strcmp(entry.key, key) != 0) continue;
    switch (CurrentUiLanguage()) {
      case MarinaUiLanguage::kJapanese:
        return entry.ja;
      case MarinaUiLanguage::kFrench:
        return entry.fr;
      case MarinaUiLanguage::kEnglish:
      default:
        return entry.en;
    }
  }
  return key;
}

void SetMarinaUiLanguageForTesting(MarinaUiLanguage language) {
  OverrideLanguage() = language;
}

}  // namespace ibus
}  // namespace mozc
