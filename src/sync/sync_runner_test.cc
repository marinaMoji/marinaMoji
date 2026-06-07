#include "sync/sync_runner.h"

#include "client/client_mock.h"
#include "sync/sync_config.h"
#include "sync/sync_key.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace sync {
namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SetArgPointee;

class SyncRunnerTest : public testing::TestWithTempUserProfile {};

TEST_F(SyncRunnerTest, CompositionBlocksSync) {
  client::ClientMock mock;
  commands::SyncState state;
  state.set_any_composing(true);
  EXPECT_CALL(mock, GetSyncState(_))
      .WillOnce(DoAll(SetArgPointee<0>(state), Return(true)));
  EXPECT_CALL(mock, BeginSyncLock()).Times(0);

  commands::UserSyncConfig config;
  config.set_enabled(true);
  config.set_sync_file_path("/tmp/unused.mmz.enc");
  ASSERT_TRUE(SaveSyncConfig(config).ok());
  ASSERT_TRUE(StoreSyncKey(GenerateSyncKey()).ok());

  RunSyncOptions options;
  const auto report_or = RunSync(&mock, options);
  ASSERT_TRUE(report_or.ok());
  EXPECT_FALSE(report_or->success());
}

TEST_F(SyncRunnerTest, ReleasesLockWhenSyncDataFails) {
  client::ClientMock mock;
  EXPECT_CALL(mock, EnsureConnection()).WillOnce(Return(true));

  {
    InSequence seq;
    EXPECT_CALL(mock, BeginSyncLock()).WillOnce(Return(true));
    EXPECT_CALL(mock, SyncData()).WillOnce(Return(false));
    EXPECT_CALL(mock, EndSyncLock()).WillOnce(Return(true));
  }

  commands::UserSyncConfig config;
  config.set_enabled(true);
  config.set_sync_file_path("/tmp/unused.mmz.enc");
  ASSERT_TRUE(SaveSyncConfig(config).ok());
  ASSERT_TRUE(StoreSyncKey(GenerateSyncKey()).ok());

  RunSyncOptions options;
  options.force = true;
  const auto report_or = RunSync(&mock, options);
  ASSERT_TRUE(report_or.ok());
  EXPECT_FALSE(report_or->success());
}

TEST_F(SyncRunnerTest, CanAutoSyncRequiresIdleAndCooldown) {
  client::ClientMock mock;
  EXPECT_CALL(mock, EnsureConnection()).WillRepeatedly(Return(true));
  commands::SyncState composing;
  composing.set_any_composing(true);
  EXPECT_CALL(mock, GetSyncState(_))
      .WillOnce(DoAll(SetArgPointee<0>(composing), Return(true)));
  EXPECT_FALSE(CanAutoSync(&mock, 60));

  commands::SyncState idle;
  idle.set_any_composing(false);
  EXPECT_CALL(mock, GetSyncState(_))
      .WillOnce(DoAll(SetArgPointee<0>(idle), Return(true)));
  EXPECT_TRUE(CanAutoSync(&mock, 0));
}

}  // namespace
}  // namespace sync
}  // namespace mozc
