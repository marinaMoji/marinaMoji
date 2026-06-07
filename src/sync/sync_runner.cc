#include "sync/sync_runner.h"

#include "sync/sync_activity.h"
#include "sync/sync_config.h"
#include "sync/sync_key.h"
#include "sync/sync_poll.h"
#include "sync/sync_service.h"
#include "sync/sync_status.h"

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "config/config_handler.h"

namespace mozc {
namespace sync {
namespace {

void SetProgress(const std::string& phase, double progress,
                 const std::string& message) {
  SyncStatus status;
  status.state = "running";
  status.phase = phase;
  status.progress = progress;
  status.message = message;
  WriteSyncStatus(status).IgnoreError();
}

commands::UserSyncReport FailureReport(absl::string_view message) {
  commands::UserSyncReport report;
  report.set_success(false);
  report.set_error_message(std::string(message));
  SyncStatus status;
  status.state = "error";
  status.message = std::string(message);
  WriteSyncStatus(status).IgnoreError();
  return report;
}

class SyncLockGuard {
 public:
  explicit SyncLockGuard(client::ClientInterface* client) : client_(client) {
    locked_ = client_->BeginSyncLock();
  }
  ~SyncLockGuard() {
    if (locked_) {
      client_->EndSyncLock();
    }
  }
  bool locked() const { return locked_; }

 private:
  client::ClientInterface* client_;
  bool locked_ = false;
};

}  // namespace

bool CanAutoSync(client::ClientInterface* client, int cooldown_seconds) {
  if (IsSyncRunning()) {
    return false;
  }
  if (!client->EnsureConnection()) {
    return false;
  }
  commands::SyncState state;
  if (!client->GetSyncState(&state)) {
    return false;
  }
  if (state.sync_locked()) {
    return false;
  }
  if (state.any_composing()) {
    return false;
  }
  return CooldownElapsed(cooldown_seconds);
}

absl::StatusOr<commands::UserSyncReport> RunSync(
    client::ClientInterface* client, const RunSyncOptions& options) {
  if (IsSyncRunning()) {
    return FailureReport("Sync already in progress");
  }

  const auto config_or = LoadSyncConfig();
  if (!config_or.ok()) {
    return FailureReport(config_or.status().ToString());
  }
  commands::UserSyncConfig config = *config_or;

  if (!config.enabled()) {
    return FailureReport(
        "Sync is disabled. Enable sync in the Config dialog and save, then "
        "try again.");
  }
  if (config.sync_file_path().empty()) {
    return FailureReport("Sync file path is not set");
  }

  if (!options.force) {
    commands::SyncState state;
    if (!client->GetSyncState(&state)) {
      return FailureReport("Cannot query converter state");
    }
    if (state.any_composing()) {
      return FailureReport("Composition in progress");
    }
    if (!options.skip_cooldown &&
        !CooldownElapsed(config.sync_cooldown_seconds())) {
      return FailureReport("Cooldown period has not elapsed");
    }
  }

  const auto key_or = LoadSyncKey();
  if (!key_or.ok()) {
    return FailureReport("Sync key is not set");
  }

  if (!client->EnsureConnection()) {
    return FailureReport("Cannot connect to converter");
  }

  SetProgress("prepare", 0.05, "Preparing sync…");
  SyncLockGuard lock(client);
  if (!lock.locked()) {
    return FailureReport("Cannot lock converter for sync");
  }

  SetProgress("flush", 0.15, "Saving local data…");
  if (!client->SyncData()) {
    return FailureReport("Failed to flush converter data");
  }
  config::ConfigHandler::Reload();

  PerformSyncOptions sync_options;
  sync_options.config = config;
  sync_options.passphrase = *key_or;
  sync_options.direction = options.direction;
  sync_options.force = options.force;

  auto progress_callback = [](const std::string& phase, double progress,
                              const std::string& message) {
    SetProgress(phase, progress, message);
  };

  SetProgress("merge", 0.35, "Merging data…");
  absl::StatusOr<commands::UserSyncReport> report_or =
      PerformSync(sync_options, progress_callback);
  if (!report_or.ok()) {
    return FailureReport(report_or.status().ToString());
  }
  if (!report_or->success()) {
    SyncStatus status;
    status.state = "error";
    status.message = report_or->error_message();
    WriteSyncStatus(status).IgnoreError();
    return *report_or;
  }

  SetProgress("reload", 0.9, "Reloading converter…");
  if (!client->ReloadAndWait()) {
    return FailureReport("Failed to reload converter");
  }

  config.set_last_sync_time(
      absl::FormatTime(absl::Now(), absl::UTCTimeZone()));
  config.set_last_sync_status("OK");
  config.set_last_sync_message(absl::StrCat(
      "Dictionary +", report_or->dictionary_added(),
      ", history merged ", report_or->history_merged()));
  if (const auto snapshot_or = CaptureSyncFingerprints(config); snapshot_or.ok()) {
    SaveSyncBaselines(*snapshot_or).IgnoreError();
  }
  SaveSyncConfig(config).IgnoreError();

  SyncStatus done;
  done.state = "done";
  done.phase = "complete";
  done.progress = 1.0;
  done.message = "Sync completed";
  WriteSyncStatus(done).IgnoreError();

  return *report_or;
}

}  // namespace sync
}  // namespace mozc
