#include "sync/sync_dictionary_tombstones.h"

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"

namespace mozc {
namespace sync {
namespace {

TEST(SyncDictionaryTombstonesTest, MergeRemovesStaleRemoteEntry) {
  const std::string local = "# d\n";
  const std::string remote = "# d\nb\t2\t名詞\t\n";
  std::vector<DictionaryTombstone> local_tombstones;
  DictionaryTombstone tombstone;
  tombstone.reading = "b";
  tombstone.surface = "2";
  tombstone.pos = "名詞";
  tombstone.deleted_at_unix = 1'700'000'000;
  tombstone.device_id = "device-a";
  local_tombstones.push_back(tombstone);

  std::string merged_dict;
  std::vector<DictionaryTombstone> merged_tombstones;
  DictionaryMergeStats dict_stats;
  DictionaryTombstoneMergeStats tomb_stats;
  ASSERT_TRUE(MergeDictionaryWithTombstones(
                  remote, local, {}, local_tombstones, "device-a", &merged_dict,
                  &merged_tombstones, &dict_stats, &tomb_stats)
                  .ok());
  EXPECT_EQ(merged_dict.find("b\t2"), std::string::npos);
  EXPECT_EQ(tomb_stats.removed_from_dictionary, 1);
}

TEST(SyncDictionaryTombstonesTest, ReAddKeepsEntry) {
  const std::string local = "# d\nb\t2\t名詞\t\n";
  const std::string remote = "# d\n";
  std::vector<DictionaryTombstone> remote_tombstones;
  DictionaryTombstone tombstone;
  tombstone.reading = "b";
  tombstone.surface = "2";
  tombstone.pos = "名詞";
  tombstone.deleted_at_unix = 1'700'000'000;
  // Stale tombstone from the same device after the word was re-added locally.
  tombstone.device_id = "device-a";
  remote_tombstones.push_back(tombstone);

  std::string merged_dict;
  std::vector<DictionaryTombstone> merged_tombstones;
  DictionaryMergeStats dict_stats;
  DictionaryTombstoneMergeStats tomb_stats;
  ASSERT_TRUE(MergeDictionaryWithTombstones(
                  remote, local, remote_tombstones, {}, "device-a", &merged_dict,
                  &merged_tombstones, &dict_stats, &tomb_stats)
                  .ok());
  EXPECT_NE(merged_dict.find("b\t2"), std::string::npos);
}

TEST(SyncDictionaryTombstonesTest, RemoteDeleteRemovesStaleLocalEntry) {
  const std::string local = "# d\nb\t2\t名詞\t\n";
  const std::string remote = "# d\n";
  std::vector<DictionaryTombstone> remote_tombstones;
  DictionaryTombstone tombstone;
  tombstone.reading = "b";
  tombstone.surface = "2";
  tombstone.pos = "名詞";
  tombstone.deleted_at_unix = 1'700'000'000;
  tombstone.device_id = "device-b";
  remote_tombstones.push_back(tombstone);

  std::string merged_dict;
  std::vector<DictionaryTombstone> merged_tombstones;
  DictionaryMergeStats dict_stats;
  DictionaryTombstoneMergeStats tomb_stats;
  ASSERT_TRUE(MergeDictionaryWithTombstones(
                  remote, local, remote_tombstones, {}, "device-a", &merged_dict,
                  &merged_tombstones, &dict_stats, &tomb_stats)
                  .ok());
  EXPECT_EQ(merged_dict.find("b\t2"), std::string::npos);
}

TEST(SyncDictionaryTombstonesTest, CompactDropsReaddedAndExpired) {
  absl::flat_hash_set<std::string> live_keys;
  live_keys.insert(DictionaryEntrySyncKey("a", "1", "名詞", ""));

  std::vector<DictionaryTombstone> tombstones;
  DictionaryTombstone readded;
  readded.reading = "a";
  readded.surface = "1";
  readded.pos = "名詞";
  readded.deleted_at_unix = 100;
  readded.device_id = "dev";
  tombstones.push_back(readded);

  DictionaryTombstone expired;
  expired.reading = "z";
  expired.surface = "9";
  expired.pos = "名詞";
  expired.deleted_at_unix = 1;
  expired.device_id = "dev";
  tombstones.push_back(expired);

  const std::vector<DictionaryTombstone> compacted =
      CompactDictionaryTombstones(tombstones, live_keys, absl::Now(), 90);
  EXPECT_TRUE(compacted.empty());
}

TEST(SyncDictionaryTombstonesTest, TombstoneLwwMerge) {
  std::vector<DictionaryTombstone> local;
  DictionaryTombstone older;
  older.reading = "x";
  older.surface = "y";
  older.pos = "名詞";
  older.deleted_at_unix = 10;
  older.device_id = "a";
  local.push_back(older);

  std::vector<DictionaryTombstone> remote;
  DictionaryTombstone newer;
  newer.reading = "x";
  newer.surface = "y";
  newer.pos = "名詞";
  newer.deleted_at_unix = 20;
  newer.device_id = "b";
  remote.push_back(newer);

  const std::vector<DictionaryTombstone> merged =
      MergeDictionaryTombstones(local, remote);
  ASSERT_EQ(merged.size(), 1u);
  EXPECT_EQ(merged[0].deleted_at_unix, 20u);
  EXPECT_EQ(merged[0].device_id, "b");
}

}  // namespace
}  // namespace sync
}  // namespace mozc
