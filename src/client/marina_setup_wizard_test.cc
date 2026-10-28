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

  static void WriteEmptyConfigFile() {
    const std::string path =
        ConfigFileStream::GetFileName("user://config1.db");
    ASSERT_FALSE(path.empty());
    ASSERT_OK(FileUtil::SetContents(path, "x"));
  }
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
  ASSERT_OK(FileUtil::SetContents(
      FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(),
                         "setup_wizard_shown"),
      "1"));
  ClientMock mock;
  EXPECT_CALL(mock, LaunchTool(_, _)).Times(0);
  MaybeLaunchSetupWizard(&mock);
}

TEST_F(MarinaSetupWizardTest, FailedLaunchDoesNotWriteMarkerAndAllowsRetry) {
  ClientMock mock;
  EXPECT_CALL(mock, LaunchTool("setup_wizard", ""))
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  MaybeLaunchSetupWizard(&mock);
  EXPECT_FALSE(IsSetupWizardShownMarkerPresentForTesting());
  MaybeLaunchSetupWizard(&mock);
  EXPECT_TRUE(IsSetupWizardShownMarkerPresentForTesting());
}

TEST_F(MarinaSetupWizardTest, SecondCallAfterSuccessDoesNotRelaunch) {
  ClientMock mock;
  EXPECT_CALL(mock, LaunchTool("setup_wizard", "")).WillOnce(Return(true));
  MaybeLaunchSetupWizard(&mock);
  MaybeLaunchSetupWizard(&mock);
}

}  // namespace
}  // namespace mozc::client
