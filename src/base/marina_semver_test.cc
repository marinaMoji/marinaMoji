#include "base/marina_semver.h"
#include "base/marina_github_releases.h"

#include "testing/gunit.h"

namespace mozc {
namespace {

TEST(MarinaSemVerTest, ParseAndCompare) {
  EXPECT_TRUE(MarinaSemVerLess("0.0.1-rc3", "0.0.2-rc1"));
  EXPECT_TRUE(MarinaSemVerLess("0.0.1-rc3", "0.0.2"));
  EXPECT_TRUE(MarinaSemVerLess("0.0.2-rc1", "0.0.2"));
  EXPECT_FALSE(MarinaSemVerLess("0.0.2", "0.0.2-rc1"));
  EXPECT_FALSE(MarinaSemVerLess("0.0.2-rc1", "0.0.2-rc1"));
  EXPECT_TRUE(MarinaSemVerLess("v0.0.1-rc3", "0.0.2-rc1"));
  EXPECT_TRUE(IsMarinaUnstableVersion("0.0.2-rc1"));
  EXPECT_FALSE(IsMarinaUnstableVersion("0.0.2"));
}

TEST(MarinaGitHubReleasesTest, SelectSkipsSameAndOlder) {
  MarinaGitHubRelease rc3;
  rc3.tag_name = "v0.0.1-rc3";
  rc3.html_url = "https://example/rc3";
  rc3.prerelease = true;
  MarinaGitHubRelease rc1;
  rc1.tag_name = "v0.0.2-rc1";
  rc1.html_url = "https://example/rc1";
  rc1.prerelease = true;
  MarinaGitHubRelease stable;
  stable.tag_name = "v0.0.2";
  stable.html_url = "https://example/stable";
  stable.prerelease = false;
  const std::vector<MarinaGitHubRelease> releases = {rc3, rc1, stable};

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
        SelectNewerMarinaRelease(releases, "0.0.2-rc1", /*include_unstable=*/true);
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
    {"tag_name":"v0.0.2-rc1","prerelease":true,"draft":false,
     "html_url":"https://github.com/x/y"}
  ])";
  const auto releases = ParseMarinaGitHubReleasesJson(kJson);
  ASSERT_EQ(releases.size(), 2u);
  EXPECT_EQ(releases[0].tag_name, "v0.0.1-rc3");
  EXPECT_TRUE(releases[0].prerelease);
  EXPECT_EQ(releases[1].tag_name, "v0.0.2-rc1");
}

}  // namespace
}  // namespace mozc
