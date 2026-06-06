#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <thread>

#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "base/init_mozc.h"
#include "base/run_level.h"
#include "client/client.h"
#include "sync/sync_config.h"
#include "sync/sync_runner.h"
#include "sync/sync_status.h"

namespace mozc {
namespace sync {
namespace {

void WriteStartupError(absl::string_view message) {
  SyncStatus status;
  status.state = "error";
  status.message = std::string(message);
  WriteSyncStatus(status).IgnoreError();
}

void PrintUsage() {
  std::cerr << "Usage: marinaMojiSync [--now | --daemon | --status]\n";
}

int PrintStatus() {
  const auto status_or = ReadSyncStatus();
  if (!status_or.ok()) {
    std::cerr << "idle\n";
    return 1;
  }
  std::cout << "state=" << status_or->state << "\n"
            << "phase=" << status_or->phase << "\n"
            << "progress=" << status_or->progress << "\n"
            << "message=" << status_or->message << "\n";
  return status_or->state == "error" ? 1 : 0;
}

int RunOnce(bool force) {
  if (!RunLevel::IsValidClientRunLevel()) {
    WriteStartupError("marinaMojiSync cannot run at this privilege level");
    return 1;
  }
  client::Client client;
  client.set_timeout(absl::Minutes(5));
  RunSyncOptions options;
  options.force = force;
  options.skip_cooldown = force;
  const auto report_or = RunSync(&client, options);
  if (!report_or.ok()) {
    std::cerr << report_or.status().ToString() << "\n";
    return 1;
  }
  if (!report_or->success()) {
    std::cerr << report_or->error_message() << "\n";
    return 1;
  }
  return 0;
}

int RunDaemon() {
  if (!RunLevel::IsValidClientRunLevel()) {
    return 1;
  }
  std::string tracked_path;
  std::optional<std::filesystem::file_time_type> tracked_mtime;

  while (true) {
    const auto config_or = LoadSyncConfig();
    int sleep_sec = 60;
    if (config_or.ok()) {
      const commands::UserSyncConfig& config = *config_or;
      sleep_sec =
          std::max(1, config.auto_sync_interval_minutes() * 60);
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
          client::Client client;
          client.set_timeout(absl::Minutes(5));
          if (CanAutoSync(&client, config.sync_cooldown_seconds())) {
            RunSyncOptions options;
            RunSync(&client, options);
          }
        }
      }
    }
    for (int i = 0; i < sleep_sec; ++i) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
}

}  // namespace
}  // namespace sync
}  // namespace mozc

int main(int argc, char* argv[]) {
  mozc::InitMozc(argv[0], &argc, &argv);

  bool run_now = false;
  bool run_daemon = false;
  bool print_status = false;
  bool force = false;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--now") {
      run_now = true;
    } else if (arg == "--daemon") {
      run_daemon = true;
    } else if (arg == "--status") {
      print_status = true;
    } else if (arg == "--force") {
      force = true;
    } else if (arg == "--help" || arg == "-h") {
      mozc::sync::PrintUsage();
      return 0;
    }
  }

  if (print_status) {
    return mozc::sync::PrintStatus();
  }
  if (run_daemon) {
    return mozc::sync::RunDaemon();
  }
  if (run_now) {
    return mozc::sync::RunOnce(force);
  }
  mozc::sync::PrintUsage();
  return 1;
}
