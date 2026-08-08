// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0

#include "renderer/win32/marina_localized_string.h"

#include <windows.h>

#include <cwchar>
#include <optional>

namespace mozc {
namespace renderer {
namespace win32 {

namespace {

struct LocalizedEntry {
  const wchar_t* key;
  const wchar_t* en;
  const wchar_t* fr;
  const wchar_t* ja;
};

// Keys and wordings mirror src/mac/Resources/{en,fr,ja}.lproj/
// Localizable.strings. Entries with no mac counterpart (the tooltips and the
// toolbar context menu, which mac gets from AppKit or its IMK menu) follow the
// same naming convention.
constexpr LocalizedEntry kEntries[] = {
    // Mode menu.
    {L"MM.Mode", L"Mode", L"Mode", L"モード"},
    {L"MM.Hiragana", L"Hiragana", L"Hiragana", L"ひらがな"},
    {L"MM.KatakanaManyoshu", L"Katakana (Manyōshū)", L"Katakana (Manyōshū)",
     L"カタカナ（万葉仮名）"},
    {L"MM.HalfWidthKatakana", L"Half width katakana", L"Katakana demi-chasse",
     L"半角カタカナ"},
    {L"MM.FullWidthRoman", L"Full-width Roman", L"Romain pleine chasse",
     L"全角ローマ字"},
    {L"MM.HalfWidthRoman", L"Half-width Roman", L"Romain demi-chasse",
     L"半角ローマ字"},
    {L"MM.DirectInputMode", L"Direct Input", L"Saisie directe", L"直接入力"},

    // Toolbar buttons (tooltips) and context menu.
    {L"MM.Toolbar", L"Toolbar", L"Barre d'outils", L"ツールバー"},
    {L"MM.InputMode", L"Input Mode", L"Mode de saisie", L"入力モード"},
    {L"MM.TraditionalKanji", L"Traditional kanji (Kyūjitai)",
     L"Kanji traditionnels (kyūjitai)", L"伝統漢字（旧字体）"},
    {L"MM.SymbolsPalette", L"Symbols Palette", L"Palette de symboles",
     L"記号パレット"},
    {L"MM.RegisterWord", L"Add Word...", L"Ajouter un mot...", L"単語登録..."},
    {L"MM.Docket", L"Docket...", L"File d'attente...", L"未登録語リスト..."},
    {L"MM.DictionaryTool", L"Dictionary Tool...", L"Outil dictionnaire...",
     L"辞書ツール..."},
    {L"MM.Settings", L"Settings...", L"Paramètres...", L"設定..."},
    {L"MM.KeyboardShortcuts", L"Keyboard Shortcuts", L"Raccourcis clavier",
     L"キーボードショートカット"},
    {L"MM.HideToolbar", L"Hide toolbar", L"Masquer la barre d'outils",
     L"ツールバーを非表示"},

    // Symbols Palette.
    {L"MM.PinPalette", L"Pin palette", L"Épingler la palette",
     L"パレットを固定"},
    {L"MM.Odoriji", L"Odoriji (iteration marks)",
     L"Odoriji (marques d'itération)", L"踊り字（繰り返し記号）"},
    {L"MM.Kaeriten", L"Kaeriten", L"Kaeriten", L"返り点"},
    {L"MM.Symbols", L"Symbols", L"Symboles", L"記号"},
    {L"MM.User", L"User", L"Utilisateur", L"ユーザー"},
    {L"MM.OdorijiHint",
     L"Clicking an odoriji inserts it and sets it as your default (same "
     L"behavior as the main Odoriji palette). Shortcuts are available in your "
     L"keymap (Ctrl+Shift+1/2 on common layouts).",
     L"Un clic sur un odoriji l'insère et le définit par défaut (comme la "
     L"palette Odoriji principale). Les raccourcis sont disponibles dans votre "
     L"table de touches (Ctrl+Maj+1/2 sur les dispositions courantes).",
     L"踊り字をクリックすると挿入され、既定の踊り字として設定されます"
     L"（メインの踊り字パレットと同じ動作）。ショートカットはキーマップで"
     L"設定できます（一般的な配列では Ctrl+Shift+1/2）。"},
    {L"MM.KaeritenHint", L"Shortcuts are available in your keymap.",
     L"Les raccourcis sont disponibles dans votre table de touches.",
     L"ショートカットはキーマップで設定できます。"},
    {L"MM.UserSymbolsHint", L"Edit user symbols in Settings.",
     L"Modifiez les symboles utilisateur dans les Paramètres.",
     L"ユーザー記号は設定で編集できます。"},

    // Shortcuts viewer.
    {L"MM.Script", L"Script", L"Script", L"スクリプト"},
    {L"MM.Composition", L"Composition", L"Composition", L"変換"},
    {L"MM.Function", L"Function", L"Fonction", L"機能"},
    {L"MM.Keys", L"Keys", L"Touches", L"キー"},
    {L"MM.Result", L"Result", L"Résultat", L"結果"},
    {L"MM.Input", L"Input", L"Saisie", L"入力"},
};

std::optional<MarinaUiLanguage>& OverrideLanguage() {
  static std::optional<MarinaUiLanguage> override_language;
  return override_language;
}

MarinaUiLanguage DetectUiLanguage() {
  switch (PRIMARYLANGID(::GetUserDefaultUILanguage())) {
    case LANG_JAPANESE:
      return MarinaUiLanguage::kJapanese;
    case LANG_FRENCH:
      return MarinaUiLanguage::kFrench;
    default:
      return MarinaUiLanguage::kEnglish;
  }
}

MarinaUiLanguage CurrentUiLanguage() {
  if (OverrideLanguage().has_value()) {
    return *OverrideLanguage();
  }
  // The UI language can't change without a sign-out, so detect once.
  static const MarinaUiLanguage detected = DetectUiLanguage();
  return detected;
}

}  // namespace

const wchar_t* MarinaLocalizedString(const wchar_t* key) {
  if (key == nullptr) {
    return L"";
  }
  for (const LocalizedEntry& entry : kEntries) {
    if (std::wcscmp(entry.key, key) != 0) {
      continue;
    }
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
  // Unknown key: show it rather than an empty control, matching how
  // NSLocalizedString falls back on mac.
  return key;
}

void SetMarinaUiLanguageForTesting(MarinaUiLanguage language) {
  OverrideLanguage() = language;
}

}  // namespace win32
}  // namespace renderer
}  // namespace mozc
