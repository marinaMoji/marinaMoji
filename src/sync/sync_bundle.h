#ifndef MOZC_SYNC_SYNC_BUNDLE_H_
#define MOZC_SYNC_SYNC_BUNDLE_H_

#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace sync {

using SyncBundleFiles = absl::flat_hash_map<std::string, std::string>;

constexpr absl::string_view kManifestFile = "manifest.txt";
constexpr absl::string_view kSettingsFile = "settings.pb";
constexpr absl::string_view kDictionaryFile = "dictionary.tsv";
constexpr absl::string_view kDictionaryTombstonesFile = "dictionary_tombstones.tsv";
constexpr absl::string_view kHistoryFile = "history.tsv";

// Pack named files into an uncompressed zip archive in memory.
absl::StatusOr<std::string> PackBundle(const SyncBundleFiles& files);

// Unpack zip archive bytes into named files.
absl::StatusOr<SyncBundleFiles> UnpackBundle(absl::string_view zip_data);

}  // namespace sync
}  // namespace mozc

#endif  // MOZC_SYNC_SYNC_BUNDLE_H_
