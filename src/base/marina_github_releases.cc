#include "base/marina_github_releases.h"

#include <cctype>
#include <string>

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "base/marina_semver.h"

namespace mozc {
namespace {

std::optional<std::string> ExtractJsonStringField(absl::string_view object,
                                                  absl::string_view key) {
  const std::string needle = absl::StrCat("\"", key, "\"");
  size_t pos = object.find(needle);
  if (pos == absl::string_view::npos) {
    return std::nullopt;
  }
  pos = object.find(':', pos + needle.size());
  if (pos == absl::string_view::npos) {
    return std::nullopt;
  }
  pos = object.find('"', pos + 1);
  if (pos == absl::string_view::npos) {
    return std::nullopt;
  }
  ++pos;
  std::string out;
  for (; pos < object.size(); ++pos) {
    const char c = object[pos];
    if (c == '\\' && pos + 1 < object.size()) {
      out.push_back(object[pos + 1]);
      ++pos;
      continue;
    }
    if (c == '"') {
      return out;
    }
    out.push_back(c);
  }
  return std::nullopt;
}

std::optional<bool> ExtractJsonBoolField(absl::string_view object,
                                         absl::string_view key) {
  const std::string needle = absl::StrCat("\"", key, "\"");
  size_t pos = object.find(needle);
  if (pos == absl::string_view::npos) {
    return std::nullopt;
  }
  pos = object.find(':', pos + needle.size());
  if (pos == absl::string_view::npos) {
    return std::nullopt;
  }
  ++pos;
  while (pos < object.size() && absl::ascii_isspace(object[pos])) {
    ++pos;
  }
  if (absl::StartsWith(object.substr(pos), "true")) {
    return true;
  }
  if (absl::StartsWith(object.substr(pos), "false")) {
    return false;
  }
  return std::nullopt;
}

}  // namespace

std::vector<MarinaGitHubRelease> ParseMarinaGitHubReleasesJson(
    absl::string_view json) {
  std::vector<MarinaGitHubRelease> releases;
  int depth = 0;
  size_t object_start = absl::string_view::npos;
  for (size_t i = 0; i < json.size(); ++i) {
    const char c = json[i];
    if (c == '{') {
      if (depth == 0) {
        object_start = i;
      }
      ++depth;
    } else if (c == '}') {
      --depth;
      if (depth == 0 && object_start != absl::string_view::npos) {
        const absl::string_view object =
            json.substr(object_start, i - object_start + 1);
        MarinaGitHubRelease release;
        if (const auto tag = ExtractJsonStringField(object, "tag_name");
            tag.has_value()) {
          release.tag_name = *tag;
        } else {
          object_start = absl::string_view::npos;
          continue;
        }
        if (const auto url = ExtractJsonStringField(object, "html_url");
            url.has_value()) {
          release.html_url = *url;
        }
        release.prerelease =
            ExtractJsonBoolField(object, "prerelease").value_or(false);
        release.draft = ExtractJsonBoolField(object, "draft").value_or(false);
        releases.push_back(std::move(release));
        object_start = absl::string_view::npos;
      }
    }
  }
  return releases;
}

std::optional<MarinaGitHubRelease> SelectNewerMarinaRelease(
    absl::Span<const MarinaGitHubRelease> releases,
    absl::string_view current_version, bool include_unstable) {
  const std::string current = NormalizeMarinaVersionTag(current_version);
  if (ParseMarinaSemVer(current) == std::nullopt) {
    return std::nullopt;
  }

  const MarinaGitHubRelease* best = nullptr;
  for (const MarinaGitHubRelease& release : releases) {
    if (release.draft) {
      continue;
    }
    if (!include_unstable &&
        (release.prerelease || IsMarinaUnstableVersion(release.tag_name))) {
      continue;
    }
    if (!MarinaSemVerLess(current, release.tag_name)) {
      continue;
    }
    if (best == nullptr ||
        MarinaSemVerLess(best->tag_name, release.tag_name)) {
      best = &release;
    }
  }
  if (best == nullptr) {
    return std::nullopt;
  }
  return *best;
}

}  // namespace mozc
