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
  // Lowercase hex SHA-256 GitHub published for this asset (its "digest"
  // field, with the "sha256:" prefix stripped), or empty if GitHub didn't
  // provide one.
  std::string digest_sha256;
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

// Prefer marinaMoji-<tag>-x64.msi or marinaMoji-<tag>-arm64.msi from assets,
// matching the naming .github/workflows/release.yaml publishes. |arch_token|
// is typically "x64" or "arm64". Unlike FindMarinaPkgDownloadUrl there is no
// "universal" Windows installer, so an unmatched arch simply returns nullopt
// rather than falling back to a mismatched asset.
std::optional<std::string> FindMarinaMsiDownloadUrl(
    const MarinaGitHubRelease& release, absl::string_view arch_token);

// The SHA-256 digest (lowercase hex, no "sha256:" prefix) GitHub published
// for the same asset FindMarinaMsiDownloadUrl(release, arch_token) would
// return, or empty if GitHub didn't provide a digest for it. Callers should
// treat an empty result as "no integrity check available" rather than as an
// error -- GitHub only started publishing asset digests in 2024, so older
// releases won't have one.
std::string FindMarinaMsiSha256Digest(const MarinaGitHubRelease& release,
                                      absl::string_view arch_token);

// "x64" / "arm64" on Windows hosts; empty elsewhere.
std::string MarinaHostWindowsArchToken();

}  // namespace mozc

#endif  // MOZC_BASE_MARINA_GITHUB_RELEASES_H_
