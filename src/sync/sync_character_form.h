#ifndef MOZC_SYNC_SYNC_CHARACTER_FORM_H_
#define MOZC_SYNC_SYNC_CHARACTER_FORM_H_

// Sync of CharacterFormManager's learned forms (the last half/full-width form
// chosen per normalized character, and the last number style), which upstream
// keeps in UserHistory.character_form_history.
//
// Bundle file `character_forms.tsv`, one record per line, '#' lines ignored:
//
//   form    <UCS2 hex>  <Config::CharacterForm>  <updated_at unix seconds>
//   number  <Config::CharacterForm>  <NumberString::Style>  <updated_at>
//
// The proto carries no timestamps, so this device stamps each value with the
// time a sync first saw it (StampCharacterForms) and remembers the stamps in
// a local sidecar holding the last merged state. Merge rule
// (MergeCharacterForms): per character, and for the number style, the value
// with the newer stamp wins; on a tie the local value is kept. Clearing the
// history is not propagated, the same as for history entries.

#include <cstdint>
#include <optional>
#include <string>

#include "absl/container/btree_map.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "prediction/user_history_predictor.pb.h"

namespace mozc {
namespace sync {

using CharacterFormHistory =
    ::mozc::user_history_predictor::UserHistory::CharacterFormHistory;

inline constexpr absl::string_view kCharacterFormHeader =
    "# marinaMoji sync character forms\n";

struct StampedCharacterForm {
  uint32_t form = 0;
  int64_t updated_at = 0;

  friend bool operator==(const StampedCharacterForm&,
                         const StampedCharacterForm&) = default;
};

struct StampedNumberStyle {
  uint32_t form = 0;
  uint32_t style = 0;
  int64_t updated_at = 0;

  friend bool operator==(const StampedNumberStyle&,
                         const StampedNumberStyle&) = default;
};

struct CharacterFormSyncState {
  // Key: normalized UCS2 code unit, as in character_form_map.
  absl::btree_map<uint32_t, StampedCharacterForm> forms;
  std::optional<StampedNumberStyle> number_style;

  friend bool operator==(const CharacterFormSyncState&,
                         const CharacterFormSyncState&) = default;
};

// Skips malformed lines, unknown record types and invalid values.
CharacterFormSyncState ParseCharacterFormTsv(absl::string_view tsv);
std::string SerializeCharacterFormTsv(const CharacterFormSyncState& state);

// Stamps the local history: a value unchanged since `previous` (the last
// merged state) keeps its stamp, a new or changed one gets `now`.
CharacterFormSyncState StampCharacterForms(
    const CharacterFormHistory& local, const CharacterFormSyncState& previous,
    int64_t now);

struct CharacterFormMergeStats {
  // Values taken from the remote side that differ from the local value.
  int from_remote = 0;
};

CharacterFormSyncState MergeCharacterForms(const CharacterFormSyncState& remote,
                                           const CharacterFormSyncState& local,
                                           CharacterFormMergeStats* stats);

CharacterFormHistory ToCharacterFormHistory(
    const CharacterFormSyncState& state);

// The sidecar in the user profile directory holding the last merged state.
// A missing or unreadable file yields an empty state, which only means every
// local value is treated as new on the next export.
CharacterFormSyncState LoadLocalCharacterFormSyncState();
absl::Status SaveLocalCharacterFormSyncState(
    const CharacterFormSyncState& state);

}  // namespace sync
}  // namespace mozc

#endif  // MOZC_SYNC_SYNC_CHARACTER_FORM_H_
