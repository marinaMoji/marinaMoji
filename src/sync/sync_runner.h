#ifndef MOZC_SYNC_SYNC_RUNNER_H_
#define MOZC_SYNC_SYNC_RUNNER_H_

#include "absl/status/statusor.h"
#include "client/client_interface.h"
#include "protocol/commands.pb.h"
#include "sync/sync_service.h"

namespace mozc {
namespace sync {

struct RunSyncOptions {
  bool force = false;
  bool skip_cooldown = false;
  commands::UserSyncConfig::Direction direction =
      commands::UserSyncConfig::BIDIRECTIONAL;
};

// Orchestrates converter lock, file sync, and reload via a separate process.
absl::StatusOr<commands::UserSyncReport> RunSync(
    client::ClientInterface* client, const RunSyncOptions& options);

// Returns true when auto-sync preconditions are met (idle + cooldown).
bool CanAutoSync(client::ClientInterface* client, int cooldown_seconds);

}  // namespace sync
}  // namespace mozc

#endif  // MOZC_SYNC_SYNC_RUNNER_H_
