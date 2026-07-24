#include "base/marina_update_throttle.h"

#include <string>

#include "absl/strings/numbers.h"
#include "absl/strings/strip.h"
#include "absl/time/clock.h"
#include "base/file_util.h"
#include "base/system_util.h"

namespace mozc {
namespace {

constexpr char kStampFileName[] = "marina_auto_update_check_unix_sec";

std::string StampPath() {
  return FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(),
                            kStampFileName);
}

}  // namespace

bool ShouldRunMarinaAutoUpdateCheck() {
  const auto contents = FileUtil::GetContents(StampPath(), std::ios::in);
  if (!contents.ok()) {
    return true;
  }
  int64_t last_sec = 0;
  if (!absl::SimpleAtoi(absl::StripAsciiWhitespace(*contents), &last_sec)) {
    return true;
  }
  const absl::Time last = absl::FromUnixSeconds(last_sec);
  return absl::Now() - last >= kMarinaAutoUpdateCheckInterval;
}

void MarkMarinaAutoUpdateCheckRan() {
  const std::string body = std::to_string(absl::ToUnixSeconds(absl::Now()));
  FileUtil::SetContents(StampPath(), body).IgnoreError();
}

}  // namespace mozc
