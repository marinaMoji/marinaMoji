#ifndef MOZC_BASE_MARINA_UPDATE_THROTTLE_H_
#define MOZC_BASE_MARINA_UPDATE_THROTTLE_H_

#include "absl/time/time.h"

namespace mozc {

// Minimum gap between automatic (non-manual) GitHub update checks.
inline constexpr absl::Duration kMarinaAutoUpdateCheckInterval = absl::Hours(24);

// Returns true if an automatic check should run now (interval elapsed or never
// checked). Persists the last-check time under the user profile directory.
bool ShouldRunMarinaAutoUpdateCheck();

// Records that an automatic check was attempted (success or "up to date").
void MarkMarinaAutoUpdateCheckRan();

}  // namespace mozc

#endif  // MOZC_BASE_MARINA_UPDATE_THROTTLE_H_
