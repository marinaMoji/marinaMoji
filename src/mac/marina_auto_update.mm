#import "mac/marina_auto_update.h"

#import <Cocoa/Cocoa.h>
#import <ServiceManagement/ServiceManagement.h>

#include <cctype>
#include <optional>
#include <string>
#include <utility>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "base/file_util.h"
#include "base/marina_curl_fetch.h"
#include "base/marina_github_releases.h"
#include "base/marina_update_throttle.h"
#include "base/system_util.h"
#include "base/version.h"

namespace mozc {
namespace mac {
namespace {

constexpr char kReleasesApiUrl[] =
    "https://api.github.com/repos/marinaMoji/marinaMoji/releases?per_page=50";

struct UpdateOffer {
  std::string tag_name;
  std::string html_url;
  std::string pkg_url;
  std::string pkg_sha256;  // empty if GitHub published no digest.
};

std::optional<UpdateOffer> ProbeForUpdate(bool include_unstable) {
  const auto json = MarinaCurlGet(kReleasesApiUrl);
  if (!json.ok()) {
    return std::nullopt;
  }
  const auto releases = ParseMarinaGitHubReleasesJson(*json);
  const auto newer = SelectNewerMarinaRelease(
      releases, Version::GetProductVersion(), include_unstable);
  if (!newer.has_value()) {
    return std::nullopt;
  }
  UpdateOffer offer;
  offer.tag_name = newer->tag_name;
  offer.html_url = newer->html_url;
  const std::string arch_token = MarinaHostMacPkgArchToken();
  if (const auto pkg = FindMarinaPkgDownloadUrl(*newer, arch_token);
      pkg.has_value()) {
    offer.pkg_url = *pkg;
    offer.pkg_sha256 = FindMarinaPkgSha256Digest(*newer, arch_token);
  }
  return offer;
}

bool DownloadAndOpen(const UpdateOffer& offer) {
  std::string safe_tag = offer.tag_name;
  for (char& c : safe_tag) {
    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-' ||
          c == '_')) {
      c = '_';
    }
  }
  const std::string dest = FileUtil::JoinPath(
      SystemUtil::GetUserProfileDirectory(),
      absl::StrCat("marinaMoji-update-", safe_tag, ".pkg"));
  if (!MarinaCurlDownload(offer.pkg_url, dest).ok()) {
    return false;
  }
  if (!offer.pkg_sha256.empty() &&
      MarinaSha256OfFile(dest) != offer.pkg_sha256) {
    LOG(ERROR) << "Downloaded update package digest mismatch for "
               << offer.tag_name;
    FileUtil::UnlinkOrLogError(dest);
    return false;
  }
  return MarinaOpenLocalPath(dest);
}

// Registers the headless update-helper LaunchDaemon with SMAppService, which
// prompts the user for one-time approval in System Settings > General >
// Login Items & Extensions. Only ever called in direct response to the user
// clicking "Enable Automatic Updates" below -- never on their behalf.
bool EnableSilentAutoUpdateHelper() {
  SMAppService* service = [SMAppService
      daemonServiceWithPlistName:@"org.mozc.inputmethod.Japanese.UpdateHelper.plist"];
  if (service.status == SMAppServiceStatusEnabled) {
    return true;
  }
  NSError* error = nil;
  if (![service registerAndReturnError:&error]) {
    LOG(ERROR) << "Silent auto-update helper registration failed: "
               << [[error localizedDescription] UTF8String];
    return false;
  }
  return true;
}

// Offered at most once, ever, right after the user accepts an update -- a
// moment they have just demonstrated they want marinaMoji to stay current.
// A "yes" here is the only consent EnableSilentAutoUpdateHelper ever acts on;
// declining (or never being asked again) leaves updates exactly as
// interactive as they are today.
void MaybeOfferSilentAutoUpdate() {
  if (HasOfferedSilentAutoUpdate()) {
    return;
  }
  MarkOfferedSilentAutoUpdate();

  NSAlert* alert = [[NSAlert alloc] init];
  alert.messageText = @"Install future updates automatically?";
  alert.informativeText =
      @"marinaMoji can check for and install future updates in the "
      "background from now on, with no further prompts.\n\n"
      "This asks for a one-time approval in System Settings.";
  [alert addButtonWithTitle:@"Enable Automatic Updates"];
  [alert addButtonWithTitle:@"Not Now"];
  const NSModalResponse response = [alert runModal];
  if (response != NSAlertFirstButtonReturn) {
    return;
  }
  if (!EnableSilentAutoUpdateHelper()) {
    NSAlert* err = [[NSAlert alloc] init];
    err.messageText = @"Couldn't enable automatic updates";
    err.informativeText =
        @"Something went wrong registering the background updater. You can "
        "try again later from System Settings > General > Login Items & "
        "Extensions.";
    [err runModal];
  }
}

void PresentUpdateOfferOnMainThread(UpdateOffer offer) {
  NSAlert* alert = [[NSAlert alloc] init];
  alert.messageText = @"marinaMoji update available";
  alert.informativeText = [NSString
      stringWithFormat:@"A newer release is available: %s\n\n"
                        "Download the notarized installer and open it now?",
                       offer.tag_name.c_str()];
  if (!offer.pkg_url.empty()) {
    [alert addButtonWithTitle:@"Download & Install…"];
  }
  [alert addButtonWithTitle:@"Open release page"];
  [alert addButtonWithTitle:@"Later"];

  const NSModalResponse response = [alert runModal];
  if (!offer.pkg_url.empty() && response == NSAlertFirstButtonReturn) {
    if (!DownloadAndOpen(offer)) {
      NSAlert* err = [[NSAlert alloc] init];
      err.messageText = @"Update download failed";
      err.informativeText =
          @"Could not download or open the installer. Try again from "
          "Preferences → Misc → Check for updates…";
      [err runModal];
      return;
    }
    MaybeOfferSilentAutoUpdate();
    return;
  }

  const bool open_page =
      offer.pkg_url.empty()
          ? (response == NSAlertFirstButtonReturn)
          : (response == NSAlertSecondButtonReturn);
  if (open_page && !offer.html_url.empty()) {
    NSURL* url =
        [NSURL URLWithString:[NSString stringWithUTF8String:offer.html_url
                                                                .c_str()]];
    if (url != nil) {
      [[NSWorkspace sharedWorkspace] openURL:url];
    }
  }
}

}  // namespace

void MaybeScheduleMarinaAutoUpdateCheck(bool include_unstable,
                                        bool auto_check_enabled) {
  if (!auto_check_enabled || !ShouldRunMarinaAutoUpdateCheck()) {
    return;
  }
  // Record immediately so concurrent activateServer: calls do not fan out.
  MarkMarinaAutoUpdateCheckRan();

  dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
    const auto offer = ProbeForUpdate(include_unstable);
    if (!offer.has_value()) {
      return;
    }
    UpdateOffer copy = *offer;
    dispatch_async(dispatch_get_main_queue(), ^{
      PresentUpdateOfferOnMainThread(std::move(copy));
    });
  });
}

}  // namespace mac
}  // namespace mozc
