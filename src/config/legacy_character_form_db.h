#ifndef MOZC_CONFIG_LEGACY_CHARACTER_FORM_DB_H_
#define MOZC_CONFIG_LEGACY_CHARACTER_FORM_DB_H_

// marinaMoji: one-time migration of the LruStorage-backed user://cform.db.
//
// Upstream (b7277bbe3, merged 2026-10) moved CharacterFormManager's learned
// forms and last number style into UserHistory.character_form_history, but
// does not read the old file, so every existing user would silently lose
// their learned forms once. CharacterFormManager::LoadStorage() calls
// TakeLegacyCharacterFormDb() and folds the result in with
// MergeMissingCharacterForms().

#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "prediction/user_history_predictor.pb.h"

namespace mozc {
namespace config {

using LegacyCharacterFormHistory =
    ::mozc::user_history_predictor::UserHistory::CharacterFormHistory;

// Returns the absolute path of user://cform.db.
std::string GetLegacyCharacterFormDbPath();

// Reads the legacy cform.db at `path` and deletes it, so the migration runs
// at most once. Returns std::nullopt when there is no file. An unreadable or
// broken file is deleted too and yields an empty history.
std::optional<LegacyCharacterFormHistory> TakeLegacyCharacterFormDb(
    absl::string_view path);

// Copies into `history` every character form and the number style from
// `legacy` that `history` does not already have; values already in
// `history` were learned after the upgrade and win. Returns true if
// `history` changed.
bool MergeMissingCharacterForms(const LegacyCharacterFormHistory& legacy,
                                LegacyCharacterFormHistory* history);

}  // namespace config
}  // namespace mozc

#endif  // MOZC_CONFIG_LEGACY_CHARACTER_FORM_DB_H_
