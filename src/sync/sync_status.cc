#include "sync/sync_status.h"

#include <fstream>
#include <sstream>

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "base/file_util.h"
#include "base/system_util.h"

namespace mozc {
namespace sync {
namespace {

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

double ExtractJsonDouble(absl::string_view json, absl::string_view key,
                         double default_value) {
  const std::string pattern = absl::StrCat("\"", key, "\":");
  const size_t pos = json.find(pattern);
  if (pos == std::string::npos) {
    return default_value;
  }
  absl::string_view tail = json.substr(pos + pattern.size());
  while (!tail.empty() && absl::ascii_isspace(tail.front())) {
    tail.remove_prefix(1);
  }
  double value = default_value;
  absl::SimpleAtod(tail, &value);
  return value;
}

int64_t ExtractJsonInt64(absl::string_view json, absl::string_view key,
                           int64_t default_value) {
  const std::string pattern = absl::StrCat("\"", key, "\":");
  const size_t pos = json.find(pattern);
  if (pos == std::string::npos) {
    return default_value;
  }
  absl::string_view tail = json.substr(pos + pattern.size());
  while (!tail.empty() && absl::ascii_isspace(tail.front())) {
    tail.remove_prefix(1);
  }
  int64_t value = default_value;
  absl::SimpleAtoi(tail, &value);
  return value;
}

}  // namespace

std::string GetSyncStatusPath() {
  return FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(),
                            "sync.status.json");
}

absl::Status WriteSyncStatus(const SyncStatus& status) {
  SyncStatus out = status;
  if (out.updated_at_unix == 0) {
    out.updated_at_unix = absl::ToUnixSeconds(absl::Now());
  }
  const std::string json = absl::StrCat(
      "{\n"
      "  \"state\": \"",
      JsonEscape(out.state), "\",\n"
      "  \"phase\": \"",
      JsonEscape(out.phase), "\",\n"
      "  \"progress\": ",
      out.progress, ",\n"
      "  \"message\": \"",
      JsonEscape(out.message), "\",\n"
      "  \"updated_at_unix\": ",
      out.updated_at_unix, "\n"
      "}\n");
  const std::string path = GetSyncStatusPath();
  const std::string tmp = path + ".tmp";
  {
    std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
    if (!ofs) {
      return absl::PermissionDeniedError("Cannot write sync.status.json");
    }
    ofs << json;
  }
  return FileUtil::AtomicRename(tmp, path);
}

absl::StatusOr<SyncStatus> ReadSyncStatus() {
  const std::string path = GetSyncStatusPath();
  std::ifstream ifs(path);
  if (!ifs) {
    SyncStatus idle;
    idle.state = "idle";
    return idle;
  }
  std::ostringstream oss;
  oss << ifs.rdbuf();
  const std::string json = oss.str();

  SyncStatus status;
  status.state = ExtractJsonString(json, "state");
  if (status.state.empty()) {
    status.state = "idle";
  }
  status.phase = ExtractJsonString(json, "phase");
  status.progress = ExtractJsonDouble(json, "progress", 0.0);
  status.message = ExtractJsonString(json, "message");
  status.updated_at_unix = ExtractJsonInt64(json, "updated_at_unix", 0);
  return status;
}

bool IsSyncRunning() {
  const auto status_or = ReadSyncStatus();
  if (!status_or.ok()) {
    return false;
  }
  if (status_or->state != "running") {
    return false;
  }
  if (status_or->updated_at_unix > 0) {
    const int64_t age =
        absl::ToUnixSeconds(absl::Now()) - status_or->updated_at_unix;
    if (age > 600) {
      return false;
    }
  }
  return true;
}

}  // namespace sync
}  // namespace mozc
