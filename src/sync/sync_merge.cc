#include "sync/sync_merge.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/hash/hash.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "protocol/config.pb.h"

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

void AppendLines(absl::string_view text, std::vector<std::string>* lines) {
  for (absl::string_view line : absl::StrSplit(text, '\n')) {
    line = absl::StripAsciiWhitespace(line);
    if (line.empty() || absl::StartsWith(line, "#")) {
      continue;
    }
    lines->push_back(std::string(line));
  }
}

config::Config CopyWhitelistedFields(const config::Config& from) {
  config::Config out;
  out.set_use_traditional_kanji(from.use_traditional_kanji());
  out.set_incognito_mode(from.incognito_mode());
  out.set_history_learning_level(from.history_learning_level());
  out.set_use_dictionary_suggest(from.use_dictionary_suggest());
  out.set_use_history_suggest(from.use_history_suggest());
  out.set_use_auto_conversion(from.use_auto_conversion());
  out.set_use_realtime_conversion(from.use_realtime_conversion());
  out.set_preedit_method(from.preedit_method());
  out.set_session_keymap(from.session_keymap());
  out.set_punctuation_method(from.punctuation_method());
  out.set_symbol_method(from.symbol_method());
  out.set_space_character_form(from.space_character_form());
  out.set_selection_shortcut(from.selection_shortcut());
  return out;
}

}  // namespace

config::Config ExtractSyncSettings(const config::Config& config) {
  return CopyWhitelistedFields(config);
}

config::Config ApplySyncSettings(const config::Config& local,
                                 const config::Config& sync_subset) {
  config::Config out = local;
  out.set_use_traditional_kanji(sync_subset.use_traditional_kanji());
  out.set_incognito_mode(sync_subset.incognito_mode());
  out.set_history_learning_level(sync_subset.history_learning_level());
  out.set_use_dictionary_suggest(sync_subset.use_dictionary_suggest());
  out.set_use_history_suggest(sync_subset.use_history_suggest());
  out.set_use_auto_conversion(sync_subset.use_auto_conversion());
  out.set_use_realtime_conversion(sync_subset.use_realtime_conversion());
  out.set_preedit_method(sync_subset.preedit_method());
  out.set_session_keymap(sync_subset.session_keymap());
  out.set_punctuation_method(sync_subset.punctuation_method());
  out.set_symbol_method(sync_subset.symbol_method());
  out.set_space_character_form(sync_subset.space_character_form());
  out.set_selection_shortcut(sync_subset.selection_shortcut());
  return out;
}

config::Config MergeSettingsConfig(const config::Config& local,
                                   const config::Config& remote) {
  return ApplySyncSettings(local, CopyWhitelistedFields(remote));
}

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
