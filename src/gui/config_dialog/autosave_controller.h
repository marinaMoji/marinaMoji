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

#ifndef MOZC_GUI_CONFIG_DIALOG_AUTOSAVE_CONTROLLER_H_
#define MOZC_GUI_CONFIG_DIALOG_AUTOSAVE_CONTROLLER_H_

#include <functional>
#include <memory>
#include <string>

namespace mozc {
namespace gui {

// The Preferences window has no Apply/OK button: every control change is
// persisted by an autosave. This class is the (Qt-free, so unit-testable)
// state machine behind it:
//
//   * changes are debounced, so a burst of edits is one save;
//   * a change that arrives while a save is running is not lost and does not
//     start a nested save: the save repeats once with the latest UI state;
//   * a save that does not succeed leaves the changes pending, so the next
//     change, or closing the window, retries it;
//   * the outcome is reported through a status callback only when it changes.
//     A successful save while nothing was wrong is completely silent.
class AutosaveController {
 public:
  enum class Outcome {
    kSaved,
    // The settings are in a state that cannot be saved yet (an unfinished
    // custom keymap, an invalid shortcut chord). The user can fix this.
    kIncomplete,
    // The settings are valid but could not be persisted (the converter is not
    // running, the config could not be written).
    kFailed,
  };

  struct Result {
    Outcome outcome = Outcome::kSaved;
    // Human readable reason for kIncomplete / kFailed; empty for kSaved.
    std::string detail;
  };

  // What the UI should currently show about autosave. kNone means nothing.
  struct Status {
    enum class Kind { kNone, kIncomplete, kFailed };
    Kind kind = Kind::kNone;
    std::string detail;

    bool operator==(const Status& other) const {
      return kind == other.kind && detail == other.detail;
    }
  };

  // Restartable single-shot timer. Start() while running restarts it.
  class Timer {
   public:
    virtual ~Timer() = default;
    virtual void Start() = 0;
    virtual void Stop() = 0;
  };

  // |interactive| is true when the user is waiting on the result (closing the
  // window, committing an editor), in which case the callback may show a modal
  // message for a problem. It is false for timer-driven saves, which must stay
  // quiet.
  using SaveFn = std::function<Result(bool interactive)>;
  using StatusFn = std::function<void(const Status&)>;

  // Upper bound on back-to-back passes caused by changes arriving mid-save, so
  // a callback that keeps reporting changes cannot spin forever.
  static constexpr int kMaxPassesPerRun = 8;

  AutosaveController(SaveFn save, StatusFn on_status,
                     std::unique_ptr<Timer> timer);

  // A control changed. Ignored while suppressed (the UI is being filled from
  // the config, which is not a user change).
  void NotifyChanged();

  // The debounce timer fired.
  void OnTimeout();

  // Save now if anything is pending, stopping the debounce timer. Used when
  // the window closes and when an editor commits. Returns true when nothing is
  // left unsaved.
  bool Flush();

  // Suppress change notifications, for programmatic updates of the controls.
  void SetSuppressed(bool suppressed);

  // True while changes are waiting to be saved (or the last save failed).
  bool pending() const { return dirty_; }
  const Status& status() const { return status_; }

 private:
  bool Run(bool interactive);
  void Report(const Result& result);

  SaveFn save_;
  StatusFn on_status_;
  std::unique_ptr<Timer> timer_;

  Status status_;
  bool dirty_ = false;
  bool running_ = false;
  bool rerun_ = false;
  bool suppressed_ = false;
};

}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_CONFIG_DIALOG_AUTOSAVE_CONTROLLER_H_
