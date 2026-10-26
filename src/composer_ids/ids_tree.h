// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// The character composer's state as a small tree: a slot is either a single
// typed component (a leaf) or a nested layout with its own operator and
// slots, to any depth. Pure functions, no UI and no data files; the Qt dialog
// holds one of these and calls into it.
//
// Behaviour mirrors the composer in the Grognard editor (composerTree.ts) so
// both tools treat the same IDS the same way. This is an independent C++
// implementation.

#ifndef MOZC_COMPOSER_IDS_IDS_TREE_H_
#define MOZC_COMPOSER_IDS_IDS_TREE_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mozc {
namespace composer_ids {

struct ComposerSlot {
  enum class Kind { kLeaf, kNested };

  Kind kind = Kind::kLeaf;
  // kLeaf: what was typed (one character, or empty).
  std::string input;
  // kNested: the layout and its parts. parts.size() == PartCountFor(op).
  std::string op;
  std::vector<ComposerSlot> parts;

  static ComposerSlot Leaf(std::string_view input = "");
  static ComposerSlot Nested(std::string_view op,
                             std::vector<ComposerSlot> parts);

  bool operator==(const ComposerSlot &other) const;
};

// True when every leaf beneath the slot has non-empty (non-blank) input.
bool IsSlotFilled(const ComposerSlot &slot);

// True when the slot has any typed text beneath it. An all-blank nested layout
// does not count, so removing it throws nothing away.
bool SlotHasContent(const ComposerSlot &slot);

// Adjusts |parts| to the length |op| requires: keeps the existing slots, pads
// with empty leaves (⿰ -> ⿲ adds a third slot) or truncates (⿲ -> ⿰).
std::vector<ComposerSlot> ResizePartsForOperator(
    std::string_view op, std::vector<ComposerSlot> parts);

// Removes a nested layout, going back to one slot. Nothing typed: an empty
// leaf. Exactly one part has content: that part. Two or more: nullopt (the
// caller asks the user to clear the extras first). A leaf is returned as is.
std::optional<ComposerSlot> FlattenNestedSlot(const ComposerSlot &slot);

// The IDS as it is being typed; an empty field is "?", so "⿰木?" can be shown
// before every box is filled. Display only.
std::string DraftIds(std::string_view op,
                     const std::vector<ComposerSlot> &parts);

// The IDS to look up. nullopt unless every field is filled, since a "?" is not
// a component.
std::optional<std::string> ComposeIds(std::string_view op,
                                      const std::vector<ComposerSlot> &parts);

// A path selects a slot: {} is the outer layout, {1, 0} is the first box inside
// the second top-level box.
using SlotPath = std::vector<int>;

// The slot at |path|, or nullptr if the path is empty or does not land on one.
const ComposerSlot *SlotAt(const std::vector<ComposerSlot> &parts,
                           const SlotPath &path);

// Returns a copy of |parts| with the slot at |path| replaced by |next|. An
// invalid path returns |parts| unchanged.
std::vector<ComposerSlot> ReplaceSlotAt(std::vector<ComposerSlot> parts,
                                        const SlotPath &path,
                                        const ComposerSlot &next);

struct Layout {
  std::string op;
  std::vector<ComposerSlot> parts;
};

// What a click on the operator palette does to the selected region:
//  - the outer square (empty |selection|): change its layout, keeping what was
//    typed;
//  - a single field: turn it into a layout of the chosen shape, moving any
//    text into the first box;
//  - a layout inside a box: change that layout's shape.
Layout ApplyPaletteOperator(std::string_view root_op,
                            const std::vector<ComposerSlot> &parts,
                            const SlotPath &selection, std::string_view next);

struct ParsedIds {
  std::string op;
  std::vector<ComposerSlot> parts;
  // Components the IDS gives as circled-number placeholders (no encoded
  // character); they become empty slots for the user to fill.
  int unencoded_count = 0;
};

// Turns "⿰言某" or "⿱⿰abc" into the tree, nesting where the IDS nests.
// nullopt for anything that is not one well-formed IDS with an operator on top
// (a bare character, malformed or trailing input).
std::optional<ParsedIds> ParseIdsToComposerTree(std::string_view ids);

}  // namespace composer_ids
}  // namespace mozc

#endif  // MOZC_COMPOSER_IDS_IDS_TREE_H_
