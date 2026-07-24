#ifndef MOZC_BASE_MARINA_SEMVER_H_
#define MOZC_BASE_MARINA_SEMVER_H_

#include <optional>
#include <string>

#include "absl/strings/string_view.h"

namespace mozc {

// Parsed marinaMoji product version: MAJOR.MINOR.PATCH[-PRERELEASE].
// Examples: "0.0.2", "v0.0.1-rc3", "0.0.2-rc1".
struct MarinaSemVer {
  int major = 0;
  int minor = 0;
  int patch = 0;
  // Empty when this is a final release of major.minor.patch.
  // Otherwise a lowercase label such as "rc", "alpha", or "beta".
  std::string prerelease_label;
  int prerelease_number = 0;

  bool IsPrerelease() const { return !prerelease_label.empty(); }
};

// Returns nullopt when the string is not a recognizable product version.
std::optional<MarinaSemVer> ParseMarinaSemVer(absl::string_view text);

// True when the tag/version denotes an unstable channel build (rc/alpha/beta).
bool IsMarinaUnstableVersion(absl::string_view text);

// Lexicographic order for update checks: returns true iff lhs < rhs.
// Final releases sort after prereleases of the same MAJOR.MINOR.PATCH
// (e.g. 0.0.2 > 0.0.2-rc1). Unknown/unparseable inputs compare as not-less.
bool MarinaSemVerLess(absl::string_view lhs, absl::string_view rhs);

// Strip a leading 'v' / 'V' if present.
std::string NormalizeMarinaVersionTag(absl::string_view text);

}  // namespace mozc

#endif  // MOZC_BASE_MARINA_SEMVER_H_
