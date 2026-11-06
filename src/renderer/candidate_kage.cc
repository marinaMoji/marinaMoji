// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "renderer/candidate_kage.h"

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "base/file_util.h"
#include "base/system_util.h"
#include "base/util.h"
#include "composer_ids/ids_table.h"
#include "composer_ids/ids_tree.h"
#include "gui/character_composer/composer_kage.h"
#include "gui/character_composer/glyphwiki_client.h"

namespace mozc {
namespace renderer {
namespace {

// Bundled GlyphWiki stroke table next to mozc_server
// (…/character_composer/kage_glyphs.tsv).
const gui::KageResources* GetCandidateKageResources() {
  static std::once_flag once;
  static std::unique_ptr<gui::KageResources> resources;
  std::call_once(once, [] {
    const std::string dir = FileUtil::JoinPath(
        SystemUtil::GetServerDirectory(), "character_composer");
    resources = gui::KageResources::LoadFromDirectory(dir);
    if (resources == nullptr) {
      LOG(WARNING) << "KAGE glyph table not found under " << dir
                   << "; missing-font candidates stay as tofu";
    }
  });
  return resources.get();
}

// Pull every leaf under |slot| into |scratch| from the disk cache (and
// optionally GlyphWiki). Used so IDS compose can fill gaps the bundled dump
// does not have (ExtB parts, rare strokes).
void EnsureComposerLeavesInBuhin(const composer_ids::ComposerSlot& slot,
                                 kage::Buhin* scratch, bool allow_network) {
  if (slot.kind == composer_ids::ComposerSlot::Kind::kLeaf) {
    const std::string leaf = gui::GlyphWikiNameForCharacter(slot.input);
    if (!leaf.empty()) {
      (void)gui::EnsureGlyphWikiGlyph(leaf, scratch, allow_network);
    }
    return;
  }
  for (const composer_ids::ComposerSlot& part : slot.parts) {
    EnsureComposerLeavesInBuhin(part, scratch, allow_network);
  }
}

}  // namespace

std::optional<std::vector<gui::Contour>> ResolveMissingFontOutlines(
    absl::string_view ch, const gui::KageResources& resources) {
  if (auto outlines = gui::NamedGlyphOutlines(ch, resources);
      outlines.has_value() && !outlines->empty()) {
    return outlines;
  }

  const std::string name = gui::GlyphWikiNameForCharacter(ch);
  kage::Buhin scratch(&resources.glyphs);

  if (!name.empty() &&
      gui::EnsureGlyphWikiGlyph(name, &scratch, /*allow_network=*/false)) {
    if (auto outlines = gui::NamedGlyphOutlinesFromBuhin(name, scratch);
        outlines.has_value() && !outlines->empty()) {
      return outlines;
    }
  }

  if (const composer_ids::IdsTable* table =
          composer_ids::IdsTable::GetDefault()) {
    for (const std::string& ids : table->FindDecompositions(ch)) {
      const auto parsed = composer_ids::ParseIdsToComposerTree(ids);
      if (!parsed.has_value()) {
        continue;
      }
      for (const composer_ids::ComposerSlot& part : parsed->parts) {
        EnsureComposerLeavesInBuhin(part, &scratch, /*allow_network=*/false);
      }
      const std::string data = gui::ComposeKageData(
          parsed->op, parsed->parts, &scratch, &resources.layout,
          &resources.subtrees);
      if (data.empty()) {
        continue;
      }
      // Temporary name so NamedGlyphOutlinesFromBuhin can stroke the layout.
      scratch.Set("mm-candidate", data);
      if (auto outlines =
              gui::NamedGlyphOutlinesFromBuhin("mm-candidate", scratch);
          outlines.has_value() && !outlines->empty()) {
        return outlines;
      }
    }
  }

  if (!name.empty() &&
      gui::EnsureGlyphWikiGlyph(name, &scratch, /*allow_network=*/true)) {
    if (auto outlines = gui::NamedGlyphOutlinesFromBuhin(name, scratch);
        outlines.has_value() && !outlines->empty()) {
      return outlines;
    }
  }
  return std::nullopt;
}

std::optional<std::vector<gui::Contour>> ResolveIdsCompositionOutlines(
    absl::string_view ids, const gui::KageResources& resources) {
  const auto parsed = composer_ids::ParseIdsToComposerTree(ids);
  if (!parsed.has_value()) {
    return std::nullopt;
  }
  auto try_compose = [&](bool allow_network)
      -> std::optional<std::vector<gui::Contour>> {
    kage::Buhin scratch(&resources.glyphs);
    for (const composer_ids::ComposerSlot& part : parsed->parts) {
      EnsureComposerLeavesInBuhin(part, &scratch, allow_network);
    }
    const std::string data = gui::ComposeKageData(
        parsed->op, parsed->parts, &scratch, &resources.layout,
        &resources.subtrees);
    if (data.empty()) {
      return std::nullopt;
    }
    scratch.Set("mm-ids-candidate", data);
    auto outlines =
        gui::NamedGlyphOutlinesFromBuhin("mm-ids-candidate", scratch);
    if (!outlines.has_value() || outlines->empty()) {
      return std::nullopt;
    }
    return outlines;
  };
  if (auto outlines = try_compose(/*allow_network=*/false)) {
    return outlines;
  }
  // Rare components: one GlyphWiki fetch (cached) then compose again.
  return try_compose(/*allow_network=*/true);
}

const std::optional<std::vector<gui::Contour>>* CachedKageOutlines(
    absl::string_view key) {
  if (key.empty()) {
    return nullptr;
  }
  const gui::KageResources* resources = GetCandidateKageResources();
  if (resources == nullptr) {
    return nullptr;
  }
  static absl::Mutex mu;
  static absl::flat_hash_map<std::string,
                             std::optional<std::vector<gui::Contour>>>
      cache;
  absl::MutexLock lock(mu);
  const std::string cache_key(key);
  const auto it = cache.find(cache_key);
  if (it != cache.end()) {
    return &it->second;
  }
  std::optional<std::vector<gui::Contour>> outlines;
  if (Util::CharsLen(key) == 1) {
    outlines = ResolveMissingFontOutlines(cache_key, *resources);
  } else {
    outlines = ResolveIdsCompositionOutlines(cache_key, *resources);
  }
  auto [inserted, _] = cache.emplace(cache_key, std::move(outlines));
  return &inserted->second;
}

bool HasKageOutlineForKey(absl::string_view key) {
  const auto* outlines = CachedKageOutlines(key);
  return outlines != nullptr && outlines->has_value() &&
         !(*outlines)->empty();
}

bool HasKageOutlineForCharacter(absl::string_view ch) {
  return Util::CharsLen(ch) == 1 && HasKageOutlineForKey(ch);
}

}  // namespace renderer
}  // namespace mozc
