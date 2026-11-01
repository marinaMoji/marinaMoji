#include "sync/sync_character_form.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "base/file_util.h"
#include "base/number_util.h"
#include "base/system_util.h"
#include "config/character_form_manager.h"
#include "protocol/config.pb.h"

namespace mozc {
namespace sync {
namespace {

constexpr absl::string_view kLocalStateFile = "character_forms.local.tsv";
constexpr absl::string_view kFormRecord = "form";
constexpr absl::string_view kNumberRecord = "number";

// NumberString::Style was stored in 4 bits in the old cform.db; anything
// larger is garbage.
constexpr uint32_t kMaxNumberStyle = 15;

bool IsValidForm(uint32_t form) {
  return config::Config::CharacterForm_IsValid(static_cast<int>(form));
}

// The form of "0" and the number style are stamped separately, so the
// per-key winners can come from different devices and disagree about the
// width. CharacterFormManager keeps them aligned on a single device, so settle
// it here, where the timestamps are known: the more recent of the two wins and
// the other takes its width and stamp, so that every device reaches the same
// state. Width-independent number styles keep their style.
void ReconcileNumberForm(CharacterFormSyncState* state) {
  constexpr uint32_t kZero = 0x0030;
  constexpr uint32_t kHalf = config::Config::HALF_WIDTH;
  constexpr uint32_t kFull = config::Config::FULL_WIDTH;
  if (!state->number_style.has_value()) {
    return;
  }
  const auto it = state->forms.find(kZero);
  if (it == state->forms.end()) {
    return;
  }
  StampedCharacterForm& zero = it->second;
  StampedNumberStyle& number = *state->number_style;
  const auto is_width = [](uint32_t form) {
    return form == kHalf || form == kFull;
  };
  if (zero.form == number.form || !is_width(zero.form) ||
      !is_width(number.form)) {
    return;
  }
  if (number.updated_at > zero.updated_at) {
    zero.form = number.form;
    zero.updated_at = number.updated_at;
  } else {
    number.form = zero.form;
    number.style = static_cast<uint32_t>(config::AdaptNumberStyleToForm(
        static_cast<NumberUtil::NumberString::Style>(number.style),
        static_cast<config::Config::CharacterForm>(zero.form)));
    number.updated_at = zero.updated_at;
  }
}

std::string GetLocalStatePath() {
  return FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(),
                            std::string(kLocalStateFile));
}

}  // namespace

CharacterFormSyncState ParseCharacterFormTsv(absl::string_view tsv) {
  CharacterFormSyncState state;
  for (absl::string_view line : absl::StrSplit(tsv, '\n')) {
    line = absl::StripAsciiWhitespace(line);
    if (line.empty() || absl::StartsWith(line, "#")) {
      continue;
    }
    const std::vector<absl::string_view> fields = absl::StrSplit(line, '\t');
    if (fields.size() < 4) {
      continue;
    }
    if (fields[0] == kFormRecord) {
      uint32_t ucs2 = 0;
      StampedCharacterForm value;
      if (!absl::SimpleHexAtoi(fields[1], &ucs2) || ucs2 > 0xFFFF ||
          !absl::SimpleAtoi(fields[2], &value.form) ||
          !IsValidForm(value.form) ||
          !absl::SimpleAtoi(fields[3], &value.updated_at)) {
        continue;
      }
      state.forms[ucs2] = value;
    } else if (fields[0] == kNumberRecord) {
      StampedNumberStyle value;
      if (!absl::SimpleAtoi(fields[1], &value.form) ||
          !IsValidForm(value.form) ||
          !absl::SimpleAtoi(fields[2], &value.style) ||
          value.style > kMaxNumberStyle ||
          !absl::SimpleAtoi(fields[3], &value.updated_at)) {
        continue;
      }
      state.number_style = value;
    }
    // Unknown record types are left for newer versions.
  }
  return state;
}

std::string SerializeCharacterFormTsv(const CharacterFormSyncState& state) {
  std::string out(kCharacterFormHeader);
  for (const auto& [ucs2, value] : state.forms) {
    absl::StrAppendFormat(&out, "%s\t%04X\t%u\t%d\n", kFormRecord, ucs2,
                          value.form, value.updated_at);
  }
  if (state.number_style.has_value()) {
    const StampedNumberStyle& n = *state.number_style;
    absl::StrAppend(&out, kNumberRecord, "\t", n.form, "\t", n.style, "\t",
                    n.updated_at, "\n");
  }
  return out;
}

CharacterFormSyncState StampCharacterForms(
    const CharacterFormHistory& local, const CharacterFormSyncState& previous,
    int64_t now) {
  CharacterFormSyncState state;
  for (const auto& [ucs2, form] : local.character_form_map()) {
    if (ucs2 > 0xFFFF || !IsValidForm(form)) {
      continue;
    }
    const auto it = previous.forms.find(ucs2);
    const bool unchanged = it != previous.forms.end() && it->second.form == form;
    state.forms[ucs2] = {form, unchanged ? it->second.updated_at : now};
  }
  if (local.has_last_number_form() && local.has_last_number_style() &&
      IsValidForm(local.last_number_form()) &&
      local.last_number_style() <= kMaxNumberStyle) {
    StampedNumberStyle n{local.last_number_form(), local.last_number_style(),
                         now};
    if (previous.number_style.has_value() &&
        previous.number_style->form == n.form &&
        previous.number_style->style == n.style) {
      n.updated_at = previous.number_style->updated_at;
    }
    state.number_style = n;
  }
  return state;
}

CharacterFormSyncState MergeCharacterForms(const CharacterFormSyncState& remote,
                                           const CharacterFormSyncState& local,
                                           CharacterFormMergeStats* stats) {
  CharacterFormSyncState merged = local;
  int from_remote = 0;
  for (const auto& [ucs2, value] : remote.forms) {
    auto [it, inserted] = merged.forms.try_emplace(ucs2, value);
    if (inserted) {
      ++from_remote;
    } else if (value.updated_at > it->second.updated_at) {
      if (value.form != it->second.form) {
        ++from_remote;
      }
      it->second = value;
    }
  }
  if (remote.number_style.has_value() &&
      (!merged.number_style.has_value() ||
       remote.number_style->updated_at > merged.number_style->updated_at)) {
    if (!merged.number_style.has_value() ||
        merged.number_style->form != remote.number_style->form ||
        merged.number_style->style != remote.number_style->style) {
      ++from_remote;
    }
    merged.number_style = remote.number_style;
  }
  ReconcileNumberForm(&merged);
  if (stats != nullptr) {
    stats->from_remote = from_remote;
  }
  return merged;
}

CharacterFormHistory ToCharacterFormHistory(
    const CharacterFormSyncState& state) {
  CharacterFormHistory history;
  auto* map = history.mutable_character_form_map();
  for (const auto& [ucs2, value] : state.forms) {
    (*map)[ucs2] = value.form;
  }
  if (state.number_style.has_value()) {
    history.set_last_number_form(state.number_style->form);
    history.set_last_number_style(state.number_style->style);
  }
  return history;
}

CharacterFormSyncState LoadLocalCharacterFormSyncState() {
  const std::string path = GetLocalStatePath();
  if (!FileUtil::FileExists(path).ok()) {
    return {};
  }
  const absl::StatusOr<std::string> data = FileUtil::GetContents(path);
  if (!data.ok()) {
    return {};
  }
  return ParseCharacterFormTsv(*data);
}

absl::Status SaveLocalCharacterFormSyncState(
    const CharacterFormSyncState& state) {
  return FileUtil::SetContents(GetLocalStatePath(),
                               SerializeCharacterFormTsv(state));
}

}  // namespace sync
}  // namespace mozc
