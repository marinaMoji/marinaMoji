#ifndef MOZC_SYNC_SYNC_ACTIVITY_H_
#define MOZC_SYNC_SYNC_ACTIVITY_H_

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"

namespace mozc {
namespace sync {

struct SyncActivity {
  absl::Time last_composition_end = absl::InfinitePast();
  absl::Time last_ime_deactivated = absl::InfinitePast();
};

std::string GetSyncActivityPath();

absl::Status WriteSyncActivity(const SyncActivity& activity);
absl::StatusOr<SyncActivity> ReadSyncActivity();

void RecordCompositionEnd();
void RecordImeDeactivated();

// Returns true if cooldown since last composition/IME-off has elapsed.
bool CooldownElapsed(int cooldown_seconds);

}  // namespace sync
}  // namespace mozc

#endif  // MOZC_SYNC_SYNC_ACTIVITY_H_
