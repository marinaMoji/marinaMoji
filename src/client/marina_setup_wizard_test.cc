#include "client/marina_setup_wizard.h"

#include "base/config_file_stream.h"
#include "base/file_util.h"
#include "base/system_util.h"
#include "client/client_mock.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc::client {
namespace {

using ::testing::_;
using ::testing::Return;

class MarinaSetupWizardTest : public testing::TestWithTempUserProfile {
 protected:
  void SetUp() override { ResetSetupWizardLaunchStateForTesting(); }
  void TearDown() override { ResetSetupWizardLaunchStateForTesting(); }

  static void WriteProfileFile(const char* uri) {
    const std::string path = ConfigFileStream::GetFileName(uri);
    ASSERT_FALSE(path.empty());
    ASSERT_OK(FileUtil::SetContents(path, "x"));
  }
  static void WriteEmptyConfigFile() { WriteProfileFile("user://config1.db"); }
};

TEST_F(MarinaSetupWizardTest, LaunchesWhenNoConfigAndNoMarker) {
  ClientMock mock;
  EXPECT_CALL(mock, LaunchTool("setup_wizard", "")).WillOnce(Return(true));
  MaybeLaunchSetupWizard(&mock);
  EXPECT_TRUE(IsSetupWizardShownMarkerPresentForTesting());
}

TEST_F(MarinaSetupWizardTest, SkipsWhenConfigExistsAndWritesMarker) {
  WriteEmptyConfigFile();
  ClientMock mock;
  EXPECT_CALL(mock, LaunchTool(_, _)).Times(0);
  MaybeLaunchSetupWizard(&mock);
  EXPECT_TRUE(IsSetupWizardShownMarkerPresentForTesting());
}

TEST_F(MarinaSetupWizardTest, SkipsWhenMarkerAlreadyPresent) {
  ASSERT_OK(FileUtil::CreateDirectory(
      FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(),
                         "setup_wizard_shown")));
  ClientMock mock;
  EXPECT_CALL(mock, LaunchTool(_, _)).Times(0);
  EXPECT_FALSE(IsSetupWizardPending());
  MaybeLaunchSetupWizard(&mock);
}

TEST_F(MarinaSetupWizardTest, UsedProfileWithoutConfigIsGrandfathered) {
  for (const char* uri : {"user://user_dictionary.db", "user://history.db"}) {
    ResetSetupWizardLaunchStateForTesting();
    WriteProfileFile(uri);
    ClientMock mock;
    EXPECT_CALL(mock, LaunchTool(_, _)).Times(0);
    MaybeLaunchSetupWizard(&mock);
    EXPECT_TRUE(IsSetupWizardShownMarkerPresentForTesting()) << uri;
    ASSERT_OK(FileUtil::UnlinkIfExists(ConfigFileStream::GetFileName(uri)));
  }
}

TEST_F(MarinaSetupWizardTest, FailedLaunchReleasesMarkerAndAllowsRetry) {
  ClientMock mock;
  EXPECT_CALL(mock, LaunchTool("setup_wizard", ""))
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  MaybeLaunchSetupWizard(&mock);
  EXPECT_FALSE(IsSetupWizardShownMarkerPresentForTesting());
  EXPECT_TRUE(IsSetupWizardPending());
  MaybeLaunchSetupWizard(&mock);
  EXPECT_TRUE(IsSetupWizardShownMarkerPresentForTesting());
  EXPECT_FALSE(IsSetupWizardPending());
}

TEST_F(MarinaSetupWizardTest, SecondCallAfterSuccessDoesNotRelaunch) {
  ClientMock mock;
  EXPECT_CALL(mock, LaunchTool("setup_wizard", "")).WillOnce(Return(true));
  MaybeLaunchSetupWizard(&mock);
  MaybeLaunchSetupWizard(&mock);
}

}  // namespace
}  // namespace mozc::client
