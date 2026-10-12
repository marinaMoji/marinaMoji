#include "config/legacy_character_form_db.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>

#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "base/config_file_stream.h"
#include "base/file_util.h"
#include "protocol/config.pb.h"
#include "storage/lru_storage.h"

namespace mozc {
namespace config {
namespace {

constexpr char kLegacyFileName[] = "user://cform.db";

// Key under which the old NumberStyleManager kept the last number style.
// GetNormalizedCharacter() never returns it, so it is not a character entry.
constexpr char16_t kNumberStyleKey = 0x004E;  // "N"

// Byte-for-byte copy of the old NumberStyleManager::NumberStyleEntry. The
// bit-field layout is compiler-defined, but cform.db never leaves the machine
// that wrote it and this is built with the same compiler, so declaring the
// same fields reads it back the way it was written.
struct LegacyNumberStyleEntry {
  [[maybe_unused]] uint32_t reversed_ : 26;
  uint32_t form_ : 2;
  uint32_t style_ : 4;
};
static_assert(sizeof(LegacyNumberStyleEntry) == sizeof(uint32_t));

// The old code used the native-endian bytes of the char16_t as the key.
absl::string_view KeyOf(const char16_t& ucs2) {
  return absl::string_view(reinterpret_cast<const char*>(&ucs2),
                           sizeof(ucs2));
}

void DeleteLegacyFile(absl::string_view path) {
  if (const absl::Status s = FileUtil::UnlinkIfExists(path); !s.ok()) {
    LOG(ERROR) << "Cannot delete " << path << ": " << s;
  }
}

}  // namespace

std::string GetLegacyCharacterFormDbPath() {
  return ConfigFileStream::GetFileName(kLegacyFileName);
}

std::optional<LegacyCharacterFormHistory> TakeLegacyCharacterFormDb(
    absl::string_view path) {
  if (!FileUtil::FileExists(path).ok()) {
    return std::nullopt;
  }

  LegacyCharacterFormHistory history;
  {
    std::unique_ptr<storage::LruStorage> storage =
        storage::LruStorage::Create(std::string(path).c_str());
    if (!storage || storage->value_size() != sizeof(uint32_t)) {
      LOG(ERROR) << "Ignoring unreadable legacy " << path;
    } else {
      // LruStorage keeps only seeded fingerprints of its keys, so the
      // entries cannot be listed; probe every possible UCS2 key instead
      // (64K hash lookups, once).
      auto* map = history.mutable_character_form_map();
      for (uint32_t i = 0; i <= 0xFFFF; ++i) {
        const char16_t ucs2 = static_cast<char16_t>(i);
        const char* value = storage->Lookup(KeyOf(ucs2));
        if (value == nullptr) {
          continue;
        }
        if (ucs2 == kNumberStyleKey) {
          LegacyNumberStyleEntry entry;
          std::memcpy(&entry, value, sizeof(entry));
          history.set_last_number_form(entry.form_);
          history.set_last_number_style(entry.style_);
          continue;
        }
        uint32_t form = 0;
        std::memcpy(&form, value, sizeof(form));
        if (Config::CharacterForm_IsValid(form)) {
          (*map)[ucs2] = form;
        }
      }
      LOG(INFO) << "Migrated " << map->size() << " character forms from "
                << path;
    }
  }  // Unmaps the file before deleting it (required on Windows).

  DeleteLegacyFile(path);
  return history;
}

bool MergeMissingCharacterForms(const LegacyCharacterFormHistory& legacy,
                                LegacyCharacterFormHistory* history) {
  bool changed = false;
  auto* map = history->mutable_character_form_map();
  for (const auto& [ucs2, form] : legacy.character_form_map()) {
    changed |= map->try_emplace(ucs2, form).second;
  }
  if (legacy.has_last_number_form() && legacy.has_last_number_style() &&
      !(history->has_last_number_form() && history->has_last_number_style())) {
    history->set_last_number_form(legacy.last_number_form());
    history->set_last_number_style(legacy.last_number_style());
    changed = true;
  }
  return changed;
}

}  // namespace config
}  // namespace mozc
