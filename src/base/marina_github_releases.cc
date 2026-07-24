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

void ExtractPkgAssets(absl::string_view object,
                      std::vector<MarinaGitHubAsset>* assets) {
  size_t pos = 0;
  while (true) {
    pos = object.find("\"browser_download_url\"", pos);
    if (pos == absl::string_view::npos) {
      break;
    }
    const auto url = ExtractJsonStringField(object.substr(pos),
                                            "browser_download_url");
    pos += 22;
    if (!url.has_value() ||
        (!absl::EndsWith(*url, ".pkg") && !absl::EndsWith(*url, ".msi") &&
         !absl::EndsWith(*url, ".deb"))) {
      continue;
    }
    MarinaGitHubAsset asset;
    asset.browser_download_url = *url;
    // Prefer an explicit name field earlier in the same asset object if present.
    const size_t name_pos = object.rfind("\"name\"", pos);
    if (name_pos != absl::string_view::npos && pos - name_pos < 400) {
      if (const auto name =
              ExtractJsonStringField(object.substr(name_pos), "name");
          name.has_value()) {
        asset.name = *name;
      }
    }
    if (asset.name.empty()) {
      const size_t slash = url->rfind('/');
      asset.name = slash == std::string::npos ? *url : url->substr(slash + 1);
    }
    assets->push_back(std::move(asset));
  }
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
        ExtractPkgAssets(object, &release.assets);
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

std::optional<std::string> FindMarinaPkgDownloadUrl(
    const MarinaGitHubRelease& release, absl::string_view arch_token) {
  const MarinaGitHubAsset* universal = nullptr;
  const MarinaGitHubAsset* any_pkg = nullptr;
  for (const MarinaGitHubAsset& asset : release.assets) {
    const bool is_pkg = absl::EndsWith(asset.name, ".pkg") ||
                        absl::EndsWith(asset.browser_download_url, ".pkg");
    if (!is_pkg) {
      continue;
    }
    if (!arch_token.empty() &&
        (absl::StrContains(asset.name, arch_token) ||
         absl::StrContains(asset.browser_download_url, arch_token))) {
      return asset.browser_download_url;
    }
    if (absl::StrContains(asset.name, "universal") ||
        absl::StrContains(asset.browser_download_url, "universal")) {
      universal = &asset;
    } else if (any_pkg == nullptr) {
      any_pkg = &asset;
    }
  }
  if (universal != nullptr) {
    return universal->browser_download_url;
  }
  // Only fall back to an unmatched .pkg when the caller did not request an arch.
  if (arch_token.empty() && any_pkg != nullptr) {
    return any_pkg->browser_download_url;
  }
  return std::nullopt;
}

std::string MarinaHostMacPkgArchToken() {
#if defined(__APPLE__)
#if defined(__aarch64__) || defined(__arm64__)
  return "arm64";
#else
  return "intel64";
#endif
#else
  return "";
#endif
}

}  // namespace mozc
