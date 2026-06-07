#ifndef MOZC_UNIX_IBUS_SYNC_OVERLAY_H_
#define MOZC_UNIX_IBUS_SYNC_OVERLAY_H_

namespace mozc {
namespace ibus {

// Center-screen "marinaMoji synchronising" overlay (shown during sync).
void SyncOverlaySetVisible(bool visible);

// Beep and briefly show the overlay when the user types during sync.
void SyncOverlayFlashBlockedInput();

// True while sync.status.json reports state=running (keys are blocked).
bool SyncOverlayIsActive();

// Start/stop polling sync.status.json (call from IBus main).
void SyncOverlayStartWatcher();
void SyncOverlayStopWatcher();

}  // namespace ibus
}  // namespace mozc

#endif  // MOZC_UNIX_IBUS_SYNC_OVERLAY_H_
