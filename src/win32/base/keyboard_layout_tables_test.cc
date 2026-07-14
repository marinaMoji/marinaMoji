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

}  // namespace
}  // namespace win32
}  // namespace mozc
