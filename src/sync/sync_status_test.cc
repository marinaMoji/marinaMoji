#include "sync/sync_status.h"

#include "base/file_util.h"
#include "base/system_util.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace sync {
namespace {

class SyncStatusTest : public testing::TestWithTempUserProfile {};

TEST_F(SyncStatusTest, WriteAndRead) {
  SyncStatus status;
  status.state = "running";
  status.phase = "merge";
  status.progress = 0.42;
  status.message = "test message";
  ASSERT_TRUE(WriteSyncStatus(status).ok());
  const auto read_or = ReadSyncStatus();
  ASSERT_TRUE(read_or.ok());
  EXPECT_EQ(read_or->state, "running");
  EXPECT_EQ(read_or->phase, "merge");
  EXPECT_DOUBLE_EQ(read_or->progress, 0.42);
  EXPECT_EQ(read_or->message, "test message");
}

}  // namespace
}  // namespace sync
}  // namespace mozc
