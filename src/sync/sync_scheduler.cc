#include "sync/sync_scheduler.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <thread>

#include "absl/time/time.h"
#include "sync/sync_config.h"

namespace mozc {
namespace sync {

SyncScheduler::SyncScheduler(SyncCallback callback)
    : callback_(std::move(callback)) {}

SyncScheduler::~SyncScheduler() { Stop(); }

void SyncScheduler::Start() {
  if (thread_) {
    return;
  }
  stop_ = false;
  thread_ = std::make_unique<std::thread>([this]() {
    std::string tracked_path;
    std::optional<std::filesystem::file_time_type> tracked_mtime;

    while (!stop_) {
      const auto config_or = LoadSyncConfig();
      if (config_or.ok()) {
        const commands::UserSyncConfig& config = *config_or;
        if (config.enabled() && config.has_sync_key()) {
          bool should_sync = false;
          if (config.auto_sync_mode() ==
              commands::UserSyncConfig::EVERY_N_MINUTES) {
            should_sync = true;
          }

          if (!config.sync_file_path().empty()) {
            if (tracked_path != config.sync_file_path()) {
              tracked_path = config.sync_file_path();
              tracked_mtime.reset();
            }
            std::error_code ec;
            const auto current_mtime =
                std::filesystem::last_write_time(tracked_path, ec);
            if (!ec) {
              if (!tracked_mtime.has_value()) {
                tracked_mtime = current_mtime;
              } else if (*tracked_mtime != current_mtime) {
                tracked_mtime = current_mtime;
                should_sync = true;
              }
            }
          }

          if (should_sync) {
            callback_();
          }
        }
      }
      const int interval_minutes =
          config_or.ok() ? config_or->auto_sync_interval_minutes() : 30;
      const int sleep_sec = std::max(60, interval_minutes * 60);
      for (int i = 0; i < sleep_sec && !stop_; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    }
  });
}

void SyncScheduler::Stop() {
  stop_ = true;
  if (thread_ && thread_->joinable()) {
    thread_->join();
  }
  thread_.reset();
}

void SyncScheduler::NotifyShutdown() {
  const auto config_or = LoadSyncConfig();
  if (!config_or.ok()) {
    return;
  }
  if (config_or->enabled() &&
      config_or->auto_sync_mode() == commands::UserSyncConfig::ON_SHUTDOWN &&
      config_or->has_sync_key()) {
    callback_();
  }
}

}  // namespace sync
}  // namespace mozc
