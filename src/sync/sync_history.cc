#include "sync/sync_history.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "config/character_form_manager.h"
#include "prediction/user_history_storage.h"
#include "sync/sync_character_form.h"

namespace mozc {
namespace sync {
namespace {

using Entry = prediction::UserHistoryStorage::Entry;

// Loads the history file synchronously. The UserHistoryStorage constructor
// schedules an AsyncLoad(); waiting for it first keeps it from finishing
// after our own changes and wiping them (and, since the merge with upstream,
// from resetting CharacterFormManager after MergeStorage()).
//
// CharacterFormManager is a process-wide singleton and the sync daemon runs
// many cycles in one process. Load() returns false without touching it when
// there is no history file, which would leave the forms of an earlier cycle
// in place; clear them so they are neither exported nor saved back.
void LoadHistory(prediction::UserHistoryStorage& storage) {
  storage.Wait();
  if (!storage.Load()) {
    config::CharacterFormManager::GetCharacterFormManager()->ClearHistory();
  }
}

}  // namespace

HistoryExport ExportHistory(int64_t now) {
  HistoryExport out;
  out.history_tsv = std::string(kHistoryHeader);

  prediction::UserHistoryStorage storage;
  LoadHistory(storage);
  storage.ForEach([&](uint64_t /*fp*/, const Entry& entry) {
    // A removed entry is a local tombstone; exporting it would make other
    // devices learn it as a live entry.
    if (entry.removed()) {
      return true;
    }
    absl::StrAppend(&out.history_tsv, entry.key(), "\t", entry.value(), "\t",
                    entry.suggestion_freq(), "\t", entry.shown_freq(), "\t",
                    entry.last_access_time(), "\t", entry.entry_flags(), "\n");
    return true;
  });

  const CharacterFormSyncState stamped = StampCharacterForms(
      config::CharacterFormManager::GetCharacterFormManager()
          ->GetStorageSnapshot(),
      LoadLocalCharacterFormSyncState(), now);
  // Remember the stamps even if this sync fails later, so an unchanged value
  // is not restamped (and made to look newer) next time.
  if (const absl::Status s = SaveLocalCharacterFormSyncState(stamped);
      !s.ok()) {
    LOG(WARNING) << "Cannot save character form sync state: " << s;
  }
  out.character_form_tsv = SerializeCharacterFormTsv(stamped);
  return out;
}

absl::Status ImportHistory(
    absl::string_view history_tsv,
    std::optional<absl::string_view> character_form_tsv) {
  prediction::UserHistoryStorage storage;
  LoadHistory(storage);

  for (absl::string_view line : absl::StrSplit(history_tsv, '\n')) {
    line = absl::StripAsciiWhitespace(line);
    if (line.empty() || absl::StartsWith(line, "#")) {
      continue;
    }
    const std::vector<std::string> fields = absl::StrSplit(line, '\t');
    if (fields.size() < 2) {
      continue;
    }
    Entry entry;
    if (const auto existing = storage.Lookup(fields[0], fields[1])) {
      entry = *existing;
    }
    entry.set_key(fields[0]);
    entry.set_value(fields[1]);
    if (fields.size() > 2) {
      uint32_t v = 0;
      if (absl::SimpleAtoi(fields[2], &v)) {
        entry.set_suggestion_freq(v);
      }
    }
    if (fields.size() > 3) {
      uint32_t v = 0;
      if (absl::SimpleAtoi(fields[3], &v)) {
        entry.set_shown_freq(v);
      }
    }
    if (fields.size() > 4) {
      uint64_t v = 0;
      if (absl::SimpleAtoi(fields[4], &v)) {
        entry.set_last_access_time(v);
      }
    }
    if (fields.size() > 5) {
      uint32_t v = 0;
      if (absl::SimpleAtoi(fields[5], &v)) {
        // Flags only accumulate (UserHistoryPredictor ORs them in).
        entry.set_entry_flags(entry.entry_flags() | v);
      }
    }
    storage.Insert(std::move(entry));
  }

  std::optional<CharacterFormSyncState> forms;
  if (character_form_tsv.has_value()) {
    forms = ParseCharacterFormTsv(*character_form_tsv);
    config::CharacterFormManager::GetCharacterFormManager()->MergeStorage(
        ToCharacterFormHistory(*forms));
  }

  if (!storage.Save()) {
    return absl::InternalError("Failed to save user history");
  }
  if (forms.has_value()) {
    if (absl::Status s = SaveLocalCharacterFormSyncState(*forms); !s.ok()) {
      return s;
    }
  }
  return absl::OkStatus();
}

}  // namespace sync
}  // namespace mozc
