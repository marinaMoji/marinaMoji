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
//
// DocketStore persists the "docket": a small queue of recently-committed,
// dictionary-unknown compounds awaiting the user's review (register /
// skip / never-ask-again), plus the permanent never-ask-again list.

#ifndef MOZC_DICTIONARY_DOCKET_STORE_H_
#define MOZC_DICTIONARY_DOCKET_STORE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace mozc {

class ProcessMutex;

namespace dictionary {

struct DocketEntry {
  std::string surface;
  std::string reading;
  // POS ids copied from the committed candidate. -1 means unknown. Kept
  // for reference; `pos` below is the label actually used for prefill.
  int32_t lid = -1;
  int32_t rid = -1;
  // Best-effort user-dictionary POS category label (e.g. "名詞"), guessed
  // from lid/rid at capture time via PosMatcher. Never empty.
  std::string pos;
  int64_t timestamp_unix = 0;
};

struct DocketData {
  std::vector<DocketEntry> pending;
  std::vector<std::string> never;
};

// Bound on how many pending entries are retained; oldest entries are
// evicted first once the docket grows past this.
inline constexpr size_t kDocketMaxPendingSize = 200;

std::string GetDocketStorePath();

// Best-effort, unlocked read. Safe to call from a process (e.g. the
// renderer, for a toolbar badge count) that only wants a snapshot and
// isn't performing a mutation.
absl::StatusOr<DocketData> ReadDocketDataUnlocked();

// DocketStore performs locked read-modify-write mutations so that the
// server process (adding candidates) and the review dialog (removing /
// registering them) don't clobber each other. Each mutating call re-reads
// the file under lock, applies the change, and writes it back — mirrors
// the pattern in UserDictionaryStorage, but scoped to a single call
// instead of requiring the caller to manage Load()/Save() explicitly.
class DocketStore {
 public:
  DocketStore();
  explicit DocketStore(std::string filename);
  ~DocketStore();

  // Adds a pending entry unless |surface| is already pending or on the
  // never-list. Deduping is by surface only.
  absl::Status AddPending(absl::string_view surface,
                          absl::string_view reading, int32_t lid,
                          int32_t rid, absl::string_view pos);

  // Removes |surface| from the pending list, if present.
  absl::Status RemovePending(absl::string_view surface);

  // Removes |surface| from the pending list (if present) and adds it to
  // the permanent never-list.
  absl::Status AddNever(absl::string_view surface);

 private:
  const std::string filename_;
  std::unique_ptr<ProcessMutex> process_mutex_;
};

}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_DOCKET_STORE_H_
