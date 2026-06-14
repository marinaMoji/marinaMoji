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

#include "unix/ibus/ibus_physical_slot.h"

#include <ibus.h>

namespace mozc {
namespace ibus {
namespace {

using ::mozc::config::MarinaPhysicalSlot;
using ::mozc::config::MarinaShortcutModifier;

// Linux evdev KEY_* codes (linux/input-event-codes.h) and X11 keycodes (evdev+8).
constexpr uint kEvdevKey1 = 2;
constexpr uint kEvdevKey0 = 11;
constexpr uint kEvdevKeyGrave = 41;
constexpr uint kEvdevKeyBackspace = 14;
constexpr uint kX11KeycodeOffset = 8;

std::optional<MarinaPhysicalSlot> SlotFromEvdevCode(uint evdev_code) {
  if (evdev_code >= kEvdevKey1 && evdev_code <= kEvdevKey0) {
    const int index = static_cast<int>(evdev_code - kEvdevKey1);
    return static_cast<MarinaPhysicalSlot>(
        static_cast<int>(MarinaPhysicalSlot::MARINA_SLOT_1) + index);
  }
  if (evdev_code == kEvdevKeyGrave) {
    return MarinaPhysicalSlot::MARINA_SLOT_GRAVE;
  }
  return std::nullopt;
}

}  // namespace

std::optional<MarinaPhysicalSlot> IbusKeycodeToPhysicalSlot(uint keycode) {
  if (keycode == 0) {
    return std::nullopt;
  }
  // IBus may pass evdev codes directly; never treat Backspace as number-row "5".
  if (keycode == kEvdevKeyBackspace) {
    return std::nullopt;
  }
  if (const auto slot = SlotFromEvdevCode(keycode); slot.has_value()) {
    return slot;
  }
  if (keycode > kX11KeycodeOffset) {
    const uint evdev_code = keycode - kX11KeycodeOffset;
    if (evdev_code == kEvdevKeyBackspace) {
      return std::nullopt;
    }
    return SlotFromEvdevCode(evdev_code);
  }
  return std::nullopt;
}

MarinaShortcutModifier IbusModifiersToMarinaModifier(uint modifiers) {
  const bool has_ctrl = (modifiers & IBUS_CONTROL_MASK) != 0;
  const bool has_shift = (modifiers & IBUS_SHIFT_MASK) != 0;
  if (has_ctrl && has_shift) {
    return MarinaShortcutModifier::MARINA_MOD_CTRL_SHIFT;
  }
  if (has_ctrl) {
    return MarinaShortcutModifier::MARINA_MOD_CTRL;
  }
  return MarinaShortcutModifier::MARINA_MOD_CTRL_SHIFT;
}

}  // namespace ibus
}  // namespace mozc
