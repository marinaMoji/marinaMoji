#include "base/marina_update_throttle.h"

#include <cstdint>
#include <string>
#include <string_view>

#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/strip.h"
#include "absl/time/clock.h"
#include "base/file_util.h"
#include "base/system_util.h"

namespace mozc {
namespace {

constexpr char kStampFileName[] = "marina_auto_update_check_unix_sec";
constexpr char kSilentOfferStampFileName[] =
    "marina_auto_update_silent_offer_shown";
constexpr char kUpdateOfferStampFileName[] = "marina_auto_update_offer";

std::string StampPath() {
  return FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(),
                            kStampFileName);
}

std::string UpdateOfferStampPath() {
  return FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(),
                            kUpdateOfferStampFileName);
}

std::string SilentOfferStampPath() {
  return FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(),
                            kSilentOfferStampFileName);
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

bool ShouldOfferMarinaUpdateGiven(std::string_view stamp, std::string_view tag,
                                  absl::Time now) {
  stamp = absl::StripAsciiWhitespace(stamp);
  const size_t space = stamp.find(' ');
  if (space == std::string_view::npos) {
    return true;  // No record, or unreadable: offer.
  }
  int64_t offered_sec = 0;
  if (!absl::SimpleAtoi(stamp.substr(0, space), &offered_sec)) {
    return true;
  }
  const std::string_view offered_tag =
      absl::StripAsciiWhitespace(stamp.substr(space + 1));
  if (offered_tag != tag) {
    return true;  // A different (newer) release: never delayed.
  }
  return now - absl::FromUnixSeconds(offered_sec) >= kMarinaUpdateOfferInterval;
}

bool ShouldOfferMarinaUpdate(std::string_view tag) {
  const auto contents =
      FileUtil::GetContents(UpdateOfferStampPath(), std::ios::in);
  if (!contents.ok()) {
    return true;
  }
  return ShouldOfferMarinaUpdateGiven(*contents, tag, absl::Now());
}

void MarkMarinaUpdateOffered(std::string_view tag) {
  FileUtil::SetContents(
      UpdateOfferStampPath(),
      absl::StrCat(absl::ToUnixSeconds(absl::Now()), " ", tag, "\n"))
      .IgnoreError();
}

bool HasOfferedSilentAutoUpdate() {
  return FileUtil::FileExists(SilentOfferStampPath()).ok();
}

void MarkOfferedSilentAutoUpdate() {
  FileUtil::SetContents(SilentOfferStampPath(), "1").IgnoreError();
}

}  // namespace mozc
