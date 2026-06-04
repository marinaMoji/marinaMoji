// Copyright 2010-2021, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "unix/ibus/message_translator.h"

#include <cstddef>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_split.h"
#include "base/util.h"

namespace {

struct TranslationMap {
  const char* message;
  const char* translated;
};

const TranslationMap kUTF8JapaneseMap[] = {
    {"Direct input", "直接入力"},
    {"Hiragana", "ひらがな"},
    {"Katakana", "カタカナ"},
    {"Latin", "半角英数"},
    {"Wide Latin", "全角英数"},
    {"Half width katakana", "半角カタカナ"},
    {"Tools", "ツール"},
    {"Properties", "プロパティ"},
    {"Dictionary Tool", "辞書ツール"},
    {"Add Word", "単語登録"},
    {"Input Mode", "入力モード"},
    {"Show toolbar", "ツールバーを表示"},
    {"Hide toolbar", "ツールバーを非表示"},
    {"Toolbar", "ツールバー"},
    {"Traditional kanji (Kyūjitai)", "伝統漢字（旧字体）"},
    {"Odoriji (iteration marks)", "踊り字（繰り返し記号）"},
    {"Privacy mode", "プライバシーモード"},
#ifdef MARINAMOJI
    {"About marinaMoji", "marinaMoji について"},
#elif defined(GOOGLE_JAPANESE_INPUT_BUILD)
    {"About Mozc", "Google 日本語入力について"},
#else   // GOOGLE_JAPANESE_INPUT_BUILD
    {"About Mozc", "Mozc について"},
#endif  // MARINAMOJI / GOOGLE_JAPANESE_INPUT_BUILD
};

const TranslationMap kUTF8FrenchMap[] = {
    {"Direct input", "Saisie directe"},
    {"Hiragana", "Hiragana"},
    {"Katakana", "Katakana"},
    {"Latin", "Alphanumérique demi-chasse"},
    {"Wide Latin", "Alphanumérique pleine chasse"},
    {"Half width katakana", "Katakana demi-chasse"},
    {"Tools", "Outils"},
    {"Properties", "Propriétés"},
    {"Dictionary Tool", "Outil dictionnaire"},
    {"Add Word", "Ajouter un mot"},
    {"Input Mode", "Mode de saisie"},
    {"Show toolbar", "Afficher la barre d'outils"},
    {"Hide toolbar", "Masquer la barre d'outils"},
    {"Toolbar", "Barre d'outils"},
    {"Traditional kanji (Kyūjitai)", "Kanji traditionnels (kyūjitai)"},
    {"Odoriji (iteration marks)", "Odoriji (marques d'itération)"},
    {"Privacy mode", "Mode confidentialité"},
#ifdef MARINAMOJI
    {"About marinaMoji", "À propos de marinaMoji"},
#elif defined(GOOGLE_JAPANESE_INPUT_BUILD)
    {"About Mozc", "À propos de Google Japanese Input"},
#else   // GOOGLE_JAPANESE_INPUT_BUILD
    {"About Mozc", "À propos de Mozc"},
#endif  // MARINAMOJI / GOOGLE_JAPANESE_INPUT_BUILD
};

bool IsUtf8Charset(absl::string_view charset) {
  std::string lowered(charset);
  Util::LowerString(&lowered);
  return lowered == "utf-8" || lowered == "utf8";
}

void LoadTranslationMap(const TranslationMap* entries, size_t count,
                        std::map<std::string, std::string>* out) {
  for (size_t i = 0; i < count; ++i) {
    const TranslationMap& mapping = entries[i];
    DCHECK(mapping.message);
    DCHECK(mapping.translated);
    out->insert(std::make_pair(mapping.message, mapping.translated));
  }
}

const TranslationMap* GetMapForLanguage(const std::string& language_code,
                                        size_t* out_count) {
  if (language_code == "ja_JP") {
    *out_count = std::size(kUTF8JapaneseMap);
    return kUTF8JapaneseMap;
  }
  if (language_code == "fr_FR" || language_code == "fr") {
    *out_count = std::size(kUTF8FrenchMap);
    return kUTF8FrenchMap;
  }
  return nullptr;
}

}  // namespace

namespace mozc {
namespace ibus {

MessageTranslatorInterface::~MessageTranslatorInterface() = default;

NullMessageTranslator::NullMessageTranslator() = default;

std::string NullMessageTranslator::MaybeTranslate(
    const std::string& message) const {
  return message;
}

LocaleBasedMessageTranslator::LocaleBasedMessageTranslator(
    const std::string& locale_name) {
  std::vector<std::string> tokens =
      absl::StrSplit(locale_name, '.', absl::SkipEmpty());
  if (tokens.empty()) {
    return;
  }
  const std::string& language_code = tokens[0];
  if (tokens.size() >= 2 && !IsUtf8Charset(tokens[1])) {
    return;
  }

  size_t count = 0;
  const TranslationMap* entries = GetMapForLanguage(language_code, &count);
  if (entries == nullptr) {
    return;
  }
  LoadTranslationMap(entries, count, &translation_map_);
}

std::string LocaleBasedMessageTranslator::MaybeTranslate(
    const std::string& message) const {
  std::map<std::string, std::string>::const_iterator itr =
      translation_map_.find(message);
  if (itr == translation_map_.end()) {
    return message;
  }

  return itr->second;
}

}  // namespace ibus
}  // namespace mozc
