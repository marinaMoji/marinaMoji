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

#ifndef MOZC_UNIX_IBUS_MOZC_ENGINE_H_
#define MOZC_UNIX_IBUS_MOZC_ENGINE_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <glib.h>

#include "absl/container/flat_hash_map.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "unix/ibus/candidate_window_handler.h"
#include "unix/ibus/engine_interface.h"
#include "unix/ibus/ibus_candidate_window_handler.h"
#include "unix/ibus/ibus_config.h"
#include "unix/ibus/ibus_wrapper.h"
#include "unix/ibus/preedit_handler.h"
#include "unix/ibus/property_handler.h"

namespace mozc {

namespace client {
class ClientInterface;
}

namespace ibus {

class CandidateWindowHandlerInterface;
class KeyEventHandler;
class LaunchToolTest;

// Implements EngineInterface and handles signals from IBus daemon.
// This class mainly does the two things:
// - Converting IBus key events to Mozc's key events and passes them to the
//   Mozc's session module and
// - Reflecting the output from the Mozc's session module in IBus UIs such as
//   the preedit, the candidate window and the result text.
class MozcEngine : public EngineInterface {
 public:
  MozcEngine();
  MozcEngine(const MozcEngine&) = delete;
  MozcEngine& operator=(const MozcEngine&) = delete;
  virtual ~MozcEngine();

  // EngineInterface functions
  void CandidateClicked(IbusEngineWrapper* engine, uint index, uint button,
                        uint state) override;
  void CursorDown(IbusEngineWrapper* engine) override;
  void CursorUp(IbusEngineWrapper* engine) override;
  void Disable(IbusEngineWrapper* engine) override;
  void Enable(IbusEngineWrapper* engine) override;
  void FocusIn(IbusEngineWrapper* engine) override;
  void FocusOut(IbusEngineWrapper* engine) override;
  void PageDown(IbusEngineWrapper* engine) override;
  void PageUp(IbusEngineWrapper* engine) override;
  bool ProcessKeyEvent(IbusEngineWrapper* engine, uint keyval, uint keycode,
                       uint state) override;
  void PropertyActivate(IbusEngineWrapper* engine, const char* property_name,
                        uint property_state) override;
  void PropertyHide(IbusEngineWrapper* engine,
                    const char* property_name) override;
  void PropertyShow(IbusEngineWrapper* engine,
                    const char* property_name) override;
  void Reset(IbusEngineWrapper* engine) override;
  void SetCapabilities(IbusEngineWrapper* engine, uint capabilities) override;
  void SetCursorLocation(IbusEngineWrapper* engine, int x, int y, int w,
                         int h) override;
  void SetContentType(IbusEngineWrapper* engine, uint purpose,
                      uint hints) override;

  // Called by the GTK toolbar when a button is clicked. Sends the session
  // command and updates UI using the last focused engine.
  void SendToolbarSessionCommand(commands::SessionCommand::CommandType type,
                                int32_t candidate_id = -1);

  // Inserts |text| into the focused application via the last focused IBus
  // engine. Returns false when there is no engine to commit into (e.g. right
  // after the character composer stole focus); callers can retry.
  bool CommitToolbarText(absl::string_view text);

  // Called by the toolbar when the user picks a composition mode from the
  // mode-indicator menu. Switches mode and updates panel/toolbar.
  void SetCompositionModeFromToolbar(commands::CompositionMode mode);

  // Called by the toolbar dict button. Launches Mozc tool by mode (e.g.
  // "word_register_dialog", "dictionary_tool"). Returns true if launched.
  bool LaunchToolFromToolbar(const char* mode);

  // Called by the toolbar shortcuts popup. Fills |config| with current config
  // from the server (including session_keymap and custom_keymap_table).
  // Returns false if the client is unavailable or GetConfig fails.
  bool GetConfig(config::Config* config);

 private:
  // Body of ProcessKeyEvent. The public wrapper records per-key bookkeeping
  // (Backspace press/release tracking) around whatever return path this
  // takes.
  bool ProcessKeyEventInternal(IbusEngineWrapper* engine, uint keyval,
                               uint keycode, uint state);

  // Updates the preedit text and the candidate window and inserts result
  // based on the content of |output|.
  bool UpdateAll(IbusEngineWrapper* engine, const commands::Output& output);
  // Inserts a result text based on the content of |output|.
  bool UpdateResult(IbusEngineWrapper* engine,
                    const commands::Output& output) const;
  // Updates |unique_candidate_ids_|.
  bool UpdateCandidateIDMapping(const commands::Output& output);
  // Updates the deletion range message based on the content of |output|.
  bool UpdateDeletionRange(IbusEngineWrapper* engine,
                           const commands::Output& output);

  // marinaMoji: re-issues SHOW_ODORIJI_PALETTE (or flushes a deferred
  // candidate Output) if the palette was opened from the panel menu and focus
  // has just come back to a text surface. No-op (and clears the pending flag)
  // outside the retry window; stays pending while no usable caret rect is
  // available. Returns true if the palette was shown.
  bool MaybeReshowOdorijiPalette(IbusEngineWrapper* engine);

  // marinaMoji: if a candidate window was deferred for lack of a caret rect
  // and one is usable now, paint it once and clear the deferral. Used for the
  // typed / Ctrl+Shift+2 path (issue #25 part 3). Returns true if it showed.
  bool MaybeFlushDeferredCandidateWindow(IbusEngineWrapper* engine);

  // marinaMoji: g_timeout for IME-menu odoriji when focus never bounces.
  void ScheduleOdorijiDeferredShowTimeout();
  void CancelOdorijiDeferredShowTimeout();
  static gboolean OdorijiDeferredShowTimeoutCb(gpointer user_data);

  // marinaMoji: false for the empty or zero-sized caret rects the engine holds
  // before an app has reported one, or while a menu owns the focus. The
  // renderer anchors the candidate window to the rect, so drawing at one of
  // those puts the window in the top-left corner of the screen.
  static bool IsUsableCursorArea(const IbusEngineWrapper::Rectangle& area);

  // marinaMoji: substitutes the last caret rect the app reported when the
  // engine's current one is unusable, so a palette opened from the panel menu
  // is drawn where the caret is rather than in the corner. Returns whether the
  // engine's cursor area is usable afterwards.
  bool EnsureUsableCursorArea(IbusEngineWrapper* engine);

  // Updates the callback message based on the content of |output|.
  bool ExecuteCallback(IbusEngineWrapper* engine,
                       const commands::Output& output);

  // Launches Mozc tool with appropriate arguments.
  bool LaunchTool(const commands::Output& output) const;

  // Updates internal preedit_method (Roman/Kana) state
  void UpdatePreeditMethod();

  // Calls SyncData command. if |force| is false, SyncData is called
  // at an appropriate timing to reduce IPC calls. if |force| is true,
  // always calls SyncData.
  void SyncData(bool force);

  // Reverts internal state of mozc_server by sending SessionCommand::REVERT IPC
  // message, then hides a preedit string and the candidate window.
  void RevertSession(IbusEngineWrapper* engine);

  // Forwards IBUS_RELEASE for tracked Shift keys, then clears modifier state.
  // Records a lifecycle callback (focus, enable/disable, reset) in the ibus
  // debug log. Focus changes are when tracked key state is flushed and
  // cleared, so a log without them cannot explain a stuck-key report.
  void LogLifecycle(const char* event, IbusEngineWrapper* engine);

  // Records the focused client's IBus capabilities. Surrounding-text support
  // decides whether an echo-back Backspace can avoid forwarding synthetic key
  // events, so it has to be visible per application in the log.
  void LogClientCapabilities(IbusEngineWrapper* engine);

  void ReleaseTrackedModifiers(IbusEngineWrapper* engine);

  CandidateWindowHandlerInterface* GetCandidateWindowHandler(
      IbusEngineWrapper* engine);

  absl::Time last_sync_time_;
  std::unique_ptr<KeyEventHandler> key_event_handler_;
  std::unique_ptr<client::ClientInterface> client_;

  std::unique_ptr<PropertyHandler> property_handler_;
  std::unique_ptr<PreeditHandler> preedit_handler_;

  // If true, uses Mozc candidate window instead of IBus default one.
  bool use_mozc_candidate_window_;
  // TODO(nona): Introduce CandidateWindowHandlerManager to avoid direct access.
  CandidateWindowHandler mozc_candidate_window_handler_;
  IBusCandidateWindowHandler ibus_candidate_window_handler_;
  config::Config::PreeditMethod preedit_method_;

  // Unique IDs of candidates that are currently shown.
  std::vector<int32_t> unique_candidate_ids_;
  IbusConfig ibus_config_;

  // Last focused IBus engine; used by toolbar to send commands and update UI.
  IBusEngine *current_engine_ = nullptr;
  // True between FocusIn and FocusOut. The character composer steals focus, so
  // CommitToolbarText must wait until this is true again before inserting.
  bool engine_focused_ = false;

  // When true, show toolbar on focus; toggled by IME menu "Hide/Show toolbar".
  // Default true (on) for first install; thereafter loaded/saved via toolbar.conf.
  bool toolbar_visible_ = true;

  // Tracks preedit for sync activity timestamps (composition end).
  bool had_preedit_ = false;

  // After Enter echo-back, surrounding text lags until set_cursor_location.
  // Backspace uses left-then-return-false until a consumed key clears this.
  bool surround_stale_after_echo_back_ = false;
  // Mirrors Session::macron_dead_key_pending_. In Direct input the IME is
  // inactive, so ProcessKey normally forwards keys to the application without
  // sending them to the server; while a macron is pending the following vowel
  // has to get through. Set when a Right Shift tap is sent in Direct, cleared
  // by the next key sent (which the server either consumes as the macron or
  // treats as a cancel).
  bool macron_dead_key_pending_ = false;

  // Whether the last Backspace key-down was consumed. Its key-up must take
  // the same path: consuming a release whose press was forwarded leaves the
  // application waiting forever for the key-up (stuck auto-repeat on
  // Wayland/GTK, which drive repeat client-side from press/release pairs).
  bool backspace_down_consumed_ = false;

  // Set right after the IME-menu "Odoriji" property is activated. Clicking
  // the ibus panel menu (a separate surface from the focused text field) can
  // bounce keyboard focus off and back, delivering a FocusOut immediately
  // after the palette is shown. FocusOut normally hides the candidate window
  // and reverts the session; the odoriji palette survives that revert
  // server-side (it isn't a preedit), so hiding it client-side just orphans
  // it until the next real keystroke resyncs -- the "flashes then
  // disappears until you press Space" bug. Suppresses that one Hide/Revert
  // if it lands within the debounce window below.
  absl::Time odoriji_property_show_time_;

  // marinaMoji: set when the palette was opened from the ibus panel menu, and
  // cleared by the first FocusIn / set_cursor_location that follows. While the
  // menu is up the keyboard focus (and, on some compositors, the reported
  // cursor rect) belongs to the menu, not to the text field, so drawing now
  // would flash in the corner (or at a stale rect) before the reshow. The
  // Output is kept in deferred_candidate_output_ and painted once focus is
  // back. See MaybeReshowOdorijiPalette().
  bool odoriji_show_pending_ = false;

  // marinaMoji: candidate Output waiting for a usable caret rect before the
  // window is shown (issue #25). Set when UpdateAll would otherwise draw at
  // the top-left corner, or when the IME-menu path defers the first paint.
  // Cleared when the window is finally shown, dismissed, or the engine is
  // disabled.
  std::optional<commands::Output> deferred_candidate_output_;

  // marinaMoji: g_timeout_add id for the no-bounce menu fallback; 0 if none.
  unsigned int odoriji_deferred_show_timeout_id_ = 0;

  // marinaMoji: set if FocusOut arrived while a menu odoriji show is pending.
  // The 80ms timeout must not paint in that case — the menu may still own a
  // caret rect, and painting then moving is the flash. FocusIn shows instead.
  bool odoriji_menu_saw_focus_out_ = false;

  // marinaMoji: the last caret rect reported through set_cursor_location that
  // was worth drawing at. Used to place a palette opened from the panel menu
  // while the app-reported rect belongs to the menu (or is still empty).
  IbusEngineWrapper::Rectangle last_usable_cursor_area_ = {0, 0, 0, 0};
  bool has_last_usable_cursor_area_ = false;

  friend class MozcEngineTestPeer;
};

bool CanUseMozcCandidateWindow(
    const IbusConfig& ibus_config,
    const absl::flat_hash_map<std::string, std::string>& env);

}  // namespace ibus
}  // namespace mozc

#endif  // MOZC_UNIX_IBUS_MOZC_ENGINE_H_
