#ifndef MOZC_MAC_SYNC_OVERLAY_H_
#define MOZC_MAC_SYNC_OVERLAY_H_

namespace mozc {
namespace mac {

// Shows or hides the center-screen "marinaMoji synchronising" overlay.
void SyncOverlaySetVisible(bool visible);

// Brief flash when the user types during sync (rate-limited internally).
void SyncOverlayFlashBlockedInput();

// True while sync.status.json reports state=running.
bool SyncOverlayIsActive();

// Start/stop polling sync.status.json (call from IMK main).
void SyncOverlayStartWatcher();
void SyncOverlayStopWatcher();

}  // namespace mac
}  // namespace mozc

#endif  // MOZC_MAC_SYNC_OVERLAY_H_
