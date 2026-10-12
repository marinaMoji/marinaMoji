#ifndef MOZC_SYNC_SYNC_HISTORY_H_
#define MOZC_SYNC_SYNC_HISTORY_H_

// Export/import of the local user history (UserHistoryStorage) for sync.
//
// history.tsv columns: key, value, suggestion_freq, shown_freq,
// last_access_time, entry_flags. Older files have only the first five and
// still import (entry_flags = 0); older readers ignore the sixth column.
//
// Loading and saving UserHistoryStorage also loads and saves the process-wide
// CharacterFormManager, so its forms travel in the same Load()/Save() pair
// and go to the separate character_forms.tsv (see sync_character_form.h).

#include <cstdint>
#include <optional>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace sync {

inline constexpr absl::string_view kHistoryHeader =
    "# marinaMoji sync history\n";

struct HistoryExport {
  std::string history_tsv;
  std::string character_form_tsv;
};

// Reads the local history. Character forms are stamped against the local
// sidecar (`now` for new or changed values), and the sidecar is updated.
HistoryExport ExportHistory(int64_t now);

// Adds the entries of `history_tsv` to the local history, keeping the fields
// sync does not carry (bigram links, segment boundaries, removed flag, ...).
// If `character_form_tsv` is given its forms overwrite the local ones (local
// forms it lacks are kept) and, on success, it becomes the sidecar state.
absl::Status ImportHistory(absl::string_view history_tsv,
                           std::optional<absl::string_view> character_form_tsv);

}  // namespace sync
}  // namespace mozc

#endif  // MOZC_SYNC_SYNC_HISTORY_H_
