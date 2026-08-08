// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0
//
// marinaMoji: minimal, dependency-free metadata for optional compiled-in
// supplementary dictionary packs -- just enough (id + default-enabled) for
// ConversionOptions to carry a per-request enabled-packs bitmask without
// request/ depending on dictionary/ (request/ is the lower layer;
// dictionary/dictionary_impl.h already depends on request/options.h, so the
// reverse edge would be circular). The DataManager-backed accessor for each
// pack's compiled blob lives in dictionary/dictionary_pack_manifest.h, which
// depends on this file rather than duplicating the id list.

#ifndef MOZC_REQUEST_DICTIONARY_PACK_IDS_H_
#define MOZC_REQUEST_DICTIONARY_PACK_IDS_H_

#include "absl/strings/string_view.h"

namespace mozc {

struct DictionaryPackId {
  // Stable id, persisted in Config::enabled_dictionary_packs -- never rename
  // once shipped.
  absl::string_view id;

  // Human-readable label for the Settings checkbox (gui/config_dialog).
  absl::string_view display_name;

  // Whether this pack is enabled when the user has never touched dictionary
  // pack settings (Config::dictionary_packs_configured() == false). See
  // protocol/config.proto.
  bool default_enabled;
};

// Fixed, build-time-known list. Index into this array is also the bit
// position ConversionOptions::enabled_dictionary_packs_mask uses for this
// pack -- keep it small (currently 1 entry; expected to stay well under 32).
inline constexpr DictionaryPackId kDictionaryPackIds[] = {
    {
        .id = "experimental",
        .display_name = "Experimental vocabulary (archaic words, era "
                         "names, court titles)",
        .default_enabled = true,
    },
};

}  // namespace mozc

#endif  // MOZC_REQUEST_DICTIONARY_PACK_IDS_H_
