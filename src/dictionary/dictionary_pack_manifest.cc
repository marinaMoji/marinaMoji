// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0

#include "dictionary/dictionary_pack_manifest.h"

#include "absl/strings/string_view.h"
#include "data_manager/data_manager.h"

namespace mozc {
namespace dictionary {

absl::string_view GetExperimentalDictionaryData(
    const DataManager& data_manager) {
  return data_manager.GetExperimentalDictionaryData();
}

}  // namespace dictionary
}  // namespace mozc
