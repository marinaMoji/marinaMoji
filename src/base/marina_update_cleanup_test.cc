#include "base/marina_update_cleanup.h"

#include <string>

#include "base/file_util.h"
#include "base/system_util.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace {

TEST(IsStaleMarinaUpdateInstallerTest, OlderAndEqualReleasesAreStale) {
  EXPECT_TRUE(IsStaleMarinaUpdateInstaller("marinaMoji-update-v0.0.16.msi",
                                           "0.0.17"));
  EXPECT_TRUE(IsStaleMarinaUpdateInstaller("marinaMoji-update-v0.0.9.pkg",
                                           "0.0.17"));
  // The release that is running: it is installed, the file is spent.
  EXPECT_TRUE(IsStaleMarinaUpdateInstaller("marinaMoji-update-v0.0.17.msi",
                                           "0.0.17"));
  EXPECT_TRUE(IsStaleMarinaUpdateInstaller(
      "marinaMoji-update-v0.0.17-rc1.msi", "0.0.17-rc1"));
}

TEST(IsStaleMarinaUpdateInstallerTest, NewerReleasesAreKept) {
  EXPECT_FALSE(IsStaleMarinaUpdateInstaller("marinaMoji-update-v0.0.18.msi",
                                            "0.0.17"));
  EXPECT_FALSE(IsStaleMarinaUpdateInstaller("marinaMoji-update-v0.0.18.pkg",
                                            "0.0.17-rc1"));
}

TEST(IsStaleMarinaUpdateInstallerTest, PrereleaseOrdering) {
  // An rc is older than its final release, newer than the previous final.
  EXPECT_TRUE(IsStaleMarinaUpdateInstaller(
      "marinaMoji-update-v0.0.17-rc1.msi", "0.0.17"));
  EXPECT_FALSE(IsStaleMarinaUpdateInstaller("marinaMoji-update-v0.0.17.msi",
                                            "0.0.17-rc1"));
  EXPECT_FALSE(IsStaleMarinaUpdateInstaller(
      "marinaMoji-update-v0.0.17-rc1.msi", "0.0.16"));
}

TEST(IsStaleMarinaUpdateInstallerTest, UnknownFilesAreNeverStale) {
  EXPECT_FALSE(IsStaleMarinaUpdateInstaller("config1.db", "0.0.17"));
  EXPECT_FALSE(IsStaleMarinaUpdateInstaller("silent-update-v0.0.9.pkg",
                                            "0.0.17"));
  EXPECT_FALSE(
      IsStaleMarinaUpdateInstaller("marinaMoji-update-v0.0.9.zip", "0.0.17"));
  EXPECT_FALSE(IsStaleMarinaUpdateInstaller("marinaMoji-update-.msi",
                                            "0.0.17"));
  EXPECT_FALSE(IsStaleMarinaUpdateInstaller("marinaMoji-update-garbage.msi",
                                            "0.0.17"));
}

TEST(IsStaleMarinaUpdateInstallerTest, UnparsableRunningVersionDeletesNothing) {
  EXPECT_FALSE(
      IsStaleMarinaUpdateInstaller("marinaMoji-update-v0.0.9.msi", ""));
  EXPECT_FALSE(
      IsStaleMarinaUpdateInstaller("marinaMoji-update-v0.0.9.msi", "dev"));
}

class RemoveStaleMarinaUpdateInstallersTest
    : public testing::TestWithTempUserProfile {};

TEST_F(RemoveStaleMarinaUpdateInstallersTest, RemovesOnlyStaleInstallers) {
  const std::string dir = SystemUtil::GetUserProfileDirectory();
  const std::string files[] = {
      "marinaMoji-update-v0.0.8.msi",   "marinaMoji-update-v0.0.16.msi",
      "marinaMoji-update-v0.0.17.pkg",  "marinaMoji-update-v0.0.18.msi",
      "config1.db",                     "silent-update-v0.0.9.pkg",
  };
  for (const std::string& name : files) {
    ASSERT_TRUE(FileUtil::SetContents(FileUtil::JoinPath(dir, name), "x").ok());
  }

  EXPECT_EQ(RemoveStaleMarinaUpdateInstallers(dir, "0.0.17"), 3);

  const std::string expect_gone[] = {"marinaMoji-update-v0.0.8.msi",
                                     "marinaMoji-update-v0.0.16.msi",
                                     "marinaMoji-update-v0.0.17.pkg"};
  const std::string expect_kept[] = {"marinaMoji-update-v0.0.18.msi",
                                     "config1.db", "silent-update-v0.0.9.pkg"};
  for (const std::string& name : expect_gone) {
    EXPECT_FALSE(FileUtil::FileExists(FileUtil::JoinPath(dir, name)).ok())
        << name;
  }
  for (const std::string& name : expect_kept) {
    EXPECT_TRUE(FileUtil::FileExists(FileUtil::JoinPath(dir, name)).ok())
        << name;
  }
  // Running it again finds nothing more.
  EXPECT_EQ(RemoveStaleMarinaUpdateInstallers(dir, "0.0.17"), 0);
}

TEST(RemoveStaleMarinaUpdateInstallersMissingDirTest, MissingDirIsFine) {
  EXPECT_EQ(RemoveStaleMarinaUpdateInstallers("/nonexistent/marina/dir",
                                              "0.0.17"),
            0);
}

}  // namespace
}  // namespace mozc
