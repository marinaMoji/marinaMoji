#include "sync/sync_dictionary_tombstones.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "absl/time/clock.h"
#include "base/file_util.h"
#include "base/system_util.h"
#include "dictionary/user_pos.h"

namespace mozc {
namespace sync {

absl::flat_hash_set<std::string> CollectDictionarySyncKeysFromTsv(
    absl::string_view tsv) {
  absl::flat_hash_set<std::string> keys;
  for (absl::string_view line : absl::StrSplit(tsv, '\n')) {
    line = absl::StripAsciiWhitespace(line);
    if (line.empty() || absl::StartsWith(line, "#")) {
      continue;
    }
    keys.insert(DictionaryLineSyncKey(line));
  }
  return keys;
}

namespace {

constexpr absl::string_view kLocalTombstonesFile =
    "dictionary_tombstones.local.tsv";

absl::flat_hash_set<std::string> CollectDictionaryLineKeys(absl::string_view tsv) {
  return CollectDictionarySyncKeysFromTsv(tsv);
}

absl::flat_hash_map<std::string, std::string> ParseDictionaryLines(
    absl::string_view tsv, std::vector<std::string>* order) {
  absl::flat_hash_map<std::string, std::string> entries;
  for (absl::string_view line : absl::StrSplit(tsv, '\n')) {
    line = absl::StripAsciiWhitespace(line);
    if (line.empty() || absl::StartsWith(line, "#")) {
      continue;
    }
    const std::string key = DictionaryLineSyncKey(line);
    if (!entries.contains(key)) {
      order->push_back(key);
    }
    entries[key] = std::string(line);
  }
  return entries;
}

bool TombstoneWinsOver(const DictionaryTombstone& a,
                       const DictionaryTombstone& b) {
  if (a.deleted_at_unix != b.deleted_at_unix) {
    return a.deleted_at_unix > b.deleted_at_unix;
  }
  return a.device_id > b.device_id;
}

}  // namespace

std::string DictionaryEntrySyncKey(absl::string_view reading,
                                   absl::string_view surface,
                                   absl::string_view pos,
                                   absl::string_view locale) {
  return absl::StrCat(reading, "\t", surface, "\t", pos, "\t", locale);
}

std::string DictionaryEntrySyncKey(
    const user_dictionary::UserDictionary::Entry& entry) {
  return DictionaryEntrySyncKey(
      entry.key(), entry.value(),
      dictionary::UserPos::GetStringPosType(entry.pos()), entry.locale());
}

std::string DictionaryLineSyncKey(absl::string_view dictionary_line) {
  const std::vector<std::string> fields =
      absl::StrSplit(dictionary_line, '\t', absl::AllowEmpty());
  if (fields.size() < 3) {
    return std::string(dictionary_line);
  }
  std::string locale;
  if (fields.size() >= 5) {
    locale = fields[4];
  }
  return DictionaryEntrySyncKey(fields[0], fields[1], fields[2], locale);
}

std::string DictionaryTombstone::SyncKey() const {
  return DictionaryEntrySyncKey(reading, surface, pos, locale);
}

std::string DictionaryTombstone::ToTsvLine() const {
  return absl::StrCat(reading, "\t", surface, "\t", pos, "\t", locale, "\t",
                      deleted_at_unix, "\t", device_id);
}

std::vector<DictionaryTombstone> ParseDictionaryTombstonesTsv(
    absl::string_view tsv) {
  std::vector<DictionaryTombstone> out;
  for (absl::string_view line : absl::StrSplit(tsv, '\n')) {
    line = absl::StripAsciiWhitespace(line);
    if (line.empty() || absl::StartsWith(line, "#")) {
      continue;
    }
    const std::vector<std::string> fields =
        absl::StrSplit(line, '\t', absl::AllowEmpty());
    if (fields.size() < 6) {
      continue;
    }
    DictionaryTombstone t;
    t.reading = fields[0];
    t.surface = fields[1];
    t.pos = fields[2];
    t.locale = fields[3];
    if (!absl::SimpleAtoi(fields[4], &t.deleted_at_unix)) {
      continue;
    }
    t.device_id = fields[5];
    out.push_back(std::move(t));
  }
  return out;
}

std::string SerializeDictionaryTombstonesTsv(
    const std::vector<DictionaryTombstone>& tombstones) {
  std::string out = std::string(kDictionaryTombstonesHeader);
  for (const DictionaryTombstone& t : tombstones) {
    out += t.ToTsvLine();
    out += '\n';
  }
  return out;
}

std::vector<DictionaryTombstone> MergeDictionaryTombstones(
    const std::vector<DictionaryTombstone>& local,
    const std::vector<DictionaryTombstone>& remote) {
  absl::flat_hash_map<std::string, DictionaryTombstone> merged;
  for (const DictionaryTombstone& t : local) {
    const auto [it, inserted] = merged.emplace(t.SyncKey(), t);
    if (!inserted && TombstoneWinsOver(t, it->second)) {
      it->second = t;
    }
  }
  for (const DictionaryTombstone& t : remote) {
    const auto [it, inserted] = merged.emplace(t.SyncKey(), t);
    if (!inserted && TombstoneWinsOver(t, it->second)) {
      it->second = t;
    }
  }
  std::vector<DictionaryTombstone> out;
  out.reserve(merged.size());
  for (auto& [key, t] : merged) {
    out.push_back(std::move(t));
  }
  std::sort(out.begin(), out.end(),
            [](const DictionaryTombstone& a, const DictionaryTombstone& b) {
              return a.SyncKey() < b.SyncKey();
            });
  return out;
}

std::vector<DictionaryTombstone> CompactDictionaryTombstones(
    const std::vector<DictionaryTombstone>& tombstones,
    const absl::flat_hash_set<std::string>& live_keys, absl::Time now,
    int retention_days) {
  const absl::Time cutoff =
      now - absl::Hours(24 * std::max(1, retention_days));
  const uint64_t cutoff_unix = absl::ToUnixSeconds(cutoff);
  std::vector<DictionaryTombstone> out;
  out.reserve(tombstones.size());
  for (const DictionaryTombstone& t : tombstones) {
    const std::string key = t.SyncKey();
    if (live_keys.contains(key)) {
      continue;
    }
    if (t.deleted_at_unix < cutoff_unix) {
      continue;
    }
    out.push_back(t);
  }
  return out;
}

absl::Status MergeDictionaryWithTombstones(
    absl::string_view remote_tsv, absl::string_view local_tsv,
    const std::vector<DictionaryTombstone>& remote_tombstones,
    const std::vector<DictionaryTombstone>& local_tombstones,
    absl::string_view local_device_id, std::string* merged_tsv,
    std::vector<DictionaryTombstone>* merged_tombstones,
    DictionaryMergeStats* dict_stats, DictionaryTombstoneMergeStats* tomb_stats) {
  if (merged_tsv == nullptr || merged_tombstones == nullptr ||
      dict_stats == nullptr || tomb_stats == nullptr) {
    return absl::InvalidArgumentError("null output argument");
  }

  const absl::flat_hash_set<std::string> local_keys =
      CollectDictionaryLineKeys(local_tsv);
  const absl::flat_hash_set<std::string> remote_keys =
      CollectDictionaryLineKeys(remote_tsv);

  std::vector<std::string> order;
  absl::flat_hash_map<std::string, std::string> entries =
      ParseDictionaryLines(local_tsv, &order);
  const size_t before = entries.size();
  {
    std::vector<std::string> remote_order;
    absl::flat_hash_map<std::string, std::string> remote_entries =
        ParseDictionaryLines(remote_tsv, &remote_order);
    for (const std::string& key : remote_order) {
      if (!entries.contains(key)) {
        order.push_back(key);
      }
      entries[key] = remote_entries[key];
    }
  }
  dict_stats->added = static_cast<int>(entries.size() - before);
  dict_stats->skipped = 0;

  std::vector<DictionaryTombstone> tombstones =
      MergeDictionaryTombstones(local_tombstones, remote_tombstones);
  tomb_stats->merged = static_cast<int>(tombstones.size());

  absl::flat_hash_map<std::string, DictionaryTombstone> tombstone_by_key;
  for (const DictionaryTombstone& tombstone : tombstones) {
    const auto [it, inserted] =
        tombstone_by_key.emplace(tombstone.SyncKey(), tombstone);
    if (!inserted && TombstoneWinsOver(tombstone, it->second)) {
      it->second = tombstone;
    }
  }

  int removed = 0;
  for (const auto& [key, tombstone] : tombstone_by_key) {
    if (!entries.contains(key)) {
      continue;
    }
    const bool in_local = local_keys.contains(key);
    const bool in_remote = remote_keys.contains(key);
    const bool local_deleted = !in_local && in_remote;
    const bool remote_deleted =
        in_local && !in_remote &&
        tombstone.device_id != std::string(local_device_id);
    if (local_deleted || remote_deleted) {
      entries.erase(key);
      ++removed;
    }
  }
  tomb_stats->removed_from_dictionary = removed;

  std::string out = "# marinaMoji sync dictionary\n";
  for (const std::string& key : order) {
    const auto it = entries.find(key);
    if (it != entries.end()) {
      out += it->second;
      out += '\n';
    }
  }
  for (const auto& [key, line] : entries) {
    if (std::find(order.begin(), order.end(), key) == order.end()) {
      out += line;
      out += '\n';
    }
  }
  *merged_tsv = std::move(out);

  const absl::flat_hash_set<std::string> live_keys =
      CollectDictionaryLineKeys(*merged_tsv);
  const int before_compact = static_cast<int>(tombstones.size());
  tombstones = CompactDictionaryTombstones(
      tombstones, live_keys, absl::Now(), kDictionaryTombstoneRetentionDays);
  tomb_stats->compacted = before_compact - static_cast<int>(tombstones.size());
  *merged_tombstones = std::move(tombstones);
  return absl::OkStatus();
}

std::string GetLocalDictionaryTombstonesPath() {
  return FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(),
                            std::string(kLocalTombstonesFile));
}

absl::StatusOr<std::vector<DictionaryTombstone>> LoadLocalDictionaryTombstones() {
  const std::string path = GetLocalDictionaryTombstonesPath();
  if (!FileUtil::FileExists(path).ok()) {
    return std::vector<DictionaryTombstone>{};
  }
  const absl::StatusOr<std::string> data = FileUtil::GetContents(path);
  if (!data.ok()) {
    return absl::InternalError("Failed to read local dictionary tombstones");
  }
  return ParseDictionaryTombstonesTsv(*data);
}

absl::Status SaveLocalDictionaryTombstones(
    const std::vector<DictionaryTombstone>& tombstones) {
  const std::string path = GetLocalDictionaryTombstonesPath();
  return FileUtil::SetContents(path, SerializeDictionaryTombstonesTsv(tombstones));
}

absl::Status AppendLocalDictionaryTombstones(
    const std::vector<DictionaryTombstone>& additions) {
  absl::StatusOr<std::vector<DictionaryTombstone>> existing =
      LoadLocalDictionaryTombstones();
  if (!existing.ok()) {
    return existing.status();
  }
  std::vector<DictionaryTombstone> merged =
      MergeDictionaryTombstones(*existing, additions);
  return SaveLocalDictionaryTombstones(merged);
}

absl::Status RecordDictionaryEntryRemovals(
    const user_dictionary::UserDictionary& previous,
    const user_dictionary::UserDictionary& current,
    absl::string_view device_id, absl::Time deleted_at) {
  absl::flat_hash_set<std::string> current_keys;
  for (const auto& entry : current.entries()) {
    current_keys.insert(DictionaryEntrySyncKey(entry));
  }

  std::vector<DictionaryTombstone> removals;
  for (const auto& entry : previous.entries()) {
    const std::string key = DictionaryEntrySyncKey(entry);
    if (current_keys.contains(key)) {
      continue;
    }
    DictionaryTombstone t;
    t.reading = entry.key();
    t.surface = entry.value();
    t.pos = dictionary::UserPos::GetStringPosType(entry.pos());
    t.locale = entry.locale();
    t.deleted_at_unix = absl::ToUnixSeconds(deleted_at);
    t.device_id = std::string(device_id);
    removals.push_back(std::move(t));
  }
  if (removals.empty()) {
    return absl::OkStatus();
  }
  return AppendLocalDictionaryTombstones(removals);
}

std::string ExportDictionaryTombstonesTsv(
    const std::vector<DictionaryTombstone>& local_tombstones,
    absl::string_view live_dictionary_tsv) {
  const absl::flat_hash_set<std::string> live_keys =
      CollectDictionaryLineKeys(live_dictionary_tsv);
  std::vector<DictionaryTombstone> exported;
  for (const DictionaryTombstone& t : local_tombstones) {
    if (!live_keys.contains(t.SyncKey())) {
      exported.push_back(t);
    }
  }
  return SerializeDictionaryTombstonesTsv(exported);
}

}  // namespace sync
}  // namespace mozc
