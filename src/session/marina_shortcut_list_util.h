// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0
//
// marinaMoji: builds the rows shown by the Windows Keyboard Shortcuts window,
// from the effective keymap table, the marina number-row bindings and the
// kaeriten table. Populated TIP-side and shipped to the renderer as
// RendererCommand::ShortcutsInfo (see
// win32/tip/tip_ui_handler_conventional.cc), because the renderer
// deliberately doesn't link config parsing.
//
// The logic is modelled on the equivalent file-local helpers in
// src/mac/mozc_toolbar.mm, deliberately as a separate implementation:
// the mac and Linux toolbars are settled and are not built on this. If they
// are ever migrated onto it, the two behaviours below need reconciling
// first -- this reads keymap tables through ConfigFileStream's "system://"
// scheme (the copies embedded in the binary at build time,
// base/config_file_stream_data.inc) rather than from loose files under the
// resource directory, and it drops duplicate (command, key) pairs.

#ifndef MOZC_SESSION_MARINA_SHORTCUT_LIST_UTIL_H_
#define MOZC_SESSION_MARINA_SHORTCUT_LIST_UTIL_H_

#include <string>
#include <vector>

#include "protocol/config.pb.h"

namespace mozc {
namespace session {

// One display row: the keymap command name (or, for kaeriten, the produced
// glyph) and the key combinations bound to it, already joined with ", ".
struct MarinaShortcutRow {
  std::string function;
  std::string keys;

  friend bool operator==(const MarinaShortcutRow& lhs,
                         const MarinaShortcutRow& rhs) {
    return lhs.function == rhs.function && lhs.keys == rhs.keys;
  }
};

// The three tabs of the shortcuts window.
struct MarinaShortcutLists {
  std::vector<MarinaShortcutRow> script;
  std::vector<MarinaShortcutRow> composition;
  std::vector<MarinaShortcutRow> kaeriten;
};

// Builds all three lists for |config|. Falls back to the ms-ime keymap and
// to bundled defaults for anything the config doesn't supply, so the result
// is never empty.
MarinaShortcutLists BuildMarinaShortcutLists(const config::Config& config);

// Same, for callers with no config at hand (defaults throughout).
MarinaShortcutLists BuildDefaultMarinaShortcutLists();

}  // namespace session
}  // namespace mozc

#endif  // MOZC_SESSION_MARINA_SHORTCUT_LIST_UTIL_H_
