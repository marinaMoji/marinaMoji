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

#ifndef MOZC_WIN32_BASE_STICKY_KEYS_UTIL_H_
#define MOZC_WIN32_BASE_STICKY_KEYS_UTIL_H_

#include <windows.h>

namespace mozc {
namespace win32 {

// marinaMoji: our Left/Right Shift tap gestures (Session::IsLeftShiftAlone,
// IsRightShiftAlone, IsCtrlAltRightShiftAlone -- see session/session.cc)
// collide with two independent Windows StickyKeys behaviors:
//
//  - The "press Shift 5 times" activation gesture pops up a system dialog
//    (and, if accepted, turns StickyKeys on) after five Shift presses with
//    no other key in between -- exactly the pattern of switching modes
//    briskly with our own shortcuts.
//  - If StickyKeys is actually on, a lone Shift tap *latches* Shift for the
//    next keystroke, which silently capitalizes the following macron vowel
//    (typing "Right Shift, a" yields Ā instead of ā) and generally fights
//    our own tap detection.
//
// StickyKeysUtil::DisableHotkey turns off only the 5x-Shift activation
// gesture (and its confirmation popup/sound), for the current process only,
// restored via RestoreHotkey. It never touches whether StickyKeys itself is
// currently on: that is a deliberate accessibility choice for some users,
// and this class only lets the caller detect it (IsCurrentlyOn) so the UI
// can warn instead of silently overriding it.
class StickyKeysUtil {
 public:
  StickyKeysUtil() = default;
  StickyKeysUtil(const StickyKeysUtil&) = delete;
  StickyKeysUtil& operator=(const StickyKeysUtil&) = delete;
  ~StickyKeysUtil() { RestoreHotkey(); }

  // Turns off the "press Shift 5 times" popup, saving the previous state so
  // RestoreHotkey can put it back. Safe to call more than once -- only the
  // first call (until restored) records the previous state and touches the
  // system setting. Returns false if the OS call failed (nothing is
  // changed); this can happen under group policy lockdown, in which case
  // the caller should fall back to telling the user to disable it by hand.
  bool DisableHotkey();

  // Restores whatever the hotkey setting was before DisableHotkey. No-op if
  // DisableHotkey was never called or already restored. Also run from the
  // destructor as a safety net.
  void RestoreHotkey();

  // True if StickyKeys is currently latching modifier keys for the user (as
  // opposed to merely being available/off). Does not require a prior
  // DisableHotkey call.
  static bool IsCurrentlyOn();

 private:
  bool saved_ = false;
  STICKYKEYS previous_{};
};

}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_BASE_STICKY_KEYS_UTIL_H_
