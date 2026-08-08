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

#include "dictionary/docket_store.h"

#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "base/file_util.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace dictionary {
namespace {

class DocketStoreTest : public testing::TestWithTempUserProfile {};

TEST_F(DocketStoreTest, EmptyByDefault) {
  const absl::StatusOr<DocketData> data = ReadDocketDataUnlocked();
  ASSERT_OK(data);
  EXPECT_TRUE(data->pending.empty());
  EXPECT_TRUE(data->never.empty());
}

TEST_F(DocketStoreTest, AddPendingPersistsAndDedupes) {
  DocketStore store;
  EXPECT_OK(store.AddPending("大元宮", "だいげんぐう", 1837, 1837, "名詞"));
  EXPECT_OK(store.AddPending("大元宮", "だいげんぐう", 1837, 1837, "名詞"));

  const absl::StatusOr<DocketData> data = ReadDocketDataUnlocked();
  ASSERT_OK(data);
  ASSERT_EQ(data->pending.size(), 1);
  EXPECT_EQ(data->pending[0].surface, "大元宮");
  EXPECT_EQ(data->pending[0].reading, "だいげんぐう");
  EXPECT_EQ(data->pending[0].lid, 1837);
  EXPECT_EQ(data->pending[0].rid, 1837);
  EXPECT_EQ(data->pending[0].pos, "名詞");
  EXPECT_GT(data->pending[0].timestamp_unix, 0);
}

TEST_F(DocketStoreTest, RemovePending) {
  DocketStore store;
  EXPECT_OK(store.AddPending("東京", "とうきょう", 1, 1, "名詞"));
  EXPECT_OK(store.RemovePending("東京"));

  const absl::StatusOr<DocketData> data = ReadDocketDataUnlocked();
  ASSERT_OK(data);
  EXPECT_TRUE(data->pending.empty());
}

TEST_F(DocketStoreTest, AddNeverRemovesFromPendingAndBlocksFuture) {
  DocketStore store;
  EXPECT_OK(store.AddPending("鬼灯", "ほおずき", 2, 2, "名詞"));
  EXPECT_OK(store.AddNever("鬼灯"));

  {
    const absl::StatusOr<DocketData> data = ReadDocketDataUnlocked();
    ASSERT_OK(data);
    EXPECT_TRUE(data->pending.empty());
    ASSERT_EQ(data->never.size(), 1);
    EXPECT_EQ(data->never[0], "鬼灯");
  }

  // Re-adding a never-listed surface is a no-op.
  EXPECT_OK(store.AddPending("鬼灯", "ほおずき", 2, 2, "名詞"));
  const absl::StatusOr<DocketData> data = ReadDocketDataUnlocked();
  ASSERT_OK(data);
  EXPECT_TRUE(data->pending.empty());
}

TEST_F(DocketStoreTest, CapsPendingSizeEvictingOldest) {
  DocketStore store;
  for (size_t i = 0; i < kDocketMaxPendingSize + 5; ++i) {
    EXPECT_OK(store.AddPending(absl::StrCat("word", i), "reading", 0, 0, "名詞"));
  }
  const absl::StatusOr<DocketData> data = ReadDocketDataUnlocked();
  ASSERT_OK(data);
  EXPECT_EQ(data->pending.size(), kDocketMaxPendingSize);
  // The oldest entries (word0..word4) should have been evicted.
  EXPECT_EQ(data->pending.front().surface, "word5");
}

}  // namespace
}  // namespace dictionary
}  // namespace mozc
