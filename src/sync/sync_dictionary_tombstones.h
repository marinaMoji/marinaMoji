#ifndef MOZC_SYNC_SYNC_DICTIONARY_TOMBSTONES_H_
#define MOZC_SYNC_SYNC_DICTIONARY_TOMBSTONES_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "protocol/user_dictionary_storage.pb.h"
#include "sync/sync_merge.h"

namespace mozc {
namespace sync {

constexpr absl::string_view kDictionaryTombstonesHeader =
    "# marinaMoji sync dictionary tombstones\n";

// Tombstones older than this are dropped during compaction after a successful
// merge when the entry is absent from the merged live dictionary.
constexpr int kDictionaryTombstoneRetentionDays = 90;

struct DictionaryTombstone {
  std::string reading;
  std::string surface;
  std::string pos;
  std::string locale;
  uint64_t deleted_at_unix = 0;
  std::string device_id;

  std::string SyncKey() const;
  std::string ToTsvLine() const;
};

struct DictionaryTombstoneMergeStats {
  int merged = 0;
  int compacted = 0;
  int removed_from_dictionary = 0;
};

// Stable sync key for a dictionary row or proto entry (reading, surface, pos,
// locale).
std::string DictionaryEntrySyncKey(
    absl::string_view reading, absl::string_view surface,
    absl::string_view pos, absl::string_view locale = "");

std::string DictionaryEntrySyncKey(
    const user_dictionary::UserDictionary::Entry& entry);

std::string DictionaryLineSyncKey(absl::string_view dictionary_line);

absl::flat_hash_set<std::string> CollectDictionarySyncKeysFromTsv(
    absl::string_view dictionary_tsv);

// Parse tombstone TSV (comments and blank lines ignored).
std::vector<DictionaryTombstone> ParseDictionaryTombstonesTsv(
    absl::string_view tsv);

std::string SerializeDictionaryTombstonesTsv(
    const std::vector<DictionaryTombstone>& tombstones);

// LWW merge by deleted_at_unix; ties broken lexicographically by device_id.
std::vector<DictionaryTombstone> MergeDictionaryTombstones(
    const std::vector<DictionaryTombstone>& local,
    const std::vector<DictionaryTombstone>& remote);

// Drop tombstones for keys present in live_keys, and tombstones older than
// retention when the key is not in live_keys.
std::vector<DictionaryTombstone> CompactDictionaryTombstones(
    const std::vector<DictionaryTombstone>& tombstones,
    const absl::flat_hash_set<std::string>& live_keys, absl::Time now,
    int retention_days = kDictionaryTombstoneRetentionDays);

// Union dictionary rows, then apply tombstones: remove a key from the union
// when it is missing from one side's live export and a tombstone exists for
// that key (re-add on one device wins).
absl::Status MergeDictionaryWithTombstones(
    absl::string_view remote_tsv, absl::string_view local_tsv,
    const std::vector<DictionaryTombstone>& remote_tombstones,
    const std::vector<DictionaryTombstone>& local_tombstones,
    absl::string_view local_device_id, std::string* merged_tsv,
    std::vector<DictionaryTombstone>* merged_tombstones,
    DictionaryMergeStats* dict_stats, DictionaryTombstoneMergeStats* tomb_stats);

// Profile-local tombstone log (written when Dictionary Tool deletes words).
std::string GetLocalDictionaryTombstonesPath();

absl::StatusOr<std::vector<DictionaryTombstone>> LoadLocalDictionaryTombstones();

absl::Status SaveLocalDictionaryTombstones(
    const std::vector<DictionaryTombstone>& tombstones);

// Append or replace-by-key (newer deleted_at) tombstones, then save locally.
absl::Status AppendLocalDictionaryTombstones(
    const std::vector<DictionaryTombstone>& additions);

// Record tombstones for entries removed between previous and current dictionary
// contents.
absl::Status RecordDictionaryEntryRemovals(
    const user_dictionary::UserDictionary& previous,
    const user_dictionary::UserDictionary& current,
    absl::string_view device_id, absl::Time deleted_at);

// Export tombstones for the bundle: local log entries whose keys are not in the
// live dictionary export.
std::string ExportDictionaryTombstonesTsv(
    const std::vector<DictionaryTombstone>& local_tombstones,
    absl::string_view live_dictionary_tsv);

}  // namespace sync
}  // namespace mozc

#endif  // MOZC_SYNC_SYNC_DICTIONARY_TOMBSTONES_H_
