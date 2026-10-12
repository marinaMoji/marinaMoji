#include "sync/sync_history.h"

#include <optional>
#include <string>
#include <utility>

#include "absl/strings/match.h"
#include "base/number_util.h"
#include "config/character_form_manager.h"
#include "gtest/gtest.h"
#include "prediction/user_history_storage.h"
#include "protocol/config.pb.h"
#include "sync/sync_character_form.h"
#include "testing/mozctest.h"

namespace mozc {
namespace sync {
namespace {

using config::CharacterFormManager;
using config::Config;
using Entry = prediction::UserHistoryStorage::Entry;

constexpr uint32_t kDigit = 0x0030;     // Normalized key of "012".
constexpr uint32_t kKatakana = 0x30A2;  // ア

CharacterFormManager* Forms() {
  return CharacterFormManager::GetCharacterFormManager();
}

Entry MakeEntry(absl::string_view key, absl::string_view value,
                uint32_t suggestion_freq, uint32_t entry_flags) {
  Entry entry;
  entry.set_key(key);
  entry.set_value(value);
  entry.set_suggestion_freq(suggestion_freq);
  entry.set_entry_flags(entry_flags);
  return entry;
}

class SyncHistoryTest : public testing::TestWithTempUserProfile {
 protected:
  void SetUp() override {
    Forms()->SetDefaultRule();
    Forms()->ClearHistory();
  }
  void TearDown() override {
    Forms()->SetDefaultRule();
    Forms()->ClearHistory();
  }

  // Runs `f` between a blocking Load() and Save() of the history file, as the
  // converter would.
  template <typename F>
  static void EditHistory(F f) {
    prediction::UserHistoryStorage storage;
    storage.Wait();
    storage.Load();
    f(storage);
    ASSERT_TRUE(storage.Save());
  }

  // Reloads the history file into a fresh storage (and CharacterFormManager).
  static std::unique_ptr<prediction::UserHistoryStorage> Reload() {
    Forms()->ClearHistory();
    auto storage = std::make_unique<prediction::UserHistoryStorage>();
    storage->Wait();
    storage->Load();
    return storage;
  }
};

TEST_F(SyncHistoryTest, ExportCarriesEntryFlagsAndCharacterForms) {
  EditHistory([](prediction::UserHistoryStorage& storage) {
    storage.Insert(MakeEntry("kai", "階", 3, 1));
    storage.Insert(MakeEntry("ni", "日", 1, 0));
    Entry removed = MakeEntry("kesu", "消す", 1, 0);
    removed.set_removed(true);
    storage.Insert(std::move(removed));
    Forms()->SetCharacterForm("012", Config::HALF_WIDTH);
    Forms()->SetLastNumberStyle(
        {Config::HALF_WIDTH, NumberUtil::NumberString::NUMBER_KANJI});
  });
  // Export must read the file, not whatever is in memory.
  Forms()->ClearHistory();

  const HistoryExport exported = ExportHistory(1000);
  EXPECT_TRUE(absl::StrContains(exported.history_tsv, "kai\t階\t3\t0\t0\t1\n"));
  EXPECT_TRUE(absl::StrContains(exported.history_tsv, "ni\t日\t1\t0\t0\t0\n"));
  EXPECT_FALSE(absl::StrContains(exported.history_tsv, "kesu"));

  const CharacterFormSyncState forms =
      ParseCharacterFormTsv(exported.character_form_tsv);
  ASSERT_TRUE(forms.forms.contains(kDigit));
  EXPECT_EQ(forms.forms.at(kDigit),
            (StampedCharacterForm{Config::HALF_WIDTH, 1000}));
  EXPECT_EQ(forms.number_style,
            (StampedNumberStyle{Config::HALF_WIDTH,
                                NumberUtil::NumberString::NUMBER_KANJI, 1000}));
  EXPECT_EQ(LoadLocalCharacterFormSyncState(), forms);

  // Unchanged values keep their stamp; a changed one is restamped.
  EXPECT_EQ(ParseCharacterFormTsv(ExportHistory(2000).character_form_tsv)
                .forms.at(kDigit)
                .updated_at,
            1000);
  EditHistory([](prediction::UserHistoryStorage&) {
    Forms()->SetCharacterForm("012", Config::FULL_WIDTH);
  });
  EXPECT_EQ(ParseCharacterFormTsv(ExportHistory(3000).character_form_tsv)
                .forms.at(kDigit),
            (StampedCharacterForm{Config::FULL_WIDTH, 3000}));
}

TEST_F(SyncHistoryTest, ExportWithoutHistoryFileDropsStaleForms) {
  // The daemon's CharacterFormManager still holds an earlier cycle's forms,
  // but the history file is gone.
  Forms()->SetCharacterForm("012", Config::HALF_WIDTH);

  const HistoryExport exported = ExportHistory(1000);
  EXPECT_EQ(exported.history_tsv, kHistoryHeader);
  EXPECT_EQ(ParseCharacterFormTsv(exported.character_form_tsv),
            CharacterFormSyncState{});
  EXPECT_TRUE(Forms()->GetStorageSnapshot().character_form_map().empty());
}

TEST_F(SyncHistoryTest, ImportOldFormatKeepsLocalFields) {
  EditHistory([](prediction::UserHistoryStorage& storage) {
    Entry entry = MakeEntry("kai", "階", 1, 1);
    entry.add_next_entry_fps(42);
    entry.add_inner_segment_boundary(7);
    storage.Insert(std::move(entry));
    Forms()->SetCharacterForm("012", Config::HALF_WIDTH);
  });

  // Five-column lines (older versions), one six-column line, and no
  // character_forms.tsv at all.
  ASSERT_TRUE(ImportHistory("# marinaMoji sync history\n"
                            "kai\t階\t5\t2\t300\n"
                            "ni\t日\t1\t0\t50\n"
                            "ko\t個\t1\t0\t60\t1\n",
                            std::nullopt)
                  .ok());

  auto storage = Reload();
  {
    const auto kai = storage->Lookup("kai", "階");
    ASSERT_TRUE(kai);
    EXPECT_EQ(kai->suggestion_freq(), 5);
    EXPECT_EQ(kai->shown_freq(), 2);
    EXPECT_EQ(kai->last_access_time(), 300);
    EXPECT_EQ(kai->entry_flags(), 1);  // Not cleared by a 5-column line.
    EXPECT_EQ(kai->next_entry_fps_size(), 1);
    EXPECT_EQ(kai->inner_segment_boundary_size(), 1);
  }
  {
    const auto ni = storage->Lookup("ni", "日");
    ASSERT_TRUE(ni);
    EXPECT_EQ(ni->entry_flags(), 0);
  }
  {
    const auto ko = storage->Lookup("ko", "個");
    ASSERT_TRUE(ko);
    EXPECT_EQ(ko->entry_flags(), 1);
  }
  // Without character_forms.tsv the local forms are left alone.
  EXPECT_EQ(Forms()->GetStorageSnapshot().character_form_map().at(kDigit),
            Config::HALF_WIDTH);
}

TEST_F(SyncHistoryTest, ImportMergesCharacterForms) {
  EditHistory([](prediction::UserHistoryStorage& storage) {
    storage.Insert(MakeEntry("kai", "階", 1, 0));
    Forms()->SetCharacterForm("012", Config::HALF_WIDTH);
  });

  // No history entries: only the character forms change, which must still
  // be saved.
  const std::string forms_tsv =
      "# marinaMoji sync character forms\n"
      "form\t30A2\t0\t500\n"
      "number\t1\t5\t500\n";
  ASSERT_TRUE(ImportHistory(kHistoryHeader, forms_tsv).ok());

  auto storage = Reload();
  EXPECT_TRUE(storage->Contains("kai", "階"));
  const CharacterFormHistory forms = Forms()->GetStorageSnapshot();
  EXPECT_EQ(forms.character_form_map().at(kDigit), Config::HALF_WIDTH);
  EXPECT_EQ(forms.character_form_map().at(kKatakana), Config::HALF_WIDTH);
  EXPECT_EQ(forms.last_number_form(), Config::FULL_WIDTH);
  EXPECT_EQ(forms.last_number_style(), 5);

  EXPECT_EQ(LoadLocalCharacterFormSyncState(),
            ParseCharacterFormTsv(forms_tsv));
}

TEST_F(SyncHistoryTest, ImportIntoEmptyProfile) {
  // A stale in-memory form from an earlier cycle must not be saved back.
  Forms()->SetCharacterForm("012", Config::HALF_WIDTH);

  ASSERT_TRUE(ImportHistory("# marinaMoji sync history\nni\t日\t1\t0\t50\n",
                            "form\t30A2\t0\t500\n")
                  .ok());

  auto storage = Reload();
  EXPECT_TRUE(storage->Contains("ni", "日"));
  const CharacterFormHistory forms = Forms()->GetStorageSnapshot();
  EXPECT_EQ(forms.character_form_map().size(), 1);
  EXPECT_EQ(forms.character_form_map().at(kKatakana), Config::HALF_WIDTH);
}

}  // namespace
}  // namespace sync
}  // namespace mozc
