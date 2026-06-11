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

#include "unix/ibus/marina_number_row_dispatcher.h"

#include <ibus.h>

#include <optional>

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "session/marina_number_row_bindings_util.h"
#include "unix/ibus/ibus_physical_slot.h"

namespace mozc {
namespace ibus {
namespace {

using ::mozc::commands::CompositionMode;
using ::mozc::commands::Output;
using ::mozc::commands::SessionCommand;
using ::mozc::config::MarinaNumberRowAction;
using ::mozc::config::MarinaPhysicalSlot;
using ::mozc::config::MarinaShortcutModifier;

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

bool EnsureImeOn(IbusEngineWrapper* engine, PropertyHandler* property_handler,
                 client::ClientInterface* client, Output* output) {
  if (property_handler->IsActivated()) {
    return true;
  }
  SessionCommand on_command;
  on_command.set_type(SessionCommand::TURN_ON_IME);
  const CompositionMode mode =
      property_handler->GetOriginalCompositionMode() == CompositionMode::DIRECT
          ? CompositionMode::HIRAGANA
          : property_handler->GetOriginalCompositionMode();
  on_command.set_composition_mode(mode);
  if (!SendSessionCommand(client, on_command, output)) {
    return false;
  }
  if (engine != nullptr) {
    property_handler->Update(engine, *output);
  }
  return true;
}

}  // namespace

bool DispatchMarinaNumberRowShortcut(
    IbusEngineWrapper* engine, uint keycode, uint ibus_modifiers,
    const config::Config& config, PropertyHandler* property_handler,
    client::ClientInterface* client, Output* output) {
  if ((ibus_modifiers & IBUS_RELEASE_MASK) != 0) {
    return false;
  }

  const std::optional<MarinaPhysicalSlot> slot =
      IbusKeycodeToPhysicalSlot(keycode);
  if (!slot.has_value()) {
    return false;
  }

  const MarinaShortcutModifier modifier =
      IbusModifiersToMarinaModifier(ibus_modifiers);
  if (modifier != MarinaShortcutModifier::MARINA_MOD_CTRL &&
      modifier != MarinaShortcutModifier::MARINA_MOD_CTRL_SHIFT) {
    return false;
  }

  const std::optional<MarinaNumberRowAction> action =
      session::FindMarinaActionForPhysicalSlot(config, modifier, *slot);
  if (!action.has_value()) {
    return false;
  }

  if (ShouldSuppressAutorepeat(modifier, *slot, *action)) {
    return true;
  }

  SessionCommand command;
  switch (*action) {
    case MarinaNumberRowAction::MARINA_NR_HIRAGANA_DIRECT:
      if (property_handler->GetOriginalCompositionMode() ==
          CompositionMode::DIRECT) {
        command.set_type(SessionCommand::TURN_OFF_IME);
        return SendSessionCommand(client, command, output);
      }
      command.set_type(SessionCommand::TURN_ON_IME);
      command.set_composition_mode(CompositionMode::HIRAGANA);
      return SendSessionCommand(client, command, output);

    case MarinaNumberRowAction::MARINA_NR_MANYOSHU_HIRAGANA:
      if (!EnsureImeOn(engine, property_handler, client, output)) {
        return false;
      }
      command.set_type(SessionCommand::SWITCH_COMPOSITION_MODE);
      if (property_handler->GetOriginalCompositionMode() ==
          CompositionMode::MANYOSHU) {
        command.set_composition_mode(CompositionMode::HIRAGANA);
      } else {
        command.set_composition_mode(CompositionMode::MANYOSHU);
      }
      return SendSessionCommand(client, command, output);

    case MarinaNumberRowAction::MARINA_NR_ODORIJI_DEFAULT:
      if (!EnsureImeOn(engine, property_handler, client, output)) {
        return false;
      }
      command.set_type(SessionCommand::INSERT_ODORIJI_DEFAULT);
      return SendSessionCommand(client, command, output);

    case MarinaNumberRowAction::MARINA_NR_ODORIJI_PALETTE:
      if (!EnsureImeOn(engine, property_handler, client, output)) {
        return false;
      }
      command.set_type(SessionCommand::SHOW_ODORIJI_PALETTE);
      return SendSessionCommand(client, command, output);

    case MarinaNumberRowAction::MARINA_NR_TRADITIONAL_KANJI:
      command.set_type(SessionCommand::TOGGLE_TRADITIONAL_KANJI);
      return SendSessionCommand(client, command, output);

    case MarinaNumberRowAction::MARINA_NR_WORD_REGISTER:
      command.set_type(SessionCommand::LAUNCH_WORD_REGISTER_DIALOG);
      return SendSessionCommand(client, command, output);

    default:
      return false;
  }
}

}  // namespace ibus
}  // namespace mozc
