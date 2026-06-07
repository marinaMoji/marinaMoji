#ifndef MOZC_SYNC_SYNC_UTIL_H_
#define MOZC_SYNC_SYNC_UTIL_H_

#include <string>

namespace mozc {
namespace sync {

// Full path to the marinaMojiSync executable (macOS app bundle binary).
std::string GetSyncProgramPath();

// Spawn marinaMojiSync --now (or --force). Returns false on failure.
bool SpawnSyncNow(bool force = false);

}  // namespace sync
}  // namespace mozc

#endif  // MOZC_SYNC_SYNC_UTIL_H_
