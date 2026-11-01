#include "sync/sync_character_form.h"

#include <string>

#include "gtest/gtest.h"
#include "base/number_util.h"
#include "prediction/user_history_predictor.pb.h"
#include "protocol/config.pb.h"
#include "testing/mozctest.h"

namespace mozc {
namespace sync {
namespace {

using config::Config;

constexpr uint32_t kKatakana = 0x30A2;  // ア
constexpr uint32_t kDigit = 0x0030;     // 0
constexpr uint32_t kBracket = 0xFF3B;   // ［

TEST(SyncCharacterFormTest, SerializeAndParseRoundTrip) {
  CharacterFormSyncState state;
  state.forms[kKatakana] = {Config::HALF_WIDTH, 100};
  state.forms[kDigit] = {Config::FULL_WIDTH, 200};
  state.number_style = StampedNumberStyle{Config::HALF_WIDTH, 5, 300};

  const std::string tsv = SerializeCharacterFormTsv(state);
  EXPECT_EQ(tsv,
            "# marinaMoji sync character forms\n"
            "form\t0030\t1\t200\n"
            "form\t30A2\t0\t100\n"
            "number\t0\t5\t300\n");
  EXPECT_EQ(ParseCharacterFormTsv(tsv), state);
}

TEST(SyncCharacterFormTest, ParseSkipsBadLines) {
  const CharacterFormSyncState state = ParseCharacterFormTsv(
      "# header\n"
      "form\t30A2\t0\t100\n"
      "form\t110000\t0\t100\n"   // Not UCS2.
      "form\t0030\t9\t100\n"     // Invalid CharacterForm.
      "form\t0041\t0\n"          // Too few fields.
      "form\tzz\t0\t100\n"       // Not hex.
      "number\t0\t99\t100\n"     // Invalid style.
      "future\t1\t2\t3\t4\n"     // Unknown record type.
      "\n");
  ASSERT_EQ(state.forms.size(), 1);
  EXPECT_EQ(state.forms.at(kKatakana), (StampedCharacterForm{0, 100}));
  EXPECT_FALSE(state.number_style.has_value());
}

TEST(SyncCharacterFormTest, ParseEmptyInput) {
  EXPECT_EQ(ParseCharacterFormTsv(""), CharacterFormSyncState{});
  EXPECT_EQ(ParseCharacterFormTsv(kCharacterFormHeader),
            CharacterFormSyncState{});
}

TEST(SyncCharacterFormTest, StampKeepsStampOfUnchangedValues) {
  CharacterFormSyncState previous;
  previous.forms[kKatakana] = {Config::HALF_WIDTH, 100};
  previous.forms[kDigit] = {Config::HALF_WIDTH, 100};
  previous.forms[kBracket] = {Config::HALF_WIDTH, 100};  // Cleared locally.
  previous.number_style = StampedNumberStyle{Config::HALF_WIDTH, 5, 100};

  CharacterFormHistory local;
  (*local.mutable_character_form_map())[kKatakana] = Config::HALF_WIDTH;
  (*local.mutable_character_form_map())[kDigit] = Config::FULL_WIDTH;
  (*local.mutable_character_form_map())[0x0041] = Config::FULL_WIDTH;
  local.set_last_number_form(Config::HALF_WIDTH);
  local.set_last_number_style(5);

  const CharacterFormSyncState stamped =
      StampCharacterForms(local, previous, 500);
  ASSERT_EQ(stamped.forms.size(), 3);
  EXPECT_EQ(stamped.forms.at(kKatakana),
            (StampedCharacterForm{Config::HALF_WIDTH, 100}));
  EXPECT_EQ(stamped.forms.at(kDigit),
            (StampedCharacterForm{Config::FULL_WIDTH, 500}));
  EXPECT_EQ(stamped.forms.at(0x0041),
            (StampedCharacterForm{Config::FULL_WIDTH, 500}));
  EXPECT_EQ(stamped.number_style,
            (StampedNumberStyle{Config::HALF_WIDTH, 5, 100}));

  local.set_last_number_style(6);
  EXPECT_EQ(StampCharacterForms(local, previous, 500).number_style,
            (StampedNumberStyle{Config::HALF_WIDTH, 6, 500}));
}

TEST(SyncCharacterFormTest, MergeNewerStampWins) {
  CharacterFormSyncState local;
  local.forms[kKatakana] = {Config::HALF_WIDTH, 100};
  local.forms[kDigit] = {Config::HALF_WIDTH, 300};
  local.forms[0x0041] = {Config::HALF_WIDTH, 100};  // Local only.
  local.number_style = StampedNumberStyle{Config::HALF_WIDTH, 1, 100};

  CharacterFormSyncState remote;
  remote.forms[kKatakana] = {Config::FULL_WIDTH, 200};  // Newer.
  remote.forms[kDigit] = {Config::FULL_WIDTH, 200};     // Older.
  remote.forms[kBracket] = {Config::FULL_WIDTH, 50};    // Remote only.
  remote.number_style = StampedNumberStyle{Config::FULL_WIDTH, 2, 200};

  CharacterFormMergeStats stats;
  const CharacterFormSyncState merged =
      MergeCharacterForms(remote, local, &stats);
  ASSERT_EQ(merged.forms.size(), 4);
  EXPECT_EQ(merged.forms.at(kKatakana),
            (StampedCharacterForm{Config::FULL_WIDTH, 200}));
  EXPECT_EQ(merged.forms.at(kDigit),
            (StampedCharacterForm{Config::HALF_WIDTH, 300}));
  EXPECT_EQ(merged.forms.at(0x0041),
            (StampedCharacterForm{Config::HALF_WIDTH, 100}));
  EXPECT_EQ(merged.forms.at(kBracket),
            (StampedCharacterForm{Config::FULL_WIDTH, 50}));
  // The remote number style (full-width, 200) is newer than the local one, but
  // the local "0" (half-width, 300) is newer still, so the number style takes
  // its width (style 2 is the full-width form of style 1).
  EXPECT_EQ(merged.number_style,
            (StampedNumberStyle{Config::HALF_WIDTH, 1, 300}));
  EXPECT_EQ(stats.from_remote, 3);  // ア, ［, number style.
}

TEST(SyncCharacterFormTest, MergeTieKeepsLocal) {
  CharacterFormSyncState local;
  local.forms[kKatakana] = {Config::HALF_WIDTH, 100};
  local.number_style = StampedNumberStyle{Config::HALF_WIDTH, 1, 100};
  CharacterFormSyncState remote;
  remote.forms[kKatakana] = {Config::FULL_WIDTH, 100};
  remote.number_style = StampedNumberStyle{Config::FULL_WIDTH, 2, 100};

  CharacterFormMergeStats stats;
  EXPECT_EQ(MergeCharacterForms(remote, local, &stats), local);
  EXPECT_EQ(stats.from_remote, 0);
}

TEST(SyncCharacterFormTest, MergeWithOldBundleKeepsLocal) {
  // A bundle written before character_forms.tsv existed merges as empty.
  CharacterFormSyncState local;
  local.forms[kKatakana] = {Config::HALF_WIDTH, 100};
  EXPECT_EQ(MergeCharacterForms(CharacterFormSyncState{}, local, nullptr),
            local);
}

TEST(SyncCharacterFormTest, DevicesConverge) {
  // Device A changed ア later than B; B changed 0 later than A. Whichever
  // order they sync in, both end up with the same state.
  CharacterFormSyncState a;
  a.forms[kKatakana] = {Config::HALF_WIDTH, 300};
  a.forms[kDigit] = {Config::HALF_WIDTH, 100};
  CharacterFormSyncState b;
  b.forms[kKatakana] = {Config::FULL_WIDTH, 200};
  b.forms[kDigit] = {Config::FULL_WIDTH, 400};

  // A syncs first (bundle = A), then B, then A again.
  const CharacterFormSyncState bundle1 = MergeCharacterForms({}, a, nullptr);
  const CharacterFormSyncState b_after = MergeCharacterForms(bundle1, b, nullptr);
  const CharacterFormSyncState a_after =
      MergeCharacterForms(b_after, a, nullptr);
  EXPECT_EQ(a_after, b_after);
  EXPECT_EQ(a_after.forms.at(kKatakana).form, Config::HALF_WIDTH);
  EXPECT_EQ(a_after.forms.at(kDigit).form, Config::FULL_WIDTH);

  // Opposite order.
  EXPECT_EQ(MergeCharacterForms(MergeCharacterForms({}, b, nullptr), a, nullptr),
            a_after);
}

// The form of "0" and the number style are stamped separately, so the per-key
// winners can come from different devices and disagree about the width. The
// merge settles it by recency, and every device reaches the same state.
TEST(SyncCharacterFormTest, MergeSettlesNumberWidthByRecency) {
  using Style = NumberUtil::NumberString;

  // Device A: "0" is half-width (newer). Device B: number style is full-width
  // (older).
  CharacterFormSyncState a;
  a.forms[kDigit] = {Config::HALF_WIDTH, 400};
  CharacterFormSyncState b;
  b.number_style = StampedNumberStyle{
      Config::FULL_WIDTH, Style::NUMBER_SEPARATED_ARABIC_FULLWIDTH, 200};

  const CharacterFormSyncState a_view = MergeCharacterForms(b, a, nullptr);
  const CharacterFormSyncState b_view = MergeCharacterForms(a, b, nullptr);
  EXPECT_EQ(a_view, b_view);
  EXPECT_EQ(a_view.forms.at(kDigit).form, Config::HALF_WIDTH);
  ASSERT_TRUE(a_view.number_style.has_value());
  EXPECT_EQ(a_view.number_style->form, Config::HALF_WIDTH);
  EXPECT_EQ(a_view.number_style->style,
            Style::NUMBER_SEPARATED_ARABIC_HALFWIDTH);
  EXPECT_EQ(a_view.number_style->updated_at, 400);

  // The other way round: the number style is newer, "0" follows it.
  a.forms[kDigit] = {Config::HALF_WIDTH, 100};
  b.number_style->updated_at = 300;
  const CharacterFormSyncState newer = MergeCharacterForms(b, a, nullptr);
  EXPECT_EQ(newer, MergeCharacterForms(a, b, nullptr));
  EXPECT_EQ(newer.forms.at(kDigit).form, Config::FULL_WIDTH);
  EXPECT_EQ(newer.forms.at(kDigit).updated_at, 300);
  EXPECT_EQ(newer.number_style->style,
            Style::NUMBER_SEPARATED_ARABIC_FULLWIDTH);

  // Width-independent styles keep their style; only the form follows.
  b.number_style = StampedNumberStyle{Config::HALF_WIDTH,
                                      Style::NUMBER_KANJI, 100};
  a.forms[kDigit] = {Config::FULL_WIDTH, 500};
  const CharacterFormSyncState kanji = MergeCharacterForms(b, a, nullptr);
  EXPECT_EQ(kanji.number_style->form, Config::FULL_WIDTH);
  EXPECT_EQ(kanji.number_style->style, Style::NUMBER_KANJI);

  // Agreeing values are left alone.
  a.forms[kDigit] = {Config::HALF_WIDTH, 100};
  b.number_style = StampedNumberStyle{Config::HALF_WIDTH,
                                      Style::DEFAULT_STYLE, 900};
  const CharacterFormSyncState same = MergeCharacterForms(b, a, nullptr);
  EXPECT_EQ(same.forms.at(kDigit).updated_at, 100);
  EXPECT_EQ(same.number_style->updated_at, 900);
}

TEST(SyncCharacterFormTest, ToCharacterFormHistory) {
  CharacterFormSyncState state;
  state.forms[kKatakana] = {Config::HALF_WIDTH, 100};
  state.number_style = StampedNumberStyle{Config::FULL_WIDTH, 5, 100};
  const CharacterFormHistory history = ToCharacterFormHistory(state);
  ASSERT_EQ(history.character_form_map().size(), 1);
  EXPECT_EQ(history.character_form_map().at(kKatakana), Config::HALF_WIDTH);
  EXPECT_EQ(history.last_number_form(), Config::FULL_WIDTH);
  EXPECT_EQ(history.last_number_style(), 5);

  EXPECT_FALSE(
      ToCharacterFormHistory(CharacterFormSyncState{}).has_last_number_form());
}

class SyncCharacterFormLocalStateTest
    : public testing::TestWithTempUserProfile {};

TEST_F(SyncCharacterFormLocalStateTest, SaveAndLoad) {
  EXPECT_EQ(LoadLocalCharacterFormSyncState(), CharacterFormSyncState{});

  CharacterFormSyncState state;
  state.forms[kKatakana] = {Config::HALF_WIDTH, 100};
  state.number_style = StampedNumberStyle{Config::FULL_WIDTH, 5, 200};
  ASSERT_TRUE(SaveLocalCharacterFormSyncState(state).ok());
  EXPECT_EQ(LoadLocalCharacterFormSyncState(), state);
}

}  // namespace
}  // namespace sync
}  // namespace mozc
