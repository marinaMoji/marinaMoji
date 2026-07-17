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

TEST(OdorijiPaletteTest, OverlayDoesNotInjectPreeditText) {
  commands::Output output;

  OdorijiPalette::OverlayOutput(&output, 0);

  EXPECT_EQ(output.preedit().segment_size(), 0);
  EXPECT_EQ(output.candidate_window().candidate_size(),
            static_cast<int>(OdorijiPalette::kCount));
}

}  // namespace
}  // namespace session
}  // namespace mozc
