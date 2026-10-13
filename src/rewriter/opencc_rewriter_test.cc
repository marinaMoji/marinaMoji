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
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/util.h"
#include "converter/candidate.h"
#include "converter/inner_segment.h"
#include "converter/segments.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace {

// Adds a single-candidate segment. |content_value| is the part the shin/kyu
// tables apply to; the rest of |value| is the functional (kana) part.
converter::Candidate* AddSegment(absl::string_view key,
                                 absl::string_view content_key,
                                 absl::string_view value,
                                 absl::string_view content_value,
                                 Segments* segments) {
  Segment* seg = segments->add_segment();
  seg->set_key(key);
  converter::Candidate* candidate = seg->add_candidate();
  candidate->key = std::string(key);
  candidate->content_key = std::string(content_key);
  candidate->value = std::string(value);
  candidate->content_value = std::string(content_value);
  return candidate;
}

// Inner segments of |candidate| as "value" or "content|functional" strings.
std::vector<std::string> InnerValues(const converter::Candidate& candidate) {
  std::vector<std::string> values;
  for (const auto& iter : candidate.inner_segments()) {
    values.push_back(iter.GetFunctionalValue().empty()
                         ? std::string(iter.GetValue())
                         : absl::StrCat(iter.GetContentValue(), "|",
                                        iter.GetFunctionalValue()));
  }
  return values;
}

// Checks that |candidate|'s inner_segment_boundary is one InnerSegments
// accepts: an inconsistent boundary is silently replaced by a single segment,
// so the segment count would not match the boundary size.
void ExpectConsistentInnerSegments(const converter::Candidate& candidate) {
  SCOPED_TRACE(candidate.value);
  EXPECT_TRUE(absl::string_view(candidate.value)
                  .starts_with(candidate.content_value));
  ASSERT_FALSE(candidate.inner_segment_boundary.empty());
  std::string key;
  std::string value;
  size_t size = 0;
  for (const auto& iter : converter::InnerSegments(
           candidate.key, candidate.value, candidate.inner_segment_boundary)) {
    EXPECT_TRUE(iter.GetValue().starts_with(iter.GetContentValue()));
    absl::StrAppend(&key, iter.GetKey());
    absl::StrAppend(&value, iter.GetValue());
    ++size;
  }
  EXPECT_EQ(size, candidate.inner_segment_boundary.size());
  EXPECT_EQ(key, candidate.key);
  EXPECT_EQ(value, candidate.value);
}

// U+E0101 VARIATION SELECTOR-18, appended by the Variants table.
constexpr absl::string_view kVs18 = "\xF3\xA0\x84\x81";

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

// 刃 maps to 刃 plus a variation selector (an IVS sequence). It used to be
// erased by EnvironmentalFilterRewriter, making words disappear from the
// candidate window (issue #7). The filter now strips unrenderable selectors,
// or keeps them when the client declares IVS_CHARACTER, so the rewriter must
// always leave a candidate whose base characters are intact.
TEST_F(OpenccRewriterTest, IvsOutputKeepsBaseCharacters) {
  for (const absl::string_view value : {"大丈夫", "丈夫", "冴", "刃", "棚"}) {
    Segments segments;
    AddSegment("か", "か", value, value, &segments);

    rewriter_.Rewrite(MakeRequest(true), &segments);
    const Segment& seg = segments.conversion_segment(0);
    EXPECT_GT(seg.candidates_size(), 0) << value;
    for (size_t i = 0; i < seg.candidates_size(); ++i) {
      std::u32string codepoints = Util::Utf8ToUtf32(seg.candidate(i).value);
      std::erase_if(codepoints, [](const char32_t c) {
        return 0xE0100 <= c && c <= 0xE01EF;
      });
      EXPECT_EQ(Util::Utf32ToUtf8(codepoints), value)
          << "base characters changed for " << value;
    }
  }
}

// 刃 and 棚 gain an IVS selector (3 -> 7 bytes), so a copied boundary would no
// longer add up to the value.
TEST_F(OpenccRewriterTest, RebuildsInnerSegmentBoundaryWhenValueGrows) {
  Segments segments;
  converter::Candidate* candidate =
      AddSegment("やいばをたな", "やいば", "刃を棚", "刃", &segments);
  candidate->inner_segment_boundary = converter::BuildInnerSegmentBoundary(
      {{12, 6, 9, 3}, {6, 3, 6, 3}}, candidate->key, candidate->value);
  ASSERT_EQ(candidate->inner_segment_boundary.size(), 2);

  EXPECT_TRUE(rewriter_.Rewrite(MakeRequest(true), &segments));
  const Segment& seg = segments.conversion_segment(0);
  ASSERT_EQ(seg.candidates_size(), 1);
  const converter::Candidate& result = seg.candidate(0);
  const std::string blade = absl::StrCat("刃", kVs18);
  const std::string shelf = absl::StrCat("棚", kVs18);
  EXPECT_EQ(result.value, absl::StrCat(blade, "を", shelf));
  EXPECT_EQ(result.content_value, blade);
  EXPECT_EQ(result.key, "やいばをたな");
  ExpectConsistentInnerSegments(result);
  EXPECT_THAT(InnerValues(result), ::testing::ElementsAre(
                                       absl::StrCat(blade, "|を"), shelf));
}

// The phrase 海里 -> 浬 shrinks the first segment (6 -> 3 bytes) while 刃 grows
// the second one.
TEST_F(OpenccRewriterTest, RebuildsInnerSegmentBoundaryWhenValueShrinks) {
  Segments segments;
  converter::Candidate* candidate =
      AddSegment("かいりのやいば", "かいり", "海里の刃", "海里", &segments);
  candidate->inner_segment_boundary = converter::BuildInnerSegmentBoundary(
      {{12, 9, 9, 6}, {9, 3, 9, 3}}, candidate->key, candidate->value);
  ASSERT_EQ(candidate->inner_segment_boundary.size(), 2);

  EXPECT_TRUE(rewriter_.Rewrite(MakeRequest(true), &segments));
  const Segment& seg = segments.conversion_segment(0);
  ASSERT_EQ(seg.candidates_size(), 1);
  const converter::Candidate& result = seg.candidate(0);
  const std::string blade = absl::StrCat("刃", kVs18);
  EXPECT_EQ(result.value, absl::StrCat("浬の", blade));
  EXPECT_EQ(result.content_value, "浬");
  ExpectConsistentInnerSegments(result);
  EXPECT_THAT(InnerValues(result), ::testing::ElementsAre("浬|の", blade));
}

// content_value need not end on an inner segment boundary: here the inner
// segment's content part is the whole "刃だ", but content_value is "刃".
TEST_F(OpenccRewriterTest, KeepsContentValueInsideInnerSegment) {
  Segments segments;
  converter::Candidate* candidate =
      AddSegment("やいばだ", "やいば", "刃だ", "刃", &segments);
  candidate->inner_segment_boundary = converter::BuildInnerSegmentBoundary(
      {{12, 6, 12, 6}}, candidate->key, candidate->value);

  EXPECT_TRUE(rewriter_.Rewrite(MakeRequest(true), &segments));
  const converter::Candidate& result =
      segments.conversion_segment(0).candidate(0);
  const std::string blade = absl::StrCat("刃", kVs18);
  EXPECT_EQ(result.value, absl::StrCat(blade, "だ"));
  EXPECT_EQ(result.content_value, blade);
  EXPECT_EQ(result.functional_value(), "だ");
  ExpectConsistentInnerSegments(result);
  EXPECT_THAT(InnerValues(result),
              ::testing::ElementsAre(absl::StrCat(blade, "だ")));
}

// Every variant inserted for a one-to-many mapping gets its own boundary.
TEST_F(OpenccRewriterTest, RebuildsInnerSegmentBoundaryForEachVariant) {
  Segments segments;
  converter::Candidate* candidate =
      AddSegment("よをやいば", "よ", "予を刃", "予", &segments);
  candidate->inner_segment_boundary = converter::BuildInnerSegmentBoundary(
      {{6, 6, 3, 3}, {9, 3, 9, 3}}, candidate->key, candidate->value);

  EXPECT_TRUE(rewriter_.Rewrite(MakeRequest(true), &segments));
  const Segment& seg = segments.conversion_segment(0);
  EXPECT_GT(seg.candidates_size(), 1);
  EXPECT_TRUE(absl::c_linear_search(Values(seg),
                                    absl::StrCat("豫を刃", kVs18)));
  for (size_t i = 0; i < seg.candidates_size(); ++i) {
    ExpectConsistentInnerSegments(seg.candidate(i));
    EXPECT_EQ(seg.candidate(i).inner_segment_boundary.size(), 2);
  }
}

// A converted segment that no longer fits the boundary encoding (255 bytes per
// segment) gets no boundary rather than an inconsistent one.
TEST_F(OpenccRewriterTest, ClearsInnerSegmentBoundaryThatCannotBeEncoded) {
  std::string key;
  std::string value;
  for (int i = 0; i < 50; ++i) {
    absl::StrAppend(&key, "は");
    absl::StrAppend(&value, "刃");
  }
  Segments segments;
  converter::Candidate* candidate =
      AddSegment(key, key, value, value, &segments);
  candidate->inner_segment_boundary = converter::BuildInnerSegmentBoundary(
      {{150, 150, 150, 150}}, key, value);
  ASSERT_FALSE(candidate->inner_segment_boundary.empty());

  EXPECT_TRUE(rewriter_.Rewrite(MakeRequest(true), &segments));
  const converter::Candidate& result =
      segments.conversion_segment(0).candidate(0);
  EXPECT_EQ(result.value.size(), 50 * 7);
  EXPECT_TRUE(result.inner_segment_boundary.empty());
}

// Without an inner segment boundary, none is made up.
TEST_F(OpenccRewriterTest, LeavesEmptyInnerSegmentBoundaryEmpty) {
  Segments segments;
  AddSegment("やいばを", "やいば", "刃を", "刃", &segments);

  EXPECT_TRUE(rewriter_.Rewrite(MakeRequest(true), &segments));
  const converter::Candidate& result =
      segments.conversion_segment(0).candidate(0);
  EXPECT_EQ(result.value, absl::StrCat("刃", kVs18, "を"));
  EXPECT_TRUE(result.inner_segment_boundary.empty());
}

}  // namespace
}  // namespace mozc
