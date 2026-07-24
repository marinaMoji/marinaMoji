#ifndef MOZC_BASE_MARINA_GITHUB_RELEASES_H_
#define MOZC_BASE_MARINA_GITHUB_RELEASES_H_

#include <optional>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"

namespace mozc {

struct MarinaGitHubAsset {
  std::string name;
  std::string browser_download_url;
};

struct MarinaGitHubRelease {
  std::string tag_name;
  std::string html_url;
  bool prerelease = false;
  bool draft = false;
  std::vector<MarinaGitHubAsset> assets;
};

// Parses the JSON body of GET /repos/.../releases. Tolerates unknown fields.
std::vector<MarinaGitHubRelease> ParseMarinaGitHubReleasesJson(
    absl::string_view json);

// Among non-draft releases, pick the newest tag strictly greater than
// |current_version|. When |include_unstable| is false, skips GitHub
// prereleases and tags that look like -rc/-alpha/-beta.
std::optional<MarinaGitHubRelease> SelectNewerMarinaRelease(
    absl::Span<const MarinaGitHubRelease> releases,
    absl::string_view current_version, bool include_unstable);

// Prefer marinaMoji-*-arm64.pkg or marinaMoji-*-intel64.pkg from assets.
// |arch_token| is typically "arm64" or "intel64". When set, does not return a
// mismatched single-arch .pkg (may still return a "universal" asset).
std::optional<std::string> FindMarinaPkgDownloadUrl(
    const MarinaGitHubRelease& release, absl::string_view arch_token);

// "arm64" / "intel64" on macOS hosts; empty elsewhere.
std::string MarinaHostMacPkgArchToken();

}  // namespace mozc

#endif  // MOZC_BASE_MARINA_GITHUB_RELEASES_H_
