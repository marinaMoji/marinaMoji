#include "sync/sync_config.h"

#include "sync/sync_key.h"

#include <fstream>
#include <sstream>
#include <string>

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "base/file_util.h"
#include "base/random.h"
#include "base/system_util.h"
#include "protocol/commands.pb.h"

namespace mozc {
namespace sync {
namespace {

constexpr absl::string_view kSyncConfigFile = "user://sync.conf";

std::string JsonEscape(absl::string_view s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      default:
        out += c;
        break;
    }
  }
  return out;
}

std::string ExtractJsonString(absl::string_view json, absl::string_view key) {
  const std::string pattern = absl::StrCat("\"", key, "\":");
  const size_t pos = json.find(pattern);
  if (pos == std::string::npos) {
    return "";
  }
  absl::string_view tail = json.substr(pos + pattern.size());
  while (!tail.empty() && absl::ascii_isspace(tail.front())) {
    tail.remove_prefix(1);
  }
  if (tail.empty() || tail.front() != '"') {
    return "";
  }
  tail.remove_prefix(1);
  std::string out;
  while (!tail.empty()) {
    const char c = tail.front();
    tail.remove_prefix(1);
    if (c == '\\' && !tail.empty()) {
      const char esc = tail.front();
      tail.remove_prefix(1);
      if (esc == 'n') {
        out += '\n';
      } else if (esc == 'r') {
        out += '\r';
      } else if (esc == '\\' || esc == '"') {
        out += esc;
      } else {
        out += esc;
      }
      continue;
    }
    if (c == '"') {
      break;
    }
    out += c;
  }
  return out;
}

bool ExtractJsonBool(absl::string_view json, absl::string_view key,
                     bool default_value) {
  const std::string pattern = absl::StrCat("\"", key, "\":");
  const size_t pos = json.find(pattern);
  if (pos == std::string::npos) {
    return default_value;
  }
  absl::string_view tail = json.substr(pos + pattern.size());
  while (!tail.empty() && absl::ascii_isspace(tail.front())) {
    tail.remove_prefix(1);
  }
  if (absl::StartsWith(tail, "true")) {
    return true;
  }
  if (absl::StartsWith(tail, "false")) {
    return false;
  }
  return default_value;
}

int ExtractJsonInt(absl::string_view json, absl::string_view key,
                   int default_value) {
  const std::string pattern = absl::StrCat("\"", key, "\":");
  const size_t pos = json.find(pattern);
  if (pos == std::string::npos) {
    return default_value;
  }
  absl::string_view tail = json.substr(pos + pattern.size());
  while (!tail.empty() && absl::ascii_isspace(tail.front())) {
    tail.remove_prefix(1);
  }
  int value = default_value;
  absl::SimpleAtoi(tail, &value);
  return value;
}

}  // namespace

std::string GetSyncConfigPath() {
  return FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(), "sync.conf");
}

commands::UserSyncConfig EnsureDeviceId(commands::UserSyncConfig config) {
  if (!config.device_id().empty()) {
    return config;
  }
  config.set_device_id(absl::StrFormat(
      "%016x", static_cast<uint64_t>(Random()())));
  return config;
}

absl::StatusOr<commands::UserSyncConfig> LoadSyncConfig() {
  const std::string path = GetSyncConfigPath();
  std::ifstream ifs(path);
  if (!ifs) {
    commands::UserSyncConfig config;
    config = EnsureDeviceId(config);
    config.set_has_sync_key(HasStoredSyncKey());
    return config;
  }
  std::ostringstream oss;
  oss << ifs.rdbuf();
  const std::string json = oss.str();

  commands::UserSyncConfig config;
  config.set_enabled(ExtractJsonBool(json, "enabled", false));
  config.set_sync_file_path(ExtractJsonString(json, "sync_file_path"));
  config.set_sync_settings(ExtractJsonBool(json, "sync_settings", true));
  config.set_sync_dictionary(ExtractJsonBool(json, "sync_dictionary", true));
  config.set_sync_history(ExtractJsonBool(json, "sync_history", true));
  config.set_direction(static_cast<commands::UserSyncConfig::Direction>(
      ExtractJsonInt(json, "direction", commands::UserSyncConfig::BIDIRECTIONAL)));
  config.set_auto_sync_mode(static_cast<commands::UserSyncConfig::AutoSyncMode>(
      ExtractJsonInt(json, "auto_sync_mode",
                     commands::UserSyncConfig::NEVER)));
  config.set_auto_sync_interval_minutes(
      ExtractJsonInt(json, "auto_sync_interval_minutes", 30));
  config.set_last_sync_time(ExtractJsonString(json, "last_sync_time"));
  config.set_last_sync_status(ExtractJsonString(json, "last_sync_status"));
  config.set_last_sync_message(ExtractJsonString(json, "last_sync_message"));
  config.set_device_id(ExtractJsonString(json, "device_id"));
  config.set_sync_cooldown_seconds(
      ExtractJsonInt(json, "sync_cooldown_seconds", 60));
  config = EnsureDeviceId(config);
  config.set_has_sync_key(HasStoredSyncKey());
  return config;
}

absl::Status SaveSyncConfig(const commands::UserSyncConfig& config) {
  const commands::UserSyncConfig normalized = EnsureDeviceId(config);
  const std::string json = absl::StrCat(
      "{\n"
      "  \"enabled\": ",
      normalized.enabled() ? "true" : "false", ",\n"
      "  \"sync_file_path\": \"", JsonEscape(normalized.sync_file_path()),
      "\",\n"
      "  \"sync_settings\": ",
      normalized.sync_settings() ? "true" : "false", ",\n"
      "  \"sync_dictionary\": ",
      normalized.sync_dictionary() ? "true" : "false", ",\n"
      "  \"sync_history\": ",
      normalized.sync_history() ? "true" : "false", ",\n"
      "  \"direction\": ", normalized.direction(), ",\n"
      "  \"auto_sync_mode\": ", normalized.auto_sync_mode(), ",\n"
      "  \"auto_sync_interval_minutes\": ",
      normalized.auto_sync_interval_minutes(), ",\n"
      "  \"last_sync_time\": \"", JsonEscape(normalized.last_sync_time()),
      "\",\n"
      "  \"last_sync_status\": \"",
      JsonEscape(normalized.last_sync_status()), "\",\n"
      "  \"last_sync_message\": \"",
      JsonEscape(normalized.last_sync_message()), "\",\n"
      "  \"device_id\": \"", JsonEscape(normalized.device_id()), "\",\n"
      "  \"sync_cooldown_seconds\": ",
      normalized.sync_cooldown_seconds(), "\n"
      "}\n");
  const std::string path = GetSyncConfigPath();
  const std::string tmp = path + ".tmp";
  {
    std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
    if (!ofs) {
      return absl::PermissionDeniedError("Cannot write sync.conf");
    }
    ofs << json;
  }
  return FileUtil::AtomicRename(tmp, path);
}

}  // namespace sync
}  // namespace mozc
