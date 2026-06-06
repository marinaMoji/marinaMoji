#ifndef MOZC_SYNC_SYNC_CONFIG_H_
#define MOZC_SYNC_SYNC_CONFIG_H_

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "protocol/commands.pb.h"

namespace mozc {
namespace sync {

// Path to sync.conf in the user profile directory.
std::string GetSyncConfigPath();

// Load/save sync settings from sync.conf (JSON sidecar).
absl::StatusOr<commands::UserSyncConfig> LoadSyncConfig();
absl::Status SaveSyncConfig(const commands::UserSyncConfig& config);

// Ensure device_id is set; returns updated config.
commands::UserSyncConfig EnsureDeviceId(commands::UserSyncConfig config);

}  // namespace sync
}  // namespace mozc

#endif  // MOZC_SYNC_SYNC_CONFIG_H_
