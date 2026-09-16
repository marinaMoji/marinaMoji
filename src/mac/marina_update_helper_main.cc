// Headless background daemon for marinaMoji's opt-in silent auto-updater.
//
// This binary is only ever registered as a LaunchDaemon (see
// mac/installer/LaunchDaemons/org.mozc.inputmethod.Japanese.UpdateHelper.plist)
// after the user has explicitly agreed, once, to automatic background updates
// -- see the consent alert in mac/marina_auto_update.mm. Nothing here is ever
// shown to a user, so every step logs its outcome instead of presenting UI.

#include <spawn.h>
#include <sys/wait.h>

#include <cctype>
#include <optional>
#include <string>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/file_util.h"
#include "base/init_mozc.h"
#include "base/marina_curl_fetch.h"
#include "base/marina_github_releases.h"
#include "base/system_util.h"
#include "base/version.h"

extern char** environ;

namespace mozc {
namespace {

constexpr char kReleasesApiUrl[] =
    "https://api.github.com/repos/marinaMoji/marinaMoji/releases?per_page=50";

// Only characters that are always filename-safe survive.
std::string SanitizeForFileName(absl::string_view tag) {
  std::string safe(tag);
  for (char& c : safe) {
    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-' ||
          c == '_')) {
      c = '_';
    }
  }
  return safe;
}

// Runs a binary to completion by absolute path. Returns its exit status, or
// -1 if it could not be spawned or waited for.
int RunCommand(const std::vector<std::string>& argv) {
  std::vector<char*> c_argv;
  c_argv.reserve(argv.size() + 1);
  for (const std::string& arg : argv) {
    c_argv.push_back(const_cast<char*>(arg.c_str()));
  }
  c_argv.push_back(nullptr);

  pid_t pid = 0;
  const int spawn_status = posix_spawn(&pid, argv[0].c_str(), nullptr, nullptr,
                                       c_argv.data(), environ);
  if (spawn_status != 0) {
    LOG(ERROR) << "posix_spawn(" << argv[0] << ") failed: " << spawn_status;
    return -1;
  }
  int wait_status = 0;
  if (waitpid(pid, &wait_status, 0) < 0) {
    LOG(ERROR) << "waitpid for " << argv[0] << " failed";
    return -1;
  }
  if (!WIFEXITED(wait_status)) {
    LOG(ERROR) << argv[0] << " did not exit normally";
    return -1;
  }
  return WEXITSTATUS(wait_status);
}

absl::Status RunSilentUpdateCheck() {
  const std::string current_version = Version::GetProductVersion();
  LOG(INFO) << "marinaMoji update helper: checking for updates (current="
            << current_version << ")";

  const absl::StatusOr<std::string> json = MarinaCurlGet(kReleasesApiUrl);
  if (!json.ok()) {
    return absl::UnavailableError(
        absl::StrCat("could not fetch releases: ", json.status().message()));
  }

  const std::vector<MarinaGitHubRelease> releases =
      ParseMarinaGitHubReleasesJson(*json);
  // Unattended installs never offer prereleases, regardless of the user's
  // interactive-update channel preference.
  const std::optional<MarinaGitHubRelease> newer =
      SelectNewerMarinaRelease(releases, current_version,
                               /*include_unstable=*/false);
  if (!newer.has_value()) {
    LOG(INFO) << "marinaMoji update helper: already up to date";
    return absl::OkStatus();
  }

  const std::string arch_token = MarinaHostMacPkgArchToken();
  const std::optional<std::string> pkg_url =
      FindMarinaPkgDownloadUrl(*newer, arch_token);
  if (!pkg_url.has_value()) {
    return absl::NotFoundError(absl::StrCat(
        "release ", newer->tag_name, " has no macOS installer asset"));
  }

  const std::string dest = FileUtil::JoinPath(
      SystemUtil::GetUserProfileDirectory(),
      absl::StrCat("silent-update-", SanitizeForFileName(newer->tag_name),
                  ".pkg"));
  const FileUnlinker cleanup(dest);
  if (const absl::Status s = MarinaCurlDownload(*pkg_url, dest); !s.ok()) {
    return absl::UnavailableError(
        absl::StrCat("download failed: ", s.message()));
  }

  const std::string expected_digest =
      FindMarinaPkgSha256Digest(*newer, arch_token);
  if (!expected_digest.empty()) {
    const std::string actual_digest = MarinaSha256OfFile(dest);
    if (actual_digest != expected_digest) {
      return absl::DataLossError(absl::StrCat(
          "downloaded package digest mismatch for ", newer->tag_name,
          ": expected ", expected_digest, ", got ",
          actual_digest.empty() ? "<unavailable>" : actual_digest));
    }
    LOG(INFO) << "marinaMoji update helper: verified SHA-256 for "
              << newer->tag_name;
  } else {
    LOG(WARNING) << "marinaMoji update helper: no published digest for "
                 << newer->tag_name
                 << "; installing without a checksum check (macOS still "
                 << "verifies the notarized installer's code signature)";
  }

  LOG(INFO) << "marinaMoji update helper: installing " << newer->tag_name;
  const int exit_code =
      RunCommand({"/usr/sbin/installer", "-pkg", dest, "-target", "/"});
  if (exit_code != 0) {
    return absl::InternalError(
        absl::StrCat("installer exited with status ", exit_code));
  }
  LOG(INFO) << "marinaMoji update helper: installed " << newer->tag_name;
  return absl::OkStatus();
}

}  // namespace
}  // namespace mozc

int main(int argc, char* argv[]) {
  mozc::InitMozc(argv[0], &argc, &argv);
  const absl::Status status = mozc::RunSilentUpdateCheck();
  if (!status.ok()) {
    LOG(ERROR) << "marinaMoji update helper failed: " << status.message();
    return 1;
  }
  return 0;
}
