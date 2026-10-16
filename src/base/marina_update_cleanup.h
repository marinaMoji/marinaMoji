// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0
//
// The updaters download installers to the user profile directory as
// marinaMoji-update-<tag>.msi (Windows) or .pkg (macOS) and open them, but
// nothing removed them afterwards, so each update left a ~27 MB file behind.
// An installer is stale once its release is not newer than the running
// version, because it can no longer be offered or needed.

#ifndef MOZC_BASE_MARINA_UPDATE_CLEANUP_H_
#define MOZC_BASE_MARINA_UPDATE_CLEANUP_H_

#include <string>

#include "absl/strings/string_view.h"

namespace mozc {

// True if |filename| (a bare file name, not a path) is an installer the
// updaters downloaded, marinaMoji-update-<tag>.msi or .pkg, and <tag> is not
// newer than |running_version| (e.g. "0.0.17-rc1"; a leading "v" is fine on
// either). Anything else, including names or versions that do not parse, is
// not stale: unknown files are never deleted.
bool IsStaleMarinaUpdateInstaller(absl::string_view filename,
                                  absl::string_view running_version);

// Deletes every stale installer (see above) directly inside |dir|. A newer
// download, which may be installing right now, is left alone. Failures are
// ignored (best effort). Returns how many files were removed.
int RemoveStaleMarinaUpdateInstallers(const std::string& dir,
                                      absl::string_view running_version);

}  // namespace mozc

#endif  // MOZC_BASE_MARINA_UPDATE_CLEANUP_H_
