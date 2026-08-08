#include "base/marina_semver.h"
#include "base/marina_github_releases.h"

#include "testing/gunit.h"

namespace mozc {
namespace {

TEST(MarinaSemVerTest, ParseAndCompare) {
  EXPECT_TRUE(MarinaSemVerLess("0.0.1-rc3", "0.0.2-rc2"));
  EXPECT_TRUE(MarinaSemVerLess("0.0.1-rc3", "0.0.2"));
  EXPECT_TRUE(MarinaSemVerLess("0.0.2-rc2", "0.0.2"));
  EXPECT_FALSE(MarinaSemVerLess("0.0.2", "0.0.2-rc2"));
  EXPECT_FALSE(MarinaSemVerLess("0.0.2-rc2", "0.0.2-rc2"));
  EXPECT_TRUE(MarinaSemVerLess("v0.0.1-rc3", "0.0.2-rc2"));
  EXPECT_TRUE(IsMarinaUnstableVersion("0.0.2-rc2"));
  EXPECT_FALSE(IsMarinaUnstableVersion("0.0.2"));
}

TEST(MarinaGitHubReleasesTest, SelectSkipsSameAndOlder) {
  MarinaGitHubRelease rc3;
  rc3.tag_name = "v0.0.1-rc3";
  rc3.html_url = "https://example/rc3";
  rc3.prerelease = true;
  MarinaGitHubRelease rc2;
  rc2.tag_name = "v0.0.2-rc2";
  rc2.html_url = "https://example/rc2";
  rc2.prerelease = true;
  MarinaGitHubRelease stable;
  stable.tag_name = "v0.0.2";
  stable.html_url = "https://example/stable";
  stable.prerelease = false;
  const std::vector<MarinaGitHubRelease> releases = {rc3, rc2, stable};

  // Stable-only: rc current still sees final 0.0.2, not another rc.
  {
    const auto newer =
        SelectNewerMarinaRelease(releases, "0.0.1-rc3", /*include_unstable=*/false);
    ASSERT_TRUE(newer.has_value());
    EXPECT_EQ(newer->tag_name, "v0.0.2");
  }

  // Unstable: prefers newest semver among rcs and finals.
  {
    const auto newer =
        SelectNewerMarinaRelease(releases, "0.0.1-rc3", /*include_unstable=*/true);
    ASSERT_TRUE(newer.has_value());
    EXPECT_EQ(newer->tag_name, "v0.0.2");
  }

  {
    const auto newer =
        SelectNewerMarinaRelease(releases, "0.0.2-rc2", /*include_unstable=*/true);
    ASSERT_TRUE(newer.has_value());
    EXPECT_EQ(newer->tag_name, "v0.0.2");
  }

  EXPECT_FALSE(
      SelectNewerMarinaRelease(releases, "0.0.2", /*include_unstable=*/true)
          .has_value());
}

TEST(MarinaGitHubReleasesTest, ParseJson) {
  constexpr char kJson[] = R"([
    {"tag_name":"v0.0.1-rc3","prerelease":true,"draft":false,
     "html_url":"https://github.com/marinaMoji/marinaMoji/releases/tag/v0.0.1-rc3"},
    {"tag_name":"v0.0.2-rc2","prerelease":true,"draft":false,
     "html_url":"https://github.com/x/y"}
  ])";
  const auto releases = ParseMarinaGitHubReleasesJson(kJson);
  ASSERT_EQ(releases.size(), 2u);
  EXPECT_EQ(releases[0].tag_name, "v0.0.1-rc3");
  EXPECT_TRUE(releases[0].prerelease);
  EXPECT_EQ(releases[1].tag_name, "v0.0.2-rc2");
}

TEST(MarinaGitHubReleasesTest, FindPkgByArch) {
  MarinaGitHubRelease release;
  release.tag_name = "v0.0.2";
  MarinaGitHubAsset arm;
  arm.name = "marinaMoji-v0.0.2-arm64.pkg";
  arm.browser_download_url =
      "https://example.com/marinaMoji-v0.0.2-arm64.pkg";
  MarinaGitHubAsset intel;
  intel.name = "marinaMoji-v0.0.2-intel64.pkg";
  intel.browser_download_url =
      "https://example.com/marinaMoji-v0.0.2-intel64.pkg";
  MarinaGitHubAsset deb;
  deb.name = "marinamoji_0.0.2_amd64.deb";
  deb.browser_download_url = "https://example.com/marinamoji_0.0.2_amd64.deb";
  release.assets = {deb, intel, arm};

  EXPECT_EQ(*FindMarinaPkgDownloadUrl(release, "arm64"),
            arm.browser_download_url);
  EXPECT_EQ(*FindMarinaPkgDownloadUrl(release, "intel64"),
            intel.browser_download_url);
  EXPECT_FALSE(FindMarinaPkgDownloadUrl(release, "ppc").has_value());
}

TEST(MarinaGitHubReleasesTest, ParseAssetsFromJson) {
  constexpr char kJson[] = R"([
    {"tag_name":"v0.0.2","prerelease":false,"draft":false,
     "html_url":"https://example/stable",
     "assets":[
       {"name":"marinaMoji-v0.0.2-arm64.pkg",
        "browser_download_url":"https://example.com/a.pkg"},
       {"name":"marinaMoji-v0.0.2-intel64.pkg",
        "browser_download_url":"https://example.com/i.pkg"}
     ]}
  ])";
  const auto releases = ParseMarinaGitHubReleasesJson(kJson);
  ASSERT_EQ(releases.size(), 1u);
  ASSERT_EQ(releases[0].assets.size(), 2u);
  EXPECT_EQ(*FindMarinaPkgDownloadUrl(releases[0], "arm64"),
            "https://example.com/a.pkg");
}

TEST(MarinaGitHubReleasesTest, ParseAssetDigestFromJson) {
  // Mirrors the field order GitHub's REST API actually uses for release
  // assets: "digest" appears before "browser_download_url" in the same
  // object. The x64 .msi carries a digest; the .zip (and the arm64 .msi)
  // don't, matching an older release published before GitHub started
  // sending asset digests.
  constexpr char kJson[] = R"([
    {"tag_name":"v0.0.2","prerelease":false,"draft":false,
     "html_url":"https://example/stable",
     "assets":[
       {"name":"marinaMoji-v0.0.2-x64.msi","size":123,
        "digest":"sha256:aaaabbbbccccddddeeeeffff00001111222233334444555566667777888899",
        "browser_download_url":"https://example.com/marinaMoji-v0.0.2-x64.msi"},
       {"name":"marinaMoji-v0.0.2-arm64.msi",
        "browser_download_url":"https://example.com/marinaMoji-v0.0.2-arm64.msi"},
       {"name":"marinaMoji-v0.0.2.zip",
        "browser_download_url":"https://example.com/marinaMoji-v0.0.2.zip"}
     ]}
  ])";
  const auto releases = ParseMarinaGitHubReleasesJson(kJson);
  ASSERT_EQ(releases.size(), 1u);
  ASSERT_EQ(releases[0].assets.size(), 3u);
  EXPECT_EQ(
      FindMarinaMsiSha256Digest(releases[0], "x64"),
      "aaaabbbbccccddddeeeeffff00001111222233334444555566667777888899");
  EXPECT_EQ(FindMarinaMsiSha256Digest(releases[0], "arm64"), "");
}

}  // namespace
}  // namespace mozc
