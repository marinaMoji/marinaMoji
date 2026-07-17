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

#include "win32/tip/win32_physical_slot.h"

namespace mozc {
namespace win32 {
namespace tsf {
namespace {

using ::mozc::config::MarinaPhysicalSlot;

// PC/AT scan-code-set-1 codes (identical to Linux evdev KEY_1..KEY_0 and
// KEY_GRAVE/KEY_BACKSPACE values used by unix/ibus/ibus_physical_slot.cc).
constexpr BYTE kScanCode1 = 0x02;
constexpr BYTE kScanCode0 = 0x0B;
constexpr BYTE kScanCodeGrave = 0x29;
constexpr BYTE kScanCodeBackspace = 0x0E;

}  // namespace

std::optional<MarinaPhysicalSlot> ScanCodeToPhysicalSlot(BYTE scan_code) {
  // Never treat Backspace as number-row "5"; its scan code can otherwise
  // collide with number-row heuristics on some paths (same guard as
  // unix/ibus/ibus_physical_slot.cc's SlotFromEvdevCode).
  if (scan_code == kScanCodeBackspace) {
    return std::nullopt;
  }
  if (scan_code >= kScanCode1 && scan_code <= kScanCode0) {
    const int index = static_cast<int>(scan_code - kScanCode1);
    return static_cast<MarinaPhysicalSlot>(
        static_cast<int>(MarinaPhysicalSlot::MARINA_SLOT_1) + index);
  }
  if (scan_code == kScanCodeGrave) {
    return MarinaPhysicalSlot::MARINA_SLOT_GRAVE;
  }
  return std::nullopt;
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
