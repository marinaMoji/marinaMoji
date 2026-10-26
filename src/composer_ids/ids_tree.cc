// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "composer_ids/ids_tree.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/strings/ascii.h"
#include "base/strings/unicode.h"
#include "composer_ids/ids_operator.h"

namespace mozc {
namespace composer_ids {
namespace {

// Trims ASCII and U+3000 whitespace, the blanks a user can type into a field.
std::string TrimInput(std::string_view input) {
  constexpr std::string_view kIdeographicSpace = "\xE3\x80\x80";
  std::string_view s = absl::StripAsciiWhitespace(input);
  while (s.substr(0, 3) == kIdeographicSpace) {
    s = absl::StripAsciiWhitespace(s.substr(3));
  }
  while (s.size() >= 3 && s.substr(s.size() - 3) == kIdeographicSpace) {
    s = absl::StripAsciiWhitespace(s.substr(0, s.size() - 3));
  }
  return std::string(s);
}

// ① .. ⑳: components that have no Unicode character of their own.
bool IsUnencodedPlaceholder(std::string_view ch) {
  const std::u32string cp = strings::Utf8ToUtf32(ch);
  return cp.size() == 1 && cp[0] >= 0x2460 && cp[0] <= 0x2473;
}

void AppendDraft(const ComposerSlot &slot, std::string *out) {
  if (slot.kind == ComposerSlot::Kind::kLeaf) {
    const std::string trimmed = TrimInput(slot.input);
    out->append(trimmed.empty() ? "?" : trimmed);
    return;
  }
  out->append(DraftIds(slot.op, slot.parts));
}

}  // namespace

ComposerSlot ComposerSlot::Leaf(std::string_view input) {
  ComposerSlot slot;
  slot.kind = Kind::kLeaf;
  slot.input = std::string(input);
  return slot;
}

ComposerSlot ComposerSlot::Nested(std::string_view op,
                                  std::vector<ComposerSlot> parts) {
  ComposerSlot slot;
  slot.kind = Kind::kNested;
  slot.op = std::string(op);
  slot.parts = std::move(parts);
  return slot;
}

bool ComposerSlot::operator==(const ComposerSlot &other) const {
  return kind == other.kind && input == other.input && op == other.op &&
         parts == other.parts;
}

bool IsSlotFilled(const ComposerSlot &slot) {
  if (slot.kind == ComposerSlot::Kind::kLeaf) {
    return !TrimInput(slot.input).empty();
  }
  return std::all_of(slot.parts.begin(), slot.parts.end(), IsSlotFilled);
}

bool SlotHasContent(const ComposerSlot &slot) {
  if (slot.kind == ComposerSlot::Kind::kLeaf) {
    return !TrimInput(slot.input).empty();
  }
  return std::any_of(slot.parts.begin(), slot.parts.end(), SlotHasContent);
}

std::vector<ComposerSlot> ResizePartsForOperator(
    std::string_view op, std::vector<ComposerSlot> parts) {
  const size_t target = static_cast<size_t>(PartCountFor(op));
  if (parts.size() > target) {
    parts.resize(target);
  }
  while (parts.size() < target) {
    parts.push_back(ComposerSlot::Leaf());
  }
  return parts;
}

std::optional<ComposerSlot> FlattenNestedSlot(const ComposerSlot &slot) {
  if (slot.kind != ComposerSlot::Kind::kNested) return slot;
  const ComposerSlot *occupied = nullptr;
  for (const ComposerSlot &part : slot.parts) {
    if (!SlotHasContent(part)) continue;
    if (occupied != nullptr) return std::nullopt;
    occupied = &part;
  }
  return occupied != nullptr ? *occupied : ComposerSlot::Leaf();
}

std::string DraftIds(std::string_view op,
                     const std::vector<ComposerSlot> &parts) {
  std::string out(op);
  for (const ComposerSlot &part : parts) {
    AppendDraft(part, &out);
  }
  return out;
}

std::optional<std::string> ComposeIds(std::string_view op,
                                      const std::vector<ComposerSlot> &parts) {
  const bool filled = std::all_of(parts.begin(), parts.end(), IsSlotFilled);
  if (parts.empty() || !filled) return std::nullopt;
  return DraftIds(op, parts);
}

const ComposerSlot *SlotAt(const std::vector<ComposerSlot> &parts,
                           const SlotPath &path) {
  if (path.empty()) return nullptr;
  const std::vector<ComposerSlot> *level = &parts;
  const ComposerSlot *current = nullptr;
  for (const int index : path) {
    if (index < 0 || static_cast<size_t>(index) >= level->size()) {
      return nullptr;
    }
    current = &(*level)[index];
    level = &current->parts;
  }
  // A path may only descend through nested slots; for a leaf |parts| is empty,
  // so a longer path already failed the bounds check above.
  return current;
}

std::vector<ComposerSlot> ReplaceSlotAt(std::vector<ComposerSlot> parts,
                                        const SlotPath &path,
                                        const ComposerSlot &next) {
  if (path.empty()) return parts;
  const int head = path.front();
  if (head < 0 || static_cast<size_t>(head) >= parts.size()) return parts;
  if (path.size() == 1) {
    parts[head] = next;
    return parts;
  }
  ComposerSlot &child = parts[head];
  if (child.kind != ComposerSlot::Kind::kNested) return parts;
  child.parts = ReplaceSlotAt(std::move(child.parts),
                              SlotPath(path.begin() + 1, path.end()), next);
  return parts;
}

Layout ApplyPaletteOperator(std::string_view root_op,
                            const std::vector<ComposerSlot> &parts,
                            const SlotPath &selection, std::string_view next) {
  if (selection.empty()) {
    return {std::string(next), ResizePartsForOperator(next, parts)};
  }
  const ComposerSlot *target = SlotAt(parts, selection);
  if (target == nullptr) return {std::string(root_op), parts};

  if (target->kind == ComposerSlot::Kind::kLeaf) {
    std::vector<ComposerSlot> nested_parts(
        static_cast<size_t>(PartCountFor(next)));
    if (!TrimInput(target->input).empty()) {
      nested_parts[0] = ComposerSlot::Leaf(target->input);
    }
    return {std::string(root_op),
            ReplaceSlotAt(parts, selection,
                          ComposerSlot::Nested(next, std::move(nested_parts)))};
  }
  return {std::string(root_op),
          ReplaceSlotAt(parts, selection,
                        ComposerSlot::Nested(
                            next, ResizePartsForOperator(next, target->parts)))};
}

std::optional<ParsedIds> ParseIdsToComposerTree(std::string_view ids) {
  std::vector<std::string_view> chars;
  for (const std::string_view ch :
       Utf8AsChars(absl::StripAsciiWhitespace(ids))) {
    chars.push_back(ch);
  }
  size_t index = 0;
  int unencoded = 0;

  // Recursive descent; |depth| bounds the recursion on hostile input.
  struct Parser {
    const std::vector<std::string_view> &chars;
    size_t &index;
    int &unencoded;

    std::optional<ComposerSlot> ParseSlot(int depth) {
      if (index >= chars.size() || depth > 64) return std::nullopt;
      const std::string_view ch = chars[index++];
      if (IsIdsOperator(ch)) {
        std::vector<ComposerSlot> parts;
        for (int i = 0; i < PartCountFor(ch); ++i) {
          std::optional<ComposerSlot> part = ParseSlot(depth + 1);
          if (!part) return std::nullopt;
          parts.push_back(*std::move(part));
        }
        return ComposerSlot::Nested(ch, std::move(parts));
      }
      if (IsUnencodedPlaceholder(ch)) {
        ++unencoded;
        return ComposerSlot::Leaf();
      }
      return ComposerSlot::Leaf(ch);
    }
  };

  Parser parser{chars, index, unencoded};
  std::optional<ComposerSlot> root = parser.ParseSlot(0);
  if (!root || root->kind != ComposerSlot::Kind::kNested ||
      index != chars.size()) {
    return std::nullopt;
  }
  ParsedIds result;
  result.op = std::move(root->op);
  result.parts = std::move(root->parts);
  result.unencoded_count = unencoded;
  return result;
}

}  // namespace composer_ids
}  // namespace mozc
