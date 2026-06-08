#ifndef MOZC_SYNC_SYNC_CONFIG_H_
#define MOZC_SYNC_SYNC_CONFIG_H_

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "protocol/commands.pb.h"

namespace mozc {
namespace sync {

struct SyncFingerprintSnapshot {
  std::string remote_bundle_sha256;
  std::string local_data_sha256;
};

// Path to sync.conf in the user profile directory.
std::string GetSyncConfigPath();

// Load/save sync configuration from sync.conf (JSON sidecar).
absl::StatusOr<commands::UserSyncConfig> LoadSyncConfig();
absl::Status SaveSyncConfig(const commands::UserSyncConfig& config);

// Interval-sync SHA-256 baselines stored in the same sync.conf file.
absl::StatusOr<SyncFingerprintSnapshot> LoadSyncBaselines();
absl::Status SaveSyncBaselines(const SyncFingerprintSnapshot& baselines);

// Ensure device_id is set; returns updated config.
commands::UserSyncConfig EnsureDeviceId(commands::UserSyncConfig config);

}  // namespace sync
}  // namespace mozc

#endif  // MOZC_SYNC_SYNC_CONFIG_H_
