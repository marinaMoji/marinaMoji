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

#include "win32/base/sticky_keys_util.h"

#include "absl/log/log.h"

namespace mozc {
namespace win32 {

bool StickyKeysUtil::DisableHotkey() {
  if (saved_) {
    // Already disabled by this instance; nothing further to save.
    return true;
  }

  STICKYKEYS current = {sizeof(STICKYKEYS)};
  if (!::SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &current,
                              0)) {
    LOG(WARNING) << "SPI_GETSTICKYKEYS failed: " << ::GetLastError();
    return false;
  }

  STICKYKEYS next = current;
  // Clear the "press Shift 5 times" activation gesture and its confirmation
  // dialog/sound. Deliberately leave SKF_STICKYKEYSON and SKF_AVAILABLE
  // untouched: this must never switch StickyKeys itself on or off, only stop
  // it from being toggled on by an incidental Shift-tap pattern.
  next.dwFlags &= ~(SKF_HOTKEYACTIVE | SKF_CONFIRMHOTKEY | SKF_HOTKEYSOUND);
  if (next.dwFlags == current.dwFlags) {
    // Hotkey was already off (or StickyKeys is policy-locked to off);
    // nothing to change or restore.
    return true;
  }

  if (!::SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &next,
                              SPIF_SENDCHANGE)) {
    LOG(WARNING) << "SPI_SETSTICKYKEYS failed: " << ::GetLastError();
    return false;
  }

  previous_ = current;
  saved_ = true;
  return true;
}

void StickyKeysUtil::RestoreHotkey() {
  if (!saved_) {
    return;
  }
  if (!::SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS),
                              &previous_, SPIF_SENDCHANGE)) {
    LOG(WARNING) << "SPI_SETSTICKYKEYS (restore) failed: "
                << ::GetLastError();
  }
  saved_ = false;
}

// static
bool StickyKeysUtil::IsCurrentlyOn() {
  STICKYKEYS current = {sizeof(STICKYKEYS)};
  if (!::SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &current,
                              0)) {
    return false;
  }
  return (current.dwFlags & SKF_STICKYKEYSON) != 0;
}

}  // namespace win32
}  // namespace mozc
