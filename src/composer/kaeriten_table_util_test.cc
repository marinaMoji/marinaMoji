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

#include "composer/kaeriten_table_util.h"

#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace mozc {
namespace composer {
namespace {

TEST(KaeritenTableUtilTest, SerializeSortsLongerSuffixFirst) {
  const std::vector<KaeritenRow> rows = {
      {"t", "㆜"},
      {"te", "㆝"},
  };
  const std::string tsv = SerializeKaeritenTable(rows);
  EXPECT_EQ(tsv.find(";te\t"), 0u);
  EXPECT_NE(tsv.find(";t\t"), 0u);
}

TEST(KaeritenTableUtilTest, ParseAndRoundTrip) {
  const std::string input = ";r\t㆑\t\tDirectInput\n;te\t㆝\t\tDirectInput\n";
  std::vector<KaeritenRow> rows;
  ParseKaeritenTsvString(input, &rows);
  ASSERT_EQ(rows.size(), 2u);
  EXPECT_EQ(rows[0].suffix, "r");
  EXPECT_EQ(rows[0].result, "㆑");
  EXPECT_EQ(rows[1].suffix, "te");
  EXPECT_EQ(rows[1].result, "㆝");

  const std::string serialized = SerializeKaeritenTable(rows);
  EXPECT_NE(serialized.find(";te\t㆝"), std::string::npos);
  EXPECT_NE(serialized.find(";r\t㆑"), std::string::npos);
}

TEST(KaeritenTableUtilTest, LoadShortcutEntriesFromCustomConfig) {
  config::Config config;
  config.set_custom_kaeriten_table(";xy\tX\t\tDirectInput\n");
  std::vector<std::pair<std::string, std::string>> entries;
  LoadKaeritenShortcutEntries(config, &entries);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0].first, ";xy");
  EXPECT_EQ(entries[0].second, "X");
}

}  // namespace
}  // namespace composer
}  // namespace mozc
