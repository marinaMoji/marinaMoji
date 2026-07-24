#include "base/marina_semver.h"

#include <cctype>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"

namespace mozc {
namespace {

std::string ToLowerCopy(absl::string_view text) {
  std::string out(text);
  for (char& c : out) {
    c = static_cast<char>(absl::ascii_tolower(c));
  }
  return out;
}

}  // namespace

std::string NormalizeMarinaVersionTag(absl::string_view text) {
  text = absl::StripAsciiWhitespace(text);
  if (!text.empty() && (text.front() == 'v' || text.front() == 'V')) {
    text.remove_prefix(1);
  }
  return std::string(absl::StripAsciiWhitespace(text));
}

std::optional<MarinaSemVer> ParseMarinaSemVer(absl::string_view text) {
  const std::string normalized = NormalizeMarinaVersionTag(text);
  if (normalized.empty()) {
    return std::nullopt;
  }

  absl::string_view core = normalized;
  absl::string_view pre;
  const size_t dash = normalized.find('-');
  if (dash != std::string::npos) {
    core = absl::string_view(normalized).substr(0, dash);
    pre = absl::string_view(normalized).substr(dash + 1);
  }

  const std::vector<absl::string_view> parts =
      absl::StrSplit(core, '.', absl::SkipEmpty());
  if (parts.size() < 2 || parts.size() > 3) {
    return std::nullopt;
  }

  MarinaSemVer ver;
  if (!absl::SimpleAtoi(parts[0], &ver.major) ||
      !absl::SimpleAtoi(parts[1], &ver.minor)) {
    return std::nullopt;
  }
  if (parts.size() == 3) {
    if (!absl::SimpleAtoi(parts[2], &ver.patch)) {
      return std::nullopt;
    }
  }

  if (!pre.empty()) {
    // Accept "rc3", "rc.3", "alpha1", "beta.2".
    std::string label;
    std::string number_text;
    for (char c : pre) {
      if (absl::ascii_isdigit(c)) {
        number_text.push_back(c);
      } else if (c == '.' || c == '_') {
        continue;
      } else {
        label.push_back(static_cast<char>(absl::ascii_tolower(c)));
      }
    }
    if (label.empty()) {
      return std::nullopt;
    }
    ver.prerelease_label = std::move(label);
    if (!number_text.empty() &&
        !absl::SimpleAtoi(number_text, &ver.prerelease_number)) {
      return std::nullopt;
    }
  }
  return ver;
}

bool IsMarinaUnstableVersion(absl::string_view text) {
  const auto parsed = ParseMarinaSemVer(text);
  if (!parsed.has_value()) {
    const std::string lower = ToLowerCopy(NormalizeMarinaVersionTag(text));
    return absl::StrContains(lower, "-rc") ||
           absl::StrContains(lower, "-alpha") ||
           absl::StrContains(lower, "-beta");
  }
  return parsed->IsPrerelease();
}

namespace {

int PrereleaseRank(absl::string_view label) {
  if (label == "alpha") {
    return 1;
  }
  if (label == "beta") {
    return 2;
  }
  if (label == "rc") {
    return 3;
  }
  return 0;
}

bool SemVerLess(const MarinaSemVer& lhs, const MarinaSemVer& rhs) {
  if (lhs.major != rhs.major) {
    return lhs.major < rhs.major;
  }
  if (lhs.minor != rhs.minor) {
    return lhs.minor < rhs.minor;
  }
  if (lhs.patch != rhs.patch) {
    return lhs.patch < rhs.patch;
  }
  // Final release > any prerelease of the same numbers.
  if (lhs.IsPrerelease() != rhs.IsPrerelease()) {
    return lhs.IsPrerelease() && !rhs.IsPrerelease();
  }
  if (!lhs.IsPrerelease()) {
    return false;
  }
  const int lhs_rank = PrereleaseRank(lhs.prerelease_label);
  const int rhs_rank = PrereleaseRank(rhs.prerelease_label);
  if (lhs_rank != rhs_rank) {
    return lhs_rank < rhs_rank;
  }
  if (lhs.prerelease_label != rhs.prerelease_label) {
    return lhs.prerelease_label < rhs.prerelease_label;
  }
  return lhs.prerelease_number < rhs.prerelease_number;
}

}  // namespace

bool MarinaSemVerLess(absl::string_view lhs, absl::string_view rhs) {
  const auto a = ParseMarinaSemVer(lhs);
  const auto b = ParseMarinaSemVer(rhs);
  if (!a.has_value() || !b.has_value()) {
    return false;
  }
  return SemVerLess(*a, *b);
}

}  // namespace mozc
