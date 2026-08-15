// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0

#include "session/marina_shortcut_list_util.h"

#include <istream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "base/config_file_stream.h"
#include "composer/kaeriten_table_util.h"
#include "protocol/config.pb.h"
#include "session/marina_number_row_bindings_util.h"

namespace mozc {
namespace session {

namespace {

using ShortcutEntry = std::pair<std::string, std::string>;

// NOTE: the command lists, the default tables and the grouping rules below
// intentionally match src/mac/mozc_toolbar.mm's private copies. Keep the two
// in step by hand -- mac is not built on this module (see the header).

// Keymap commands surfaced on the Script tab, in display order.
const char* const kScriptCommands[] = {
    "ToggleAlphanumericMode", "ToggleHiraganaDirect", "ToggleTraditionalKanji",
    "ToggleManyoshuHiragana", "ToggleHiraganaKatakana", "ConvertToFullKatakana",
    "ConvertToHalfWidth",
    "ConvertToFullAlphanumeric", "ConvertToHiragana", nullptr};

// ... and on the Composition tab.
const char* const kCompositionCommands[] = {
    "Commit", "InsertOdorijiDefault", "ShowOdorijiPalette",
    "LaunchWordRegisterDialog", "SegmentWidthShrink", "SegmentWidthExpand",
    nullptr};

bool CommandInList(const std::string& command, const char* const* list) {
  for (; *list != nullptr; ++list) {
    if (command == *list) {
      return true;
    }
  }
  return false;
}

// Parses a keymap TSV ("status\tkey\tcommand") from |stream|, keeping only
// the rows the two tabs display.
void ParseKeymapStream(std::istream* stream, std::vector<ShortcutEntry>* script,
                       std::vector<ShortcutEntry>* composition) {
  if (stream == nullptr) {
    return;
  }
  std::string line;
  while (std::getline(*stream, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    const size_t first_tab = line.find('\t');
    if (first_tab == std::string::npos) {
      continue;
    }
    const size_t second_tab = line.find('\t', first_tab + 1);
    if (second_tab == std::string::npos) {
      continue;
    }
    const std::string state = line.substr(0, first_tab);
    const std::string key =
        line.substr(first_tab + 1, second_tab - (first_tab + 1));
    std::string command = line.substr(second_tab + 1);
    while (!command.empty() &&
           (command.back() == '\r' || command.back() == ' ')) {
      command.pop_back();
    }
    if (state == "status" && key == "key") {
      continue;  // header row
    }
    if (CommandInList(command, kScriptCommands)) {
      script->emplace_back(key, command);
    }
    if (CommandInList(command, kCompositionCommands)) {
      composition->emplace_back(key, command);
    }
  }
}

// "system://ms-ime.tsv" and friends: the tables embedded in the binary, the
// same ones keymap.cc loads.
const char* KeymapResourceName(config::Config::SessionKeymap keymap) {
  switch (keymap) {
    case config::Config::ATOK:
      return "system://atok.tsv";
    case config::Config::MSIME:
      return "system://ms-ime.tsv";
    case config::Config::KOTOERI:
      return "system://kotoeri.tsv";
    case config::Config::MOBILE:
      return "system://mobile.tsv";
    case config::Config::CHROMEOS:
      return "system://chromeos.tsv";
    case config::Config::CUSTOM:
      return nullptr;  // caller uses custom_keymap_table() instead
    case config::Config::NONE:
    default:
      return "system://ms-ime.tsv";
  }
}

void LoadKeymapResource(const char* resource_name,
                        std::vector<ShortcutEntry>* script,
                        std::vector<ShortcutEntry>* composition) {
  if (resource_name == nullptr) {
    return;
  }
  std::unique_ptr<std::istream> stream(
      ConfigFileStream::LegacyOpen(resource_name));
  ParseKeymapStream(stream.get(), script, composition);
}

void FillDefaultScriptShortcuts(std::vector<ShortcutEntry>* script) {
  if (!script->empty()) {
    return;
  }
  const std::pair<const char*, const char*> kDefault[] = {
      {"Ctrl Shift `", "ToggleAlphanumericMode"},
      {"Eisu", "ToggleAlphanumericMode"},
      {"Ctrl Shift 5", "ToggleHiraganaDirect"},
      {"Ctrl Shift F", "ToggleTraditionalKanji"},
      {"Ctrl Shift 3", "ToggleTraditionalKanji"},
      {"Ctrl Shift #", "ToggleTraditionalKanji"},
      {"Ctrl Shift 4", "ToggleManyoshuHiragana"},
      {"Ctrl Shift $", "ToggleManyoshuHiragana"},
      {"Ctrl Shift %", "ToggleHiraganaDirect"},
      {"RightShift", "ToggleManyoshuHiragana"},
      {"Ctrl i", "ConvertToFullKatakana"},
      {"F7", "ConvertToFullKatakana"},
      {"Ctrl o", "ConvertToHalfWidth"},
      {"F8", "ConvertToHalfWidth"},
      {"Ctrl p", "ConvertToFullAlphanumeric"},
      {"F9", "ConvertToFullAlphanumeric"},
      {"Ctrl u", "ConvertToHiragana"},
      {"F6", "ConvertToHiragana"},
  };
  for (const auto& entry : kDefault) {
    script->emplace_back(entry.first, entry.second);
  }
}

void FillDefaultCompositionShortcuts(std::vector<ShortcutEntry>* composition) {
  if (!composition->empty()) {
    return;
  }
  const std::pair<const char*, const char*> kDefault[] = {
      {"Enter", "Commit"},
      {"Ctrl Enter", "Commit"},
      {"Ctrl m", "Commit"},
      {"Ctrl Shift 0", "LaunchWordRegisterDialog"},
      {"Ctrl k", "SegmentWidthShrink"},
      {"Shift Left", "SegmentWidthShrink"},
      {"Ctrl l", "SegmentWidthExpand"},
      {"Shift Right", "SegmentWidthExpand"},
  };
  for (const auto& entry : kDefault) {
    composition->emplace_back(entry.first, entry.second);
  }
}

void LoadKaeritenEntries(const config::Config& config,
                         std::vector<ShortcutEntry>* kaeriten) {
  kaeriten->clear();
  std::vector<std::pair<std::string, std::string>> pairs;
  mozc::composer::LoadKaeritenShortcutEntries(config, &pairs);
  if (pairs.empty()) {
    LOG(ERROR) << "Kaeriten shortcut table loaded empty; "
               << "system://kaeriten.tsv may be missing from the build.";
  }
  for (const auto& pair : pairs) {
    kaeriten->emplace_back(pair.first, pair.second);
  }
}

// Collapses several bindings of one command into a single row, following
// |command_order| when given (so the tab reads in a deliberate order rather
// than alphabetically) and map order otherwise.
void GroupShortcutsByCommand(const std::vector<ShortcutEntry>& entries,
                             const char* const* command_order,
                             std::vector<MarinaShortcutRow>* out) {
  out->clear();
  std::map<std::string, std::vector<std::string>> by_command;
  std::set<std::string> seen_keys;
  for (const auto& entry : entries) {
    // The same (command, key) pair can legitimately appear twice once the
    // number-row overrides are merged in; don't show it twice.
    if (!seen_keys.insert(entry.second + "\t" + entry.first).second) {
      continue;
    }
    by_command[entry.second].push_back(entry.first);
  }

  const auto append = [out](const std::string& command,
                            const std::vector<std::string>& keys) {
    std::string joined;
    for (size_t i = 0; i < keys.size(); ++i) {
      if (i > 0) {
        joined += ", ";
      }
      joined += keys[i];
    }
    out->push_back(MarinaShortcutRow{command, joined});
  };

  if (command_order != nullptr) {
    for (const char* const* command = command_order; *command != nullptr;
         ++command) {
      const auto it = by_command.find(*command);
      if (it == by_command.end()) {
        continue;
      }
      append(it->first, it->second);
    }
    return;
  }
  for (const auto& pair : by_command) {
    append(pair.first, pair.second);
  }
}

}  // namespace

MarinaShortcutLists BuildMarinaShortcutLists(const config::Config& config) {
  std::vector<ShortcutEntry> script;
  std::vector<ShortcutEntry> composition;

  if (config.session_keymap() == config::Config::CUSTOM &&
      config.has_custom_keymap_table() &&
      !config.custom_keymap_table().empty()) {
    std::istringstream stream(config.custom_keymap_table());
    ParseKeymapStream(&stream, &script, &composition);
  } else {
    LoadKeymapResource(KeymapResourceName(config.session_keymap()), &script,
                       &composition);
  }
  if (script.empty() && composition.empty()) {
    // Unknown or unreadable keymap: fall back to the default table before
    // falling back to the hardcoded lists below.
    LoadKeymapResource("system://ms-ime.tsv", &script, &composition);
  }
  FillDefaultScriptShortcuts(&script);
  FillDefaultCompositionShortcuts(&composition);

  // marinaMoji's number-row shortcuts are configurable and override whatever
  // the keymap table says for those rows.
  ApplyMarinaNumberRowShortcutEntries(config, &script, &composition);

  std::vector<ShortcutEntry> kaeriten;
  LoadKaeritenEntries(config, &kaeriten);

  MarinaShortcutLists lists;
  GroupShortcutsByCommand(script, kScriptCommands, &lists.script);
  GroupShortcutsByCommand(composition, kCompositionCommands,
                          &lists.composition);
  GroupShortcutsByCommand(kaeriten, nullptr, &lists.kaeriten);
  return lists;
}

MarinaShortcutLists BuildDefaultMarinaShortcutLists() {
  return BuildMarinaShortcutLists(config::Config());
}

}  // namespace session
}  // namespace mozc
