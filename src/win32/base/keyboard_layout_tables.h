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

#ifndef MOZC_WIN32_BASE_KEYBOARD_LAYOUT_TABLES_H_
#define MOZC_WIN32_BASE_KEYBOARD_LAYOUT_TABLES_H_

#include <windows.h>

#include "protocol/config.pb.h"

namespace mozc {
namespace win32 {

// Emulates key -> character resolution for a small set of fixed, hardcoded
// Western keyboard layouts, independent of the OS's actual active layout.
// This lets a user explicitly pick e.g. "French AZERTY" and get
// AZERTY-correct romaji letters/punctuation regardless of what Windows'
// active input layout happens to be.
//
// This is a deliberate alternative to following the OS's live active
// layout: Windows assigns VK_A..VK_Z by physical key position (not by
// printed character), so KeyEventHandler::ConvertToKeyEvent hardcodes
// 'a'+(VK-'A') for those keys (see keyevent_handler.cc), which implicitly
// assumes a US/QWERTY-shaped keyboard. This class provides the equivalent
// fixed lookup for other layouts, mirroring the existing
// JapaneseKeyboardLayoutEmulator pattern (see keyboard.h/.cc) but as a
// user-selected, always-on table rather than a Kana-lock emulation.
//
// Limitation shared with the rest of this file: there is no AltGr or
// dead-key composition support. Every entry is a single non-combining
// character, so layout keys that normally require dead-key composition
// (e.g. French AZERTY's circumflex/diaeresis key, Spanish's accent keys)
// will type the raw base glyph instead of composing an accented letter.
class RomajiKeyboardLayoutEmulator {
 public:
  RomajiKeyboardLayoutEmulator() = delete;
  RomajiKeyboardLayoutEmulator(const RomajiKeyboardLayoutEmulator&) = delete;
  RomajiKeyboardLayoutEmulator& operator=(const RomajiKeyboardLayoutEmulator&) =
      delete;

  // Returns the character produced by |virtual_key| under |layout|, given
  // |shift| and |capslock| state, or L'\0' if this VK produces no character
  // for the given layout (VK not covered by the table, or an unpopulated
  // best-effort entry). |capslock| only affects letters (A-Z), matching the
  // semantics already used for MARINA_KBD_OS_DEFAULT in KeyEventHandler.
  // MARINA_KBD_JIS delegates to JapaneseKeyboardLayoutEmulator.
  static wchar_t GetCharacterForKeyDown(
      config::MarinaKeyboardLayout layout, BYTE virtual_key,
      bool shift, bool capslock);
};

}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_BASE_KEYBOARD_LAYOUT_TABLES_H_
