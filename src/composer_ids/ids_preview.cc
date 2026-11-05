// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "composer_ids/ids_preview.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/strings/unicode.h"

namespace mozc {
namespace composer_ids {
namespace {

bool IsPreviewOperator(std::string_view op) {
  return op == "⿰" || op == "⿱" || op == "⿲" || op == "⿳";
}

}  // namespace

std::vector<PreviewCell> PreviewCellsFor(std::string_view op) {
  if (op == "⿱") {
    return {{0, 0, 1, 0.5}, {0, 0.5, 1, 0.5}};
  }
  if (op == "⿲") {
    return {{0, 0, 1.0 / 3.0, 1},
            {1.0 / 3.0, 0, 1.0 / 3.0, 1},
            {2.0 / 3.0, 0, 1.0 / 3.0, 1}};
  }
  if (op == "⿳") {
    return {{0, 0, 1, 1.0 / 3.0},
            {0, 1.0 / 3.0, 1, 1.0 / 3.0},
            {0, 2.0 / 3.0, 1, 1.0 / 3.0}};
  }
  // ⿰ (default)
  return {{0, 0, 0.5, 1}, {0.5, 0, 0.5, 1}};
}

std::optional<IdsPreview> ParseIdsPreview(std::string_view ids) {
  std::vector<std::string> chars;
  for (const std::string_view ch : Utf8AsChars(ids)) {
    if (!ch.empty()) chars.emplace_back(ch);
  }
  if (chars.size() < 3 || !IsPreviewOperator(chars[0])) {
    return std::nullopt;
  }
  IdsPreview preview;
  preview.op = chars[0];
  preview.cells = PreviewCellsFor(preview.op);
  if (chars.size() - 1 != preview.cells.size()) {
    return std::nullopt;
  }
  for (size_t i = 1; i < chars.size(); ++i) {
    // A nested layout or another operator: not a simple composition.
    if (chars[i].size() == 3 && static_cast<unsigned char>(chars[i][0]) == 0xE2 &&
        static_cast<unsigned char>(chars[i][1]) == 0xBF) {
      return std::nullopt;
    }
    preview.parts.push_back(chars[i]);
  }
  return preview;
}

}  // namespace composer_ids
}  // namespace mozc
