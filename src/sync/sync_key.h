#ifndef MOZC_SYNC_SYNC_KEY_H_
#define MOZC_SYNC_SYNC_KEY_H_

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace sync {

// Generate a human-copyable sync key (word groups separated by hyphens).
std::string GenerateSyncKey();

// Store passphrase locally (profile dir, mode 0600).
absl::Status StoreSyncKey(absl::string_view passphrase);

// Load stored passphrase; NotFound if unset.
absl::StatusOr<std::string> LoadSyncKey();

// Remove stored passphrase.
absl::Status ClearSyncKey();

bool HasStoredSyncKey();

}  // namespace sync
}  // namespace mozc

#endif  // MOZC_SYNC_SYNC_KEY_H_
