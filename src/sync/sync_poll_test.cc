#include "sync/sync_poll.h"

#include "testing/gunit.h"

namespace mozc {
namespace sync {
namespace {

TEST(SyncPollTest, EvaluateIntervalSyncBaseline) {
  SyncFingerprintSnapshot baseline;
  SyncFingerprintSnapshot current;
  current.remote_bundle_sha256 = "abc";
  current.local_data_sha256 = "def";
  EXPECT_EQ(EvaluateIntervalSync(baseline, current),
            IntervalSyncDecision::kBaselineOnly);
}

TEST(SyncPollTest, EvaluateIntervalSyncSkipWhenUnchanged) {
  SyncFingerprintSnapshot baseline;
  baseline.remote_bundle_sha256 = "abc";
  baseline.local_data_sha256 = "def";
  SyncFingerprintSnapshot current;
  current.remote_bundle_sha256 = "abc";
  current.local_data_sha256 = "def";
  EXPECT_EQ(EvaluateIntervalSync(baseline, current),
            IntervalSyncDecision::kSkip);
}

TEST(SyncPollTest, EvaluateIntervalSyncRemoteChanged) {
  SyncFingerprintSnapshot baseline;
  baseline.remote_bundle_sha256 = "abc";
  baseline.local_data_sha256 = "def";
  SyncFingerprintSnapshot current;
  current.remote_bundle_sha256 = "xyz";
  current.local_data_sha256 = "def";
  EXPECT_EQ(EvaluateIntervalSync(baseline, current),
            IntervalSyncDecision::kSync);
}

TEST(SyncPollTest, EvaluateIntervalSyncLocalChanged) {
  SyncFingerprintSnapshot baseline;
  baseline.remote_bundle_sha256 = "abc";
  baseline.local_data_sha256 = "def";
  SyncFingerprintSnapshot current;
  current.remote_bundle_sha256 = "abc";
  current.local_data_sha256 = "xyz";
  EXPECT_EQ(EvaluateIntervalSync(baseline, current),
            IntervalSyncDecision::kSync);
}

}  // namespace
}  // namespace sync
}  // namespace mozc
