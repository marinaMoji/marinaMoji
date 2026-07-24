#ifndef MOZC_BASE_MARINA_GITHUB_RELEASES_H_
#define MOZC_BASE_MARINA_GITHUB_RELEASES_H_

#include <optional>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"

namespace mozc {

struct MarinaGitHubRelease {
  std::string tag_name;
  std::string html_url;
  bool prerelease = false;
  bool draft = false;
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

}  // namespace mozc

#endif  // MOZC_BASE_MARINA_GITHUB_RELEASES_H_
