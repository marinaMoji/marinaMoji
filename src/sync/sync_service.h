#ifndef MOZC_SYNC_SYNC_SERVICE_H_
#define MOZC_SYNC_SYNC_SERVICE_H_

#include <functional>
#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "protocol/commands.pb.h"

namespace mozc {
namespace sync {

using SyncProgressCallback =
    std::function<void(const std::string& phase, double progress,
                       const std::string& message)>;

struct PerformSyncOptions {
  commands::UserSyncConfig config;
  absl::string_view passphrase;
  commands::UserSyncConfig::Direction direction =
      commands::UserSyncConfig::BIDIRECTIONAL;
  bool force = false;
};

// Export local data, merge with remote encrypted bundle, write back, import.
absl::StatusOr<commands::UserSyncReport> PerformSync(
    const PerformSyncOptions& options,
    SyncProgressCallback progress = nullptr);

}  // namespace sync
}  // namespace mozc

#endif  // MOZC_SYNC_SYNC_SERVICE_H_
