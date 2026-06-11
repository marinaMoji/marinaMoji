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

#include "protocol/config.pb.h"
#include "testing/gunit.h"

namespace mozc {
namespace ibus {
namespace {

TEST(IbusPhysicalSlotTest, EvdevKeycodes) {
  const auto slot1 = IbusKeycodeToPhysicalSlot(2);
  ASSERT_TRUE(slot1.has_value());
  EXPECT_EQ(*slot1, config::MarinaPhysicalSlot::MARINA_SLOT_1);

  const auto slot0 = IbusKeycodeToPhysicalSlot(11);
  ASSERT_TRUE(slot0.has_value());
  EXPECT_EQ(*slot0, config::MarinaPhysicalSlot::MARINA_SLOT_0);
}

TEST(IbusPhysicalSlotTest, X11Keycodes) {
  const auto slot3 = IbusKeycodeToPhysicalSlot(12);
  ASSERT_TRUE(slot3.has_value());
  EXPECT_EQ(*slot3, config::MarinaPhysicalSlot::MARINA_SLOT_3);
}

TEST(IbusPhysicalSlotTest, Modifiers) {
  EXPECT_EQ(IbusModifiersToMarinaModifier(IBUS_CONTROL_MASK),
            config::MarinaShortcutModifier::MARINA_MOD_CTRL);
  EXPECT_EQ(IbusModifiersToMarinaModifier(IBUS_CONTROL_MASK | IBUS_SHIFT_MASK),
            config::MarinaShortcutModifier::MARINA_MOD_CTRL_SHIFT);
}

}  // namespace
}  // namespace ibus
}  // namespace mozc
