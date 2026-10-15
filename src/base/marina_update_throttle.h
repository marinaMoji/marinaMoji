#ifndef MOZC_BASE_MARINA_UPDATE_THROTTLE_H_
#define MOZC_BASE_MARINA_UPDATE_THROTTLE_H_

#include <string_view>

#include "absl/time/time.h"

namespace mozc {

// Minimum gap between automatic (non-manual) GitHub update checks. Checking is
// cheap (one small request), so this is short enough for a new release to reach
// users within hours. How often a user is *bothered* is a separate setting, see
// kMarinaUpdateOfferInterval.
inline constexpr absl::Duration kMarinaAutoUpdateCheckInterval = absl::Hours(6);

// Minimum gap before the same release is offered again after it was shown
// (whatever the user answered, e.g. "Later"). A newer release is never held
// back by this: it is offered at the next check.
inline constexpr absl::Duration kMarinaUpdateOfferInterval = absl::Hours(24);

// Returns true if an automatic check should run now (interval elapsed or never
// checked). Persists the last-check time under the user profile directory.
bool ShouldRunMarinaAutoUpdateCheck();

// Records that an automatic check was attempted (success or "up to date").
void MarkMarinaAutoUpdateCheckRan();

// Decision logic behind ShouldOfferMarinaUpdate, separated so it can be tested
// without touching the filesystem or the clock. |stamp| is the contents of the
// offer file ("<unix seconds> <tag>"), or empty if there is none.
bool ShouldOfferMarinaUpdateGiven(std::string_view stamp, std::string_view tag,
                                  absl::Time now);

// Returns true if the update |tag| should be shown to the user now: it differs
// from the last release offered, or the last offer was at least
// kMarinaUpdateOfferInterval ago. Manual "Check for updates" never calls this.
bool ShouldOfferMarinaUpdate(std::string_view tag);

// Records that |tag| was offered now.
void MarkMarinaUpdateOffered(std::string_view tag);

// Whether the user has already been asked, once, whether marinaMoji should
// install future updates automatically in the background. Consulted so the
// offer is made at most once ever, regardless of the answer.
bool HasOfferedSilentAutoUpdate();

// Records that the silent-auto-update offer was shown, regardless of what
// the user chose.
void MarkOfferedSilentAutoUpdate();

}  // namespace mozc

#endif  // MOZC_BASE_MARINA_UPDATE_THROTTLE_H_
