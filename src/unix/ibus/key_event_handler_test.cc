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

#include "unix/ibus/key_event_handler.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "absl/random/random.h"
#include "absl/strings/str_format.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "testing/gunit.h"

namespace mozc {
namespace ibus {

class KeyEventHandlerTest : public testing::Test {
 protected:
  virtual void SetUp() {
    handler_.reset(new KeyEventHandler);

    keyval_to_modifier_.clear();
    keyval_to_modifier_[IBUS_Shift_L] = commands::KeyEvent::SHIFT;
    keyval_to_modifier_[IBUS_Shift_R] = commands::KeyEvent::SHIFT;
    keyval_to_modifier_[IBUS_Control_L] = commands::KeyEvent::CTRL;
    keyval_to_modifier_[IBUS_Control_R] = commands::KeyEvent::CTRL;
    keyval_to_modifier_[IBUS_Alt_L] = commands::KeyEvent::ALT;
    keyval_to_modifier_[IBUS_Alt_R] = commands::KeyEvent::ALT;
  }

  // Currently this function does not supports special keys.
  void AppendToKeyEvent(uint keyval, commands::KeyEvent* key) const {
    const std::map<uint, commands::KeyEvent::ModifierKey>::const_iterator it =
        keyval_to_modifier_.find(keyval);
    if (it != keyval_to_modifier_.end()) {
      bool found = false;
      for (int i = 0; i < key->modifier_keys_size(); ++i) {
        if (key->modifier_keys(i) == it->second) {
          found = true;
          break;
        }
      }
      if (!found) {
        key->add_modifier_keys(it->second);
      }
    } else {
      key->set_key_code(keyval);
    }
  }

  bool ProcessKey(bool is_key_up, uint keyval, commands::KeyEvent* key) {
    AppendToKeyEvent(keyval, key);
    return handler_->ProcessModifiers(is_key_up, keyval, /*keycode=*/0, key);
  }

  bool ProcessKeyWithCapsLock(bool is_key_up, uint keyval,
                              commands::KeyEvent* key) {
    key->add_modifier_keys(commands::KeyEvent::CAPS);
    return ProcessKey(is_key_up, keyval, key);
  }

  bool IsPressed(uint keyval) const {
    return handler_->currently_pressed_modifiers_.count(keyval) != 0;
  }

  bool is_non_modifier_key_pressed() {
    return handler_->is_non_modifier_key_pressed_;
  }

  const std::map<uint, uint>& currently_pressed_modifiers() {
    return handler_->currently_pressed_modifiers_;
  }

  std::vector<std::pair<uint, uint>> TrackedReleaseKeys() {
    return handler_->TrackedReleaseKeys();
  }

  const std::set<commands::KeyEvent::ModifierKey>& modifiers_to_be_sent() {
    return handler_->modifiers_to_be_sent_;
  }

  testing::AssertionResult CheckModifiersToBeSent(uint32_t modifiers) {
    uint32_t to_be_sent_mask = 0;
    for (std::set<commands::KeyEvent::ModifierKey>::iterator it =
             modifiers_to_be_sent().begin();
         it != modifiers_to_be_sent().end(); ++it) {
      to_be_sent_mask |= *it;
    }

    if (modifiers != to_be_sent_mask) {
      return testing::AssertionFailure()
             << "\n"
             << "Expected: " << modifiers << "\n"
             << "  Actual: " << to_be_sent_mask << "\n";
    }

    return testing::AssertionSuccess();
  }

  testing::AssertionResult CheckModifiersPressed(bool expect_pressed) {
    bool pressed = !currently_pressed_modifiers().empty();

    if (pressed == expect_pressed) {
      return testing::AssertionSuccess();
    } else {
      return testing::AssertionFailure();
    }
  }

  std::unique_ptr<KeyEventHandler> handler_;
  std::map<uint, commands::KeyEvent::ModifierKey> keyval_to_modifier_;
};

#define EXPECT_MODIFIERS_TO_BE_SENT(modifiers) \
  EXPECT_TRUE(CheckModifiersToBeSent(modifiers))
#define EXPECT_MODIFIERS_PRESSED() EXPECT_TRUE(CheckModifiersPressed(true))
#define EXPECT_NO_MODIFIERS_PRESSED() EXPECT_TRUE(CheckModifiersPressed(false))

namespace {
constexpr uint32_t kNoModifiers = 0;
const uint kDummyKeycode = 0;
}  // namespace

TEST_F(KeyEventHandlerTest, GetKeyEvent) {
  commands::KeyEvent key;

  {  // Alt down => a down => a up => Alt up
    key.Clear();
    EXPECT_FALSE(handler_->GetKeyEvent(IBUS_Alt_L, kDummyKeycode,
                                       IBUS_MOD1_MASK, config::Config::ROMAN,
                                       true, &key));
    EXPECT_MODIFIERS_TO_BE_SENT(commands::KeyEvent::ALT);
    EXPECT_MODIFIERS_PRESSED();

    key.Clear();
    EXPECT_TRUE(handler_->GetKeyEvent(IBUS_a, 'a', IBUS_MOD1_MASK,
                                      config::Config::ROMAN, true, &key));
    EXPECT_MODIFIERS_TO_BE_SENT(kNoModifiers);
    EXPECT_MODIFIERS_PRESSED();

    key.Clear();
    EXPECT_FALSE(handler_->GetKeyEvent(IBUS_a, 'a',
                                       (IBUS_MOD1_MASK | IBUS_RELEASE_MASK),
                                       config::Config::ROMAN, true, &key));
    EXPECT_MODIFIERS_TO_BE_SENT(kNoModifiers);
    EXPECT_MODIFIERS_PRESSED();

    key.Clear();
    EXPECT_FALSE(handler_->GetKeyEvent(IBUS_Alt_L, kDummyKeycode,
                                       (IBUS_MOD1_MASK | IBUS_RELEASE_MASK),
                                       config::Config::ROMAN, true, &key));
    EXPECT_MODIFIERS_TO_BE_SENT(kNoModifiers);
    EXPECT_NO_MODIFIERS_PRESSED();
  }

  {  // Ignore Super (Mod4)
    key.Clear();
    EXPECT_FALSE(handler_->GetKeyEvent(IBUS_space, kDummyKeycode,
                                       IBUS_MOD4_MASK, config::Config::ROMAN,
                                       true, &key));
  }

  // This test fails in current implementation.
  // TODO(hsumita): Enables it.
  /*
  {  // a down => Alt down => Alt up => a up
    key.Clear();
    EXPECT_TRUE(handler_->GetKeyEvent(
        IBUS_a, 'a', kDummyKeycode, config::Config::ROMAN, true, &key));
    EXPECT_MODIFIERS_TO_BE_SENT(kNoModifiers);
    EXPECT_NO_MODIFIERS_PRESSED();

    key.Clear();
    EXPECT_FALSE(handler_->GetKeyEvent(
        IBUS_Alt_L, kDummyKeycode, IBUS_MOD1_MASK,
        config::Config::ROMAN, true, &key));
    EXPECT_MODIFIERS_TO_BE_SENT(kNoModifiers);
    EXPECT_MODIFIERS_PRESSED();

    key.Clear();
    EXPECT_TRUE(handler_->GetKeyEvent(
        IBUS_Alt_L, kDummyKeycode, (IBUS_MOD1_MASK | IBUS_RELEASE_MASK),
        config::Config::ROMAN, true, &key));
    EXPECT_MODIFIERS_TO_BE_SENT(kNoModifiers);
    EXPECT_NO_MODIFIERS_PRESSED();

    key.Clear();
    EXPECT_TRUE(handler_->GetKeyEvent(
        IBUS_a, 'a', IBUS_RELEASE_MASK, config::Config::ROMAN, true, &key));
    EXPECT_MODIFIERS_TO_BE_SENT(kNoModifiers);
    EXPECT_NO_MODIFIERS_PRESSED();
  }
  */
}

TEST_F(KeyEventHandlerTest, ProcessShiftModifiers) {
  commands::KeyEvent key;

  // 'Shift-a' senario
  // Shift down
  EXPECT_FALSE(ProcessKey(false, IBUS_Shift_L, &key));
  EXPECT_TRUE(IsPressed(IBUS_Shift_L));
  EXPECT_MODIFIERS_TO_BE_SENT(commands::KeyEvent::SHIFT);

  // "a" down. Shift stays tracked while it is physically held (upstream
  // wiped it here, which desynced the release); the pending-toggle state is
  // what gets discarded.
  key.Clear();
  EXPECT_TRUE(ProcessKey(false, 'a', &key));
  EXPECT_TRUE(IsPressed(IBUS_Shift_L));
  EXPECT_MODIFIERS_TO_BE_SENT(kNoModifiers);

  // "a" up
  key.Clear();
  EXPECT_FALSE(ProcessKey(true, 'a', &key));
  EXPECT_TRUE(IsPressed(IBUS_Shift_L));
  EXPECT_MODIFIERS_TO_BE_SENT(kNoModifiers);

  // Shift up: no toggle fires, tracking is clean.
  key.Clear();
  EXPECT_FALSE(ProcessKey(true, IBUS_Shift_L, &key));
  EXPECT_NO_MODIFIERS_PRESSED();
  EXPECT_MODIFIERS_TO_BE_SENT(kNoModifiers);

  /* Currently following test scenario does not pass.
   * This bug was issued as b/4338394
  // 'Shift-0' senario
  // Shift down
  EXPECT_FALSE(ProcessKey(false, IBUS_Shift_L, &key));
  EXPECT_TRUE(IsPressed(IBUS_Shift_L));
  EXPECT_MODIFIERS_TO_BE_SENT(commands::KeyEvent::SHIFT);

  // "0" down
  key.Clear();
  EXPECT_TRUE(ProcessKey(false, '0', &key));
  EXPECT_TRUE(IsPressed(IBUS_Shift_L));
  EXPECT_MODIFIERS_TO_BE_SENT(kNoModifiers);
  EXPECT_EQ(modifiers_to_be_sent_.size(), 0);

  // "0" up
  key.Clear();
  EXPECT_FALSE(ProcessKey(true, '0', &key));
  EXPECT_TRUE(IsPressed(IBUS_Shift_L));
  EXPECT_MODIFIERS_TO_BE_SENT(kNoModifiers);

  // Shift up
  key.Clear();
  EXPECT_TRUE(ProcessKey(true, IBUS_Shift_L, &key));
  EXPECT_NO_MODIFIERS_PRESSED();
  EXPECT_MODIFIERS_TO_BE_SENT(kNoModifiers);
  */
}

TEST_F(KeyEventHandlerTest, ProcessAltModifiers) {
  commands::KeyEvent key;

  // Alt down
  EXPECT_FALSE(ProcessKey(false, IBUS_Alt_L, &key));
  EXPECT_TRUE(IsPressed(IBUS_Alt_L));
  EXPECT_MODIFIERS_TO_BE_SENT(commands::KeyEvent::ALT);

  // "a" down
  key.Clear();
  key.add_modifier_keys(commands::KeyEvent::ALT);
  EXPECT_TRUE(ProcessKey(false, 'a', &key));
  EXPECT_TRUE(IsPressed(IBUS_Alt_L));
  EXPECT_MODIFIERS_TO_BE_SENT(kNoModifiers);

  // "a" up
  key.Clear();
  key.add_modifier_keys(commands::KeyEvent::ALT);
  EXPECT_FALSE(ProcessKey(true, 'a', &key));
  EXPECT_TRUE(IsPressed(IBUS_Alt_L));
  EXPECT_MODIFIERS_TO_BE_SENT(kNoModifiers);

  // ALt up
  key.Clear();
  EXPECT_FALSE(ProcessKey(true, IBUS_Alt_L, &key));
  EXPECT_NO_MODIFIERS_PRESSED();
  EXPECT_MODIFIERS_TO_BE_SENT(kNoModifiers);
}

TEST_F(KeyEventHandlerTest, ProcessCtrlModifiers) {
  commands::KeyEvent key;

  // Ctrl down
  EXPECT_FALSE(ProcessKey(false, IBUS_Control_L, &key));
  EXPECT_TRUE(IsPressed(IBUS_Control_L));
  EXPECT_MODIFIERS_TO_BE_SENT(commands::KeyEvent::CTRL);

  // "a" down
  key.Clear();
  key.add_modifier_keys(commands::KeyEvent::CTRL);
  EXPECT_TRUE(ProcessKey(false, 'a', &key));
  EXPECT_TRUE(IsPressed(IBUS_Control_L));
  EXPECT_MODIFIERS_TO_BE_SENT(kNoModifiers);

  // "a" up
  key.Clear();
  key.add_modifier_keys(commands::KeyEvent::CTRL);
  EXPECT_FALSE(ProcessKey(true, 'a', &key));
  EXPECT_TRUE(IsPressed(IBUS_Control_L));
  EXPECT_MODIFIERS_TO_BE_SENT(kNoModifiers);

  // Ctrl up
  key.Clear();
  EXPECT_FALSE(ProcessKey(true, IBUS_Control_L, &key));
  EXPECT_NO_MODIFIERS_PRESSED();
  EXPECT_MODIFIERS_TO_BE_SENT(kNoModifiers);
}

TEST_F(KeyEventHandlerTest, ProcessShiftModifiersWithCapsLockOn) {
  commands::KeyEvent key;

  // 'Shift-a' senario
  // Shift down
  EXPECT_FALSE(ProcessKeyWithCapsLock(false, IBUS_Shift_L, &key));
  EXPECT_TRUE(IsPressed(IBUS_Shift_L));
  EXPECT_MODIFIERS_TO_BE_SENT(
      (commands::KeyEvent::CAPS | commands::KeyEvent::SHIFT));

  // "a" down. As above, the held Shift keeps its tracking.
  key.Clear();
  EXPECT_TRUE(ProcessKeyWithCapsLock(false, 'a', &key));
  EXPECT_TRUE(IsPressed(IBUS_Shift_L));
  EXPECT_MODIFIERS_TO_BE_SENT(kNoModifiers);

  // "a" up
  key.Clear();
  EXPECT_FALSE(ProcessKeyWithCapsLock(true, 'a', &key));
  EXPECT_TRUE(IsPressed(IBUS_Shift_L));
  EXPECT_MODIFIERS_TO_BE_SENT(kNoModifiers);

  // Shift up
  key.Clear();
  EXPECT_FALSE(ProcessKeyWithCapsLock(true, IBUS_Shift_L, &key));
  EXPECT_NO_MODIFIERS_PRESSED();
  EXPECT_MODIFIERS_TO_BE_SENT(kNoModifiers);
}

TEST_F(KeyEventHandlerTest, LeftRightModifiers) {
  commands::KeyEvent key;

  // Left-Shift down
  EXPECT_FALSE(ProcessKey(false, IBUS_Shift_L, &key));
  EXPECT_TRUE(IsPressed(IBUS_Shift_L));
  EXPECT_MODIFIERS_TO_BE_SENT(commands::KeyEvent::SHIFT);

  // Right-Shift down with Left Shift already pressed: not sent (normal modifier)
  key.Clear();
  key.add_modifier_keys(commands::KeyEvent::SHIFT);
  EXPECT_FALSE(ProcessKey(false, IBUS_Shift_R, &key));
  EXPECT_TRUE(IsPressed(IBUS_Shift_L));
  EXPECT_TRUE(IsPressed(IBUS_Shift_R));
  EXPECT_MODIFIERS_TO_BE_SENT(commands::KeyEvent::SHIFT);
}

TEST_F(KeyEventHandlerTest, RightShiftAloneSentOnKeyUp) {
  // Right Shift pressed alone is sent on key up for ToggleManyoshuHiragana
  // (so it only toggles when released by itself, not when used to capitalize).
  commands::KeyEvent key;
  key.add_modifier_keys(commands::KeyEvent::SHIFT);
  key.add_modifier_keys(commands::KeyEvent::RIGHT_SHIFT);
  EXPECT_FALSE(ProcessKey(false, IBUS_Shift_R, &key));  // down: not sent
  key.Clear();
  key.add_modifier_keys(commands::KeyEvent::SHIFT);
  key.add_modifier_keys(commands::KeyEvent::RIGHT_SHIFT);
  EXPECT_TRUE(ProcessKey(true, IBUS_Shift_R, &key));  // up: sent
  EXPECT_NO_MODIFIERS_PRESSED();
  EXPECT_MODIFIERS_TO_BE_SENT(kNoModifiers);
}

TEST_F(KeyEventHandlerTest, TrackedReleaseKeys) {
  commands::KeyEvent key;
  EXPECT_TRUE(TrackedReleaseKeys().empty());

  ProcessKey(false, IBUS_Shift_R, &key);
  std::vector<std::pair<uint, uint>> tracked = TrackedReleaseKeys();
  ASSERT_EQ(tracked.size(), 1u);
  EXPECT_EQ(tracked[0].first, static_cast<uint>(IBUS_Shift_R));

  key.Clear();
  ProcessKey(false, IBUS_Shift_L, &key);
  key.Clear();
  ProcessKey(false, IBUS_Control_L, &key);
  EXPECT_EQ(TrackedReleaseKeys().size(), 3u);

  // Alt is tracked but not release-forwarded.
  key.Clear();
  ProcessKey(false, IBUS_Alt_L, &key);
  EXPECT_EQ(TrackedReleaseKeys().size(), 3u);

  // Forwarding with no engine must be a safe no-op.
  handler_->ForwardTrackedReleases(nullptr);

  handler_->Clear();
  EXPECT_TRUE(TrackedReleaseKeys().empty());
}

TEST_F(KeyEventHandlerTest, NonModifierKeysTrackedForRelease) {
  commands::KeyEvent key;
  constexpr uint kReturnKeycode = 28;

  EXPECT_TRUE(handler_->GetKeyEvent(IBUS_Return, kReturnKeycode, 0,
                                    config::Config::ROMAN, true, &key));
  std::vector<std::pair<uint, uint>> tracked = TrackedReleaseKeys();
  ASSERT_EQ(tracked.size(), 1u);
  EXPECT_EQ(tracked[0].first, static_cast<uint>(IBUS_Return));
  EXPECT_EQ(tracked[0].second, kReturnKeycode);

  key.Clear();
  handler_->GetKeyEvent(IBUS_Return, kReturnKeycode, IBUS_RELEASE_MASK,
                        config::Config::ROMAN, true, &key);
  EXPECT_TRUE(TrackedReleaseKeys().empty());
}

TEST_F(KeyEventHandlerTest, NotifyKeyStateTracksKeysOutsideGetKeyEvent) {
  // The synthesized Backspace paths in MozcEngine bypass GetKeyEvent;
  // NotifyKeyState must keep the release failsafe covering them.
  constexpr uint kBackSpaceKeycode = 14;

  handler_->NotifyKeyState(IBUS_BackSpace, kBackSpaceKeycode, false);
  std::vector<std::pair<uint, uint>> tracked = TrackedReleaseKeys();
  ASSERT_EQ(tracked.size(), 1u);
  EXPECT_EQ(tracked[0].first, static_cast<uint>(IBUS_BackSpace));
  EXPECT_EQ(tracked[0].second, kBackSpaceKeycode);

  handler_->NotifyKeyState(IBUS_BackSpace, kBackSpaceKeycode, true);
  EXPECT_TRUE(TrackedReleaseKeys().empty());

  // Modifier keyvals are ignored: they must go through the state machine.
  handler_->NotifyKeyState(IBUS_Shift_L, 42, false);
  EXPECT_TRUE(TrackedReleaseKeys().empty());
}

TEST_F(KeyEventHandlerTest, ShiftHeldThroughCapitalTypingStaysTracked) {
  // Regression test for the stuck-Shift bug: Shift + 'a' translates to 'A'
  // with zero modifier keys, which used to blanket-Clear() all tracking and
  // desync the still-held Shift.
  commands::KeyEvent key;
  constexpr uint kShiftLKeycode = 42;

  EXPECT_FALSE(handler_->GetKeyEvent(IBUS_Shift_L, kShiftLKeycode, 0,
                                     config::Config::ROMAN, true, &key));
  EXPECT_TRUE(IsPressed(IBUS_Shift_L));

  key.Clear();
  EXPECT_TRUE(handler_->GetKeyEvent(IBUS_A, 30, IBUS_SHIFT_MASK,
                                    config::Config::ROMAN, true, &key));
  EXPECT_TRUE(IsPressed(IBUS_Shift_L));
  EXPECT_TRUE(is_non_modifier_key_pressed());

  key.Clear();
  EXPECT_FALSE(handler_->GetKeyEvent(IBUS_A, 30,
                                     (IBUS_SHIFT_MASK | IBUS_RELEASE_MASK),
                                     config::Config::ROMAN, true, &key));
  EXPECT_TRUE(IsPressed(IBUS_Shift_L));

  // Shift release: no Left-Shift-alone toggle fires (Shift was used to
  // capitalize) and tracking ends up clean.
  key.Clear();
  EXPECT_FALSE(handler_->GetKeyEvent(IBUS_Shift_L, kShiftLKeycode,
                                     (IBUS_SHIFT_MASK | IBUS_RELEASE_MASK),
                                     config::Config::ROMAN, true, &key));
  EXPECT_NO_MODIFIERS_PRESSED();
  EXPECT_MODIFIERS_TO_BE_SENT(kNoModifiers);
  EXPECT_FALSE(is_non_modifier_key_pressed());
}

TEST_F(KeyEventHandlerTest, StaleCtrlPrunedByHardwareMask) {
  // Ctrl down is tracked, then its release is eaten (focus change). The next
  // event's hardware mask shows Control up, so the stale entry is pruned and
  // the key goes out plain instead of as a phantom Ctrl chord.
  commands::KeyEvent key;

  EXPECT_FALSE(handler_->GetKeyEvent(IBUS_Control_L, 29, 0,
                                     config::Config::ROMAN, true, &key));
  EXPECT_TRUE(IsPressed(IBUS_Control_L));

  key.Clear();
  EXPECT_TRUE(handler_->GetKeyEvent(IBUS_a, 30, 0, config::Config::ROMAN,
                                    true, &key));
  EXPECT_FALSE(IsPressed(IBUS_Control_L));
  EXPECT_NO_MODIFIERS_PRESSED();
  EXPECT_MODIFIERS_TO_BE_SENT(kNoModifiers);
  EXPECT_EQ(key.modifier_keys_size(), 0);
}

TEST_F(KeyEventHandlerTest, StaleShiftPrunedOnRepress) {
  // Shift_L down tracked, release missed, Shift_L pressed again: the second
  // down event's mask still shows Shift up, so the stale entry is replaced
  // and the following release fires the toggle exactly once.
  commands::KeyEvent key;
  constexpr uint kShiftLKeycode = 42;

  EXPECT_FALSE(handler_->GetKeyEvent(IBUS_Shift_L, kShiftLKeycode, 0,
                                     config::Config::ROMAN, true, &key));
  key.Clear();
  EXPECT_FALSE(handler_->GetKeyEvent(IBUS_Shift_L, kShiftLKeycode, 0,
                                     config::Config::ROMAN, true, &key));
  EXPECT_TRUE(IsPressed(IBUS_Shift_L));
  EXPECT_EQ(currently_pressed_modifiers().size(), 1u);

  key.Clear();
  EXPECT_TRUE(handler_->GetKeyEvent(IBUS_Shift_L, kShiftLKeycode,
                                    IBUS_RELEASE_MASK, config::Config::ROMAN,
                                    true, &key));
  EXPECT_NO_MODIFIERS_PRESSED();
  EXPECT_MODIFIERS_TO_BE_SENT(kNoModifiers);
}

TEST_F(KeyEventHandlerTest, LeftShiftAloneSentOnKeyUp) {
  commands::KeyEvent key;
  EXPECT_FALSE(handler_->GetKeyEvent(IBUS_Shift_L, kDummyKeycode, 0,
                                     config::Config::ROMAN, true, &key));
  key.Clear();
  EXPECT_TRUE(handler_->GetKeyEvent(IBUS_Shift_L, kDummyKeycode,
                                    IBUS_RELEASE_MASK, config::Config::ROMAN,
                                    true, &key));
  EXPECT_NO_MODIFIERS_PRESSED();
  bool has_left_shift = false;
  for (int i = 0; i < key.modifier_keys_size(); ++i) {
    if (key.modifier_keys(i) == commands::KeyEvent::LEFT_SHIFT) {
      has_left_shift = true;
    }
  }
  EXPECT_TRUE(has_left_shift);
}

TEST_F(KeyEventHandlerTest, CtrlLeftShiftModeLockSentOnKeyUp) {
  commands::KeyEvent key;
  EXPECT_FALSE(handler_->GetKeyEvent(IBUS_Control_L, kDummyKeycode,
                                     IBUS_CONTROL_MASK, config::Config::ROMAN,
                                     true, &key));
  key.Clear();
  EXPECT_FALSE(handler_->GetKeyEvent(IBUS_Shift_L, kDummyKeycode,
                                     (IBUS_SHIFT_MASK | IBUS_CONTROL_MASK),
                                     config::Config::ROMAN, true, &key));
  key.Clear();
  EXPECT_TRUE(handler_->GetKeyEvent(IBUS_Shift_L, kDummyKeycode,
                                    (IBUS_SHIFT_MASK | IBUS_CONTROL_MASK |
                                     IBUS_RELEASE_MASK),
                                    config::Config::ROMAN, true, &key));
  EXPECT_EQ(key.modifier_keys_size(), 2);
  EXPECT_EQ(key.modifier_keys(0), commands::KeyEvent::CTRL);
  EXPECT_EQ(key.modifier_keys(1), commands::KeyEvent::LEFT_SHIFT);
}

TEST_F(KeyEventHandlerTest, ProcessModifiers) {
  commands::KeyEvent key;

  // Shift down => Shift up: Left Shift alone fires on key up, carrying the
  // marinaMoji LEFT_SHIFT distinction for the hiragana/katakana toggle.
  key.Clear();
  ProcessKey(false, IBUS_Shift_L, &key);

  key.Clear();
  EXPECT_TRUE(ProcessKey(true, IBUS_Shift_L, &key));
  EXPECT_NO_MODIFIERS_PRESSED();
  EXPECT_MODIFIERS_TO_BE_SENT(kNoModifiers);
  EXPECT_EQ(key.modifier_keys_size(), 2);
  EXPECT_EQ(key.modifier_keys(0) | key.modifier_keys(1),
            commands::KeyEvent::SHIFT | commands::KeyEvent::LEFT_SHIFT);

  // Shift down => Ctrl down => Shift up: the Ctrl+Left Shift mode-lock chord
  // fires on the Shift release; the still-held Ctrl stays tracked and its own
  // release is silent.
  key.Clear();
  ProcessKey(false, IBUS_Shift_L, &key);
  key.Clear();
  EXPECT_FALSE(ProcessKey(false, IBUS_Control_L, &key));
  key.Clear();
  EXPECT_TRUE(ProcessKey(true, IBUS_Shift_L, &key));
  EXPECT_EQ(key.modifier_keys_size(), 2);
  EXPECT_EQ(key.modifier_keys(0), commands::KeyEvent::CTRL);
  EXPECT_EQ(key.modifier_keys(1), commands::KeyEvent::LEFT_SHIFT);
  EXPECT_TRUE(IsPressed(IBUS_Control_L));
  key.Clear();
  EXPECT_FALSE(ProcessKey(true, IBUS_Control_L, &key));
  EXPECT_NO_MODIFIERS_PRESSED();
  EXPECT_MODIFIERS_TO_BE_SENT(kNoModifiers);

  // Alt down => Ctrl down => Alt up => Ctrl up: a chord without Shift still
  // sends the combined modifier event on the last release.
  key.Clear();
  ProcessKey(false, IBUS_Alt_L, &key);
  key.Clear();
  EXPECT_FALSE(ProcessKey(false, IBUS_Control_L, &key));
  key.Clear();
  EXPECT_FALSE(ProcessKey(true, IBUS_Alt_L, &key));
  key.Clear();
  EXPECT_TRUE(ProcessKey(true, IBUS_Control_L, &key));
  EXPECT_NO_MODIFIERS_PRESSED();
  EXPECT_MODIFIERS_TO_BE_SENT(kNoModifiers);
  EXPECT_EQ(key.modifier_keys_size(), 2);
  EXPECT_EQ(key.modifier_keys(0) | key.modifier_keys(1),
            commands::KeyEvent::CTRL | commands::KeyEvent::ALT);
}

TEST_F(KeyEventHandlerTest, ProcessModifiersRandomTest) {
  // This test generates random key sequences and checks that all states are
  // cleared once every key has been released.

  const uint kKeySet[] = {
      IBUS_Alt_L,   IBUS_Alt_R,   IBUS_Control_L, IBUS_Control_R,
      IBUS_Shift_L, IBUS_Shift_R, IBUS_Caps_Lock, IBUS_a,
  };
  constexpr size_t kKeySetSize = std::size(kKeySet);
  absl::BitGen gen;
  constexpr int kTrialNum = 1000;
  for (int trial = 0; trial < kTrialNum; ++trial) {
    handler_->Clear();

    std::set<uint> pressed_keys;
    std::string key_sequence;

    constexpr int kSequenceLength = 100;
    for (int i = 0; i < kSequenceLength; ++i) {
      const int key_index = absl::Uniform<int>(gen, 0, kKeySetSize);
      const uint key_value = kKeySet[key_index];

      bool is_key_up;
      if (pressed_keys.find(key_value) == pressed_keys.end()) {
        pressed_keys.insert(key_value);
        is_key_up = false;
      } else {
        pressed_keys.erase(key_value);
        is_key_up = true;
      }

      key_sequence += absl::StrFormat("is_key_up: %d, key_index = %d\n",
                                      is_key_up, key_index);

      commands::KeyEvent key;
      for (std::set<uint>::const_iterator it = pressed_keys.begin();
           it != pressed_keys.end(); ++it) {
        AppendToKeyEvent(*it, &key);
      }

      ProcessKey(is_key_up, key_value, &key);

      if (pressed_keys.empty()) {
        SCOPED_TRACE("key_sequence:\n" + key_sequence);
        EXPECT_FALSE(is_non_modifier_key_pressed());
        EXPECT_NO_MODIFIERS_PRESSED();
        EXPECT_MODIFIERS_TO_BE_SENT(kNoModifiers);
      }
    }

    // Under per-key tracking, a bare non-modifier key no longer wipes state
    // for keys that are still physically held (that blanket Clear() was the
    // stuck-Shift bug; stale entries are pruned via the hardware modifier
    // mask in GetKeyEvent instead). Releasing every held key must leave the
    // handler fully clean.
    while (!pressed_keys.empty()) {
      const uint key_value = *pressed_keys.begin();
      pressed_keys.erase(pressed_keys.begin());

      commands::KeyEvent key;
      for (std::set<uint>::const_iterator it = pressed_keys.begin();
           it != pressed_keys.end(); ++it) {
        AppendToKeyEvent(*it, &key);
      }
      ProcessKey(true, key_value, &key);
    }

    {
      SCOPED_TRACE("Should be clean after releasing all keys. key_sequence:\n" +
                   key_sequence);
      EXPECT_FALSE(is_non_modifier_key_pressed());
      EXPECT_NO_MODIFIERS_PRESSED();
      EXPECT_MODIFIERS_TO_BE_SENT(kNoModifiers);
    }
  }
}

#undef EXPECT_MODIFIERS_TO_BE_SENT
#undef EXPECT_MODIFIERS_PRESSED
#undef EXPECT_NO_MODIFIERS_PRESSED

}  // namespace ibus
}  // namespace mozc
