// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MOZC_KAGE_KAGE_DATA_H_
#define MOZC_KAGE_KAGE_DATA_H_

#include <string_view>

#include "absl/status/statusor.h"
#include "kage/kage_engine.h"

namespace mozc {
namespace kage {

// Reads a glyph table: one "name<TAB>KAGE data" per line (see
// //data/kage/README.md). Blank lines and lines starting with '#' are ignored.
absl::StatusOr<Buhin> LoadBuhinFromFile(std::string_view path);

// Same, from the text of the file.
Buhin ParseBuhin(std::string_view contents);

}  // namespace kage
}  // namespace mozc

#endif  // MOZC_KAGE_KAGE_DATA_H_
