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

#include "rewriter/opencc_rewriter.h"

#include <cstdlib>
#include <string>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/strings/string_view.h"
#include "base/util.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace {

// Adds a single-candidate segment. |content_value| is the part the shin/kyu
// tables apply to; the rest of |value| is the functional (kana) part.
void AddSegment(absl::string_view key, absl::string_view content_key,
                absl::string_view value, absl::string_view content_value,
                Segments* segments) {
  Segment* seg = segments->add_segment();
  seg->set_key(key);
  converter::Candidate* candidate = seg->add_candidate();
  candidate->key = std::string(key);
  candidate->content_key = std::string(content_key);
  candidate->value = std::string(value);
  candidate->content_value = std::string(content_value);
}

std::vector<std::string> Values(const Segment& segment) {
  std::vector<std::string> values;
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    values.push_back(segment.candidate(i).value);
  }
  return values;
}

ConversionRequest MakeRequest(bool use_traditional_kanji) {
  config::Config config;
  config.set_use_traditional_kanji(use_traditional_kanji);
  return ConversionRequestBuilder().SetConfig(config).Build();
}

class OpenccRewriterTest : public testing::TestWithTempUserProfile {
 protected:
  static void SetUpTestSuite() {
    // The rewriter looks for marinaShin2Kyu.json here before falling back to
    // the installed server directory. Must be set before the first conversion,
    // as the converter is built once via std::call_once.
    const std::string data_dir =
        testing::GetSourceDirOrDie({"data", "marina_opencc"});
#ifdef _WIN32
    ::_putenv_s("OPENCC_DATA_DIR", data_dir.c_str());
#else   // _WIN32
    ::setenv("OPENCC_DATA_DIR", data_dir.c_str(), 1);
#endif  // _WIN32
  }

  OpenccRewriter rewriter_;
};

TEST_F(OpenccRewriterTest, DoesNothingWhenDisabled) {
  Segments segments;
  AddSegment("くに", "くに", "国", "国", &segments);

  const ConversionRequest request = MakeRequest(false);
  EXPECT_EQ(rewriter_.capability(request), RewriterInterface::NOT_AVAILABLE);
  EXPECT_FALSE(rewriter_.Rewrite(request, &segments));
  EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, "国");
}

TEST_F(OpenccRewriterTest, ConvertsToKyujitai) {
  Segments segments;
  AddSegment("くに", "くに", "国", "国", &segments);

  EXPECT_TRUE(rewriter_.Rewrite(MakeRequest(true), &segments));
  const Segment& seg = segments.conversion_segment(0);
  ASSERT_EQ(seg.candidates_size(), 1);
  EXPECT_EQ(seg.candidate(0).value, "國");
  EXPECT_EQ(seg.candidate(0).content_value, "國");
}

// The functional part is kana and must survive unconverted, and content_value
// must keep holding only the content part. Converting the whole value and then
// content_value in a second pass used to leave content_value holding the full
// surface, and ran the tables twice over already-converted text.
TEST_F(OpenccRewriterTest, PreservesFunctionalValue) {
  Segments segments;
  AddSegment("くにだ", "くに", "国だ", "国", &segments);

  EXPECT_TRUE(rewriter_.Rewrite(MakeRequest(true), &segments));
  const Segment& seg = segments.conversion_segment(0);
  ASSERT_EQ(seg.candidates_size(), 1);
  EXPECT_EQ(seg.candidate(0).value, "國だ");
  EXPECT_EQ(seg.candidate(0).content_value, "國");
  EXPECT_EQ(seg.candidate(0).functional_value(), "だ");
}

TEST_F(OpenccRewriterTest, ExpandsOneToManyVariants) {
  Segments segments;
  AddSegment("よ", "よ", "予", "予", &segments);

  EXPECT_TRUE(rewriter_.Rewrite(MakeRequest(true), &segments));
  const Segment& seg = segments.conversion_segment(0);
  EXPECT_GT(seg.candidates_size(), 1);
  EXPECT_TRUE(absl::c_linear_search(Values(seg), "豫"));
  // Every expanded candidate keeps the invariant that content_value is a
  // prefix of value.
  for (size_t i = 0; i < seg.candidates_size(); ++i) {
    const converter::Candidate& candidate = seg.candidate(i);
    EXPECT_TRUE(absl::string_view(candidate.value)
                    .starts_with(candidate.content_value))
        << candidate.value << " / " << candidate.content_value;
  }
}

// 丈, 冴, 刃 and 棚 have no distinct kyujitai codepoint. They used to map to an
// invisible IVS sequence, which EnvironmentalFilterRewriter then erased,
// making 大丈夫 disappear from the candidate window entirely (issue #7).
TEST_F(OpenccRewriterTest, NoIvsSequencesInOutput) {
  for (const absl::string_view value : {"大丈夫", "丈夫", "冴", "刃", "棚"}) {
    Segments segments;
    AddSegment("か", "か", value, value, &segments);

    rewriter_.Rewrite(MakeRequest(true), &segments);
    const Segment& seg = segments.conversion_segment(0);
    for (size_t i = 0; i < seg.candidates_size(); ++i) {
      const std::u32string codepoints =
          Util::Utf8ToUtf32(seg.candidate(i).value);
      for (const char32_t c : codepoints) {
        EXPECT_FALSE(0xE0100 <= c && c <= 0xE01EF)
            << "IVS codepoint in candidate for " << value;
      }
    }
  }
}

}  // namespace
}  // namespace mozc
