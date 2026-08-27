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

#ifndef MOZC_WIN32_TIP_MARINA_NUMBER_ROW_DISPATCHER_H_
#define MOZC_WIN32_TIP_MARINA_NUMBER_ROW_DISPATCHER_H_

#include <windows.h>

#include "client/client_interface.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"

namespace mozc {
namespace win32 {
namespace tsf {

// Returns true when the physical key event (scan_code, with Ctrl/Shift state)
// was consumed by a marina number-row binding (Ctrl(+Shift)+1-5/0/`). Mirrors
// unix/ibus/marina_number_row_dispatcher.cc; call only for key-down events,
// before the normal KeyEventHandler::ImeToAsciiEx path, so these physical
// shortcuts bypass the per-character key pipeline the same way IBus/macOS do.
// |is_autorepeat| must come from the WM_KEYDOWN lparam's previous-key-state
// bit (LParamKeyInfo::IsPreviousStateDwon). A held-down chord is claimed but
// fires only once; a genuine second press always fires, however fast it
// follows the first.
bool DispatchMarinaNumberRowShortcut(
    BYTE scan_code, bool ctrl, bool shift, bool is_autorepeat, bool is_open,
    commands::CompositionMode original_composition_mode,
    const config::Config& config, client::ClientInterface* client,
    commands::Output* output);

// Cheap pre-check with no config lookup: true when this key could possibly be
// a number-row chord at all. Fetching the config costs an IPC round trip to
// the server, so callers use this first rather than paying that on every
// keystroke the user types.
bool CouldBeMarinaNumberRowShortcut(BYTE scan_code, bool ctrl);

// Non-mutating check for OnTestKeyDown: returns true iff
// DispatchMarinaNumberRowShortcut would consume this scan_code/Ctrl/Shift
// combination (i.e. some MarinaNumberRowAction is bound to it), without
// sending any command. Needed because TSF only delivers a key event to
// OnKeyDown if OnTestKeyDown reported it as consumed, and this must be
// checked independent of whether the IME is currently open, since these
// shortcuts (e.g. via EnsureImeOn) are meant to work from a closed IME too.
bool WouldConsumeMarinaNumberRowShortcut(BYTE scan_code, bool ctrl, bool shift,
                                         const config::Config& config);

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_TIP_MARINA_NUMBER_ROW_DISPATCHER_H_
