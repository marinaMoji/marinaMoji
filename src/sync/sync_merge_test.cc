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

TEST(SyncMergeTest, MergeHistoryOrsEntryFlags) {
  const std::string local = "# h\nkai\t階\t1\t0\t100\t1\nko\t個\t1\t0\t100\t0\n";
  const std::string remote = "# h\nkai\t階\t2\t1\t200\t0\nko\t個\t1\t0\t100\t1\n";
  std::string merged;
  HistoryMergeStats stats;
  ASSERT_TRUE(MergeHistoryTsv(remote, local, &merged, &stats).ok());
  EXPECT_EQ(stats.merged, 2);
  EXPECT_NE(merged.find("kai\t階\t3\t1\t200\t1\n"), std::string::npos);
  EXPECT_NE(merged.find("ko\t個\t2\t0\t100\t1\n"), std::string::npos);
}

TEST(SyncMergeTest, MergeHistoryAcceptsFiveColumnFiles) {
  // history.tsv written before entry_flags was synced.
  const std::string local = "# h\nkai\t階\t1\t0\t100\t1\n";
  const std::string remote = "# h\nkai\t階\t2\t1\t200\nni\t日\t1\t0\t50\n";
  std::string merged;
  HistoryMergeStats stats;
  ASSERT_TRUE(MergeHistoryTsv(remote, local, &merged, &stats).ok());
  EXPECT_NE(merged.find("kai\t階\t3\t1\t200\t1\n"), std::string::npos);
  EXPECT_NE(merged.find("ni\t日\t1\t0\t50\t0\n"), std::string::npos);
}

}  // namespace
}  // namespace sync
}  // namespace mozc
