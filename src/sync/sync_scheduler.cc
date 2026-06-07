#include "sync/sync_scheduler.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <thread>

#include "absl/time/time.h"
#include "sync/sync_config.h"
#include "sync/sync_poll.h"

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
    while (!stop_) {
      const auto config_or = LoadSyncConfig();
      if (config_or.ok()) {
        const commands::UserSyncConfig& config = *config_or;
        if (config.enabled() && config.has_sync_key() &&
            config.auto_sync_mode() == commands::UserSyncConfig::EVERY_N_MINUTES) {
          const auto baseline_or = LoadSyncBaselines();
          const auto current_or = CaptureSyncFingerprints(config);
          if (baseline_or.ok() && current_or.ok()) {
            switch (EvaluateIntervalSync(*baseline_or, *current_or)) {
              case IntervalSyncDecision::kBaselineOnly:
                SaveSyncBaselines(*current_or).IgnoreError();
                break;
              case IntervalSyncDecision::kSync:
                callback_();
                break;
              case IntervalSyncDecision::kSkip:
                break;
            }
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
