#include "config/legacy_character_form_db.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>

#include "base/file_util.h"
#include "base/number_util.h"
#include "config/character_form_manager.h"
#include "prediction/user_history_predictor.pb.h"
#include "protocol/config.pb.h"
#include "storage/lru_storage.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace config {
namespace {

// Parameters and layout the pre-2026-10 CharacterFormManager used.
constexpr uint32_t kLruSize = 128;
constexpr uint32_t kSeedValue = 0x7fe1fed1;

struct OldNumberStyleEntry {
  [[maybe_unused]] uint32_t reversed_ : 26;
  uint32_t form_ : 2;
  uint32_t style_ : 4;
};

absl::string_view KeyOf(const char16_t& ucs2) {
  return absl::string_view(reinterpret_cast<const char*>(&ucs2),
                           sizeof(ucs2));
}

// Writes a cform.db the way the old CharacterFormManager did.
void WriteOldCformDb(const std::string& path) {
  std::unique_ptr<storage::LruStorage> storage = storage::LruStorage::Create(
      path.c_str(), sizeof(uint32_t), kLruSize, kSeedValue);
  ASSERT_NE(storage, nullptr);
  const uint32_t half = Config::HALF_WIDTH;
  const uint32_t full = Config::FULL_WIDTH;
  storage->Insert(KeyOf(u'ア'), reinterpret_cast<const char*>(&half));
  storage->Insert(KeyOf(u'0'), reinterpret_cast<const char*>(&half));
  storage->Insert(KeyOf(u'['), reinterpret_cast<const char*>(&full));
  OldNumberStyleEntry number = {};
  number.form_ = Config::HALF_WIDTH;
  number.style_ = NumberUtil::NumberString::NUMBER_KANJI;
  storage->Insert(KeyOf(u'N'), reinterpret_cast<const char*>(&number));
}

class LegacyCharacterFormDbTest : public testing::TestWithTempUserProfile {
 protected:
  void SetUp() override {
    CharacterFormManager::GetCharacterFormManager()->SetDefaultRule();
    CharacterFormManager::GetCharacterFormManager()->ClearHistory();
  }
  void TearDown() override {
    CharacterFormManager::GetCharacterFormManager()->SetDefaultRule();
    CharacterFormManager::GetCharacterFormManager()->ClearHistory();
  }
};

TEST_F(LegacyCharacterFormDbTest, NoFileReturnsNullopt) {
  EXPECT_EQ(TakeLegacyCharacterFormDb(GetLegacyCharacterFormDbPath()),
            std::nullopt);
}

TEST_F(LegacyCharacterFormDbTest, ReadsAndDeletesOldFile) {
  const std::string path = GetLegacyCharacterFormDbPath();
  WriteOldCformDb(path);

  const std::optional<LegacyCharacterFormHistory> legacy =
      TakeLegacyCharacterFormDb(path);
  ASSERT_TRUE(legacy.has_value());
  EXPECT_EQ(legacy->character_form_map().size(), 3);
  EXPECT_EQ(legacy->character_form_map().at(u'ア'), Config::HALF_WIDTH);
  EXPECT_EQ(legacy->character_form_map().at(u'0'), Config::HALF_WIDTH);
  EXPECT_EQ(legacy->character_form_map().at(u'['), Config::FULL_WIDTH);
  EXPECT_EQ(legacy->character_form_map().count(u'N'), 0);
  EXPECT_EQ(legacy->last_number_form(), Config::HALF_WIDTH);
  EXPECT_EQ(legacy->last_number_style(),
            NumberUtil::NumberString::NUMBER_KANJI);

  EXPECT_FALSE(FileUtil::FileExists(path).ok());
  EXPECT_EQ(TakeLegacyCharacterFormDb(path), std::nullopt);
}

TEST_F(LegacyCharacterFormDbTest, BrokenFileIsDeleted) {
  const std::string path = GetLegacyCharacterFormDbPath();
  ASSERT_TRUE(FileUtil::SetContents(path, "not an lru storage").ok());

  const std::optional<LegacyCharacterFormHistory> legacy =
      TakeLegacyCharacterFormDb(path);
  ASSERT_TRUE(legacy.has_value());
  EXPECT_TRUE(legacy->character_form_map().empty());
  EXPECT_FALSE(FileUtil::FileExists(path).ok());
}

TEST_F(LegacyCharacterFormDbTest, MergeKeepsNewerValues) {
  LegacyCharacterFormHistory legacy;
  (*legacy.mutable_character_form_map())[u'ア'] = Config::HALF_WIDTH;
  (*legacy.mutable_character_form_map())[u'0'] = Config::HALF_WIDTH;
  legacy.set_last_number_form(Config::HALF_WIDTH);
  legacy.set_last_number_style(NumberUtil::NumberString::NUMBER_KANJI);

  LegacyCharacterFormHistory history;
  (*history.mutable_character_form_map())[u'ア'] = Config::FULL_WIDTH;
  history.set_last_number_form(Config::FULL_WIDTH);
  history.set_last_number_style(NumberUtil::NumberString::DEFAULT_STYLE);

  EXPECT_TRUE(MergeMissingCharacterForms(legacy, &history));
  EXPECT_EQ(history.character_form_map().at(u'ア'), Config::FULL_WIDTH);
  EXPECT_EQ(history.character_form_map().at(u'0'), Config::HALF_WIDTH);
  EXPECT_EQ(history.last_number_form(), Config::FULL_WIDTH);

  EXPECT_FALSE(MergeMissingCharacterForms(legacy, &history));
}

TEST_F(LegacyCharacterFormDbTest, LoadStorageMigratesOnce) {
  const std::string path = GetLegacyCharacterFormDbPath();
  WriteOldCformDb(path);

  // A history.db already written by the new code: its ア wins.
  user_history_predictor::UserHistory history;
  (*history.mutable_character_form_history()
        ->mutable_character_form_map())[u'ア'] = Config::FULL_WIDTH;

  CharacterFormManager* manager = CharacterFormManager::GetCharacterFormManager();
  manager->LoadStorage(history);

  EXPECT_FALSE(FileUtil::FileExists(path).ok());
  EXPECT_TRUE(manager->IsStorageDirty());
  EXPECT_EQ(manager->GetConversionCharacterForm("ア"), Config::FULL_WIDTH);
  EXPECT_EQ(manager->GetConversionCharacterForm("012"), Config::HALF_WIDTH);
  const std::optional<const CharacterFormManager::NumberFormStyle> style =
      manager->GetLastNumberStyle();
  ASSERT_TRUE(style.has_value());
  EXPECT_EQ(style->form, Config::HALF_WIDTH);
  EXPECT_EQ(style->style, NumberUtil::NumberString::NUMBER_KANJI);

  // The next save persists the migrated forms; a reload does not migrate
  // again.
  user_history_predictor::UserHistory saved;
  manager->SaveStorage(&saved);
  EXPECT_EQ(saved.character_form_history().character_form_map().size(), 3);
  manager->LoadStorage(saved);
  EXPECT_FALSE(manager->IsStorageDirty());
}

}  // namespace
}  // namespace config
}  // namespace mozc
