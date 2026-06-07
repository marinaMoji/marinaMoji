#include "sync/sync_activity.h"

#include <fstream>
#include <sstream>

#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/time/clock.h"
#include "base/file_util.h"
#include "base/system_util.h"

namespace mozc {
namespace sync {
namespace {

int64_t ExtractUnixField(absl::string_view json, absl::string_view key) {
  const std::string pattern = absl::StrCat("\"", key, "\":");
  const size_t pos = json.find(pattern);
  if (pos == std::string::npos) {
    return 0;
  }
  int64_t value = 0;
  absl::SimpleAtoi(json.substr(pos + pattern.size()), &value);
  return value;
}

absl::Status WriteActivityJson(int64_t composition_end, int64_t ime_deactivated) {
  const std::string json = absl::StrCat(
      "{\n"
      "  \"last_composition_end\": ",
      composition_end, ",\n"
      "  \"last_ime_deactivated\": ",
      ime_deactivated, "\n"
      "}\n");
  const std::string path = GetSyncActivityPath();
  const std::string tmp = path + ".tmp";
  {
    std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
    if (!ofs) {
      return absl::PermissionDeniedError("Cannot write sync.activity.json");
    }
    ofs << json;
  }
  return FileUtil::AtomicRename(tmp, path);
}

}  // namespace

std::string GetSyncActivityPath() {
  return FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(),
                            "sync.activity.json");
}

absl::Status WriteSyncActivity(const SyncActivity& activity) {
  return WriteActivityJson(absl::ToUnixSeconds(activity.last_composition_end),
                           absl::ToUnixSeconds(activity.last_ime_deactivated));
}

absl::StatusOr<SyncActivity> ReadSyncActivity() {
  SyncActivity activity;
  const std::string path = GetSyncActivityPath();
  std::ifstream ifs(path);
  if (!ifs) {
    return activity;
  }
  std::ostringstream oss;
  oss << ifs.rdbuf();
  const std::string json = oss.str();
  const int64_t composition_end = ExtractUnixField(json, "last_composition_end");
  const int64_t ime_deactivated = ExtractUnixField(json, "last_ime_deactivated");
  if (composition_end > 0) {
    activity.last_composition_end = absl::FromUnixSeconds(composition_end);
  }
  if (ime_deactivated > 0) {
    activity.last_ime_deactivated = absl::FromUnixSeconds(ime_deactivated);
  }
  return activity;
}

void RecordCompositionEnd() {
  SyncActivity activity;
  if (auto current = ReadSyncActivity(); current.ok()) {
    activity = *current;
  }
  activity.last_composition_end = absl::Now();
  WriteSyncActivity(activity).IgnoreError();
}

void RecordImeDeactivated() {
  SyncActivity activity;
  if (auto current = ReadSyncActivity(); current.ok()) {
    activity = *current;
  }
  activity.last_ime_deactivated = absl::Now();
  WriteSyncActivity(activity).IgnoreError();
}

bool CooldownElapsed(int cooldown_seconds) {
  if (cooldown_seconds <= 0) {
    return true;
  }
  const auto activity_or = ReadSyncActivity();
  if (!activity_or.ok()) {
    return true;
  }
  const absl::Time now = absl::Now();
  const absl::Duration cooldown = absl::Seconds(cooldown_seconds);
  absl::Time last_activity = activity_or->last_composition_end;
  if (activity_or->last_ime_deactivated > last_activity) {
    last_activity = activity_or->last_ime_deactivated;
  }
  if (last_activity == absl::InfinitePast()) {
    return true;
  }
  return (now - last_activity) >= cooldown;
}

}  // namespace sync
}  // namespace mozc
