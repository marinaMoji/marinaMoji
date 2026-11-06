// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// Disk cache and optional network fetch for GlyphWiki KAGE data, used when
// prefer_glyphwiki_drawings is on. Local dump is tried first by the caller;
// this layer fills gaps and keeps results under the user profile.

#ifndef MOZC_GUI_CHARACTER_COMPOSER_GLYPHWIKI_CLIENT_H_
#define MOZC_GUI_CHARACTER_COMPOSER_GLYPHWIKI_CLIENT_H_

#include <optional>
#include <string>
#include <string_view>

#include "kage/kage_engine.h"

namespace mozc {
namespace gui {

// Directory for cached GlyphWiki KAGE files (created on demand).
std::string GlyphWikiCacheDirectory();

// Reads cached KAGE data for |name| (e.g. "u4e00"), or nullopt.
std::optional<std::string> LoadGlyphWikiCache(std::string_view name);

// Writes |data| for |name| into the cache. Returns false on I/O failure.
bool StoreGlyphWikiCache(std::string_view name, std::string_view data);

// Parses the "data" field from a GlyphWiki /api/glyph JSON body.
std::optional<std::string> ParseGlyphWikiApiData(std::string_view json);

// True if the `curl` the fetch relies on can be started (checked once per
// process). When false, FetchGlyphWikiKage() always fails.
bool GlyphWikiFetchAvailable();

// Fetches KAGE data for |name| from glyphwiki.org (blocking, short timeout).
// Returns nullopt on network/parse failure. Does not touch the cache.
std::optional<std::string> FetchGlyphWikiKage(std::string_view name);

// Ensures |name| and every component it needs are present in |scratch|
// (typically layered on the bundled buhin). Tries memory/parent, then disk
// cache, then network when |allow_network| is true. Returns false if any
// required glyph remains missing.
bool EnsureGlyphWikiGlyph(std::string_view name, kage::Buhin *scratch,
                          bool allow_network);

}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_CHARACTER_COMPOSER_GLYPHWIKI_CLIENT_H_
