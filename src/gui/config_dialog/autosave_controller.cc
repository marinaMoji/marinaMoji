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

#include "gui/config_dialog/autosave_controller.h"

#include <memory>
#include <utility>

namespace mozc {
namespace gui {

AutosaveController::AutosaveController(SaveFn save, StatusFn on_status,
                                       std::unique_ptr<Timer> timer)
    : save_(std::move(save)),
      on_status_(std::move(on_status)),
      timer_(std::move(timer)) {}

void AutosaveController::NotifyChanged() {
  if (suppressed_) {
    return;
  }
  dirty_ = true;
  if (running_) {
    // Do not start a nested save or arm the timer: the running Run() repeats
    // with the latest UI state before it returns.
    rerun_ = true;
    return;
  }
  timer_->Start();
}

void AutosaveController::OnTimeout() {
  if (suppressed_ || !dirty_) {
    return;
  }
  Run(/*interactive=*/false);
}

bool AutosaveController::Flush() {
  timer_->Stop();
  if (running_) {
    // A save is on the stack; it has not finished, so nothing is saved yet. A
    // change made before this call already queued a repeat.
    return false;
  }
  if (suppressed_ || !dirty_) {
    return !dirty_;
  }
  return Run(/*interactive=*/true);
}

void AutosaveController::SetSuppressed(bool suppressed) {
  suppressed_ = suppressed;
}

bool AutosaveController::Run(bool interactive) {
  if (running_) {
    // A save is already on the stack (a modal or IPC call re-entered the event
    // loop). Let it repeat rather than nesting.
    dirty_ = true;
    rerun_ = true;
    return false;
  }
  running_ = true;
  bool saved = false;
  int passes = 0;
  do {
    rerun_ = false;
    dirty_ = false;
    const Result result = save_(interactive);
    saved = result.outcome == Outcome::kSaved;
    if (!saved) {
      // Keep the changes pending so the next change or Flush() retries.
      dirty_ = true;
    }
    Report(result);
    ++passes;
  } while (rerun_ && passes < kMaxPassesPerRun);
  running_ = false;
  if (rerun_) {
    // Hit the pass limit with changes still arriving; try again later rather
    // than looping here.
    dirty_ = true;
    timer_->Start();
  }
  return saved && !dirty_;
}

void AutosaveController::Report(const Result& result) {
  Status next;
  switch (result.outcome) {
    case Outcome::kSaved:
      break;
    case Outcome::kIncomplete:
      next.kind = Status::Kind::kIncomplete;
      next.detail = result.detail;
      break;
    case Outcome::kFailed:
      next.kind = Status::Kind::kFailed;
      next.detail = result.detail;
      break;
  }
  if (next == status_) {
    return;
  }
  status_ = std::move(next);
  if (on_status_) {
    on_status_(status_);
  }
}

}  // namespace gui
}  // namespace mozc
