#ifndef MOZC_SYNC_SYNC_POLL_H_
#define MOZC_SYNC_SYNC_POLL_H_

#include <string>

#include "absl/status/statusor.h"
#include "protocol/commands.pb.h"
#include "sync/sync_config.h"

namespace mozc {
namespace sync {

enum class IntervalSyncDecision {
  kSkip,
  kSync,
  kBaselineOnly,
};

// SHA-256 (hex) of a file's contents; empty if the file is missing.
absl::StatusOr<std::string> Sha256HexFile(absl::string_view path);

// Fingerprint local profile files included in sync (respects sync toggles).
absl::StatusOr<std::string> LocalSyncDataSha256(
    const commands::UserSyncConfig& config);

absl::StatusOr<SyncFingerprintSnapshot> CaptureSyncFingerprints(
    const commands::UserSyncConfig& config);

IntervalSyncDecision EvaluateIntervalSync(
    const SyncFingerprintSnapshot& baseline,
    const SyncFingerprintSnapshot& current);

}  // namespace sync
}  // namespace mozc

#endif  // MOZC_SYNC_SYNC_POLL_H_
