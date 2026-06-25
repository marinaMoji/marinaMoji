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
#include <fstream>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "converter/candidate.h"
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

void CopyCandidateFields(const converter::Candidate& source,
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
  dest->inner_segment_boundary = source.inner_segment_boundary;
  dest->command = source.command;
  dest->category = source.category;
}

void ApplyVariantToCandidate(const converter::Candidate& source,
                             const std::string& variant,
                             converter::Candidate* dest) {
  CopyCandidateFields(source, dest);
  if (source.content_value.empty()) {
    dest->content_value.clear();
    dest->value = variant;
    return;
  }
  dest->content_value = variant;
  dest->value = absl::StrCat(variant, source.functional_value());
}

bool ExpandCandidateVariants(
    converter::Segment* seg, int candidate_index,
    const absl::flat_hash_set<std::string>& existing_values,
    const std::vector<std::string>& variants) {
  if (variants.empty()) {
    return false;
  }

  converter::Candidate* original = seg->mutable_candidate(candidate_index);
  if (!original) {
    return false;
  }

  const std::string original_value = original->value;
  const std::string primary_variant = variants.front();
  if (original->content_value.empty()) {
    original->value = primary_variant;
  } else {
    original->content_value = primary_variant;
    original->value = absl::StrCat(primary_variant, original->functional_value());
  }
  bool modified = original->value != original_value;

  absl::flat_hash_set<std::string> inserted;
  inserted.insert(original->value);

  for (size_t i = 1; i < variants.size(); ++i) {
    const std::string& variant = variants[i];
    if (!inserted.insert(variant).second) {
      continue;
    }
    if (existing_values.contains(variant)) {
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

bool ExpandFieldVariants(converter::Segment* seg, int candidate_index,
                         const std::vector<std::string>& variants,
                         bool value_field) {
  if (variants.empty()) {
    return false;
  }

  converter::Candidate* original = seg->mutable_candidate(candidate_index);
  if (!original) {
    return false;
  }

  if (!value_field) {
    if (original->content_value.empty()) {
      return false;
    }
    const std::string original_content = original->content_value;
    original->content_value = variants.front();
    return original->content_value != original_content;
  }

  absl::flat_hash_set<std::string> existing_values;
  for (size_t i = 0; i < seg->candidates_size(); ++i) {
    existing_values.insert(seg->candidate(i).value);
  }

  return ExpandCandidateVariants(seg, candidate_index, existing_values,
                                 variants);
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

      if (!cand->value.empty()) {
        const std::vector<std::string> variants =
            ConvertAllVariants(converter, cand->value);
        if (!variants.empty()) {
          modified |= ExpandFieldVariants(seg, j, variants, /*value_field=*/true);
        }
      }
      if (!cand->content_value.empty()) {
        const std::vector<std::string> variants =
            ConvertAllVariants(converter, cand->content_value);
        if (!variants.empty()) {
          modified |= ExpandFieldVariants(seg, j, variants,
                                          /*value_field=*/false);
        }
      }
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
