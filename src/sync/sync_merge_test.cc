#include "sync/sync_merge.h"

#include "gtest/gtest.h"

namespace mozc {
namespace sync {
namespace {

TEST(SyncMergeTest, MergeDictionaryAdditive) {
  const std::string local = "# d\na\t1\t名詞\t\n";
  const std::string remote = "# d\nb\t2\t名詞\t\n";
  std::string merged;
  DictionaryMergeStats stats;
  ASSERT_TRUE(MergeDictionaryTsv(remote, local, &merged, &stats).ok());
  EXPECT_EQ(stats.added, 1);
  EXPECT_NE(merged.find("a\t1"), std::string::npos);
  EXPECT_NE(merged.find("b\t2"), std::string::npos);
}

TEST(SyncMergeTest, MergeHistorySumsFreq) {
  const std::string local = "# h\nni\t日\t1\t0\t100\n";
  const std::string remote = "# h\nni\t日\t2\t1\t200\n";
  std::string merged;
  HistoryMergeStats stats;
  ASSERT_TRUE(MergeHistoryTsv(remote, local, &merged, &stats).ok());
  EXPECT_NE(merged.find("ni\t日\t3\t1\t200"), std::string::npos);
}

}  // namespace
}  // namespace sync
}  // namespace mozc
