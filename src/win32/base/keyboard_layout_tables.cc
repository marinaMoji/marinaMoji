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

#include <cstddef>

#include "win32/base/keyboard.h"

namespace mozc {
namespace win32 {
namespace {

struct CharPair {
  wchar_t base;
  wchar_t shifted;
};

// Shared VK order for every per-layout table below: 26 letters, 10 digits,
// then the 12 "OEM" punctuation VKs. These OEM_* constants are positional
// (tied to a fixed physical key on the standard 101/102-key scan-code
// layout), not tied to any particular language's printed legend -- see
// keyevent_handler.cc's existing kCharForVK_BA.._E2 tables for the same
// convention. VK_OEM_102 is the extra ISO key (absent on US ANSI keyboards)
// near the left Shift key on most European physical keyboards.
constexpr BYTE kVkOrder[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
    'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    VK_OEM_1, VK_OEM_PLUS, VK_OEM_COMMA, VK_OEM_MINUS, VK_OEM_PERIOD,
    VK_OEM_2, VK_OEM_3, VK_OEM_4, VK_OEM_5, VK_OEM_6, VK_OEM_7, VK_OEM_102,
};
constexpr size_t kVkOrderSize = std::size(kVkOrder);

int FindVkIndex(BYTE virtual_key) {
  for (size_t i = 0; i < kVkOrderSize; ++i) {
    if (kVkOrder[i] == virtual_key) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

// US QWERTY. This is the reference table: letters and digits are the
// identity mapping already used by MARINA_KBD_OS_DEFAULT.
constexpr CharPair kUs[kVkOrderSize] = {
    {L'a', L'A'}, {L'b', L'B'}, {L'c', L'C'}, {L'd', L'D'}, {L'e', L'E'},
    {L'f', L'F'}, {L'g', L'G'}, {L'h', L'H'}, {L'i', L'I'}, {L'j', L'J'},
    {L'k', L'K'}, {L'l', L'L'}, {L'm', L'M'}, {L'n', L'N'}, {L'o', L'O'},
    {L'p', L'P'}, {L'q', L'Q'}, {L'r', L'R'}, {L's', L'S'}, {L't', L'T'},
    {L'u', L'U'}, {L'v', L'V'}, {L'w', L'W'}, {L'x', L'X'}, {L'y', L'Y'},
    {L'z', L'Z'},
    {L'0', L')'}, {L'1', L'!'}, {L'2', L'@'}, {L'3', L'#'}, {L'4', L'$'},
    {L'5', L'%'}, {L'6', L'^'}, {L'7', L'&'}, {L'8', L'*'}, {L'9', L'('},
    {L';', L':'}, {L'=', L'+'}, {L',', L'<'}, {L'-', L'_'}, {L'.', L'>'},
    {L'/', L'?'}, {L'`', L'~'}, {L'[', L'{'}, {L'\\', L'|'}, {L']', L'}'},
    {L'\'', L'"'}, {L'\0', L'\0'},  // OEM_102: not present on US ANSI.
};

// UK QWERTY. Letters are identical to US; punctuation/shifted-digits
// differ, and OEM_102 (present on UK ISO keyboards) types backslash/pipe.
// Verified against xkeyboard-config's symbols/gb "basic" variant.
constexpr CharPair kUk[kVkOrderSize] = {
    {L'a', L'A'}, {L'b', L'B'}, {L'c', L'C'}, {L'd', L'D'}, {L'e', L'E'},
    {L'f', L'F'}, {L'g', L'G'}, {L'h', L'H'}, {L'i', L'I'}, {L'j', L'J'},
    {L'k', L'K'}, {L'l', L'L'}, {L'm', L'M'}, {L'n', L'N'}, {L'o', L'O'},
    {L'p', L'P'}, {L'q', L'Q'}, {L'r', L'R'}, {L's', L'S'}, {L't', L'T'},
    {L'u', L'U'}, {L'v', L'V'}, {L'w', L'W'}, {L'x', L'X'}, {L'y', L'Y'},
    {L'z', L'Z'},
    {L'0', L')'}, {L'1', L'!'}, {L'2', L'"'}, {L'3', L'£'},
    {L'4', L'$'}, {L'5', L'%'}, {L'6', L'^'}, {L'7', L'&'}, {L'8', L'*'},
    {L'9', L'('},
    {L';', L':'}, {L'=', L'+'}, {L',', L'<'}, {L'-', L'_'}, {L'.', L'>'},
    {L'/', L'?'}, {L'`', L'¬'}, {L'[', L'{'}, {L'#', L'~'},
    {L']', L'}'}, {L'\'', L'@'}, {L'\\', L'|'},
};

// French AZERTY. Letters and the number row are substantially reshuffled
// vs. QWERTY; several OEM keys are dead keys on a real AZERTY keyboard
// (circumflex/diaeresis) which are typed here as plain, non-combining
// characters (see class comment: no dead-key composition support).
// Verified against xkeyboard-config's symbols/fr "basic" variant.
constexpr CharPair kFrAzerty[kVkOrderSize] = {
    /*A*/ {L'q', L'Q'}, /*B*/ {L'b', L'B'}, /*C*/ {L'c', L'C'},
    /*D*/ {L'd', L'D'}, /*E*/ {L'e', L'E'}, /*F*/ {L'f', L'F'},
    /*G*/ {L'g', L'G'}, /*H*/ {L'h', L'H'}, /*I*/ {L'i', L'I'},
    /*J*/ {L'j', L'J'}, /*K*/ {L'k', L'K'}, /*L*/ {L'l', L'L'},
    /*M*/ {L',', L'?'}, /*N*/ {L'n', L'N'}, /*O*/ {L'o', L'O'},
    /*P*/ {L'p', L'P'}, /*Q*/ {L'a', L'A'}, /*R*/ {L'r', L'R'},
    /*S*/ {L's', L'S'}, /*T*/ {L't', L'T'}, /*U*/ {L'u', L'U'},
    /*V*/ {L'v', L'V'}, /*W*/ {L'z', L'Z'}, /*X*/ {L'x', L'X'},
    /*Y*/ {L'y', L'Y'}, /*Z*/ {L'w', L'W'},
    /*0*/ {L'à', L'0'}, /*1*/ {L'&', L'1'}, /*2*/ {L'é', L'2'},
    /*3*/ {L'"', L'3'}, /*4*/ {L'\'', L'4'}, /*5*/ {L'(', L'5'},
    /*6*/ {L'-', L'6'}, /*7*/ {L'è', L'7'}, /*8*/ {L'_', L'8'},
    /*9*/ {L'ç', L'9'},
    /*OEM_1*/ {L'm', L'M'}, /*OEM_PLUS*/ {L'=', L'+'},
    /*OEM_COMMA*/ {L';', L'.'}, /*OEM_MINUS*/ {L')', L'°'},
    /*OEM_PERIOD*/ {L':', L'/'}, /*OEM_2*/ {L'!', L'§'},
    /*OEM_3*/ {L'²', L'~'}, /*OEM_4*/ {L'^', L'¨'},
    /*OEM_5*/ {L'*', L'µ'}, /*OEM_6*/ {L'$', L'£'},
    /*OEM_7*/ {L'ù', L'%'}, /*OEM_102*/ {L'<', L'>'},
};

// German QWERTZ. Letters match QWERTY except Y/Z are swapped. Verified
// against xkeyboard-config's symbols/de "basic" variant.
constexpr CharPair kDeQwertz[kVkOrderSize] = {
    {L'a', L'A'}, {L'b', L'B'}, {L'c', L'C'}, {L'd', L'D'}, {L'e', L'E'},
    {L'f', L'F'}, {L'g', L'G'}, {L'h', L'H'}, {L'i', L'I'}, {L'j', L'J'},
    {L'k', L'K'}, {L'l', L'L'}, {L'm', L'M'}, {L'n', L'N'}, {L'o', L'O'},
    {L'p', L'P'}, {L'q', L'Q'}, {L'r', L'R'}, {L's', L'S'}, {L't', L'T'},
    {L'u', L'U'}, {L'v', L'V'}, {L'w', L'W'}, {L'x', L'X'},
    /*Y*/ {L'z', L'Z'}, /*Z*/ {L'y', L'Y'},
    {L'0', L'='}, {L'1', L'!'}, {L'2', L'"'}, {L'3', L'§'},
    {L'4', L'$'}, {L'5', L'%'}, {L'6', L'&'}, {L'7', L'/'}, {L'8', L'('},
    {L'9', L')'},
    /*OEM_1*/ {L'ö', L'Ö'}, /*OEM_PLUS*/ {L'´', L'`'},
    /*OEM_COMMA*/ {L',', L';'}, /*OEM_MINUS*/ {L'ß', L'?'},
    /*OEM_PERIOD*/ {L'.', L':'}, /*OEM_2*/ {L'-', L'_'},
    /*OEM_3*/ {L'^', L'°'}, /*OEM_4*/ {L'ü', L'Ü'},
    /*OEM_5*/ {L'#', L'\''}, /*OEM_6*/ {L'+', L'*'},
    /*OEM_7*/ {L'ä', L'Ä'}, /*OEM_102*/ {L'<', L'>'},
};

// Spanish. Letters match QWERTY. Verified against xkeyboard-config's
// symbols/es "basic" variant.
constexpr CharPair kEs[kVkOrderSize] = {
    {L'a', L'A'}, {L'b', L'B'}, {L'c', L'C'}, {L'd', L'D'}, {L'e', L'E'},
    {L'f', L'F'}, {L'g', L'G'}, {L'h', L'H'}, {L'i', L'I'}, {L'j', L'J'},
    {L'k', L'K'}, {L'l', L'L'}, {L'm', L'M'}, {L'n', L'N'}, {L'o', L'O'},
    {L'p', L'P'}, {L'q', L'Q'}, {L'r', L'R'}, {L's', L'S'}, {L't', L'T'},
    {L'u', L'U'}, {L'v', L'V'}, {L'w', L'W'}, {L'x', L'X'}, {L'y', L'Y'},
    {L'z', L'Z'},
    {L'0', L'='}, {L'1', L'!'}, {L'2', L'"'}, {L'3', L'·'},
    {L'4', L'$'}, {L'5', L'%'}, {L'6', L'&'}, {L'7', L'/'}, {L'8', L'('},
    {L'9', L')'},
    /*OEM_1*/ {L'ñ', L'Ñ'}, /*OEM_PLUS*/ {L'¡', L'¿'},
    /*OEM_COMMA*/ {L',', L';'}, /*OEM_MINUS*/ {L'\'', L'?'},
    /*OEM_PERIOD*/ {L'.', L':'}, /*OEM_2*/ {L'-', L'_'},
    /*OEM_3*/ {L'º', L'ª'}, /*OEM_4*/ {L'`', L'^'},
    /*OEM_5*/ {L'ç', L'Ç'}, /*OEM_6*/ {L'+', L'*'},
    /*OEM_7*/ {L'´', L'¨'}, /*OEM_102*/ {L'<', L'>'},
};

// Italian. Letters match QWERTY. Verified against xkeyboard-config's
// symbols/it "basic" variant.
constexpr CharPair kIt[kVkOrderSize] = {
    {L'a', L'A'}, {L'b', L'B'}, {L'c', L'C'}, {L'd', L'D'}, {L'e', L'E'},
    {L'f', L'F'}, {L'g', L'G'}, {L'h', L'H'}, {L'i', L'I'}, {L'j', L'J'},
    {L'k', L'K'}, {L'l', L'L'}, {L'm', L'M'}, {L'n', L'N'}, {L'o', L'O'},
    {L'p', L'P'}, {L'q', L'Q'}, {L'r', L'R'}, {L's', L'S'}, {L't', L'T'},
    {L'u', L'U'}, {L'v', L'V'}, {L'w', L'W'}, {L'x', L'X'}, {L'y', L'Y'},
    {L'z', L'Z'},
    {L'0', L'='}, {L'1', L'!'}, {L'2', L'"'}, {L'3', L'£'},
    {L'4', L'$'}, {L'5', L'%'}, {L'6', L'&'}, {L'7', L'/'}, {L'8', L'('},
    {L'9', L')'},
    /*OEM_1*/ {L'ò', L'ç'}, /*OEM_PLUS*/ {L'ì', L'^'},
    /*OEM_COMMA*/ {L',', L';'}, /*OEM_MINUS*/ {L'\'', L'?'},
    /*OEM_PERIOD*/ {L'.', L':'}, /*OEM_2*/ {L'-', L'_'},
    /*OEM_3*/ {L'\\', L'|'}, /*OEM_4*/ {L'è', L'é'},
    /*OEM_5*/ {L'ù', L'§'}, /*OEM_6*/ {L'+', L'*'},
    /*OEM_7*/ {L'à', L'°'}, /*OEM_102*/ {L'<', L'>'},
};

// Dutch. Letters match QWERTY. Verified against xkeyboard-config's
// symbols/nl "basic" variant.
constexpr CharPair kNl[kVkOrderSize] = {
    {L'a', L'A'}, {L'b', L'B'}, {L'c', L'C'}, {L'd', L'D'}, {L'e', L'E'},
    {L'f', L'F'}, {L'g', L'G'}, {L'h', L'H'}, {L'i', L'I'}, {L'j', L'J'},
    {L'k', L'K'}, {L'l', L'L'}, {L'm', L'M'}, {L'n', L'N'}, {L'o', L'O'},
    {L'p', L'P'}, {L'q', L'Q'}, {L'r', L'R'}, {L's', L'S'}, {L't', L'T'},
    {L'u', L'U'}, {L'v', L'V'}, {L'w', L'W'}, {L'x', L'X'}, {L'y', L'Y'},
    {L'z', L'Z'},
    {L'0', L'\''}, {L'1', L'!'}, {L'2', L'"'}, {L'3', L'#'}, {L'4', L'$'},
    {L'5', L'%'}, {L'6', L'&'}, {L'7', L'_'}, {L'8', L'('}, {L'9', L')'},
    /*OEM_1*/ {L'+', L'±'}, /*OEM_PLUS*/ {L'°', L'~'},
    /*OEM_COMMA*/ {L',', L';'}, /*OEM_MINUS*/ {L'/', L'?'},
    /*OEM_PERIOD*/ {L'.', L':'}, /*OEM_2*/ {L'-', L'='},
    /*OEM_3*/ {L'@', L'§'}, /*OEM_4*/ {L'¨', L'^'},
    /*OEM_5*/ {L'<', L'>'}, /*OEM_6*/ {L'*', L'|'},
    /*OEM_7*/ {L'´', L'`'}, /*OEM_102*/ {L']', L'['},
};

// US Dvorak (ANSI Simplified Dvorak). Digits/shifted-digits match US;
// letters and OEM punctuation are extensively remapped.
constexpr CharPair kDvorak[kVkOrderSize] = {
    /*A*/ {L'a', L'A'}, /*B*/ {L'x', L'X'}, /*C*/ {L'j', L'J'},
    /*D*/ {L'e', L'E'}, /*E*/ {L'.', L'>'}, /*F*/ {L'u', L'U'},
    /*G*/ {L'i', L'I'}, /*H*/ {L'd', L'D'}, /*I*/ {L'c', L'C'},
    /*J*/ {L'h', L'H'}, /*K*/ {L't', L'T'}, /*L*/ {L'n', L'N'},
    /*M*/ {L'm', L'M'}, /*N*/ {L'b', L'B'}, /*O*/ {L'r', L'R'},
    /*P*/ {L'l', L'L'}, /*Q*/ {L'\'', L'"'}, /*R*/ {L'p', L'P'},
    /*S*/ {L'o', L'O'}, /*T*/ {L'y', L'Y'}, /*U*/ {L'g', L'G'},
    /*V*/ {L'k', L'K'}, /*W*/ {L',', L'<'}, /*X*/ {L'q', L'Q'},
    /*Y*/ {L'f', L'F'}, /*Z*/ {L';', L':'},
    {L'0', L')'}, {L'1', L'!'}, {L'2', L'@'}, {L'3', L'#'}, {L'4', L'$'},
    {L'5', L'%'}, {L'6', L'^'}, {L'7', L'&'}, {L'8', L'*'}, {L'9', L'('},
    /*OEM_1*/ {L's', L'S'}, /*OEM_PLUS*/ {L']', L'}'},
    /*OEM_COMMA*/ {L'w', L'W'}, /*OEM_MINUS*/ {L'[', L'{'},
    /*OEM_PERIOD*/ {L'v', L'V'}, /*OEM_2*/ {L'z', L'Z'},
    /*OEM_3*/ {L'`', L'~'}, /*OEM_4*/ {L'/', L'?'},
    /*OEM_5*/ {L'\\', L'|'}, /*OEM_6*/ {L'=', L'+'},
    /*OEM_7*/ {L'-', L'_'}, /*OEM_102*/ {L'\0', L'\0'},
};

// BEPO (French ergonomic layout). Verified against xkeyboard-config's
// symbols/fr "bepo" variant. Digit row requires Shift (base=symbol,
// shift=digit), matching the French AZERTY convention.
constexpr CharPair kBepo[kVkOrderSize] = {
    /*A*/ {L'a', L'A'}, /*B*/ {L'k', L'K'}, /*C*/ {L'x', L'X'},
    /*D*/ {L'i', L'I'}, /*E*/ {L'p', L'P'}, /*F*/ {L'e', L'E'},
    /*G*/ {L',', L';'}, /*H*/ {L'c', L'C'}, /*I*/ {L'd', L'D'},
    /*J*/ {L't', L'T'}, /*K*/ {L's', L'S'}, /*L*/ {L'r', L'R'},
    /*M*/ {L'q', L'Q'}, /*N*/ {L'\'', L'?'}, /*O*/ {L'l', L'L'},
    /*P*/ {L'j', L'J'}, /*Q*/ {L'b', L'B'}, /*R*/ {L'o', L'O'},
    /*S*/ {L'u', L'U'}, /*T*/ {L'è', L'È'}, /*U*/ {L'v', L'V'},
    /*V*/ {L'.', L':'}, /*W*/ {L'é', L'É'}, /*X*/ {L'y', L'Y'},
    /*Y*/ {L'^', L'!'}, /*Z*/ {L'à', L'À'},
    /*0*/ {L'*', L'0'}, /*1*/ {L'"', L'1'}, /*2*/ {L'«', L'2'},
    /*3*/ {L'»', L'3'}, /*4*/ {L'(', L'4'}, /*5*/ {L')', L'5'},
    /*6*/ {L'@', L'6'}, /*7*/ {L'+', L'7'}, /*8*/ {L'-', L'8'},
    /*9*/ {L'/', L'9'},
    /*OEM_1*/ {L'n', L'N'}, /*OEM_PLUS*/ {L'%', L'`'},
    /*OEM_COMMA*/ {L'g', L'G'}, /*OEM_MINUS*/ {L'=', L'°'},
    /*OEM_PERIOD*/ {L'h', L'H'}, /*OEM_2*/ {L'f', L'F'},
    /*OEM_3*/ {L'$', L'#'}, /*OEM_4*/ {L'z', L'Z'},
    /*OEM_5*/ {L'ç', L'Ç'}, /*OEM_6*/ {L'w', L'W'},
    /*OEM_7*/ {L'm', L'M'}, /*OEM_102*/ {L'ê', L'Ê'},
};

// ---------------------------------------------------------------------------
// Direct-input-mode extras: dead keys and the AltGr layer.
//
// These are deliberately sparse side tables so the base CharPair tables above
// (used by composition-mode romaji resolution) stay untouched: composition
// mode keeps treating accent keys as plain spacing characters, while direct
// input mode gets real dead-key composition and AltGr characters.
//
// Sources: Windows' shipped layout DLLs (kbdfr/kbdgr/kbdsp/kbdit/kbdne/
// kbdus/kbddv/kbduk) cross-checked against xkeyboard-config where noted.
// Entries marked "best effort" have not been verified key-by-key against a
// live Windows layout; unpopulated AltGr combinations simply pass through
// to the application.
// ---------------------------------------------------------------------------

// Marks (vk, shift-level or AltGr-level) as producing a dead key whose
// spacing form is |accent|. |shift| is ignored when |altgr| is true only if
// noted per-entry; matching is exact on both flags.
struct DeadKeySpec {
  BYTE vk;
  bool shift;
  bool altgr;
  wchar_t accent;
};

// One AltGr-layer entry: |base| without Shift, |shifted| with Shift.
// L'\0' means "no character" (falls back to unhandled/pass-through).
struct AltGrPair {
  BYTE vk;
  wchar_t base;
  wchar_t shifted;
};

// French AZERTY (kbdfr). The OEM_4 key left of Enter is the classic
// circumflex/diaeresis dead key. AltGr digits carry the programming
// characters; the tilde and grave there are dead keys as well.
constexpr DeadKeySpec kFrDeadKeys[] = {
    {VK_OEM_4, /*shift=*/false, /*altgr=*/false, L'^'},
    {VK_OEM_4, /*shift=*/true, /*altgr=*/false, L'¨'},
    {'2', /*shift=*/false, /*altgr=*/true, L'~'},
    {'7', /*shift=*/false, /*altgr=*/true, L'`'},
};
constexpr AltGrPair kFrAltGr[] = {
    {'0', L'@', L'\0'}, {'2', L'\0', L'\0'} /* dead ~ */,
    {'3', L'#', L'\0'}, {'4', L'{', L'\0'},  {'5', L'[', L'\0'},
    {'6', L'|', L'\0'}, {'7', L'\0', L'\0'} /* dead ` */,
    {'8', L'\\', L'\0'},
    // Best effort: kbdfr's AltGr+9 caret is a plain (non-dead) character,
    // unlike xkeyboard-config's fr(basic) which makes it dead.
    {'9', L'^', L'\0'},
    {'E', L'€', L'\0'},
    {VK_OEM_MINUS, L']', L'\0'},  // ')' key
    {VK_OEM_PLUS, L'}', L'\0'},   // '=' key
    {VK_OEM_6, L'¤', L'\0'},      // '$' key
};

// German QWERTZ (kbdgr): dead circumflex on the key left of '1', dead
// acute/grave on the key right of 'ß'.
constexpr DeadKeySpec kDeDeadKeys[] = {
    {VK_OEM_3, /*shift=*/false, /*altgr=*/false, L'^'},
    {VK_OEM_PLUS, /*shift=*/false, /*altgr=*/false, L'´'},
    {VK_OEM_PLUS, /*shift=*/true, /*altgr=*/false, L'`'},
};
constexpr AltGrPair kDeAltGr[] = {
    {'Q', L'@', L'\0'}, {'E', L'€', L'\0'}, {'M', L'µ', L'\0'},
    {'2', L'²', L'\0'}, {'3', L'³', L'\0'}, {'7', L'{', L'\0'},
    {'8', L'[', L'\0'}, {'9', L']', L'\0'}, {'0', L'}', L'\0'},
    {VK_OEM_MINUS, L'\\', L'\0'},  // 'ß' key
    {VK_OEM_6, L'~', L'\0'},       // '+' key
    {VK_OEM_102, L'|', L'\0'},
};

// Spanish (kbdsp): dead grave/circumflex left of Enter, dead acute/diaeresis
// right of 'ñ'.
constexpr DeadKeySpec kEsDeadKeys[] = {
    {VK_OEM_4, /*shift=*/false, /*altgr=*/false, L'`'},
    {VK_OEM_4, /*shift=*/true, /*altgr=*/false, L'^'},
    {VK_OEM_7, /*shift=*/false, /*altgr=*/false, L'´'},
    {VK_OEM_7, /*shift=*/true, /*altgr=*/false, L'¨'},
};
constexpr AltGrPair kEsAltGr[] = {
    {'1', L'|', L'\0'}, {'2', L'@', L'\0'}, {'3', L'#', L'\0'},
    {'4', L'~', L'\0'}, {'6', L'¬', L'\0'}, {'E', L'€', L'\0'},
    {VK_OEM_3, L'\\', L'\0'},  // 'º' key
    {VK_OEM_4, L'[', L'\0'},   // '`' dead key's AltGr level
    {VK_OEM_6, L']', L'\0'},   // '+' key
    {VK_OEM_7, L'{', L'\0'},   // '´' dead key's AltGr level
    {VK_OEM_5, L'}', L'\0'},   // 'ç' key
};

// Italian (kbdit): no dead keys on the basic layout.
constexpr AltGrPair kItAltGr[] = {
    {'E', L'€', L'\0'},
    {VK_OEM_1, L'@', L'\0'},  // 'ò' key
    {VK_OEM_7, L'#', L'\0'},  // 'à' key
    {VK_OEM_4, L'[', L'{'},   // 'è' key
    {VK_OEM_6, L']', L'}'},   // '+' key
};

// Dutch (kbdne): the layout is unusually dead-key heavy.
constexpr DeadKeySpec kNlDeadKeys[] = {
    {VK_OEM_4, /*shift=*/false, /*altgr=*/false, L'¨'},
    {VK_OEM_4, /*shift=*/true, /*altgr=*/false, L'^'},
    {VK_OEM_7, /*shift=*/false, /*altgr=*/false, L'´'},
    {VK_OEM_7, /*shift=*/true, /*altgr=*/false, L'`'},
    {VK_OEM_PLUS, /*shift=*/true, /*altgr=*/false, L'~'},
};
// Best effort: only the euro sign; the full kbdne AltGr layer is extensive.
constexpr AltGrPair kNlAltGr[] = {
    {'E', L'€', L'\0'},
};

// UK QWERTY (kbduk): no dead keys; AltGr carries the euro sign, broken bar,
// and acute-accented vowels.
constexpr AltGrPair kUkAltGr[] = {
    {'4', L'€', L'\0'}, {'A', L'á', L'Á'}, {'E', L'é', L'É'},
    {'I', L'í', L'Í'},  {'O', L'ó', L'Ó'}, {'U', L'ú', L'Ú'},
    {VK_OEM_3, L'¦', L'\0'},
};

// BEPO: the base-layer circumflex (physical Y position) is a dead key.
constexpr DeadKeySpec kBepoDeadKeys[] = {
    {'Y', /*shift=*/false, /*altgr=*/false, L'^'},
};
// Best effort: the BEPO AltGr layer is very large; this covers the most
// common programming characters and French ligatures. Not shipped with
// Windows, so verified against xkeyboard-config's fr(bepo) only.
constexpr AltGrPair kBepoAltGr[] = {
    {'A', L'æ', L'Æ'},        // physical A = bépo 'a'
    {'R', L'œ', L'Œ'},        // physical R = bépo 'o'
    {'F', L'€', L'\0'},       // physical F = bépo 'e'
    {'Z', L'\\', L'\0'},      // physical Z = bépo 'à'
    {'2', L'<', L'\0'},       // bépo '«' key
    {'3', L'>', L'\0'},       // bépo '»' key
    {'4', L'[', L'{'},        // bépo '(' key
    {'5', L']', L'}'},        // bépo ')' key
    {'E', L'&', L'\0'},       // physical E = bépo 'p'
};

// Windows composes CapsLock into the shift level for letters only; the
// composition pairs below cover the spacing accents used by the layouts
// above. A pair that is missing falls back to "<accent><char>".
struct DeadKeyComposition {
  wchar_t accent;
  wchar_t base;
  wchar_t composed;
};

constexpr DeadKeyComposition kDeadKeyCompositions[] = {
    // Circumflex.
    {L'^', L'a', L'â'}, {L'^', L'e', L'ê'}, {L'^', L'i', L'î'},
    {L'^', L'o', L'ô'}, {L'^', L'u', L'û'},
    {L'^', L'A', L'Â'}, {L'^', L'E', L'Ê'}, {L'^', L'I', L'Î'},
    {L'^', L'O', L'Ô'}, {L'^', L'U', L'Û'},
    // Diaeresis.
    {L'¨', L'a', L'ä'}, {L'¨', L'e', L'ë'}, {L'¨', L'i', L'ï'},
    {L'¨', L'o', L'ö'}, {L'¨', L'u', L'ü'}, {L'¨', L'y', L'ÿ'},
    {L'¨', L'A', L'Ä'}, {L'¨', L'E', L'Ë'}, {L'¨', L'I', L'Ï'},
    {L'¨', L'O', L'Ö'}, {L'¨', L'U', L'Ü'}, {L'¨', L'Y', L'Ÿ'},
    // Acute.
    {L'´', L'a', L'á'}, {L'´', L'e', L'é'}, {L'´', L'i', L'í'},
    {L'´', L'o', L'ó'}, {L'´', L'u', L'ú'}, {L'´', L'y', L'ý'},
    {L'´', L'A', L'Á'}, {L'´', L'E', L'É'}, {L'´', L'I', L'Í'},
    {L'´', L'O', L'Ó'}, {L'´', L'U', L'Ú'}, {L'´', L'Y', L'Ý'},
    // Grave.
    {L'`', L'a', L'à'}, {L'`', L'e', L'è'}, {L'`', L'i', L'ì'},
    {L'`', L'o', L'ò'}, {L'`', L'u', L'ù'},
    {L'`', L'A', L'À'}, {L'`', L'E', L'È'}, {L'`', L'I', L'Ì'},
    {L'`', L'O', L'Ò'}, {L'`', L'U', L'Ù'},
    // Tilde.
    {L'~', L'a', L'ã'}, {L'~', L'n', L'ñ'}, {L'~', L'o', L'õ'},
    {L'~', L'A', L'Ã'}, {L'~', L'N', L'Ñ'}, {L'~', L'O', L'Õ'},
};

struct LayoutExtras {
  const DeadKeySpec* dead_keys = nullptr;
  size_t num_dead_keys = 0;
  const AltGrPair* altgr = nullptr;
  size_t num_altgr = 0;
};

LayoutExtras ExtrasForLayout(config::MarinaKeyboardLayout layout) {
  switch (layout) {
    case config::MARINA_KBD_UK:
      return {nullptr, 0, kUkAltGr, std::size(kUkAltGr)};
    case config::MARINA_KBD_FR_AZERTY:
      return {kFrDeadKeys, std::size(kFrDeadKeys), kFrAltGr,
              std::size(kFrAltGr)};
    case config::MARINA_KBD_DE_QWERTZ:
      return {kDeDeadKeys, std::size(kDeDeadKeys), kDeAltGr,
              std::size(kDeAltGr)};
    case config::MARINA_KBD_ES:
      return {kEsDeadKeys, std::size(kEsDeadKeys), kEsAltGr,
              std::size(kEsAltGr)};
    case config::MARINA_KBD_IT:
      return {nullptr, 0, kItAltGr, std::size(kItAltGr)};
    case config::MARINA_KBD_NL:
      return {kNlDeadKeys, std::size(kNlDeadKeys), kNlAltGr,
              std::size(kNlAltGr)};
    case config::MARINA_KBD_BEPO:
      return {kBepoDeadKeys, std::size(kBepoDeadKeys), kBepoAltGr,
              std::size(kBepoAltGr)};
    default:
      // US, Dvorak, JIS: no dead keys, no AltGr layer.
      return {};
  }
}

wchar_t DeadKeyAccentFor(config::MarinaKeyboardLayout layout, BYTE virtual_key,
                         bool shift, bool altgr) {
  const LayoutExtras extras = ExtrasForLayout(layout);
  for (size_t i = 0; i < extras.num_dead_keys; ++i) {
    const DeadKeySpec& spec = extras.dead_keys[i];
    if (spec.vk == virtual_key && spec.shift == shift &&
        spec.altgr == altgr) {
      return spec.accent;
    }
  }
  return L'\0';
}

wchar_t AltGrCharacterFor(config::MarinaKeyboardLayout layout,
                          BYTE virtual_key, bool shift) {
  const LayoutExtras extras = ExtrasForLayout(layout);
  for (size_t i = 0; i < extras.num_altgr; ++i) {
    const AltGrPair& pair = extras.altgr[i];
    if (pair.vk == virtual_key) {
      return shift ? pair.shifted : pair.base;
    }
  }
  return L'\0';
}

wchar_t ComposeDeadKey(wchar_t accent, wchar_t base) {
  for (const DeadKeyComposition& composition : kDeadKeyCompositions) {
    if (composition.accent == accent && composition.base == base) {
      return composition.composed;
    }
  }
  return L'\0';
}

const CharPair* TableForLayout(config::MarinaKeyboardLayout layout) {
  switch (layout) {
    case config::MARINA_KBD_US:
      return kUs;
    case config::MARINA_KBD_UK:
      return kUk;
    case config::MARINA_KBD_FR_AZERTY:
      return kFrAzerty;
    case config::MARINA_KBD_DE_QWERTZ:
      return kDeQwertz;
    case config::MARINA_KBD_ES:
      return kEs;
    case config::MARINA_KBD_IT:
      return kIt;
    case config::MARINA_KBD_NL:
      return kNl;
    case config::MARINA_KBD_DVORAK:
      return kDvorak;
    case config::MARINA_KBD_BEPO:
      return kBepo;
    default:
      return nullptr;
  }
}

}  // namespace

// static
wchar_t RomajiKeyboardLayoutEmulator::GetCharacterForKeyDown(
    config::MarinaKeyboardLayout layout, BYTE virtual_key, bool shift,
    bool capslock) {
  if (layout == config::MARINA_KBD_JIS) {
    BYTE keyboard_state[256] = {};
    if (shift) {
      keyboard_state[VK_SHIFT] = 0x80;
    }
    if (capslock) {
      keyboard_state[VK_CAPITAL] = 0x1;
    }
    return JapaneseKeyboardLayoutEmulator::GetCharacterForKeyDown(
        virtual_key, keyboard_state, /*is_menu_active=*/false);
  }

  const CharPair* table = TableForLayout(layout);
  if (table == nullptr) {
    return L'\0';
  }
  const int index = FindVkIndex(virtual_key);
  if (index < 0) {
    return L'\0';
  }

  const bool is_letter = ('A' <= virtual_key && virtual_key <= 'Z');
  bool effective_shift = shift;
  if (is_letter && capslock) {
    effective_shift = !effective_shift;
  }

  const CharPair& pair = table[index];
  return effective_shift ? pair.shifted : pair.base;
}

// static
RomajiKeyboardLayoutEmulator::DirectModeKeyOutput
RomajiKeyboardLayoutEmulator::ResolveDirectModeKey(
    config::MarinaKeyboardLayout layout, BYTE virtual_key, bool shift,
    bool altgr, bool capslock, wchar_t pending_dead_key) {
  DirectModeKeyOutput output;
  if (layout == config::MARINA_KBD_OS_DEFAULT) {
    return output;
  }

  // Space commits the spacing form of a pending dead key ("^" + space types
  // "^"), exactly like a real Windows dead key. Without a pending dead key,
  // space is none of our business.
  if (virtual_key == VK_SPACE) {
    if (pending_dead_key == L'\0') {
      return output;
    }
    output.handled = true;
    output.commit_text.assign(1, pending_dead_key);
    output.next_pending_dead_key = L'\0';
    return output;
  }

  // Dead key pressed: consume it. If another dead key was already pending,
  // Windows commits the first accent's spacing form and the new key becomes
  // the pending one.
  const wchar_t accent = DeadKeyAccentFor(layout, virtual_key, shift, altgr);
  if (accent != L'\0') {
    output.handled = true;
    if (pending_dead_key != L'\0') {
      output.commit_text.assign(1, pending_dead_key);
    }
    output.next_pending_dead_key = accent;
    return output;
  }

  wchar_t character = L'\0';
  if (altgr) {
    // Only explicitly populated AltGr combinations are handled; everything
    // else passes through to the application.
    character = AltGrCharacterFor(layout, virtual_key, shift);
  } else {
    character = GetCharacterForKeyDown(layout, virtual_key, shift, capslock);
  }
  if (character == L'\0') {
    return output;
  }

  output.handled = true;
  output.next_pending_dead_key = L'\0';
  if (pending_dead_key != L'\0') {
    const wchar_t composed = ComposeDeadKey(pending_dead_key, character);
    if (composed != L'\0') {
      output.commit_text.assign(1, composed);
    } else {
      // Non-composable pair: commit the spacing accent followed by the
      // character, mirroring Windows behavior.
      output.commit_text.assign(1, pending_dead_key);
      output.commit_text.push_back(character);
    }
    return output;
  }
  output.commit_text.assign(1, character);
  return output;
}

}  // namespace win32
}  // namespace mozc
