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

#include "win32/base/sticky_keys_util.h"

#include <windows.h>

#include "testing/gunit.h"

namespace mozc {
namespace win32 {
namespace {

// Reads the raw hotkey flag directly, independent of the class under test.
bool RawHotkeyActive() {
  STICKYKEYS current = {sizeof(STICKYKEYS)};
  EXPECT_TRUE(
      ::SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &current, 0));
  return (current.dwFlags & SKF_HOTKEYACTIVE) != 0;
}

TEST(StickyKeysUtilTest, DisableThenRestoreRoundTrips) {
  const bool original = RawHotkeyActive();

  {
    StickyKeysUtil util;
    ASSERT_TRUE(util.DisableHotkey());
    EXPECT_FALSE(RawHotkeyActive());
    util.RestoreHotkey();
  }

  EXPECT_EQ(RawHotkeyActive(), original);
}

TEST(StickyKeysUtilTest, DestructorRestoresIfNotDoneExplicitly) {
  const bool original = RawHotkeyActive();

  {
    StickyKeysUtil util;
    ASSERT_TRUE(util.DisableHotkey());
    EXPECT_FALSE(RawHotkeyActive());
    // No explicit RestoreHotkey() -- the destructor must do it.
  }

  EXPECT_EQ(RawHotkeyActive(), original);
}

TEST(StickyKeysUtilTest, DisableHotkeyIsIdempotentUntilRestored) {
  StickyKeysUtil util;
  ASSERT_TRUE(util.DisableHotkey());
  EXPECT_FALSE(RawHotkeyActive());
  // Calling again before restoring must not clobber the saved "previous"
  // state with the already-disabled state.
  ASSERT_TRUE(util.DisableHotkey());
  EXPECT_FALSE(RawHotkeyActive());
  util.RestoreHotkey();
}

TEST(StickyKeysUtilTest, IsCurrentlyOnMatchesRawFlag) {
  STICKYKEYS current = {sizeof(STICKYKEYS)};
  ASSERT_TRUE(
      ::SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &current, 0));
  EXPECT_EQ(StickyKeysUtil::IsCurrentlyOn(),
           (current.dwFlags & SKF_STICKYKEYSON) != 0);
}

}  // namespace
}  // namespace win32
}  // namespace mozc
