#include "base/marina_github_releases.h"

#include <optional>
#include <string>

#include "testing/gunit.h"

namespace mozc {
namespace {

MarinaGitHubAsset Asset(absl::string_view name, absl::string_view url) {
  MarinaGitHubAsset asset;
  asset.name = std::string(name);
  asset.browser_download_url = std::string(url);
  return asset;
}

TEST(FindMarinaMsiDownloadUrlTest, PicksMatchingArch) {
  MarinaGitHubRelease release;
  release.assets = {
      Asset("marinaMoji-v1.2.3-x64.msi",
           "https://example.com/marinaMoji-v1.2.3-x64.msi"),
      Asset("marinaMoji-v1.2.3-arm64.msi",
           "https://example.com/marinaMoji-v1.2.3-arm64.msi"),
      Asset("marinaMoji-v1.2.3.zip", "https://example.com/marinaMoji.zip"),
  };

  EXPECT_EQ(FindMarinaMsiDownloadUrl(release, "x64"),
           "https://example.com/marinaMoji-v1.2.3-x64.msi");
  EXPECT_EQ(FindMarinaMsiDownloadUrl(release, "arm64"),
           "https://example.com/marinaMoji-v1.2.3-arm64.msi");
}

TEST(FindMarinaMsiDownloadUrlTest, NoUniversalFallback) {
  // Unlike the mac .pkg lookup, there is no "universal" Windows installer:
  // an arch that isn't present must return nullopt, not some other arch's
  // asset.
  MarinaGitHubRelease release;
  release.assets = {
      Asset("marinaMoji-v1.2.3-x64.msi",
           "https://example.com/marinaMoji-v1.2.3-x64.msi"),
  };

  EXPECT_EQ(FindMarinaMsiDownloadUrl(release, "arm64"), std::nullopt);
}

TEST(FindMarinaMsiDownloadUrlTest, EmptyArchTokenReturnsFirstMsi) {
  MarinaGitHubRelease release;
  release.assets = {
      Asset("marinaMoji-v1.2.3.zip", "https://example.com/marinaMoji.zip"),
      Asset("marinaMoji-v1.2.3-x64.msi",
           "https://example.com/marinaMoji-v1.2.3-x64.msi"),
  };

  EXPECT_EQ(FindMarinaMsiDownloadUrl(release, ""),
           "https://example.com/marinaMoji-v1.2.3-x64.msi");
}

TEST(FindMarinaMsiDownloadUrlTest, IgnoresNonMsiAssets) {
  MarinaGitHubRelease release;
  release.assets = {
      Asset("marinaMoji-v1.2.3-x64.zip",
           "https://example.com/marinaMoji-v1.2.3-x64.zip"),
      Asset("marinaMoji-v1.2.3-x64.deb",
           "https://example.com/marinaMoji-v1.2.3-x64.deb"),
  };

  EXPECT_EQ(FindMarinaMsiDownloadUrl(release, "x64"), std::nullopt);
}

TEST(MarinaHostWindowsArchTokenTest, ReturnsPlausibleTokenOrEmpty) {
  // Hermetic across build hosts: on Windows this is "x64" or "arm64"; on
  // every other platform (this test typically runs on macOS/Linux CI) it
  // must be empty, exactly mirroring MarinaHostMacPkgArchToken's contract.
  const std::string token = MarinaHostWindowsArchToken();
#if defined(_WIN32)
  EXPECT_TRUE(token == "x64" || token == "arm64");
#else
  EXPECT_TRUE(token.empty());
#endif
}

}  // namespace
}  // namespace mozc
