#include "sync/sync_config.h"

#include "base/file_util.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace sync {
namespace {

class SyncConfigTest : public testing::TestWithTempUserProfile {};

TEST_F(SyncConfigTest, SaveAndLoadRoundTrip) {
  commands::UserSyncConfig config;
  config.set_enabled(true);
  config.set_sync_file_path(
      "/Users/test/Library/Application Support/sync.mmz.enc");
  config.set_sync_settings(true);
  config.set_sync_dictionary(false);
  config.set_sync_history(true);
  config.set_direction(commands::UserSyncConfig::UPLOAD);
  config.set_auto_sync_mode(commands::UserSyncConfig::EVERY_N_MINUTES);
  config.set_auto_sync_interval_minutes(45);
  config.set_last_sync_time("2026-06-05T12:00:00Z");
  config.set_last_sync_status("ok");
  config.set_last_sync_message("Synced successfully");
  config.set_device_id("abc123device");
  config.set_sync_cooldown_seconds(90);

  ASSERT_TRUE(SaveSyncConfig(config).ok());
  const auto loaded_or = LoadSyncConfig();
  ASSERT_TRUE(loaded_or.ok());

  const commands::UserSyncConfig& loaded = *loaded_or;
  EXPECT_TRUE(loaded.enabled());
  EXPECT_EQ(loaded.sync_file_path(), config.sync_file_path());
  EXPECT_TRUE(loaded.sync_settings());
  EXPECT_FALSE(loaded.sync_dictionary());
  EXPECT_TRUE(loaded.sync_history());
  EXPECT_EQ(loaded.direction(), commands::UserSyncConfig::UPLOAD);
  EXPECT_EQ(loaded.auto_sync_mode(), commands::UserSyncConfig::EVERY_N_MINUTES);
  EXPECT_EQ(loaded.auto_sync_interval_minutes(), 45);
  EXPECT_EQ(loaded.last_sync_time(), "2026-06-05T12:00:00Z");
  EXPECT_EQ(loaded.last_sync_status(), "ok");
  EXPECT_EQ(loaded.last_sync_message(), "Synced successfully");
  EXPECT_EQ(loaded.device_id(), "abc123device");
  EXPECT_EQ(loaded.sync_cooldown_seconds(), 90);
}

TEST_F(SyncConfigTest, LoadPrettyPrintedJson) {
  const std::string json = R"({
  "enabled": true,
  "sync_file_path": "/Users/daniel/Documents/vm_share/marinamoji_sync.mmz.enc",
  "sync_settings": true,
  "sync_dictionary": true,
  "sync_history": false,
  "direction": 0,
  "auto_sync_mode": 0,
  "auto_sync_interval_minutes": 30,
  "last_sync_time": "",
  "last_sync_status": "",
  "last_sync_message": "",
  "device_id": "deadbeef",
  "sync_cooldown_seconds": 60
}
)";

  const std::string path = GetSyncConfigPath();
  ASSERT_TRUE(FileUtil::SetContents(path, json).ok());

  const auto loaded_or = LoadSyncConfig();
  ASSERT_TRUE(loaded_or.ok());
  EXPECT_TRUE(loaded_or->enabled());
  EXPECT_EQ(loaded_or->sync_file_path(),
            "/Users/daniel/Documents/vm_share/marinamoji_sync.mmz.enc");
  EXPECT_FALSE(loaded_or->sync_history());
  EXPECT_EQ(loaded_or->device_id(), "deadbeef");
}

}  // namespace
}  // namespace sync
}  // namespace mozc
