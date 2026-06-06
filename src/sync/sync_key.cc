#include "sync/sync_key.h"

#include <fstream>
#include <string>
#include <vector>

#if !defined(_WIN32)
#include <sys/stat.h>
#endif

#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "base/file_util.h"
#include "base/random.h"
#include "base/system_util.h"

namespace mozc {
namespace {

constexpr char kSyncKeyFile[] = "user://.sync_key";

const char* kWordList[] = {
    "anchor", "amber",  "bridge", "coral",  "delta",  "ember",  "frost",
    "garden", "harbor", "ivory",  "jade",   "kite",   "lotus",  "maple",
    "north",  "ocean",  "pearl",  "quartz", "river",  "stone",  "tide",
    "ultra",  "violet", "willow", "xenon",  "yacht",  "zenith",
};

std::string SyncKeyPath() {
  return FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(), ".sync_key");
}

std::string NormalizeSyncKey(absl::string_view raw) {
  std::string key(raw);
  while (!key.empty() && absl::ascii_isspace(key.front())) {
    key.erase(key.begin());
  }
  while (!key.empty() && absl::ascii_isspace(key.back())) {
    key.pop_back();
  }
  return key;
}

}  // namespace

namespace sync {

std::string GenerateSyncKey() {
  Random random;
  std::string key;
  const int num_words = 6;
  const int word_count = sizeof(kWordList) / sizeof(kWordList[0]);
  for (int i = 0; i < num_words; ++i) {
    if (i > 0) {
      key += '-';
    }
    key += kWordList[random() % word_count];
  }
  return key;
}

absl::Status StoreSyncKey(absl::string_view passphrase) {
  const std::string normalized = NormalizeSyncKey(passphrase);
  if (normalized.empty()) {
    return absl::InvalidArgumentError("Empty sync key");
  }
  const std::string path = SyncKeyPath();
  const std::string tmp = path + ".tmp";
  {
    std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
    if (!ofs) {
      return absl::PermissionDeniedError("Cannot write sync key");
    }
    ofs.write(normalized.data(), normalized.size());
  }
#if !defined(_WIN32)
  chmod(tmp.c_str(), 0600);
#endif
  return FileUtil::AtomicRename(tmp, path);
}

absl::StatusOr<std::string> LoadSyncKey() {
  const std::string path = SyncKeyPath();
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) {
    return absl::NotFoundError("Sync key not set");
  }
  return NormalizeSyncKey(std::string(std::istreambuf_iterator<char>(ifs),
                                      std::istreambuf_iterator<char>()));
}

absl::Status ClearSyncKey() { return FileUtil::Unlink(SyncKeyPath()); }

bool HasStoredSyncKey() { return LoadSyncKey().ok(); }

}  // namespace sync
}  // namespace mozc
