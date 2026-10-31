// Copyright 2010-2021, Google Inc.
#ifndef MOZC_GUI_BASE_PREFERENCES_PAGE_REQUEST_H_
#define MOZC_GUI_BASE_PREFERENCES_PAGE_REQUEST_H_

#include <string>

#include "absl/strings/string_view.h"

namespace mozc {
namespace gui {

// Cross-process request so About (and similar entry points) can open
// Preferences on a named page, including when Preferences is already running.
//
// Page ids are stable English tokens (e.g. "application"), not UI labels.
inline constexpr char kPreferencesPageApplication[] = "application";

// Writes a one-shot request under the user profile directory.
void RequestPreferencesPage(absl::string_view page_id);

// Returns a pending request (and clears it). Empty if none.
std::string ConsumePreferencesPageRequest();

// Absolute path of the request file (for QFileSystemWatcher).
std::string PreferencesPageRequestPath();

}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_BASE_PREFERENCES_PAGE_REQUEST_H_
