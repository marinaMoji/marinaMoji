#ifndef MOZC_SYNC_SYNC_UTIL_H_
#define MOZC_SYNC_SYNC_UTIL_H_

#include <string>

namespace mozc {
namespace sync {

// Full path to the sync executable (macOS app bundle or Linux mozc_sync).
std::string GetSyncProgramPath();

// Spawn sync --now (or --force). Returns false on failure.
bool SpawnSyncNow(bool force = false);

// CLI hint for timeout/error dialogs (e.g. "mozc_sync --now --force").
std::string GetSyncManualCliHint();

}  // namespace sync
}  // namespace mozc

#endif  // MOZC_SYNC_SYNC_UTIL_H_
