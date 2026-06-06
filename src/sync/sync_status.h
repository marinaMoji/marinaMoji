#ifndef MOZC_SYNC_SYNC_STATUS_H_
#define MOZC_SYNC_SYNC_STATUS_H_

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace mozc {
namespace sync {

struct SyncStatus {
  std::string state;  // idle, running, done, error
  std::string phase;
  double progress = 0.0;
  std::string message;
  int64_t updated_at_unix = 0;
};

std::string GetSyncStatusPath();

absl::Status WriteSyncStatus(const SyncStatus& status);
absl::StatusOr<SyncStatus> ReadSyncStatus();

bool IsSyncRunning();

}  // namespace sync
}  // namespace mozc

#endif  // MOZC_SYNC_SYNC_STATUS_H_
