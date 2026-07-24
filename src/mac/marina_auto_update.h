#ifndef MOZC_MAC_MARINA_AUTO_UPDATE_H_
#define MOZC_MAC_MARINA_AUTO_UPDATE_H_

namespace mozc {
namespace mac {

// If auto-check is enabled and the 24h throttle allows it, queries GitHub on a
// background queue and shows an NSAlert when a newer .pkg is available.
// Safe to call from activateServer: on the main thread; no-ops quickly when
// throttled or disabled.
void MaybeScheduleMarinaAutoUpdateCheck(bool include_unstable,
                                        bool auto_check_enabled);

}  // namespace mac
}  // namespace mozc

#endif  // MOZC_MAC_MARINA_AUTO_UPDATE_H_
