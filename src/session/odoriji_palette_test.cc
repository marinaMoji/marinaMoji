#include "session/odoriji_palette.h"

#include <cstdint>
#include <string>

#include "protocol/commands.pb.h"
#include "testing/gunit.h"

namespace mozc {
namespace session {
namespace {

commands::KeyEvent KeyWithCode(uint32_t code) {
  commands::KeyEvent key;
  key.set_special_key(commands::KeyEvent::NO_SPECIALKEY);
  key.set_key_code(code);
  return key;
}

TEST(OdorijiPaletteTest, SpaceKeyCodeCyclesAndIsPreflightConsumed) {
  commands::Command command;
  bool visible = true;
  int focused_index = 0;

  const commands::KeyEvent key = KeyWithCode(0x20);
  EXPECT_TRUE(OdorijiPalette::WouldConsumeKey(key));
  EXPECT_TRUE(OdorijiPalette::HandleKey(key, &command, &visible,
                                       &focused_index));
  EXPECT_TRUE(visible);
  EXPECT_EQ(focused_index, 1);
}

TEST(OdorijiPaletteTest, NumberKeyWithNoSpecialKeyCommitsCandidate) {
  commands::Command command;
  bool visible = true;
  int focused_index = 0;
  std::string commit_result;

  const commands::KeyEvent key = KeyWithCode('2');
  EXPECT_TRUE(OdorijiPalette::WouldConsumeKey(key));
  EXPECT_TRUE(OdorijiPalette::HandleKey(key, &command, &visible,
                                       &focused_index, nullptr,
                                       &commit_result));
  EXPECT_FALSE(visible);
  EXPECT_EQ(commit_result, OdorijiPalette::GetCharacter(1));
}

TEST(OdorijiPaletteTest, ModifierOnlyKeyKeepsPaletteOpen) {
  commands::Command command;
  bool visible = true;
  int focused_index = 3;

  commands::KeyEvent key;
  key.add_modifier_keys(commands::KeyEvent::LEFT_SHIFT);
  EXPECT_TRUE(OdorijiPalette::WouldConsumeKey(key));
  EXPECT_TRUE(OdorijiPalette::HandleKey(key, &command, &visible,
                                       &focused_index));
  EXPECT_TRUE(visible);
  EXPECT_EQ(focused_index, 3);
}

TEST(OdorijiPaletteTest, OverlayDoesNotInjectPreeditText) {
  commands::Output output;

  OdorijiPalette::OverlayOutput(&output, 0);

  EXPECT_EQ(output.preedit().segment_size(), 0);
  EXPECT_EQ(output.candidate_window().candidate_size(),
            static_cast<int>(OdorijiPalette::kCount));
}

TEST(OdorijiPaletteTest, TryCommitCandidateFromSelectId) {
  commands::Command command;
  command.mutable_input()->mutable_command()->set_id(3);
  bool visible = true;
  int default_index = 0;
  std::string commit_result;

  EXPECT_TRUE(OdorijiPalette::TryCommitCandidate(
      &command, &visible, &default_index, &commit_result));
  EXPECT_FALSE(visible);
  EXPECT_EQ(default_index, 3);
  EXPECT_EQ(commit_result, OdorijiPalette::GetCharacter(3));
}

TEST(OdorijiPaletteTest, TryHighlightCandidateUpdatesFocus) {
  commands::Command command;
  command.mutable_input()->mutable_command()->set_id(5);
  int focused_index = 0;

  EXPECT_TRUE(
      OdorijiPalette::TryHighlightCandidate(&command, true, &focused_index));
  EXPECT_EQ(focused_index, 5);

  EXPECT_FALSE(
      OdorijiPalette::TryHighlightCandidate(&command, false, &focused_index));
  EXPECT_EQ(focused_index, 5);
}

TEST(OdorijiPaletteTest, BrokenVerticalMarksAreIncluded) {
  EXPECT_EQ(OdorijiPalette::kCount, 11);
  // 〳〴〵 follow the solid long marks 〱〲.
  EXPECT_EQ(OdorijiPalette::GetCharacter(8),
            std::string("\xe3\x80\xb3"));  // 〳
  EXPECT_EQ(OdorijiPalette::GetCharacter(9),
            std::string("\xe3\x80\xb4"));  // 〴
  EXPECT_EQ(OdorijiPalette::GetCharacter(10),
            std::string("\xe3\x80\xb5"));  // 〵
}

TEST(OdorijiPaletteTest, NumberNineCommitsNinthCandidate) {
  commands::Command command;
  bool visible = true;
  int focused_index = 0;
  std::string commit_result;

  const commands::KeyEvent key = KeyWithCode('9');
  EXPECT_TRUE(OdorijiPalette::WouldConsumeKey(key));
  EXPECT_TRUE(OdorijiPalette::HandleKey(key, &command, &visible,
                                       &focused_index, nullptr,
                                       &commit_result));
  EXPECT_FALSE(visible);
  EXPECT_EQ(commit_result, OdorijiPalette::GetCharacter(8));  // 〳
}

}  // namespace
}  // namespace session
}  // namespace mozc
