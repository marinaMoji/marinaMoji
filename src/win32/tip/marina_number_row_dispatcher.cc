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

#include "win32/tip/marina_number_row_dispatcher.h"

#include <optional>

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "session/marina_number_row_bindings_util.h"
#include "absl/strings/str_cat.h"
#include "base/marina_debug_log.h"
#include "win32/tip/win32_physical_slot.h"

namespace mozc {
namespace win32 {
namespace tsf {
namespace {

using ::mozc::commands::CompositionMode;
using ::mozc::commands::Output;
using ::mozc::commands::SessionCommand;
using ::mozc::config::MarinaNumberRowAction;
using ::mozc::config::MarinaPhysicalSlot;
using ::mozc::config::MarinaShortcutModifier;

// Same 300ms autorepeat-suppression window as
// unix/ibus/marina_number_row_dispatcher.cc, to avoid re-firing on OS
// key-repeat while a Ctrl+Shift+digit chord is held down.
bool ShouldSuppressAutorepeat(MarinaShortcutModifier modifier,
                              MarinaPhysicalSlot slot,
                              MarinaNumberRowAction action) {
  if (modifier != MarinaShortcutModifier::MARINA_MOD_CTRL_SHIFT) {
    return false;
  }
  static MarinaPhysicalSlot last_slot = MarinaPhysicalSlot::MARINA_SLOT_1;
  static MarinaNumberRowAction last_action =
      MarinaNumberRowAction::MARINA_NR_ODORIJI_DEFAULT;
  static absl::Time last_time;
  const absl::Time now = absl::Now();
  if (slot == last_slot && action == last_action &&
      now - last_time < absl::Milliseconds(300)) {
    return true;
  }
  last_slot = slot;
  last_action = action;
  last_time = now;
  return false;
}

bool SendSessionCommand(client::ClientInterface* client, SessionCommand command,
                        Output* output) {
  return client != nullptr && client->SendCommand(command, output);
}

// Unlike unix/ibus (which mirrors the on/off state into a PropertyHandler for
// the lang-bar icon), Windows applies the final SessionCommand's Output via
// TipEditSession::OnOutputReceivedSync in the caller, so no separate UI-sync
// step is needed here; the intermediate TURN_ON_IME Output can be discarded.
bool EnsureImeOn(bool is_open, CompositionMode original_composition_mode,
                 client::ClientInterface* client, Output* output) {
  if (is_open) {
    return true;
  }
  SessionCommand on_command;
  on_command.set_type(SessionCommand::TURN_ON_IME);
  const CompositionMode mode = original_composition_mode == CompositionMode::DIRECT
                                    ? CompositionMode::HIRAGANA
                                    : original_composition_mode;
  on_command.set_composition_mode(mode);
  return SendSessionCommand(client, on_command, output);
}

}  // namespace

bool WouldConsumeMarinaNumberRowShortcut(BYTE scan_code, bool ctrl, bool shift,
                                         const config::Config& config) {
  if (!ctrl) {
    return false;
  }
  const std::optional<MarinaPhysicalSlot> slot =
      ScanCodeToPhysicalSlot(scan_code);
  if (!slot.has_value()) {
    return false;
  }
  const MarinaShortcutModifier modifier =
      shift ? MarinaShortcutModifier::MARINA_MOD_CTRL_SHIFT
            : MarinaShortcutModifier::MARINA_MOD_CTRL;
  return session::FindMarinaActionForPhysicalSlot(config, modifier, *slot)
      .has_value();
}

bool DispatchMarinaNumberRowShortcut(
    BYTE scan_code, bool ctrl, bool shift, bool is_open,
    CompositionMode original_composition_mode, const config::Config& config,
    client::ClientInterface* client, Output* output) {
  if (!ctrl) {
    return false;
  }

  const std::optional<MarinaPhysicalSlot> slot =
      ScanCodeToPhysicalSlot(scan_code);
  if (!slot.has_value()) {
    return false;
  }

  const MarinaShortcutModifier modifier =
      shift ? MarinaShortcutModifier::MARINA_MOD_CTRL_SHIFT
            : MarinaShortcutModifier::MARINA_MOD_CTRL;

  const std::optional<MarinaNumberRowAction> action =
      session::FindMarinaActionForPhysicalSlot(config, modifier, *slot);
  // TEMPORARY: see base/marina_debug_log.h.
  MarinaDebugLog(absl::StrCat(
      "dispatch: scan=0x", absl::Hex(scan_code), " ctrl=", ctrl,
      " shift=", shift, " open=", is_open, " slot=", static_cast<int>(*slot),
      " action=", action.has_value() ? static_cast<int>(*action) : -1,
      " bindings_in_config=", config.marina_number_row_bindings_size()));
  if (!action.has_value()) {
    return false;
  }

  if (ShouldSuppressAutorepeat(modifier, *slot, *action)) {
    MarinaDebugLog("dispatch: suppressed as autorepeat, sending nothing");
    return true;
  }

  SessionCommand command;
  switch (*action) {
    case MarinaNumberRowAction::MARINA_NR_HIRAGANA_DIRECT:
      // marinaMoji: direction must be decided from |is_open|, not
      // |original_composition_mode| -- ConversionModeUtil::ToMozcMode can
      // never return DIRECT (it collides with HALF_ASCII's native bit
      // pattern), so comparing against CompositionMode::DIRECT was always
      // false and this shortcut only ever turned the IME off, never back on.
      if (!is_open) {
        command.set_type(SessionCommand::TURN_ON_IME);
        command.set_composition_mode(CompositionMode::HIRAGANA);
        return SendSessionCommand(client, command, output);
      }
      command.set_type(SessionCommand::TURN_OFF_IME);
      return SendSessionCommand(client, command, output);

    case MarinaNumberRowAction::MARINA_NR_MANYOSHU_HIRAGANA:
      if (!EnsureImeOn(is_open, original_composition_mode, client, output)) {
        return false;
      }
      command.set_type(SessionCommand::SWITCH_COMPOSITION_MODE);
      if (original_composition_mode == CompositionMode::MANYOSHU) {
        command.set_composition_mode(CompositionMode::HIRAGANA);
      } else {
        command.set_composition_mode(CompositionMode::MANYOSHU);
      }
      return SendSessionCommand(client, command, output);

    case MarinaNumberRowAction::MARINA_NR_ODORIJI_DEFAULT:
      if (!EnsureImeOn(is_open, original_composition_mode, client, output)) {
        return false;
      }
      command.set_type(SessionCommand::INSERT_ODORIJI_DEFAULT);
      return SendSessionCommand(client, command, output);

    case MarinaNumberRowAction::MARINA_NR_ODORIJI_PALETTE:
      if (!EnsureImeOn(is_open, original_composition_mode, client, output)) {
        return false;
      }
      command.set_type(SessionCommand::SHOW_ODORIJI_PALETTE);
      return SendSessionCommand(client, command, output);

    case MarinaNumberRowAction::MARINA_NR_TRADITIONAL_KANJI: {
      command.set_type(SessionCommand::TOGGLE_TRADITIONAL_KANJI);
      const bool sent = SendSessionCommand(client, command, output);
      // TEMPORARY: see base/marina_debug_log.h.
      MarinaDebugLog(absl::StrCat("dispatch: TOGGLE_TRADITIONAL_KANJI sent=", sent,
                                " client=", client != nullptr,
                                " output_has_config=", output->has_config(),
                                " use_traditional_kanji=",
                                output->has_config() &&
                                    output->config().use_traditional_kanji()));
      return sent;
    }

    case MarinaNumberRowAction::MARINA_NR_WORD_REGISTER:
      command.set_type(SessionCommand::LAUNCH_WORD_REGISTER_DIALOG);
      return SendSessionCommand(client, command, output);

    default:
      return false;
  }
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
