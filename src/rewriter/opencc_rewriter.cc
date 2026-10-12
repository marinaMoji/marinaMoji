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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "converter/candidate.h"
#include "converter/inner_segment.h"
#include "converter/segments.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "base/system_util.h"
#include "rewriter/rewriter_interface.h"

#ifdef MOZC_USE_OPENCC
#include <opencc/Config.hpp>
#include <opencc/Conversion.hpp>
#include <opencc/ConversionChain.hpp>
#include <opencc/Converter.hpp>
#include <opencc/Dict.hpp>
#include <opencc/UTF8Util.hpp>
#endif  // MOZC_USE_OPENCC

namespace mozc {

namespace {

#ifdef MOZC_USE_OPENCC
// Cap fan-out so one-to-many tables cannot explode the candidate window.
constexpr size_t kMaxOpenccVariants = 32;
constexpr char kMarinaOpenccConfig[] = "marinaShin2Kyu.json";

std::string GetOpenccConfigPath() {
  const char* env = std::getenv("OPENCC_DATA_DIR");
  if (env && env[0] != '\0') {
    return std::string(env) + "/" + kMarinaOpenccConfig;
  }
  // Bundled marina tables: macOS .app Resources/opencc/, Linux mozc.zip opencc/.
  const std::string bundled_path =
      SystemUtil::GetServerDirectory() + "/opencc/" + kMarinaOpenccConfig;
  if (std::ifstream(bundled_path).good()) {
    return bundled_path;
  }
  return bundled_path;
}

opencc::ConverterPtr GetConverter() {
  static opencc::ConverterPtr converter;
  static std::once_flag once;
  std::call_once(once, []() {
    try {
      opencc::Config config;
      converter = config.NewFromFile(GetOpenccConfigPath());
    } catch (...) {
      converter = nullptr;
    }
  });
  return converter;
}

void AppendUnique(std::vector<std::string>* out, std::string value) {
  for (const std::string& existing : *out) {
    if (existing == value) {
      return;
    }
  }
  out->push_back(std::move(value));
}

void CollectPhraseVariants(const opencc::DictPtr& dict, const char* phrase,
                           const char* phrase_end, std::string prefix,
                           std::vector<std::string>* out) {
  if (out->size() >= kMaxOpenccVariants) {
    return;
  }
  if (phrase == phrase_end) {
    AppendUnique(out, std::move(prefix));
    return;
  }

  const size_t remaining = static_cast<size_t>(phrase_end - phrase);
  const opencc::Optional<const opencc::DictEntry*>& matched =
      dict->MatchPrefix(phrase, remaining);
  if (matched.IsNull()) {
    size_t matched_length = opencc::UTF8Util::NextCharLength(phrase);
    if (matched_length > remaining) {
      matched_length = remaining;
    }
    prefix += opencc::UTF8Util::FromSubstr(phrase, matched_length);
    CollectPhraseVariants(dict, phrase + matched_length, phrase_end,
                          std::move(prefix), out);
    return;
  }

  size_t matched_length = matched.Get()->KeyLength();
  if (matched_length > remaining) {
    matched_length = remaining;
  }
  const opencc::DictEntry* entry = matched.Get();
  std::vector<std::string> replacements;
  if (entry->NumValues() == 0) {
    replacements.push_back(entry->GetDefault());
  } else {
    replacements = entry->Values();
  }
  for (const std::string& replacement : replacements) {
    if (out->size() >= kMaxOpenccVariants) {
      break;
    }
    CollectPhraseVariants(dict, phrase + matched_length, phrase_end,
                          prefix + replacement, out);
  }
}

std::vector<std::string> ConvertSegmentAllVariants(
    const opencc::ConversionChainPtr& chain, const std::string& segment) {
  std::vector<std::string> current;
  AppendUnique(&current, segment);
  for (const opencc::ConversionPtr& conversion : chain->GetConversions()) {
    const opencc::DictPtr& dict = conversion->GetDict();
    std::vector<std::string> next;
    for (const std::string& text : current) {
      std::vector<std::string> phrase_variants;
      CollectPhraseVariants(dict, text.c_str(), text.c_str() + text.size(), "",
                            &phrase_variants);
      for (std::string& variant : phrase_variants) {
        if (next.size() >= kMaxOpenccVariants) {
          break;
        }
        AppendUnique(&next, std::move(variant));
      }
    }
    current = std::move(next);
    if (current.empty()) {
      break;
    }
  }
  return current;
}

std::vector<std::string> ConvertAllVariants(const opencc::ConverterPtr& converter,
                                            const std::string& input) {
  if (!converter || input.empty()) {
    return {};
  }

  const opencc::SegmentsPtr& segments =
      converter->GetSegmentation()->Segment(input);
  const opencc::ConversionChainPtr& chain = converter->GetConversionChain();

  std::vector<std::string> results;
  AppendUnique(&results, "");
  for (const char* segment : *segments) {
    const std::vector<std::string> segment_variants =
        ConvertSegmentAllVariants(chain, std::string(segment));
    std::vector<std::string> combined;
    for (const std::string& prefix : results) {
      for (const std::string& segment_variant : segment_variants) {
        if (combined.size() >= kMaxOpenccVariants) {
          break;
        }
        AppendUnique(&combined, prefix + segment_variant);
      }
    }
    results = std::move(combined);
    if (results.empty()) {
      return {};
    }
  }
  return results;
}

// One OpenCC conversion of a whole candidate.
struct CandidateVariant {
  std::string value;
  std::string content_value;
  converter::InnerSegmentBoundary inner_segment_boundary;
};

// The candidate's value split into runs. Every inner segment boundary, every
// inner content/functional boundary and the end of content_value falls between
// two runs, so each of them can be located again in a converted value by
// summing the converted run lengths.
struct CandidateLayout {
  struct InnerSegment {
    uint32_t key_len;
    uint32_t content_key_len;
    // Run indices one past the content part and one past the whole segment.
    size_t content_end_run;
    size_t end_run;
  };

  std::vector<absl::string_view> runs;
  // Whether runs[i] belongs to a content part, which is converted. Functional
  // parts are kana (okurigana, particles) and are carried over unconverted.
  std::vector<bool> is_content;
  std::vector<InnerSegment> inner_segments;
  // Run index one past the end of the candidate's content_value.
  size_t content_value_end_run = 0;
};

// Splits |cand|'s value along its inner segments. A candidate without an inner
// segment boundary is laid out as a single segment whose content part is
// content_value, as converter::InnerSegments does.
CandidateLayout BuildCandidateLayout(const converter::Candidate& cand) {
  struct Range {
    size_t key_len;
    size_t content_key_len;
    size_t content_end;
    size_t end;
  };
  std::vector<Range> ranges;
  std::vector<size_t> cuts;
  size_t offset = 0;
  for (const auto& iter : cand.inner_segments()) {
    const size_t content_end = offset + iter.GetContentValue().size();
    offset += iter.GetValue().size();
    ranges.push_back({iter.GetKey().size(), iter.GetContentKey().size(),
                      content_end, offset});
    cuts.push_back(content_end);
    cuts.push_back(offset);
  }
  // content_value need not end on an inner segment boundary: for "走った" the
  // inner segment's content part is the whole word, but content_value is "走っ".
  size_t content_value_end = cand.value.size();
  if (!cand.content_value.empty() &&
      cand.content_value.size() < cand.value.size() &&
      (static_cast<uint8_t>(cand.value[cand.content_value.size()]) & 0xc0) !=
          0x80) {
    content_value_end = cand.content_value.size();
  }
  cuts.push_back(content_value_end);
  absl::c_sort(cuts);
  cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());
  std::erase(cuts, 0);

  // Number of runs that end at or before |pos|, which is always one of |cuts|.
  const auto run_index = [&cuts](size_t pos) -> size_t {
    return absl::c_lower_bound(cuts, pos) - cuts.begin() + 1;
  };

  CandidateLayout layout;
  size_t run_start = 0;
  size_t range_index = 0;
  for (const size_t cut : cuts) {
    while (range_index < ranges.size() && ranges[range_index].end <= run_start) {
      ++range_index;
    }
    layout.runs.push_back(
        absl::string_view(cand.value).substr(run_start, cut - run_start));
    layout.is_content.push_back(range_index < ranges.size() &&
                                cut <= ranges[range_index].content_end);
    run_start = cut;
  }
  for (const Range& range : ranges) {
    layout.inner_segments.push_back(
        {static_cast<uint32_t>(range.key_len),
         static_cast<uint32_t>(range.content_key_len),
         run_index(range.content_end), run_index(range.end)});
  }
  layout.content_value_end_run = run_index(content_value_end);
  return layout;
}

// Converts each content run of |cand| separately and combines the results, so
// the inner segment boundary can be rebuilt from the converted run lengths.
// Shinjitai-to-kyujitai mappings and IVS selectors change the UTF-8 length, so
// copying the source boundary would leave it inconsistent with the value.
std::vector<CandidateVariant> ConvertCandidate(
    const opencc::ConverterPtr& converter, const converter::Candidate& cand) {
  const CandidateLayout layout = BuildCandidateLayout(cand);

  std::vector<std::vector<std::string>> run_variants;
  run_variants.reserve(layout.runs.size());
  for (size_t i = 0; i < layout.runs.size(); ++i) {
    const std::string run(layout.runs[i]);
    std::vector<std::string> variants;
    if (layout.is_content[i]) {
      variants = ConvertAllVariants(converter, run);
    }
    if (variants.empty()) {
      variants.push_back(run);
    }
    run_variants.push_back(std::move(variants));
  }

  // Each combination picks one variant per run. The first one picks the
  // primary variant of every run.
  std::vector<std::vector<absl::string_view>> combinations = {{}};
  for (const std::vector<std::string>& variants : run_variants) {
    std::vector<std::vector<absl::string_view>> next;
    for (const std::vector<absl::string_view>& prefix : combinations) {
      for (const std::string& variant : variants) {
        if (next.size() >= kMaxOpenccVariants) {
          break;
        }
        next.push_back(prefix);
        next.back().push_back(variant);
      }
    }
    combinations = std::move(next);
  }

  std::vector<CandidateVariant> results;
  absl::flat_hash_set<std::string> seen_values;
  for (const std::vector<absl::string_view>& combination : combinations) {
    CandidateVariant result;
    // offsets[i] is where run i starts in the converted value.
    std::vector<size_t> offsets = {0};
    for (const absl::string_view run : combination) {
      absl::StrAppend(&result.value, run);
      offsets.push_back(result.value.size());
    }
    if (!seen_values.insert(result.value).second) {
      continue;
    }
    if (!cand.content_value.empty()) {
      result.content_value =
          result.value.substr(0, offsets[layout.content_value_end_run]);
    }
    if (!cand.inner_segment_boundary.empty()) {
      // Keys are readings and are not converted, so the key lengths carry
      // over. Build() returns an empty boundary rather than an inconsistent
      // one, e.g. when a converted segment exceeds the encodable length.
      converter::InnerSegmentBoundaryBuilder builder;
      size_t start_run = 0;
      for (const CandidateLayout::InnerSegment& segment :
           layout.inner_segments) {
        const size_t start = offsets[start_run];
        builder.Add(segment.key_len, offsets[segment.end_run] - start,
                    segment.content_key_len,
                    offsets[segment.content_end_run] - start);
        start_run = segment.end_run;
      }
      result.inner_segment_boundary = builder.Build(cand.key, result.value);
    }
    results.push_back(std::move(result));
  }
  return results;
}

void ApplyVariantToCandidate(const converter::Candidate& source,
                             const CandidateVariant& variant,
                             converter::Candidate* dest) {
  dest->key = source.key;
  dest->content_key = source.content_key;
  dest->cost = source.cost;
  dest->wcost = source.wcost;
  dest->structure_cost = source.structure_cost;
  dest->lid = source.lid;
  dest->rid = source.rid;
  dest->attributes = source.attributes;
  dest->consumed_key_size = source.consumed_key_size;
  dest->converted_segment_count = source.converted_segment_count;
  dest->command = source.command;
  dest->category = source.category;
  dest->value = variant.value;
  dest->content_value = variant.content_value;
  dest->inner_segment_boundary = variant.inner_segment_boundary;
}

// Rewrites the candidate at |candidate_index| to the first variant and inserts
// the remaining variants after it.
bool ExpandCandidateVariants(converter::Segment* seg, int candidate_index,
                             const std::vector<CandidateVariant>& variants) {
  if (variants.empty()) {
    return false;
  }

  converter::Candidate* original = seg->mutable_candidate(candidate_index);
  if (!original) {
    return false;
  }

  absl::flat_hash_set<std::string> existing_values;
  for (size_t i = 0; i < seg->candidates_size(); ++i) {
    existing_values.insert(seg->candidate(i).value);
  }

  const CandidateVariant& primary_variant = variants.front();
  bool modified = original->value != primary_variant.value;
  original->value = primary_variant.value;
  original->content_value = primary_variant.content_value;
  original->inner_segment_boundary = primary_variant.inner_segment_boundary;

  for (size_t i = 1; i < variants.size(); ++i) {
    const CandidateVariant& variant = variants[i];
    if (existing_values.contains(variant.value)) {
      continue;
    }

    converter::Candidate* new_candidate =
        seg->insert_candidate(candidate_index + 1);
    if (!new_candidate) {
      continue;
    }
    ApplyVariantToCandidate(*original, variant, new_candidate);
    modified = true;
  }

  return modified;
}
#endif  // MOZC_USE_OPENCC

}  // namespace

int OpenccRewriter::capability(const ConversionRequest& request) const {
  if (!request.config().use_traditional_kanji()) {
    return RewriterInterface::NOT_AVAILABLE;
  }
  return RewriterInterface::ALL;
}

bool OpenccRewriter::Rewrite(const ConversionRequest& request,
                              Segments* segments) const {
  if (!request.config().use_traditional_kanji()) {
    return false;
  }

#ifdef MOZC_USE_OPENCC
  const opencc::ConverterPtr converter = GetConverter();
  if (!converter) {
    return false;
  }

  bool modified = false;
  for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
    converter::Segment* seg = segments->mutable_conversion_segment(i);
    if (!seg) continue;

    for (int j = static_cast<int>(seg->candidates_size()) - 1; j >= 0; --j) {
      converter::Candidate* cand = seg->mutable_candidate(j);
      if (!cand) continue;

      // Convert the content parts only. The functional parts are kana and are
      // carried over unconverted. Converting the whole value and then
      // content_value in a second pass ran the tables twice over
      // already-converted text, and left content_value holding the full
      // surface rather than just the content part.
      if (cand->value.empty()) {
        continue;
      }
      const std::vector<CandidateVariant> variants =
          ConvertCandidate(converter, *cand);
      modified |= ExpandCandidateVariants(seg, j, variants);
    }
  }
  return modified;
#else   // !MOZC_USE_OPENCC
  (void)request;
  (void)segments;
  return false;
#endif  // MOZC_USE_OPENCC
}

}  // namespace mozc
