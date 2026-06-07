#include "sync/sync_util.h"

#include "base/const.h"
#include "base/file_util.h"
#include "base/process.h"
#include "base/system_util.h"
#if defined(__APPLE__)
#include "base/mac/mac_util.h"
#endif

namespace mozc {
namespace sync {

std::string GetSyncProgramPath() {
  const auto try_resources = [](absl::string_view resources) -> std::string {
    if (resources.empty()) {
      return "";
    }
    return FileUtil::JoinPath(
        FileUtil::JoinPath(std::string(resources), kMozcSyncName),
        "Contents/MacOS/" + std::string(kMozcSyncExecutable));
  };

  std::string program = try_resources(SystemUtil::GetServerDirectory());
  if (!program.empty() && FileUtil::FileExists(program).ok()) {
    return program;
  }
#if defined(__APPLE__)
  program = try_resources(MacUtil::GetServerDirectory());
  if (!program.empty() && FileUtil::FileExists(program).ok()) {
    return program;
  }
#endif
  return "";
}

bool SpawnSyncNow(bool force) {
  const std::string program = GetSyncProgramPath();
  if (program.empty()) {
    return false;
  }
  std::string arg = force ? "--now --force" : "--now";
  size_t pid = 0;
  return Process::SpawnProcess(program, arg, &pid);
}

}  // namespace sync
}  // namespace mozc
