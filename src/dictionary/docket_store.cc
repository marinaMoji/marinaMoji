#include "dictionary/docket_store.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/functional/function_ref.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "base/file_util.h"
#include "base/process_mutex.h"
#include "base/system_util.h"

namespace mozc {
namespace dictionary {
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

// Parses a JSON string literal starting at the opening quote. Advances
// |s| past the closing quote. Returns "" (and leaves |s| unchanged) if
// |s| doesn't start with a quote.
std::string ParseJsonString(absl::string_view& s) {
  if (s.empty() || s.front() != '"') {
    return "";
  }
  s.remove_prefix(1);
  std::string out;
  while (!s.empty()) {
    const char c = s.front();
    s.remove_prefix(1);
    if (c == '"') {
      break;
    }
    if (c == '\\' && !s.empty()) {
      const char esc = s.front();
      s.remove_prefix(1);
      switch (esc) {
        case 'n':
          out += '\n';
          break;
        case 'r':
          out += '\r';
          break;
        default:
          out += esc;
          break;
      }
      continue;
    }
    out += c;
  }
  return out;
}

void SkipSpace(absl::string_view& s) {
  while (!s.empty() && absl::ascii_isspace(s.front())) {
    s.remove_prefix(1);
  }
}

// Extracts the substring between the matching outer bracket pair (e.g.
// "[...]" or "{...}") that follows "key": in |json|, or "" if not found.
absl::string_view ExtractJsonBlock(absl::string_view json, absl::string_view key,
                                    char open, char close) {
  const std::string pattern = absl::StrCat("\"", key, "\":");
  const size_t pos = json.find(pattern);
  if (pos == absl::string_view::npos) {
    return "";
  }
  absl::string_view tail = json.substr(pos + pattern.size());
  SkipSpace(tail);
  if (tail.empty() || tail.front() != open) {
    return "";
  }
  int depth = 0;
  for (size_t i = 0; i < tail.size(); ++i) {
    if (tail[i] == open) {
      ++depth;
    } else if (tail[i] == close) {
      --depth;
      if (depth == 0) {
        return tail.substr(1, i - 1);
      }
    }
  }
  return "";
}

// Splits a sequence of top-level "{...}" objects (as found inside a JSON
// array) into their individual (unparsed) bodies.
std::vector<absl::string_view> SplitJsonObjects(absl::string_view block) {
  std::vector<absl::string_view> objects;
  int depth = 0;
  size_t start = 0;
  for (size_t i = 0; i < block.size(); ++i) {
    if (block[i] == '{') {
      if (depth == 0) {
        start = i;
      }
      ++depth;
    } else if (block[i] == '}') {
      --depth;
      if (depth == 0) {
        objects.push_back(block.substr(start, i - start + 1));
      }
    }
  }
  return objects;
}

DocketEntry ParseDocketEntry(absl::string_view obj) {
  DocketEntry entry;
  // Simple field extraction (flat object, no nesting needed for entries).
  auto extract_string = [&](absl::string_view key) -> std::string {
    const std::string pattern = absl::StrCat("\"", key, "\":");
    const size_t pos = obj.find(pattern);
    if (pos == absl::string_view::npos) {
      return "";
    }
    absl::string_view tail = obj.substr(pos + pattern.size());
    SkipSpace(tail);
    return ParseJsonString(tail);
  };
  auto extract_int = [&](absl::string_view key, int64_t default_value) -> int64_t {
    const std::string pattern = absl::StrCat("\"", key, "\":");
    const size_t pos = obj.find(pattern);
    if (pos == absl::string_view::npos) {
      return default_value;
    }
    absl::string_view tail = obj.substr(pos + pattern.size());
    SkipSpace(tail);
    int64_t value = default_value;
    absl::SimpleAtoi(tail, &value);
    return value;
  };
  entry.surface = extract_string("surface");
  entry.reading = extract_string("reading");
  entry.lid = static_cast<int32_t>(extract_int("lid", -1));
  entry.rid = static_cast<int32_t>(extract_int("rid", -1));
  entry.pos = extract_string("pos");
  entry.timestamp_unix = extract_int("timestamp", 0);
  return entry;
}

std::vector<std::string> ParseStringArray(absl::string_view block) {
  std::vector<std::string> out;
  absl::string_view s = block;
  while (true) {
    SkipSpace(s);
    if (s.empty() || s.front() != '"') {
      break;
    }
    out.push_back(ParseJsonString(s));
    SkipSpace(s);
    if (!s.empty() && s.front() == ',') {
      s.remove_prefix(1);
      continue;
    }
    break;
  }
  return out;
}

std::string SerializeDocketData(const DocketData& data) {
  std::string out = "{\n  \"pending\": [\n";
  for (size_t i = 0; i < data.pending.size(); ++i) {
    const DocketEntry& e = data.pending[i];
    absl::StrAppend(&out, "    {\"surface\": \"", JsonEscape(e.surface),
                    "\", \"reading\": \"", JsonEscape(e.reading),
                    "\", \"lid\": ", e.lid, ", \"rid\": ", e.rid,
                    ", \"pos\": \"", JsonEscape(e.pos),
                    "\", \"timestamp\": ", e.timestamp_unix, "}");
    if (i + 1 < data.pending.size()) {
      absl::StrAppend(&out, ",");
    }
    absl::StrAppend(&out, "\n");
  }
  absl::StrAppend(&out, "  ],\n  \"never\": [\n");
  for (size_t i = 0; i < data.never.size(); ++i) {
    absl::StrAppend(&out, "    \"", JsonEscape(data.never[i]), "\"");
    if (i + 1 < data.never.size()) {
      absl::StrAppend(&out, ",");
    }
    absl::StrAppend(&out, "\n");
  }
  absl::StrAppend(&out, "  ]\n}\n");
  return out;
}

absl::StatusOr<DocketData> ReadDocketDataFrom(absl::string_view path) {
  std::ifstream ifs{std::string(path)};
  if (!ifs) {
    return DocketData();  // No file yet: empty docket.
  }
  std::ostringstream oss;
  oss << ifs.rdbuf();
  const std::string json = oss.str();

  DocketData data;
  const absl::string_view pending_block =
      ExtractJsonBlock(json, "pending", '[', ']');
  for (absl::string_view obj : SplitJsonObjects(pending_block)) {
    data.pending.push_back(ParseDocketEntry(obj));
  }
  const absl::string_view never_block =
      ExtractJsonBlock(json, "never", '[', ']');
  data.never = ParseStringArray(never_block);
  return data;
}

absl::Status WriteDocketDataTo(absl::string_view path, const DocketData& data) {
  const std::string path_str(path);
  const std::string tmp = absl::StrCat(path_str, ".tmp");
  {
    std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
    if (!ofs) {
      return absl::PermissionDeniedError(
          absl::StrCat("Cannot write ", tmp));
    }
    ofs << SerializeDocketData(data);
  }
  return FileUtil::AtomicRename(tmp, path_str);
}

}  // namespace

std::string GetDocketStorePath() {
  return FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(),
                            "docket.json");
}

absl::StatusOr<DocketData> ReadDocketDataUnlocked() {
  return ReadDocketDataFrom(GetDocketStorePath());
}

DocketStore::DocketStore() : DocketStore(GetDocketStorePath()) {}

DocketStore::DocketStore(std::string filename)
    : filename_(std::move(filename)),
      process_mutex_(
          std::make_unique<ProcessMutex>(FileUtil::Basename(filename_))) {}

DocketStore::~DocketStore() { process_mutex_->UnLock(); }

namespace {

// Runs |mutator| against the current on-disk state under the process
// lock, then writes the result back. |mutator| returns true if the data
// actually changed (to avoid a needless write).
absl::Status MutateUnderLock(
    ProcessMutex& mutex, absl::string_view filename,
    absl::FunctionRef<bool(DocketData&)> mutator) {
  if (!mutex.Lock()) {
    return absl::FailedPreconditionError("Failed to lock docket store");
  }
  absl::Status status = absl::OkStatus();
  {
    absl::StatusOr<DocketData> data = ReadDocketDataFrom(filename);
    if (!data.ok()) {
      status = data.status();
    } else if (mutator(*data)) {
      status = WriteDocketDataTo(filename, *data);
    }
  }
  mutex.UnLock();
  return status;
}

}  // namespace

absl::Status DocketStore::AddPending(absl::string_view surface,
                                     absl::string_view reading, int32_t lid,
                                     int32_t rid, absl::string_view pos) {
  const std::string surface_copy(surface);
  const std::string reading_copy(reading);
  const std::string pos_copy(pos);
  return MutateUnderLock(
      *process_mutex_, filename_, [&](DocketData& data) {
        if (absl::c_linear_search(data.never, surface_copy)) {
          return false;
        }
        if (absl::c_any_of(data.pending, [&](const DocketEntry& e) {
              return e.surface == surface_copy;
            })) {
          return false;
        }
        DocketEntry entry;
        entry.surface = surface_copy;
        entry.reading = reading_copy;
        entry.lid = lid;
        entry.rid = rid;
        entry.pos = pos_copy;
        entry.timestamp_unix = absl::ToUnixSeconds(absl::Now());
        data.pending.push_back(std::move(entry));
        if (data.pending.size() > kDocketMaxPendingSize) {
          data.pending.erase(data.pending.begin(),
                             data.pending.begin() +
                                 (data.pending.size() - kDocketMaxPendingSize));
        }
        return true;
      });
}

absl::Status DocketStore::RemovePending(absl::string_view surface) {
  const std::string surface_copy(surface);
  return MutateUnderLock(
      *process_mutex_, filename_, [&](DocketData& data) {
        const size_t before = data.pending.size();
        data.pending.erase(
            std::remove_if(data.pending.begin(), data.pending.end(),
                           [&](const DocketEntry& e) {
                             return e.surface == surface_copy;
                           }),
            data.pending.end());
        return data.pending.size() != before;
      });
}

absl::Status DocketStore::AddNever(absl::string_view surface) {
  const std::string surface_copy(surface);
  return MutateUnderLock(
      *process_mutex_, filename_, [&](DocketData& data) {
        const size_t before = data.pending.size();
        data.pending.erase(
            std::remove_if(data.pending.begin(), data.pending.end(),
                           [&](const DocketEntry& e) {
                             return e.surface == surface_copy;
                           }),
            data.pending.end());
        bool changed = data.pending.size() != before;
        if (!absl::c_linear_search(data.never, surface_copy)) {
          data.never.push_back(surface_copy);
          changed = true;
        }
        return changed;
      });
}

}  // namespace dictionary
}  // namespace mozc
