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

// Whether the user has already been asked, once, whether marinaMoji should
// install future updates automatically in the background. Consulted so the
// offer is made at most once ever, regardless of the answer.
bool HasOfferedSilentAutoUpdate();

// Records that the silent-auto-update offer was shown, regardless of what
// the user chose.
void MarkOfferedSilentAutoUpdate();

}  // namespace mozc

#endif  // MOZC_BASE_MARINA_UPDATE_THROTTLE_H_
