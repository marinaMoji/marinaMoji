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

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "testing/gunit.h"

namespace mozc {
namespace gui {
namespace {

using Outcome = AutosaveController::Outcome;
using Result = AutosaveController::Result;
using Status = AutosaveController::Status;

// Records timer use; the test fires it by hand so debounce is deterministic.
class FakeTimer : public AutosaveController::Timer {
 public:
  void Start() override {
    ++starts;
    armed = true;
  }
  void Stop() override {
    ++stops;
    armed = false;
  }
  int starts = 0;
  int stops = 0;
  bool armed = false;
};

class AutosaveControllerTest : public testing::Test {
 protected:
  void SetUp() override {
    auto timer = std::make_unique<FakeTimer>();
    timer_ = timer.get();
    controller_ = std::make_unique<AutosaveController>(
        [this](bool interactive) { return OnSave(interactive); },
        [this](const Status& status) { statuses_.push_back(status); },
        std::move(timer));
  }

  Result OnSave(bool interactive) {
    ++depth_;
    max_depth_ = std::max(max_depth_, depth_);
    interactive_calls_.push_back(interactive);
    ++saves_;
    if (during_save_) {
      during_save_();
    }
    --depth_;
    return next_result_;
  }

  // Fires the debounce timer the way QTimer would, if it is armed.
  void FireTimer() {
    if (timer_->armed) {
      timer_->armed = false;
      controller_->OnTimeout();
    }
  }

  static Result Ok() { return Result{Outcome::kSaved, ""}; }
  static Result Incomplete(const std::string& why) {
    return Result{Outcome::kIncomplete, why};
  }
  static Result Failed(const std::string& why) {
    return Result{Outcome::kFailed, why};
  }

  FakeTimer* timer_ = nullptr;
  std::unique_ptr<AutosaveController> controller_;
  std::vector<Status> statuses_;
  std::vector<bool> interactive_calls_;
  std::function<void()> during_save_;
  Result next_result_ = Ok();
  int saves_ = 0;
  int depth_ = 0;
  int max_depth_ = 0;
};

TEST_F(AutosaveControllerTest, NothingHappensWithoutAChange) {
  FireTimer();
  EXPECT_TRUE(controller_->Flush());
  EXPECT_EQ(saves_, 0);
  EXPECT_FALSE(controller_->pending());
}

TEST_F(AutosaveControllerTest, DebounceCoalescesRapidChangesIntoOneSave) {
  for (int i = 0; i < 5; ++i) {
    controller_->NotifyChanged();
  }
  EXPECT_EQ(saves_, 0) << "nothing is saved before the debounce fires";
  EXPECT_EQ(timer_->starts, 5) << "each change restarts the timer";
  EXPECT_TRUE(controller_->pending());

  FireTimer();
  EXPECT_EQ(saves_, 1);
  EXPECT_FALSE(controller_->pending());
  ASSERT_EQ(interactive_calls_.size(), 1);
  EXPECT_FALSE(interactive_calls_[0]) << "timer saves must stay quiet";
}

TEST_F(AutosaveControllerTest, SuccessfulSaveIsSilent) {
  controller_->NotifyChanged();
  FireTimer();
  controller_->NotifyChanged();
  FireTimer();
  EXPECT_EQ(saves_, 2);
  EXPECT_TRUE(statuses_.empty()) << "no status callback on plain success";
}

TEST_F(AutosaveControllerTest, SuppressedChangesAreIgnored) {
  controller_->SetSuppressed(true);
  controller_->NotifyChanged();
  EXPECT_EQ(timer_->starts, 0);
  EXPECT_FALSE(controller_->pending());
  controller_->SetSuppressed(false);
  FireTimer();
  EXPECT_EQ(saves_, 0);
}

TEST_F(AutosaveControllerTest, FlushSavesPendingChangesInteractively) {
  controller_->NotifyChanged();
  EXPECT_TRUE(controller_->Flush());
  EXPECT_EQ(saves_, 1);
  ASSERT_EQ(interactive_calls_.size(), 1);
  EXPECT_TRUE(interactive_calls_[0]) << "closing may show a modal";
  EXPECT_FALSE(timer_->armed) << "flush cancels the debounce";

  // The debounce that was already cancelled must not save a second time.
  FireTimer();
  EXPECT_EQ(saves_, 1);
}

TEST_F(AutosaveControllerTest, FlushWithNothingPendingDoesNotSave) {
  controller_->NotifyChanged();
  FireTimer();
  ASSERT_EQ(saves_, 1);
  EXPECT_TRUE(controller_->Flush());
  EXPECT_EQ(saves_, 1) << "closing right after an autosave saves once";
}

TEST_F(AutosaveControllerTest, ChangeDuringSaveRepeatsWithLatestStateOnce) {
  int calls = 0;
  during_save_ = [&] {
    // The user edits again while the first save is inside SetConfig.
    if (++calls == 1) {
      controller_->NotifyChanged();
    }
  };
  controller_->NotifyChanged();
  FireTimer();
  EXPECT_EQ(saves_, 2) << "the late change is saved, not dropped";
  EXPECT_EQ(max_depth_, 1) << "saves never nest";
  EXPECT_FALSE(controller_->pending());
  EXPECT_FALSE(timer_->armed) << "the repeat runs inline, no extra timer";
}

TEST_F(AutosaveControllerTest, FlushInsideASaveDoesNotNest) {
  int calls = 0;
  during_save_ = [&] {
    // e.g. a modal error dialog runs a nested event loop and an editor
    // commits (or the window closes) while the save is on the stack.
    if (++calls == 1) {
      controller_->NotifyChanged();
      EXPECT_FALSE(controller_->Flush());
    }
  };
  controller_->NotifyChanged();
  FireTimer();
  EXPECT_EQ(max_depth_, 1);
  EXPECT_EQ(saves_, 2) << "the re-entrant request becomes a repeat";
  EXPECT_FALSE(controller_->pending());
}

TEST_F(AutosaveControllerTest, RunawayChangeNotificationsAreBounded) {
  during_save_ = [&] { controller_->NotifyChanged(); };
  controller_->NotifyChanged();
  FireTimer();
  EXPECT_EQ(saves_, AutosaveController::kMaxPassesPerRun);
  EXPECT_EQ(max_depth_, 1);
  EXPECT_TRUE(controller_->pending()) << "left for later, not looped forever";
  EXPECT_TRUE(timer_->armed) << "and rescheduled";
}

TEST_F(AutosaveControllerTest, FailureIsReportedAndKeepsChangesPending) {
  next_result_ = Failed("converter is not running");
  controller_->NotifyChanged();
  FireTimer();

  ASSERT_EQ(statuses_.size(), 1);
  EXPECT_EQ(statuses_[0].kind, Status::Kind::kFailed);
  EXPECT_EQ(statuses_[0].detail, "converter is not running");
  EXPECT_TRUE(controller_->pending()) << "a failed save is still unsaved";
}

TEST_F(AutosaveControllerTest, NextChangeRetriesAfterFailureAndClearsStatus) {
  next_result_ = Failed("converter is not running");
  controller_->NotifyChanged();
  FireTimer();
  ASSERT_EQ(saves_, 1);

  next_result_ = Ok();  // the converter came back
  controller_->NotifyChanged();
  FireTimer();
  EXPECT_EQ(saves_, 2);
  EXPECT_FALSE(controller_->pending());
  ASSERT_EQ(statuses_.size(), 2);
  EXPECT_EQ(statuses_[1].kind, Status::Kind::kNone) << "problem resolved";
}

TEST_F(AutosaveControllerTest, RepeatedIdenticalFailureReportsOnlyOnce) {
  next_result_ = Failed("converter is not running");
  for (int i = 0; i < 3; ++i) {
    controller_->NotifyChanged();
    FireTimer();
  }
  EXPECT_EQ(saves_, 3) << "each change still retries";
  EXPECT_EQ(statuses_.size(), 1) << "but the UI is told only once";
}

TEST_F(AutosaveControllerTest, IncompleteAndFailedAreDistinguished) {
  next_result_ = Incomplete("custom keymap is empty");
  controller_->NotifyChanged();
  FireTimer();
  next_result_ = Failed("converter is not running");
  controller_->NotifyChanged();
  FireTimer();
  next_result_ = Ok();
  controller_->NotifyChanged();
  FireTimer();

  ASSERT_EQ(statuses_.size(), 3);
  EXPECT_EQ(statuses_[0].kind, Status::Kind::kIncomplete);
  EXPECT_EQ(statuses_[0].detail, "custom keymap is empty");
  EXPECT_EQ(statuses_[1].kind, Status::Kind::kFailed);
  EXPECT_EQ(statuses_[2].kind, Status::Kind::kNone);
}

TEST_F(AutosaveControllerTest, ClosingWithPendingChangesRetriesInteractively) {
  next_result_ = Failed("converter is not running");
  controller_->NotifyChanged();
  FireTimer();
  ASSERT_TRUE(controller_->pending());

  // Closing: still failing, so the caller learns nothing was saved.
  EXPECT_FALSE(controller_->Flush());
  ASSERT_EQ(interactive_calls_.size(), 2);
  EXPECT_TRUE(interactive_calls_[1]);
  EXPECT_TRUE(controller_->pending());

  // The converter recovers; closing now succeeds and clears the problem.
  next_result_ = Ok();
  EXPECT_TRUE(controller_->Flush());
  EXPECT_EQ(controller_->status().kind, Status::Kind::kNone);
  EXPECT_FALSE(controller_->pending());
}

TEST_F(AutosaveControllerTest, ClosingWithPendingDebounceSavesTheEdit) {
  controller_->NotifyChanged();  // user edits, then hits Esc immediately
  ASSERT_TRUE(timer_->armed);
  EXPECT_TRUE(controller_->Flush());
  EXPECT_EQ(saves_, 1);
}

TEST_F(AutosaveControllerTest, FlushWhileSuppressedReportsUnsaved) {
  controller_->NotifyChanged();
  controller_->SetSuppressed(true);
  EXPECT_FALSE(controller_->Flush());
  EXPECT_EQ(saves_, 0);
  controller_->SetSuppressed(false);
  EXPECT_TRUE(controller_->Flush());
  EXPECT_EQ(saves_, 1);
}

}  // namespace
}  // namespace gui
}  // namespace mozc
