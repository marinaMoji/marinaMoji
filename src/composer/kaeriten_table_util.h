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

#ifndef MOZC_COMPOSER_KAERITEN_TABLE_UTIL_H_
#define MOZC_COMPOSER_KAERITEN_TABLE_UTIL_H_

#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "protocol/config.pb.h"

namespace mozc {
namespace composer {

struct KaeritenRow {
  // Keys after the leading ';' (e.g. "te", "r", ".").
  std::string suffix;
  std::string result;
};

// Read bundled system://kaeriten.tsv as raw TSV bytes.
std::string GetBundledKaeritenTable();

// Parse kaeriten TSV (input \\t result \\t ...). Skips comments and blank lines.
void ParseKaeritenTsvString(absl::string_view tsv,
                            std::vector<KaeritenRow>* rows);

// Build TSV rows sorted longest-suffix-first with DirectInput attribute.
std::string SerializeKaeritenTable(const std::vector<KaeritenRow>& rows);

// Effective table for config (custom bytes or bundled file).
std::string GetEffectiveKaeritenTable(const config::Config& config);

// Shortcut list for toolbar display: (full input e.g. ";r", result glyph).
void LoadKaeritenShortcutEntries(
    const config::Config& config,
    std::vector<std::pair<std::string, std::string>>* entries);

}  // namespace composer
}  // namespace mozc

#endif  // MOZC_COMPOSER_KAERITEN_TABLE_UTIL_H_
