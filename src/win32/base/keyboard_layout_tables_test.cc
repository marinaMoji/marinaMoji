// Copyright 2010-2021, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "win32/base/keyboard_layout_tables.h"

#include <windows.h>

#include "protocol/config.pb.h"
#include "testing/gunit.h"

namespace mozc {
namespace win32 {
namespace {

TEST(RomajiKeyboardLayoutEmulatorTest, UsIsIdentity) {
  EXPECT_EQ(RomajiKeyboardLayoutEmulator::GetCharacterForKeyDown(
                config::MARINA_KBD_US, 'A', false, false),
            L'a');
  EXPECT_EQ(RomajiKeyboardLayoutEmulator::GetCharacterForKeyDown(
                config::MARINA_KBD_US, 'A', true, false),
            L'A');
  EXPECT_EQ(RomajiKeyboardLayoutEmulator::GetCharacterForKeyDown(
                config::MARINA_KBD_US, '1', true, false),
            L'!');
}

TEST(RomajiKeyboardLayoutEmulatorTest, CapsLockFlipsLetterCaseOnly) {
  // CapsLock alone (no shift) should behave like Shift for letters...
  EXPECT_EQ(RomajiKeyboardLayoutEmulator::GetCharacterForKeyDown(
                config::MARINA_KBD_US, 'A', false, true),
            L'A');
  // ...but not for digits/punctuation.
  EXPECT_EQ(RomajiKeyboardLayoutEmulator::GetCharacterForKeyDown(
                config::MARINA_KBD_US, '1', false, true),
            L'1');
  // CapsLock + Shift cancels back out for letters.
  EXPECT_EQ(RomajiKeyboardLayoutEmulator::GetCharacterForKeyDown(
                config::MARINA_KBD_US, 'A', true, true),
            L'a');
}

TEST(RomajiKeyboardLayoutEmulatorTest, FrAzertyRemapsLetterPositions) {
  // Physical "Q position" key types 'a' on AZERTY.
  EXPECT_EQ(RomajiKeyboardLayoutEmulator::GetCharacterForKeyDown(
                config::MARINA_KBD_FR_AZERTY, 'Q', false, false),
            L'a');
  // Physical "A position" key types 'q' on AZERTY.
  EXPECT_EQ(RomajiKeyboardLayoutEmulator::GetCharacterForKeyDown(
                config::MARINA_KBD_FR_AZERTY, 'A', false, false),
            L'q');
  // Physical "W position" key types 'z' on AZERTY.
  EXPECT_EQ(RomajiKeyboardLayoutEmulator::GetCharacterForKeyDown(
                config::MARINA_KBD_FR_AZERTY, 'W', false, false),
            L'z');
  // Number row requires Shift to type digits on AZERTY.
  EXPECT_EQ(RomajiKeyboardLayoutEmulator::GetCharacterForKeyDown(
                config::MARINA_KBD_FR_AZERTY, '1', false, false),
            L'&');
  EXPECT_EQ(RomajiKeyboardLayoutEmulator::GetCharacterForKeyDown(
                config::MARINA_KBD_FR_AZERTY, '1', true, false),
            L'1');
}

TEST(RomajiKeyboardLayoutEmulatorTest, DeQwertzSwapsYAndZ) {
  EXPECT_EQ(RomajiKeyboardLayoutEmulator::GetCharacterForKeyDown(
                config::MARINA_KBD_DE_QWERTZ, 'Y', false, false),
            L'z');
  EXPECT_EQ(RomajiKeyboardLayoutEmulator::GetCharacterForKeyDown(
                config::MARINA_KBD_DE_QWERTZ, 'Z', false, false),
            L'y');
}

TEST(RomajiKeyboardLayoutEmulatorTest, DvorakRemapsLetterPositions) {
  // Home-row-equivalent physical position types Dvorak letters.
  EXPECT_EQ(RomajiKeyboardLayoutEmulator::GetCharacterForKeyDown(
                config::MARINA_KBD_DVORAK, 'S', false, false),
            L'o');
  EXPECT_EQ(RomajiKeyboardLayoutEmulator::GetCharacterForKeyDown(
                config::MARINA_KBD_DVORAK, 'D', false, false),
            L'e');
  EXPECT_EQ(RomajiKeyboardLayoutEmulator::GetCharacterForKeyDown(
                config::MARINA_KBD_DVORAK, VK_OEM_1, false, false),
            L's');
}

TEST(RomajiKeyboardLayoutEmulatorTest, UnmappedVkReturnsNul) {
  EXPECT_EQ(RomajiKeyboardLayoutEmulator::GetCharacterForKeyDown(
                config::MARINA_KBD_US, VK_F1, false, false),
            L'\0');
}

TEST(RomajiKeyboardLayoutEmulatorTest, FrAzertyGraveKeyShiftIsTilde) {
  // Verified against xkeyboard-config symbols/fr "basic": TLDE is
  // superscript-two unshifted, tilde shifted (not superscript-two twice).
  EXPECT_EQ(RomajiKeyboardLayoutEmulator::GetCharacterForKeyDown(
                config::MARINA_KBD_FR_AZERTY, VK_OEM_3, false, false),
            L'²');
  EXPECT_EQ(RomajiKeyboardLayoutEmulator::GetCharacterForKeyDown(
                config::MARINA_KBD_FR_AZERTY, VK_OEM_3, true, false),
            L'~');
}

TEST(RomajiKeyboardLayoutEmulatorTest, DutchPlusKeyAndDigitRow) {
  // Verified against xkeyboard-config symbols/nl "basic".
  EXPECT_EQ(RomajiKeyboardLayoutEmulator::GetCharacterForKeyDown(
                config::MARINA_KBD_NL, VK_OEM_1, false, false),
            L'+');
  EXPECT_EQ(RomajiKeyboardLayoutEmulator::GetCharacterForKeyDown(
                config::MARINA_KBD_NL, VK_OEM_1, true, false),
            L'±');
  EXPECT_EQ(RomajiKeyboardLayoutEmulator::GetCharacterForKeyDown(
                config::MARINA_KBD_NL, '7', true, false),
            L'_');
}

TEST(RomajiKeyboardLayoutEmulatorTest, BepoRemapsLetterPositions) {
  // Verified against xkeyboard-config symbols/fr "bepo".
  EXPECT_EQ(RomajiKeyboardLayoutEmulator::GetCharacterForKeyDown(
                config::MARINA_KBD_BEPO, 'Q', false, false),
            L'b');
  EXPECT_EQ(RomajiKeyboardLayoutEmulator::GetCharacterForKeyDown(
                config::MARINA_KBD_BEPO, 'A', false, false),
            L'a');
  EXPECT_EQ(RomajiKeyboardLayoutEmulator::GetCharacterForKeyDown(
                config::MARINA_KBD_BEPO, VK_OEM_1, false, false),
            L'n');
  // Digit row requires Shift, same convention as AZERTY.
  EXPECT_EQ(RomajiKeyboardLayoutEmulator::GetCharacterForKeyDown(
                config::MARINA_KBD_BEPO, '1', false, false),
            L'"');
  EXPECT_EQ(RomajiKeyboardLayoutEmulator::GetCharacterForKeyDown(
                config::MARINA_KBD_BEPO, '1', true, false),
            L'1');
}

using DirectModeKeyOutput =
    RomajiKeyboardLayoutEmulator::DirectModeKeyOutput;

DirectModeKeyOutput Resolve(config::MarinaKeyboardLayout layout, BYTE vk,
                            bool shift = false, bool altgr = false,
                            bool capslock = false,
                            wchar_t pending = L'\0') {
  return RomajiKeyboardLayoutEmulator::ResolveDirectModeKey(
      layout, vk, shift, altgr, capslock, pending);
}

TEST(ResolveDirectModeKeyTest, OsDefaultIsNeverHandled) {
  EXPECT_FALSE(Resolve(config::MARINA_KBD_OS_DEFAULT, 'A').handled);
}

TEST(ResolveDirectModeKeyTest, DvorakCommitsRemappedCharacter) {
  const auto output = Resolve(config::MARINA_KBD_DVORAK, 'S');
  EXPECT_TRUE(output.handled);
  EXPECT_EQ(output.commit_text, L"o");
  EXPECT_EQ(output.next_pending_dead_key, L'\0');

  const auto shifted = Resolve(config::MARINA_KBD_DVORAK, 'S', /*shift=*/true);
  EXPECT_EQ(shifted.commit_text, L"O");
}

TEST(ResolveDirectModeKeyTest, UnmappedKeyPassesThrough) {
  // Navigation and function keys are not in the tables.
  EXPECT_FALSE(Resolve(config::MARINA_KBD_DVORAK, VK_LEFT).handled);
  EXPECT_FALSE(Resolve(config::MARINA_KBD_DVORAK, VK_F5).handled);
  // Space without a pending dead key passes through too.
  EXPECT_FALSE(Resolve(config::MARINA_KBD_DVORAK, VK_SPACE).handled);
}

TEST(ResolveDirectModeKeyTest, FrenchDeadKeyThenVowelComposes) {
  // AZERTY: the key right of P is dead circumflex (unshifted).
  const auto dead = Resolve(config::MARINA_KBD_FR_AZERTY, VK_OEM_4);
  EXPECT_TRUE(dead.handled);
  EXPECT_TRUE(dead.commit_text.empty());
  EXPECT_EQ(dead.next_pending_dead_key, L'^');

  // Physical E with pending '^' composes 'ê'.
  const auto composed =
      Resolve(config::MARINA_KBD_FR_AZERTY, 'E', /*shift=*/false,
              /*altgr=*/false, /*capslock=*/false, /*pending=*/L'^');
  EXPECT_TRUE(composed.handled);
  EXPECT_EQ(composed.commit_text, L"ê");
  EXPECT_EQ(composed.next_pending_dead_key, L'\0');
}

TEST(ResolveDirectModeKeyTest, FrenchDeadDiaeresisIsShiftedLevel) {
  const auto dead =
      Resolve(config::MARINA_KBD_FR_AZERTY, VK_OEM_4, /*shift=*/true);
  EXPECT_TRUE(dead.handled);
  EXPECT_EQ(dead.next_pending_dead_key, L'¨');

  const auto composed =
      Resolve(config::MARINA_KBD_FR_AZERTY, 'I', /*shift=*/false,
              /*altgr=*/false, /*capslock=*/false, /*pending=*/L'¨');
  EXPECT_EQ(composed.commit_text, L"ï");
}

TEST(ResolveDirectModeKeyTest, NonComposablePairEmitsAccentThenChar) {
  // '^' + 't' does not compose; Windows types "^t".
  const auto output =
      Resolve(config::MARINA_KBD_FR_AZERTY, 'T', /*shift=*/false,
              /*altgr=*/false, /*capslock=*/false, /*pending=*/L'^');
  EXPECT_TRUE(output.handled);
  EXPECT_EQ(output.commit_text, L"^t");
  EXPECT_EQ(output.next_pending_dead_key, L'\0');
}

TEST(ResolveDirectModeKeyTest, DeadKeyThenSpaceEmitsSpacingAccent) {
  const auto output =
      Resolve(config::MARINA_KBD_FR_AZERTY, VK_SPACE, /*shift=*/false,
              /*altgr=*/false, /*capslock=*/false, /*pending=*/L'^');
  EXPECT_TRUE(output.handled);
  EXPECT_EQ(output.commit_text, L"^");
  EXPECT_EQ(output.next_pending_dead_key, L'\0');
}

TEST(ResolveDirectModeKeyTest, TwoDeadKeysEmitFirstAndKeepSecondPending) {
  const auto output =
      Resolve(config::MARINA_KBD_FR_AZERTY, VK_OEM_4, /*shift=*/true,
              /*altgr=*/false, /*capslock=*/false, /*pending=*/L'^');
  EXPECT_TRUE(output.handled);
  EXPECT_EQ(output.commit_text, L"^");
  EXPECT_EQ(output.next_pending_dead_key, L'¨');
}

TEST(ResolveDirectModeKeyTest, GermanAcuteAndGraveDeadKeys) {
  const auto acute = Resolve(config::MARINA_KBD_DE_QWERTZ, VK_OEM_PLUS);
  EXPECT_EQ(acute.next_pending_dead_key, L'´');
  const auto grave =
      Resolve(config::MARINA_KBD_DE_QWERTZ, VK_OEM_PLUS, /*shift=*/true);
  EXPECT_EQ(grave.next_pending_dead_key, L'`');

  const auto composed =
      Resolve(config::MARINA_KBD_DE_QWERTZ, 'E', /*shift=*/false,
              /*altgr=*/false, /*capslock=*/false, /*pending=*/L'´');
  EXPECT_EQ(composed.commit_text, L"é");
}

TEST(ResolveDirectModeKeyTest, AltGrLayer) {
  // French AltGr+0 types '@'.
  const auto at = Resolve(config::MARINA_KBD_FR_AZERTY, '0', /*shift=*/false,
                          /*altgr=*/true);
  EXPECT_TRUE(at.handled);
  EXPECT_EQ(at.commit_text, L"@");

  // German AltGr+Q types '@'; AltGr+E types the euro sign.
  EXPECT_EQ(Resolve(config::MARINA_KBD_DE_QWERTZ, 'Q', false, true)
                .commit_text,
            L"@");
  EXPECT_EQ(Resolve(config::MARINA_KBD_DE_QWERTZ, 'E', false, true)
                .commit_text,
            L"€");
}

TEST(ResolveDirectModeKeyTest, AltGrDeadKey) {
  // French AltGr+7 is a dead grave.
  const auto dead = Resolve(config::MARINA_KBD_FR_AZERTY, '7', /*shift=*/false,
                            /*altgr=*/true);
  EXPECT_TRUE(dead.handled);
  EXPECT_TRUE(dead.commit_text.empty());
  EXPECT_EQ(dead.next_pending_dead_key, L'`');

  const auto composed =
      Resolve(config::MARINA_KBD_FR_AZERTY, 'Q', /*shift=*/false,
              /*altgr=*/false, /*capslock=*/false, /*pending=*/L'`');
  // Physical Q is 'a' on AZERTY; '`' + 'a' -> 'à'.
  EXPECT_EQ(composed.commit_text, L"à");
}

TEST(ResolveDirectModeKeyTest, UnpopulatedAltGrPassesThrough) {
  // Dvorak has no AltGr layer at all.
  EXPECT_FALSE(
      Resolve(config::MARINA_KBD_DVORAK, 'E', false, /*altgr=*/true).handled);
  // French AltGr+B is not populated.
  EXPECT_FALSE(Resolve(config::MARINA_KBD_FR_AZERTY, 'B', false, /*altgr=*/true)
                   .handled);
}

TEST(ResolveDirectModeKeyTest, BepoBaseLayerCircumflexIsDead) {
  const auto dead = Resolve(config::MARINA_KBD_BEPO, 'Y');
  EXPECT_TRUE(dead.handled);
  EXPECT_EQ(dead.next_pending_dead_key, L'^');
  // Shift+Y on BEPO is '!', not a dead key.
  const auto bang = Resolve(config::MARINA_KBD_BEPO, 'Y', /*shift=*/true);
  EXPECT_EQ(bang.commit_text, L"!");
}

TEST(ResolveDirectModeKeyTest, CapsLockAffectsLettersOnly) {
  const auto letter = Resolve(config::MARINA_KBD_DVORAK, 'S', /*shift=*/false,
                              /*altgr=*/false, /*capslock=*/true);
  EXPECT_EQ(letter.commit_text, L"O");
  const auto digit = Resolve(config::MARINA_KBD_DVORAK, '1', /*shift=*/false,
                             /*altgr=*/false, /*capslock=*/true);
  EXPECT_EQ(digit.commit_text, L"1");
}

}  // namespace
}  // namespace win32
}  // namespace mozc
