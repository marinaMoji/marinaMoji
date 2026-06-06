#ifndef MOZC_SYNC_SYNC_MERGE_H_
#define MOZC_SYNC_SYNC_MERGE_H_

#include <string>

#include "absl/status/status.h"
#include "protocol/config.pb.h"

namespace mozc {
namespace sync {

struct DictionaryMergeStats {
  int added = 0;
  int skipped = 0;
};

// Merge remote TSV into local user dictionary storage (additive).
absl::Status MergeDictionaryTsv(absl::string_view remote_tsv,
                                absl::string_view local_tsv,
                                std::string* merged_tsv,
                                DictionaryMergeStats* stats);

// Apply whitelisted fields from remote config onto local.
config::Config MergeSettingsConfig(const config::Config& local,
                                   const config::Config& remote);

// Merge history TSV lines; sum frequencies for duplicate key/value pairs.
struct HistoryMergeStats {
  int merged = 0;
};

absl::Status MergeHistoryTsv(absl::string_view remote_tsv,
                             absl::string_view local_tsv,
                             std::string* merged_tsv,
                             HistoryMergeStats* stats);

// Extract syncable settings subset into a Config proto (copies whitelisted fields).
config::Config ExtractSyncSettings(const config::Config& config);

// Apply extracted sync settings onto a full local config.
config::Config ApplySyncSettings(const config::Config& local,
                                 const config::Config& sync_subset);

}  // namespace sync
}  // namespace mozc

#endif  // MOZC_SYNC_SYNC_MERGE_H_
