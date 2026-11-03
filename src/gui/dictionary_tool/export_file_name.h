// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// File-name handling for "Export dictionary". Kept free of Qt so it can be
// unit-tested without a GUI.

#ifndef MOZC_GUI_DICTIONARY_TOOL_EXPORT_FILE_NAME_H_
#define MOZC_GUI_DICTIONARY_TOOL_EXPORT_FILE_NAME_H_

#include <string>

#include "absl/strings/string_view.h"

namespace mozc {
namespace gui {

// Extension given to an exported dictionary when the user types none. It
// must stay in the import dialog's "Text Files" filter.
inline constexpr absl::string_view kDictionaryExportExtension = "txt";

// Returns `path` with ".txt" appended when its last path component has no
// extension ("words" -> "words.txt", "words." -> "words.txt"). A path whose
// file name already has an extension ("words.tsv", "words.csv") is returned
// unchanged. A leading dot alone (".words") does not count as an extension.
// Both '/' and '\\' are treated as separators so Windows paths work too.
std::string AddDefaultExportExtension(absl::string_view path);

}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_DICTIONARY_TOOL_EXPORT_FILE_NAME_H_
