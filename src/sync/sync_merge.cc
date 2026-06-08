#include "sync/sync_merge.h"

#include <algorithm>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/hash/hash.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"

namespace mozc {
namespace sync {
namespace {

std::string DictionaryEntryKey(absl::string_view line) {
  const std::vector<std::string> fields =
      absl::StrSplit(line, '\t', absl::SkipEmpty());
  if (fields.size() < 3) {
    return std::string(line);
  }
  return absl::StrCat(fields[0], "\t", fields[1], "\t", fields.size() >= 3 ? fields[2] : "");
}

std::string HistoryEntryKey(absl::string_view line) {
  const std::vector<std::string> fields = absl::StrSplit(line, '\t');
  if (fields.size() < 2) {
    return std::string(line);
  }
  return absl::StrCat(fields[0], "\t", fields[1]);
}

}  // namespace

absl::Status MergeDictionaryTsv(absl::string_view remote_tsv,
                                absl::string_view local_tsv,
                                std::string* merged_tsv,
                                DictionaryMergeStats* stats) {
  absl::flat_hash_map<std::string, std::string> entries;
  std::vector<std::string> order;

  auto ingest = [&](absl::string_view tsv) {
    for (absl::string_view line : absl::StrSplit(tsv, '\n')) {
      line = absl::StripAsciiWhitespace(line);
      if (line.empty() || absl::StartsWith(line, "#")) {
        continue;
      }
      const std::string key = DictionaryEntryKey(line);
      if (!entries.contains(key)) {
        order.push_back(key);
      }
      entries[key] = std::string(line);
    }
  };

  ingest(local_tsv);
  const size_t before = entries.size();
  ingest(remote_tsv);
  stats->added = static_cast<int>(entries.size() - before);
  stats->skipped = 0;

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
  return absl::OkStatus();
}

absl::Status MergeHistoryTsv(absl::string_view remote_tsv,
                             absl::string_view local_tsv,
                             std::string* merged_tsv,
                             HistoryMergeStats* stats) {
  struct HistEntry {
    std::string key;
    std::string value;
    uint32_t suggestion_freq = 0;
    uint32_t shown_freq = 0;
    uint64_t last_access_time = 0;
  };

  absl::flat_hash_map<std::string, HistEntry> entries;

  auto parse = [&](absl::string_view tsv) {
    for (absl::string_view line : absl::StrSplit(tsv, '\n')) {
      line = absl::StripAsciiWhitespace(line);
      if (line.empty() || absl::StartsWith(line, "#")) {
        continue;
      }
      const std::vector<std::string> fields = absl::StrSplit(line, '\t');
      if (fields.size() < 2) {
        continue;
      }
      HistEntry e;
      e.key = fields[0];
      e.value = fields[1];
      if (fields.size() > 2) {
        uint32_t v = 0;
        if (absl::SimpleAtoi(fields[2], &v)) {
          e.suggestion_freq = v;
        }
      }
      if (fields.size() > 3) {
        uint32_t v = 0;
        if (absl::SimpleAtoi(fields[3], &v)) {
          e.shown_freq = v;
        }
      }
      if (fields.size() > 4) {
        uint64_t v = 0;
        if (absl::SimpleAtoi(fields[4], &v)) {
          e.last_access_time = v;
        }
      }
      const std::string id = HistoryEntryKey(line);
      auto it = entries.find(id);
      if (it == entries.end()) {
        entries[id] = e;
      } else {
        it->second.suggestion_freq += e.suggestion_freq;
        it->second.shown_freq += e.shown_freq;
        it->second.last_access_time =
            std::max(it->second.last_access_time, e.last_access_time);
      }
    }
  };

  parse(local_tsv);
  parse(remote_tsv);
  stats->merged = static_cast<int>(entries.size());

  std::string out = "# marinaMoji sync history\n";
  for (const auto& [id, e] : entries) {
    out += absl::StrCat(e.key, "\t", e.value, "\t", e.suggestion_freq, "\t",
                       e.shown_freq, "\t", e.last_access_time, "\n");
  }
  *merged_tsv = std::move(out);
  return absl::OkStatus();
}

}  // namespace sync
}  // namespace mozc
