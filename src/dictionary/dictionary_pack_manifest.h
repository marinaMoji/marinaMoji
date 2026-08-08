// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0
//
// marinaMoji: static registry of optional, compiled-in supplementary
// dictionary packs (archaic vocabulary, era/emperor names, court titles,
// and future packs such as the classical-Japanese Kyōgen import). Packs are
// known at Bazel build time, not discovered at runtime -- this is a fixed
// list, not a plugin system. See dictionary/tables_final/ in the companion
// marinaMoji/dictionary repo for how each pack's TSV is generated, and
// protocol/config.proto's enabled_dictionary_packs field for how the user
// enables/disables packs.
//
// The id + default-enabled metadata lives in request/dictionary_pack_ids.h
// (dependency-free, so request/ConversionOptions can carry an enabled-packs
// bitmask without request/ depending on dictionary/). This file pairs those
// ids with their DataManager-backed compiled-blob accessor, for consumers
// that actually build dictionaries (engine/modules.cc).

#ifndef MOZC_DICTIONARY_DICTIONARY_PACK_MANIFEST_H_
#define MOZC_DICTIONARY_DICTIONARY_PACK_MANIFEST_H_

#include "absl/strings/string_view.h"
#include "request/dictionary_pack_ids.h"

namespace mozc {

class DataManager;

namespace dictionary {

// One compiled-in optional dictionary pack, paired with its data accessor.
struct DictionaryPackInfo {
  const DictionaryPackId* id_info;

  // Returns this pack's compiled blob from the data manager, or empty if
  // this data-manager variant doesn't bundle it (DataManager::InitFromReader
  // treats the section as optional).
  absl::string_view (*get_data)(const DataManager& data_manager);
};

absl::string_view GetExperimentalDictionaryData(
    const DataManager& data_manager);

// Same order/indices as request::kDictionaryPackIds -- the bit position in
// ConversionOptions::enabled_dictionary_packs_mask matches the index here.
inline constexpr DictionaryPackInfo kDictionaryPacks[] = {
    {
        .id_info = &kDictionaryPackIds[0],  // "experimental"
        .get_data = &GetExperimentalDictionaryData,
    },
};

}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_DICTIONARY_PACK_MANIFEST_H_
