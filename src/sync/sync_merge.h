#ifndef MOZC_SYNC_SYNC_MERGE_H_
#define MOZC_SYNC_SYNC_MERGE_H_

#include <string>

#include "absl/status/status.h"

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

// Merge history TSV lines; sum frequencies for duplicate key/value pairs.
struct HistoryMergeStats {
  int merged = 0;
};

absl::Status MergeHistoryTsv(absl::string_view remote_tsv,
                             absl::string_view local_tsv,
                             std::string* merged_tsv,
                             HistoryMergeStats* stats);

}  // namespace sync
}  // namespace mozc

#endif  // MOZC_SYNC_SYNC_MERGE_H_
