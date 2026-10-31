// Copyright 2010-2021, Google Inc.
// All rights reserved.
//
// marinaMoji: macOS floating toolbar.  A non-activating NSPanel with branded
// SVG icons for mode switching, shin/kyu toggle, odoriji, dictionary, and
// keyboard shortcuts.  Mirrors the Linux GTK toolbar in functionality.

#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "base/file_util.h"
#include "base/mac/mac_util.h"
#include "composer/kaeriten_table_util.h"
#include "composer_ids/composer_result.h"
#include "session/marina_number_row_bindings_util.h"
#include "base/system_util.h"
#include "client/client_interface.h"
#include "mac/common.h"
#include "mac/marina_localized_string.h"
#include "mac/mozc_toolbar.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"

@class MozcShortcutsWindowController;

// Declared before @implementation MozcToolbarView.
static __weak id<ControllerCallback> g_active_controller = nil;
static NSTimeInterval g_suppress_set_value_direct_until = 0;
static bool g_symbols_palette_visible = false;
static bool g_toolbar_reshow_after_palette_close = false;
static MozcShortcutsWindowController *g_shortcuts_window_controller = nil;

// marinaMoji: character composer. CharacterComposer.app is a separate app (the
// toolbar lives in the input method process and cannot show Qt windows). An
// app started through LaunchServices cannot be given arguments, so it leaves
// the character the user inserted in composer_ids::DefaultResultFilePath()
// just before it quits; this watches for the quit and then commits that text
// through the active controller, the way a Symbols Palette click does.
static BOOL g_character_composer_watching = NO;

// Commits |text| through the active controller. The composer's window takes
// the focus, so right after it quits the controller may not be active again
// yet; retry for a few seconds before giving up.
static void CommitCharacterComposerText(NSString *text, int attemptsLeft) {
  id<ControllerCallback> controller = g_active_controller;
  if (controller) {
    mozc::commands::Output output;
    output.mutable_result()->set_value(text.UTF8String);
    [controller outputResult:output];
    return;
  }
  if (attemptsLeft <= 0) {
    LOG(WARNING) << "character composer: no active controller; result dropped";
    return;
  }
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.2 * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), ^{
                   CommitCharacterComposerText(text, attemptsLeft - 1);
                 });
}

static void WatchCharacterComposer(NSRunningApplication *app) {
  if (g_character_composer_watching) return;  // the same app, merely re-activated
  g_character_composer_watching = YES;
  NSTimer *timer = [NSTimer timerWithTimeInterval:0.25
                                          repeats:YES
                                            block:^(NSTimer *timer) {
    if (!app.terminated) return;
    [timer invalidate];
    g_character_composer_watching = NO;
    const std::string path = mozc::composer_ids::DefaultResultFilePath();
    const std::optional<std::string> text = mozc::composer_ids::ReadComposerResult(path);
    mozc::FileUtil::Unlink(path).IgnoreError();
    if (text.has_value()) {
      CommitCharacterComposerText([NSString stringWithUTF8String:text->c_str()], 15);
    }
  }];
  [[NSRunLoop mainRunLoop] addTimer:timer forMode:NSRunLoopCommonModes];
}

namespace {

constexpr int kIconSize = 24;
constexpr int kLogoWidth = 120;
constexpr int kToolbarHeight = 36;
constexpr int kButtonWidth = 36;
constexpr int kToolbarMargin = 20;
constexpr CGFloat kCornerRadius = 10.0;
constexpr int kSymbolsTabOdoriji = 0;
NSString *const kPrefsSymbolsPinnedKey = @"symbols_palette_pinned";
NSString *const kPrefsSymbolsLastTabKey = @"symbols_palette_last_tab";

static NSString *ToolbarPrefsPath() {
  const std::string path =
      mozc::FileUtil::JoinPath(mozc::SystemUtil::GetUserProfileDirectory(),
                               "toolbar.conf");
  return [NSString stringWithUTF8String:path.c_str()];
}

using ShortcutEntry = std::pair<std::string, std::string>;
using GroupedShortcutRow = std::pair<std::string, std::string>;

const char *const kScriptCommands[] = {
    "ToggleAlphanumericMode", "ToggleHiraganaDirect", "ToggleTraditionalKanji",
    "ToggleLiveConversion", "ToggleManyoshuHiragana", "ToggleHiraganaKatakana",
    "ConvertToFullKatakana", "ConvertToHalfWidth", "ConvertToFullAlphanumeric",
    "ConvertToHiragana", nullptr};
const char *const kCompositionCommands[] = {
    "Commit", "InsertOdorijiDefault", "ShowOdorijiPalette",
    "LaunchWordRegisterDialog", "SegmentWidthShrink", "SegmentWidthExpand",
    nullptr};

bool CommandInList(const char *cmd, const char *const *list) {
  for (; *list; ++list)
    if (strcmp(cmd, *list) == 0) return true;
  return false;
}

void ParseKeymapTsv(const std::string &path,
                    std::vector<ShortcutEntry> *script,
                    std::vector<ShortcutEntry> *composition) {
  script->clear();
  composition->clear();
  std::ifstream f(path);
  if (!f) return;
  std::string line;
  while (std::getline(f, line)) {
    if (line.empty() || line[0] == '#') continue;
    size_t t1 = line.find('\t');
    if (t1 == std::string::npos) continue;
    size_t t2 = line.find('\t', t1 + 1);
    if (t2 == std::string::npos) continue;
    std::string state = line.substr(0, t1);
    std::string key = line.substr(t1 + 1, t2 - (t1 + 1));
    std::string command = line.substr(t2 + 1);
    while (!command.empty() && (command.back() == '\r' || command.back() == ' '))
      command.pop_back();
    if (state == "status" && key == "key") continue;
    if (CommandInList(command.c_str(), kScriptCommands))
      script->emplace_back(key, command);
    if (CommandInList(command.c_str(), kCompositionCommands))
      composition->emplace_back(key, command);
  }
}

void ParseKeymapFromString(const std::string &content,
                           std::vector<ShortcutEntry> *script,
                           std::vector<ShortcutEntry> *composition) {
  script->clear();
  composition->clear();
  std::istringstream stream(content);
  std::string line;
  while (std::getline(stream, line)) {
    if (line.empty() || line[0] == '#') continue;
    size_t t1 = line.find('\t');
    if (t1 == std::string::npos) continue;
    size_t t2 = line.find('\t', t1 + 1);
    if (t2 == std::string::npos) continue;
    std::string state = line.substr(0, t1);
    std::string key = line.substr(t1 + 1, t2 - (t1 + 1));
    std::string command = line.substr(t2 + 1);
    while (!command.empty() && (command.back() == '\r' || command.back() == ' '))
      command.pop_back();
    if (state == "status" && key == "key") continue;
    if (CommandInList(command.c_str(), kScriptCommands))
      script->emplace_back(key, command);
    if (CommandInList(command.c_str(), kCompositionCommands))
      composition->emplace_back(key, command);
  }
}

void GroupShortcutsByCommand(const std::vector<ShortcutEntry> &entries,
                             const char *const *command_order,
                             std::vector<GroupedShortcutRow> *out) {
  out->clear();
  std::map<std::string, std::vector<std::string>> by_cmd;
  for (const auto &p : entries) by_cmd[p.second].push_back(p.first);
  if (command_order) {
    for (; *command_order; ++command_order) {
      auto it = by_cmd.find(*command_order);
      if (it == by_cmd.end()) continue;
      std::string keys;
      for (size_t i = 0; i < it->second.size(); ++i) {
        if (i) keys += ", ";
        keys += it->second[i];
      }
      out->emplace_back(it->first, keys);
    }
  } else {
    for (const auto &p : by_cmd) {
      std::string keys;
      for (size_t i = 0; i < p.second.size(); ++i) {
        if (i) keys += ", ";
        keys += p.second[i];
      }
      out->emplace_back(p.first, keys);
    }
  }
}

void FillDefaultScriptShortcuts(std::vector<ShortcutEntry> *script) {
  if (!script->empty()) return;
  const std::pair<const char *, const char *> kDefault[] = {
      {"Ctrl Shift `", "ToggleAlphanumericMode"},
      {"Eisu", "ToggleAlphanumericMode"},
      {"Ctrl Shift 5", "ToggleHiraganaDirect"},
      {"Ctrl Shift F", "ToggleTraditionalKanji"},
      {"Ctrl Shift 3", "ToggleTraditionalKanji"},
      {"Ctrl Shift #", "ToggleTraditionalKanji"},
      {"Ctrl Shift L", "ToggleLiveConversion"},
      {"Ctrl Shift 6", "ToggleLiveConversion"},
      {"Ctrl Shift ^", "ToggleLiveConversion"},
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
  for (const auto &p : kDefault) script->emplace_back(p.first, p.second);
}

void FillDefaultCompositionShortcuts(std::vector<ShortcutEntry> *composition) {
  if (!composition->empty()) return;
  const std::pair<const char *, const char *> kDefault[] = {
      {"Enter", "Commit"},
      {"Ctrl Enter", "Commit"},
      {"Ctrl m", "Commit"},
      {"Ctrl Shift 0", "LaunchWordRegisterDialog"},
      {"Ctrl k", "SegmentWidthShrink"},
      {"Shift Left", "SegmentWidthShrink"},
      {"Ctrl l", "SegmentWidthExpand"},
      {"Shift Right", "SegmentWidthExpand"},
  };
  for (const auto &p : kDefault) composition->emplace_back(p.first, p.second);
}

void LoadKaeritenEntries(const mozc::config::Config *config,
                         std::vector<ShortcutEntry> *kaeriten) {
  kaeriten->clear();
  mozc::config::Config effective;
  if (config != nullptr) {
    effective = *config;
  }
  std::vector<std::pair<std::string, std::string>> pairs;
  mozc::composer::LoadKaeritenShortcutEntries(effective, &pairs);
  if (pairs.empty()) {
    LOG(ERROR) << "Kaeriten shortcut table loaded empty; "
               << "system://kaeriten.tsv may be missing from the build.";
  }
  for (const auto &pair : pairs) {
    kaeriten->emplace_back(pair.first, pair.second);
  }
}

std::string GetKeymapPath(const std::string &filename);

std::vector<std::string> BuildDefaultOdorijiSymbols() {
  return {"々", "ゝ", "ゞ", "ヽ", "ヾ", "〻", "〱", "〲", "〳", "〴", "〵"};
}

std::vector<std::string> BuildKaeritenSymbols(
    mozc::client::ClientInterface *client) {
  std::vector<ShortcutEntry> kaeriten_entries;
  mozc::config::Config config;
  const mozc::config::Config *config_ptr = nullptr;
  if (client != nullptr && client->GetConfig(&config)) {
    config_ptr = &config;
  }
  LoadKaeritenEntries(config_ptr, &kaeriten_entries);
  std::vector<std::string> symbols;
  std::set<std::string> seen;
  for (const auto &entry : kaeriten_entries) {
    if (!entry.second.empty() && seen.insert(entry.second).second) {
      symbols.push_back(entry.second);
    }
  }
  return symbols;
}

std::vector<std::string> BuildDefaultGeneralSymbols() {
  return {"〔", "〕", "［", "］", "【", "】", "〈", "〉", "《", "》", "（", "）",
          "｛", "｝", "□", "■", "○", "△", "×", "※", "〓", "◆", "◇", "◎",
          "▲", "▽", "…", "—", "ヶ"};
}

std::string UserSymbolsPath() {
  return mozc::MacUtil::GetApplicationSupportDirectory() + "/user_symbols.txt";
}

std::vector<std::string> LoadUserSymbolsFromFile() {
  std::vector<std::string> result;
  std::ifstream ifs(UserSymbolsPath());
  if (!ifs) return result;
  std::string line;
  while (std::getline(ifs, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (!line.empty()) {
      result.push_back(line);
    }
  }
  return result;
}

std::string KeymapFilenameFromSessionKeymap(
    mozc::config::Config::SessionKeymap keymap) {
  switch (keymap) {
    case mozc::config::Config::ATOK: return "atok.tsv";
    case mozc::config::Config::MSIME: return "ms-ime.tsv";
    case mozc::config::Config::KOTOERI: return "kotoeri.tsv";
    case mozc::config::Config::MOBILE: return "mobile.tsv";
    case mozc::config::Config::CHROMEOS: return "chromeos.tsv";
    case mozc::config::Config::CUSTOM: return "";
    case mozc::config::Config::NONE:
    default: return "ms-ime.tsv";
  }
}

std::string GetKeymapPath(const std::string &filename) {
  return mozc::MacUtil::GetServerDirectory() + "/keymap/" + filename;
}

}  // namespace

// ---------------------------------------------------------------------------
#pragma mark - MozcShortcutsWindowController

@interface MozcShortcutsWindowController : NSWindowController
    <NSTabViewDelegate, NSTableViewDataSource, NSTableViewDelegate,
     NSWindowDelegate> {
  NSTabView *tabView_;
  NSTableView *scriptTable_;
  NSTableView *compositionTable_;
  NSTableView *kaeritenTable_;
  std::vector<GroupedShortcutRow> scriptData_;
  std::vector<GroupedShortcutRow> compositionData_;
  std::vector<GroupedShortcutRow> kaeritenData_;
}
- (instancetype)initWithClient:(mozc::client::ClientInterface *)client;
@end

@implementation MozcShortcutsWindowController

- (instancetype)initWithClient:(mozc::client::ClientInterface *)client {
  // Must be a non-activating floating NSPanel, exactly like the symbols
  // palette below: the IME bundle is LSBackgroundOnly/LSUIElement, and a
  // background-only process cannot order a plain NSWindow to the front --
  // |makeKeyAndOrderFront:| silently did nothing, so the button looked dead.
  NSPanel *window =
      [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 440, 400)
                                 styleMask:NSWindowStyleMaskTitled |
                                           NSWindowStyleMaskClosable |
                                           NSWindowStyleMaskResizable |
                                           NSWindowStyleMaskNonactivatingPanel
                                   backing:NSBackingStoreBuffered
                                     defer:YES];
  window.title = MarinaLocalizedString(@"MM.KeyboardShortcuts");
  window.releasedWhenClosed = NO;
  window.delegate = self;
  [window setFloatingPanel:YES];
  [window setLevel:NSPopUpMenuWindowLevel];
  [window setHidesOnDeactivate:NO];
  [window setBecomesKeyOnlyIfNeeded:YES];
  window.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces;
  [window center];

  self = [super initWithWindow:window];
  if (!self) return nil;

  [self loadDataWithClient:client];

  tabView_ = [[NSTabView alloc] initWithFrame:NSMakeRect(10, 10, 420, 380)];
  tabView_.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

  NSTabViewItem *scriptTab = [self makeTabWithTitle:MarinaLocalizedString(@"MM.Script")
                                        funcHeader:MarinaLocalizedString(@"MM.Function")
                                         keyHeader:MarinaLocalizedString(@"MM.Keys")];
  scriptTable_ = ((NSScrollView *)scriptTab.view).documentView;
  [tabView_ addTabViewItem:scriptTab];

  NSTabViewItem *compTab = [self makeTabWithTitle:MarinaLocalizedString(@"MM.Composition")
                                      funcHeader:MarinaLocalizedString(@"MM.Function")
                                       keyHeader:MarinaLocalizedString(@"MM.Keys")];
  compositionTable_ = ((NSScrollView *)compTab.view).documentView;
  [tabView_ addTabViewItem:compTab];

  NSTabViewItem *kaeritenTab = [self makeTabWithTitle:MarinaLocalizedString(@"MM.Kaeriten")
                                          funcHeader:MarinaLocalizedString(@"MM.Result")
                                           keyHeader:MarinaLocalizedString(@"MM.Input")];
  kaeritenTable_ = ((NSScrollView *)kaeritenTab.view).documentView;
  [tabView_ addTabViewItem:kaeritenTab];

  [window.contentView addSubview:tabView_];

  return self;
}

- (void)loadDataWithClient:(mozc::client::ClientInterface *)client {
  std::vector<ShortcutEntry> script_entries, comp_entries, kaeriten_entries;

  if (client) {
    mozc::config::Config config;
    if (client->GetConfig(&config)) {
      auto keymap = config.session_keymap();
      if (keymap == mozc::config::Config::CUSTOM &&
          config.has_custom_keymap_table() &&
          !config.custom_keymap_table().empty()) {
        ParseKeymapFromString(config.custom_keymap_table(), &script_entries,
                              &comp_entries);
      } else {
        std::string filename = KeymapFilenameFromSessionKeymap(keymap);
        if (!filename.empty())
          ParseKeymapTsv(GetKeymapPath(filename), &script_entries,
                         &comp_entries);
      }
    }
  }
  if (script_entries.empty() && comp_entries.empty())
    ParseKeymapTsv(GetKeymapPath("ms-ime.tsv"), &script_entries, &comp_entries);
  FillDefaultScriptShortcuts(&script_entries);
  FillDefaultCompositionShortcuts(&comp_entries);

  if (client) {
    mozc::config::Config marina_config;
    if (client->GetConfig(&marina_config)) {
      mozc::session::ApplyMarinaNumberRowShortcutEntries(
          marina_config, &script_entries, &comp_entries);
    }
  } else {
    mozc::session::ApplyMarinaNumberRowShortcutEntries(mozc::config::Config(),
                                                     &script_entries,
                                                     &comp_entries);
  }

  if (client) {
    mozc::config::Config kaeriten_config;
    const mozc::config::Config *kaeriten_config_ptr = nullptr;
    if (client->GetConfig(&kaeriten_config)) {
      kaeriten_config_ptr = &kaeriten_config;
    }
    LoadKaeritenEntries(kaeriten_config_ptr, &kaeriten_entries);
  } else {
    LoadKaeritenEntries(nullptr, &kaeriten_entries);
  }

  GroupShortcutsByCommand(script_entries, kScriptCommands, &scriptData_);
  GroupShortcutsByCommand(comp_entries, kCompositionCommands,
                          &compositionData_);
  GroupShortcutsByCommand(kaeriten_entries, nullptr, &kaeritenData_);
}

- (NSTabViewItem *)makeTabWithTitle:(NSString *)title
                        funcHeader:(NSString *)funcHeader
                         keyHeader:(NSString *)keyHeader {
  NSTabViewItem *item = [[NSTabViewItem alloc] initWithIdentifier:title];
  item.label = title;

  NSTableView *table = [[NSTableView alloc] initWithFrame:NSZeroRect];
  table.usesAlternatingRowBackgroundColors = YES;
  table.rowSizeStyle = NSTableViewRowSizeStyleDefault;

  NSTableColumn *funcCol =
      [[NSTableColumn alloc] initWithIdentifier:@"func"];
  funcCol.title = funcHeader;
  funcCol.width = 200;
  funcCol.resizingMask = NSTableColumnAutoresizingMask;
  [table addTableColumn:funcCol];

  NSTableColumn *keyCol =
      [[NSTableColumn alloc] initWithIdentifier:@"key"];
  keyCol.title = keyHeader;
  keyCol.width = 200;
  keyCol.resizingMask = NSTableColumnAutoresizingMask;
  [table addTableColumn:keyCol];

  table.dataSource = self;
  table.delegate = self;

  NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSZeroRect];
  scroll.documentView = table;
  scroll.hasVerticalScroller = YES;
  scroll.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

  item.view = scroll;
  return item;
}

#pragma mark - NSTableViewDataSource

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView {
  const std::vector<GroupedShortcutRow> *data = [self dataForTable:tableView];
  return data ? static_cast<NSInteger>(data->size()) : 0;
}

- (id)tableView:(NSTableView *)tableView
    objectValueForTableColumn:(NSTableColumn *)tableColumn
                          row:(NSInteger)row {
  const std::vector<GroupedShortcutRow> *data = [self dataForTable:tableView];
  if (!data || row < 0 || static_cast<size_t>(row) >= data->size()) return nil;
  const auto &entry = (*data)[static_cast<size_t>(row)];
  if ([tableColumn.identifier isEqualToString:@"func"]) {
    return [NSString stringWithUTF8String:entry.first.c_str()];
  }
  return [NSString stringWithUTF8String:entry.second.c_str()];
}

- (const std::vector<GroupedShortcutRow> *)dataForTable:
    (NSTableView *)tableView {
  if (tableView == scriptTable_) return &scriptData_;
  if (tableView == compositionTable_) return &compositionData_;
  if (tableView == kaeritenTable_) return &kaeritenData_;
  return nullptr;
}

- (void)windowWillClose:(NSNotification *)notification {
  if (g_shortcuts_window_controller == self) {
    g_shortcuts_window_controller = nil;
  }
}

@end

// ---------------------------------------------------------------------------
#pragma mark - MozcSymbolsPaletteWindowController

// Compact typesetter's-tray palette. Visual tokens mirror Preferences
// (config_dialog_theme.h) without depending on Qt.
namespace {

constexpr CGFloat kPaletteDefaultWidth = 560.0;
// Tab + hint + three cell rows + discreet footer (Auto Layout floor ~248).
constexpr CGFloat kPaletteDefaultHeight = 248.0;
constexpr CGFloat kPaletteMinWidth = 420.0;
constexpr CGFloat kPaletteMinHeight = 210.0;
constexpr CGFloat kSymbolCellSize = 46.0;
constexpr CGFloat kSymbolCellGap = 7.0;
// ~8% smaller than the first redesign pass; cells stay 46px for breathing room.
constexpr CGFloat kSymbolGlyphSize = 24.0;
constexpr CGFloat kSymbolCellRadius = 4.5;
constexpr NSInteger kSymbolColumns = 9;
constexpr NSInteger kSymbolsTabCount = 4;

NSColor *PaletteColor(uint8_t r, uint8_t g, uint8_t b, CGFloat a = 1.0) {
  return [NSColor colorWithCalibratedRed:r / 255.0
                                   green:g / 255.0
                                    blue:b / 255.0
                                   alpha:a];
}

bool PaletteIsDarkAppearance() {
  NSAppearanceName best = [NSApp.effectiveAppearance
      bestMatchFromAppearancesWithNames:@[
        NSAppearanceNameAqua, NSAppearanceNameDarkAqua
      ]];
  return [best isEqualToString:NSAppearanceNameDarkAqua];
}

struct PaletteChrome {
  NSColor *window;
  NSColor *glyph;       // character ink (blue-black, not saturated Prussian)
  NSColor *secondary;   // hints, footer, inactive tabs
  NSColor *tabActive;   // Prussian blue
  NSColor *tabHover;
  NSColor *cellBg;
  NSColor *cellBorder;
  NSColor *cellHover;
  NSColor *cellHoverBorder;
  NSColor *cellPressed;
  NSColor *focus;       // keyboard focus outline (Prussian, never system orange)
  NSColor *divider;
};

PaletteChrome MakePaletteChrome(bool dark) {
  PaletteChrome chrome;
  if (dark) {
    chrome.window = PaletteColor(0x18, 0x26, 0x3B);
    chrome.glyph = PaletteColor(0xF4, 0xF7, 0xFB);
    chrome.secondary = PaletteColor(0xA9, 0xB9, 0xCE);
    chrome.tabActive = PaletteColor(0x94, 0xB8, 0xE5);
    chrome.tabHover = PaletteColor(0xC5, 0xD7, 0xEE);
    chrome.cellBg = PaletteColor(0x22, 0x35, 0x50);
    chrome.cellBorder = PaletteColor(0x4A, 0x60, 0x7C);
    chrome.cellHover = PaletteColor(0x30, 0x4B, 0x6C);
    chrome.cellHoverBorder = PaletteColor(0x94, 0xB8, 0xE5);
    chrome.cellPressed = PaletteColor(0x1A, 0x2C, 0x44);
    chrome.focus = PaletteColor(0xC5, 0xD7, 0xEE);
    chrome.divider = PaletteColor(0x34, 0x47, 0x61);
  } else {
    // Light: white porcelain + pale blue-grey + deep Prussian (~#173B61).
    chrome.window = PaletteColor(0xFF, 0xFF, 0xFF);
    chrome.glyph = PaletteColor(0x25, 0x3D, 0x55);
    chrome.secondary = PaletteColor(0x61, 0x77, 0x8E);
    chrome.tabActive = PaletteColor(0x17, 0x3B, 0x61);
    chrome.tabHover = PaletteColor(0x40, 0x6B, 0x96);
    chrome.cellBg = PaletteColor(0xFA, 0xFC, 0xFE);
    chrome.cellBorder = PaletteColor(0xCE, 0xDA, 0xE6);
    chrome.cellHover = PaletteColor(0xEA, 0xF2, 0xF9);
    chrome.cellHoverBorder = PaletteColor(0x40, 0x6B, 0x96);
    chrome.cellPressed = PaletteColor(0xDC, 0xE9, 0xF5);
    chrome.focus = PaletteColor(0x17, 0x3B, 0x61);
    chrome.divider = PaletteColor(0xD8, 0xE2, 0xEC);
  }
  return chrome;
}

// Conventional Unicode names for the stock odoriji / symbols inventories.
// Do not invent scholarly names; omit unknowns (tooltips then show code points).
NSString *ConventionalSymbolName(NSString *symbol) {
  static NSDictionary<NSString *, NSString *> *names = @{
    @"々" : @"Ideographic iteration mark",
    @"ゝ" : @"Hiragana iteration mark",
    @"ゞ" : @"Hiragana voiced iteration mark",
    @"ヽ" : @"Katakana iteration mark",
    @"ヾ" : @"Katakana voiced iteration mark",
    @"〻" : @"Vertical ideographic iteration mark",
    @"〱" : @"Vertical kana repeat mark",
    @"〲" : @"Vertical kana repeat with voiced sound mark",
    @"〳" : @"Vertical kana repeat mark upper half",
    @"〴" : @"Vertical kana repeat with voiced sound mark upper half",
    @"〵" : @"Vertical kana repeat mark lower half",
    @"ヶ" : @"Katakana letter small ke",
    @"…" : @"Horizontal ellipsis",
    @"—" : @"Em dash",
    @"※" : @"Reference mark",
    @"×" : @"Multiplication sign",
  };
  return names[symbol];
}

NSString *TooltipForSymbol(NSString *symbol) {
  if (symbol.length == 0) return nil;
  NSMutableString *codes = [NSMutableString string];
  for (NSUInteger i = 0; i < symbol.length; ) {
    const unichar high = [symbol characterAtIndex:i];
    uint32_t cp = high;
    NSUInteger advance = 1;
    if (CFStringIsSurrogateHighCharacter(high) && i + 1 < symbol.length) {
      const unichar low = [symbol characterAtIndex:i + 1];
      if (CFStringIsSurrogateLowCharacter(low)) {
        cp = CFStringGetLongCharacterForSurrogatePair(high, low);
        advance = 2;
      }
    }
    if (codes.length > 0) [codes appendString:@" "];
    [codes appendFormat:@"U+%04X", cp];
    i += advance;
  }
  NSString *name = ConventionalSymbolName(symbol);
  if (name.length > 0) {
    return [NSString stringWithFormat:@"%@ — %@\n%@", symbol, name, codes];
  }
  return [NSString stringWithFormat:@"%@\n%@", symbol, codes];
}

}  // namespace

// Top-left origin so the character tray sits under the description, not
// stranded at the bottom of a taller scroll view (AppKit default is bottom-left).
@interface MozcFlippedView : NSView
@end
@implementation MozcFlippedView
- (BOOL)isFlipped {
  return YES;
}
@end

// Suppresses the system accent focus ring (often orange) and draws a
// Prussian-blue outline instead — active-tab underline stays separate.
@interface MozcPaletteTabButton : NSButton
@property(nonatomic, assign) BOOL isDark;
@property(nonatomic, assign) BOOL isActiveTab;
- (void)applyTabChrome;
@end

@implementation MozcPaletteTabButton {
  BOOL hovering_;
}

- (instancetype)initWithFrame:(NSRect)frame {
  self = [super initWithFrame:frame];
  if (!self) return nil;
  self.bezelStyle = NSBezelStyleInline;
  self.bordered = NO;
  self.wantsLayer = YES;
  self.layer.cornerRadius = 3.0;
  self.layer.borderWidth = 0.0;
  self.focusRingType = NSFocusRingTypeNone;
  NSTrackingArea *area = [[NSTrackingArea alloc]
      initWithRect:NSZeroRect
           options:(NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways |
                    NSTrackingInVisibleRect)
             owner:self
          userInfo:nil];
  [self addTrackingArea:area];
  return self;
}

- (void)mouseEntered:(NSEvent *)event {
  hovering_ = YES;
  [self applyTabChrome];
}

- (void)mouseExited:(NSEvent *)event {
  hovering_ = NO;
  [self applyTabChrome];
}

- (BOOL)becomeFirstResponder {
  const BOOL ok = [super becomeFirstResponder];
  [self applyTabChrome];
  return ok;
}

- (BOOL)resignFirstResponder {
  const BOOL ok = [super resignFirstResponder];
  [self applyTabChrome];
  return ok;
}

- (void)applyTabChrome {
  const PaletteChrome chrome = MakePaletteChrome(self.isDark);
  const BOOL focused = (self.window.firstResponder == self);
  NSColor *colour = chrome.secondary;
  if (self.isActiveTab) {
    colour = chrome.tabActive;
  } else if (hovering_) {
    colour = chrome.tabHover;
  }
  NSMutableAttributedString *title = [[NSMutableAttributedString alloc]
      initWithString:self.title ?: @""];
  [title addAttribute:NSForegroundColorAttributeName
                value:colour
                range:NSMakeRange(0, title.length)];
  [title addAttribute:NSFontAttributeName
                value:[NSFont systemFontOfSize:12.5
                                        weight:(self.isActiveTab
                                                    ? NSFontWeightSemibold
                                                    : NSFontWeightRegular)]
                range:NSMakeRange(0, title.length)];
  self.attributedTitle = title;
  // Focus ≠ active: thin Prussian outline only when this control is key.
  if (focused) {
    self.layer.borderWidth = 1.5;
    self.layer.borderColor = chrome.focus.CGColor;
    self.layer.backgroundColor = [NSColor clearColor].CGColor;
  } else {
    self.layer.borderWidth = 0.0;
    self.layer.borderColor = [NSColor clearColor].CGColor;
    self.layer.backgroundColor = [NSColor clearColor].CGColor;
  }
}

@end

@interface MozcSymbolCellButton : NSButton
@property(nonatomic, assign) BOOL isDark;
- (void)applyPaletteChrome;
@end

@implementation MozcSymbolCellButton {
  BOOL hovering_;
}

+ (NSFont *)symbolFont {
  NSFont *font = [NSFont fontWithName:@"Hiragino Mincho ProN" size:kSymbolGlyphSize];
  if (font == nil) {
    font = [NSFont fontWithName:@"HiraginoSans-W3" size:kSymbolGlyphSize];
  }
  if (font == nil) {
    font = [NSFont systemFontOfSize:kSymbolGlyphSize];
  }
  return font;
}

- (instancetype)initWithFrame:(NSRect)frame {
  self = [super initWithFrame:frame];
  if (!self) return nil;
  self.bezelStyle = NSBezelStyleShadowlessSquare;
  self.bordered = NO;
  self.wantsLayer = YES;
  self.layer.cornerRadius = kSymbolCellRadius;
  self.layer.borderWidth = 1.0;
  self.font = [MozcSymbolCellButton symbolFont];
  self.focusRingType = NSFocusRingTypeNone;
  NSTrackingArea *area = [[NSTrackingArea alloc]
      initWithRect:NSZeroRect
           options:(NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways |
                    NSTrackingInVisibleRect)
             owner:self
          userInfo:nil];
  [self addTrackingArea:area];
  return self;
}

- (void)mouseEntered:(NSEvent *)event {
  hovering_ = YES;
  [self applyPaletteChrome];
}

- (void)mouseExited:(NSEvent *)event {
  hovering_ = NO;
  [self applyPaletteChrome];
}

- (void)mouseDown:(NSEvent *)event {
  [self applyPaletteChrome];
  [super mouseDown:event];
  [self applyPaletteChrome];
}

- (BOOL)becomeFirstResponder {
  const BOOL ok = [super becomeFirstResponder];
  [self applyPaletteChrome];
  return ok;
}

- (BOOL)resignFirstResponder {
  const BOOL ok = [super resignFirstResponder];
  [self applyPaletteChrome];
  return ok;
}

- (void)applyPaletteChrome {
  const PaletteChrome chrome = MakePaletteChrome(self.isDark);
  const BOOL focused = (self.window.firstResponder == self);
  const BOOL pressed = self.highlighted;
  NSColor *bg = chrome.cellBg;
  NSColor *border = chrome.cellBorder;
  CGFloat borderWidth = 1.0;
  if (pressed) {
    bg = chrome.cellPressed;
    border = chrome.cellHoverBorder;
  } else if (focused) {
    bg = hovering_ ? chrome.cellHover : chrome.cellBg;
    border = chrome.focus;
    borderWidth = 1.5;
  } else if (hovering_) {
    bg = chrome.cellHover;
    border = chrome.cellHoverBorder;
  }
  self.layer.backgroundColor = bg.CGColor;
  self.layer.borderColor = border.CGColor;
  self.layer.borderWidth = borderWidth;
  NSMutableAttributedString *title = [[NSMutableAttributedString alloc]
      initWithString:self.title ?: @""];
  [title addAttribute:NSForegroundColorAttributeName
                value:chrome.glyph
                range:NSMakeRange(0, title.length)];
  [title addAttribute:NSFontAttributeName
                value:self.font
                range:NSMakeRange(0, title.length)];
  NSMutableParagraphStyle *style = [[NSMutableParagraphStyle alloc] init];
  style.alignment = NSTextAlignmentCenter;
  [title addAttribute:NSParagraphStyleAttributeName
                value:style
                range:NSMakeRange(0, title.length)];
  self.attributedTitle = title;
}

@end

@interface MozcSymbolsPaletteWindowController : NSWindowController
    <NSWindowDelegate> {
  mozc::client::ClientInterface *client_;
  __weak id<ControllerCallback> callbackController_;
  NSStackView *rootStack_;
  NSStackView *tabStrip_;
  NSMutableArray<MozcPaletteTabButton *> *tabButtons_;
  NSView *tabUnderline_;
  NSLayoutConstraint *tabUnderlineLeading_;
  NSLayoutConstraint *tabUnderlineWidth_;
  NSTextField *descriptionLabel_;
  NSScrollView *scrollView_;
  NSView *gridHost_;
  NSButton *pinCheckbox_;
  NSView *footerDivider_;
  NSInteger selectedTab_;
  std::vector<std::string> odorijiSymbols_;
  std::vector<std::string> kaeritenSymbols_;
  std::vector<std::string> generalSymbols_;
  std::vector<std::string> userSymbols_;
}
- (instancetype)initWithClient:(mozc::client::ClientInterface *)client
                    controller:(id<ControllerCallback>)controller;
@end

@implementation MozcSymbolsPaletteWindowController

- (instancetype)initWithClient:(mozc::client::ClientInterface *)client
                    controller:(id<ControllerCallback>)controller {
  NSPanel *window = [[NSPanel alloc]
      initWithContentRect:NSMakeRect(0, 0, kPaletteDefaultWidth,
                                     kPaletteDefaultHeight)
                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                          NSWindowStyleMaskResizable |
                          NSWindowStyleMaskNonactivatingPanel
                  backing:NSBackingStoreBuffered
                    defer:YES];
  window.title = MarinaLocalizedString(@"MM.SymbolsPalette");
  window.releasedWhenClosed = NO;
  [window setFloatingPanel:YES];
  [window setLevel:NSPopUpMenuWindowLevel];
  [window setHidesOnDeactivate:NO];
  [window setBecomesKeyOnlyIfNeeded:YES];
  window.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces;
  window.minSize = NSMakeSize(kPaletteMinWidth, kPaletteMinHeight);
  [window center];

  self = [super initWithWindow:window];
  if (!self) return nil;
  client_ = client;
  callbackController_ = controller;
  window.delegate = self;
  selectedTab_ = kSymbolsTabOdoriji;
  tabButtons_ = [NSMutableArray array];

  odorijiSymbols_ = BuildDefaultOdorijiSymbols();
  kaeritenSymbols_ = BuildKaeritenSymbols(client_);
  generalSymbols_ = BuildDefaultGeneralSymbols();
  userSymbols_ = LoadUserSymbolsFromFile();

  [self buildChrome];
  [self applyAppearance];

  NSInteger saved_tab = [self loadLastTabPreference];
  if (saved_tab < 0 || saved_tab >= kSymbolsTabCount) {
    saved_tab = kSymbolsTabOdoriji;
  }
  [self selectTab:saved_tab notify:NO];

  [NSApp addObserver:self
          forKeyPath:@"effectiveAppearance"
             options:NSKeyValueObservingOptionNew
             context:nullptr];

  return self;
}

- (void)dealloc {
  @try {
    [NSApp removeObserver:self forKeyPath:@"effectiveAppearance"];
  } @catch (NSException *) {
  }
}

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary *)change
                       context:(void *)context {
  if ([keyPath isEqualToString:@"effectiveAppearance"]) {
    dispatch_async(dispatch_get_main_queue(), ^{
      [self applyAppearance];
    });
  }
}

- (void)buildChrome {
  NSView *content = self.window.contentView;
  content.wantsLayer = YES;

  rootStack_ = [[NSStackView alloc] initWithFrame:NSZeroRect];
  rootStack_.orientation = NSUserInterfaceLayoutOrientationVertical;
  rootStack_.alignment = NSLayoutAttributeLeading;
  rootStack_.distribution = NSStackViewDistributionFill;
  rootStack_.spacing = 6.0;
  rootStack_.edgeInsets = NSEdgeInsetsMake(8, 12, 8, 12);
  rootStack_.translatesAutoresizingMaskIntoConstraints = NO;
  [content addSubview:rootStack_];
  [NSLayoutConstraint activateConstraints:@[
    [rootStack_.leadingAnchor constraintEqualToAnchor:content.leadingAnchor],
    [rootStack_.trailingAnchor constraintEqualToAnchor:content.trailingAnchor],
    [rootStack_.topAnchor constraintEqualToAnchor:content.topAnchor],
    [rootStack_.bottomAnchor constraintEqualToAnchor:content.bottomAnchor],
  ]];

  NSView *tabRow = [[NSView alloc] initWithFrame:NSZeroRect];
  tabRow.translatesAutoresizingMaskIntoConstraints = NO;
  tabStrip_ = [[NSStackView alloc] initWithFrame:NSZeroRect];
  tabStrip_.orientation = NSUserInterfaceLayoutOrientationHorizontal;
  tabStrip_.alignment = NSLayoutAttributeCenterY;
  tabStrip_.spacing = 16.0;
  tabStrip_.translatesAutoresizingMaskIntoConstraints = NO;
  [tabRow addSubview:tabStrip_];

  tabUnderline_ = [[NSView alloc] initWithFrame:NSZeroRect];
  tabUnderline_.wantsLayer = YES;
  tabUnderline_.translatesAutoresizingMaskIntoConstraints = NO;
  [tabRow addSubview:tabUnderline_];

  NSArray<NSString *> *tabKeys =
      @[ @"MM.OdorijiTab", @"MM.Kaeriten", @"MM.Symbols", @"MM.User" ];
  for (NSInteger i = 0; i < tabKeys.count; ++i) {
    MozcPaletteTabButton *tab =
        [[MozcPaletteTabButton alloc] initWithFrame:NSZeroRect];
    tab.title = MarinaLocalizedString(tabKeys[i]);
    tab.target = self;
    tab.action = @selector(tabClicked:);
    tab.tag = i;
    [tabButtons_ addObject:tab];
    [tabStrip_ addArrangedSubview:tab];
  }

  [NSLayoutConstraint activateConstraints:@[
    [tabRow.heightAnchor constraintEqualToConstant:26.0],
    [tabStrip_.leadingAnchor constraintEqualToAnchor:tabRow.leadingAnchor],
    [tabStrip_.topAnchor constraintEqualToAnchor:tabRow.topAnchor],
    [tabStrip_.bottomAnchor constraintEqualToAnchor:tabRow.bottomAnchor
                                           constant:-3.0],
    [tabUnderline_.heightAnchor constraintEqualToConstant:1.5],
    [tabUnderline_.bottomAnchor constraintEqualToAnchor:tabRow.bottomAnchor],
  ]];
  tabUnderlineLeading_ =
      [tabUnderline_.leadingAnchor constraintEqualToAnchor:tabRow.leadingAnchor];
  tabUnderlineWidth_ = [tabUnderline_.widthAnchor constraintEqualToConstant:40];
  tabUnderlineLeading_.active = YES;
  tabUnderlineWidth_.active = YES;
  [rootStack_ addArrangedSubview:tabRow];
  [tabRow.widthAnchor constraintEqualToAnchor:rootStack_.widthAnchor
                                     constant:-24.0]
      .active = YES;

  descriptionLabel_ = [NSTextField labelWithString:@""];
  // Visually subordinate to the 12.5pt tabs.
  descriptionLabel_.font = [NSFont systemFontOfSize:11.0 weight:NSFontWeightRegular];
  descriptionLabel_.lineBreakMode = NSLineBreakByTruncatingTail;
  descriptionLabel_.maximumNumberOfLines = 1;
  descriptionLabel_.translatesAutoresizingMaskIntoConstraints = NO;
  [rootStack_ addArrangedSubview:descriptionLabel_];
  [descriptionLabel_.widthAnchor constraintEqualToAnchor:rootStack_.widthAnchor
                                                constant:-24.0]
      .active = YES;

  scrollView_ = [[NSScrollView alloc] initWithFrame:NSZeroRect];
  scrollView_.hasVerticalScroller = YES;
  scrollView_.hasHorizontalScroller = NO;
  scrollView_.borderType = NSNoBorder;
  scrollView_.drawsBackground = NO;
  scrollView_.translatesAutoresizingMaskIntoConstraints = NO;
  // Three cell rows (46+7+46+7+46) without reserving a blank fourth band.
  [scrollView_.heightAnchor constraintGreaterThanOrEqualToConstant:152.0]
      .active = YES;
  [rootStack_ addArrangedSubview:scrollView_];
  [scrollView_.widthAnchor constraintEqualToAnchor:rootStack_.widthAnchor
                                          constant:-24.0]
      .active = YES;
  [scrollView_ setContentHuggingPriority:1
                          forOrientation:NSLayoutConstraintOrientationVertical];
  [scrollView_
      setContentCompressionResistancePriority:1
                               forOrientation:
                                   NSLayoutConstraintOrientationVertical];

  footerDivider_ = [[NSView alloc] initWithFrame:NSZeroRect];
  footerDivider_.wantsLayer = YES;
  footerDivider_.translatesAutoresizingMaskIntoConstraints = NO;
  [footerDivider_.heightAnchor constraintEqualToConstant:1.0].active = YES;
  [rootStack_ addArrangedSubview:footerDivider_];
  [footerDivider_.widthAnchor constraintEqualToAnchor:rootStack_.widthAnchor
                                             constant:-24.0]
      .active = YES;

  pinCheckbox_ = [[NSButton alloc] initWithFrame:NSZeroRect];
  pinCheckbox_.buttonType = NSButtonTypeSwitch;
  pinCheckbox_.title = MarinaLocalizedString(@"MM.PinPalette");
  pinCheckbox_.controlSize = NSControlSizeSmall;
  pinCheckbox_.font = [NSFont systemFontOfSize:11.0 weight:NSFontWeightRegular];
  pinCheckbox_.target = self;
  pinCheckbox_.action = @selector(pinCheckboxChanged:);
  pinCheckbox_.state = [self loadPinnedPreference] ? NSControlStateValueOn
                                                   : NSControlStateValueOff;
  pinCheckbox_.focusRingType = NSFocusRingTypeNone;
  if ([pinCheckbox_ respondsToSelector:@selector(setContentTintColor:)]) {
    pinCheckbox_.contentTintColor =
        MakePaletteChrome(PaletteIsDarkAppearance()).tabActive;
  }
  [rootStack_ addArrangedSubview:pinCheckbox_];
}

- (NSString *)hintKeyForTab:(NSInteger)tab {
  switch (tab) {
    case 0:
      return @"MM.OdorijiHint";
    case 1:
      return @"MM.KaeritenHint";
    case 2:
      return @"MM.SymbolsHint";
    case 3:
      return @"MM.UserSymbolsHint";
    default:
      return nil;
  }
}

- (const std::vector<std::string> &)symbolsForTab:(NSInteger)tab {
  switch (tab) {
    case 0:
      return odorijiSymbols_;
    case 1:
      return kaeritenSymbols_;
    case 2:
      return generalSymbols_;
    case 3:
      return userSymbols_;
    default:
      return odorijiSymbols_;
  }
}

- (void)rebuildGrid {
  const bool dark = PaletteIsDarkAppearance();
  const std::vector<std::string> &symbols = [self symbolsForTab:selectedTab_];
  const BOOL isOdoriji = (selectedTab_ == kSymbolsTabOdoriji);

  const CGFloat hostWidth =
      MAX(scrollView_.contentSize.width, kPaletteMinWidth - 40.0);
  NSInteger columns = kSymbolColumns;
  if (hostWidth > 0) {
    columns = MAX(
        1, static_cast<NSInteger>((hostWidth + kSymbolCellGap) /
                                  (kSymbolCellSize + kSymbolCellGap)));
  }
  const NSInteger count = static_cast<NSInteger>(symbols.size());
  const NSInteger rows =
      count == 0 ? 1 : (count + columns - 1) / columns;
  const CGFloat hostHeight =
      rows * kSymbolCellSize + MAX(0, rows - 1) * kSymbolCellGap + 4.0;

  gridHost_ =
      [[MozcFlippedView alloc] initWithFrame:NSMakeRect(0, 0, hostWidth, hostHeight)];
  NSInteger index = 0;
  for (const std::string &value : symbols) {
    if (value.empty()) continue;
    const NSInteger col = index % columns;
    const NSInteger row = index / columns;
    const CGFloat x = col * (kSymbolCellSize + kSymbolCellGap);
    const CGFloat y = row * (kSymbolCellSize + kSymbolCellGap);
    MozcSymbolCellButton *button = [[MozcSymbolCellButton alloc]
        initWithFrame:NSMakeRect(x, y, kSymbolCellSize, kSymbolCellSize)];
    NSString *text = [NSString stringWithUTF8String:value.c_str()];
    button.title = text;
    button.toolTip = TooltipForSymbol(text);
    button.target = self;
    button.action = @selector(symbolClicked:);
    button.isDark = dark;
    if (isOdoriji) {
      button.identifier = @"odorijiSymbolButton";
      button.tag = index;
    }
    [button applyPaletteChrome];
    [gridHost_ addSubview:button];
    ++index;
  }

  if (count == 0 && selectedTab_ == 3) {
    NSTextField *empty = [NSTextField
        labelWithString:MarinaLocalizedString(@"MM.UserSymbolsEmpty")];
    empty.font = [NSFont systemFontOfSize:11.0];
    empty.textColor = MakePaletteChrome(dark).secondary;
    empty.frame = NSMakeRect(0, 4, hostWidth, 20);
    [gridHost_ addSubview:empty];
  }

  scrollView_.documentView = gridHost_;
  [scrollView_.contentView scrollToPoint:NSMakePoint(0, 0)];
  [scrollView_ reflectScrolledClipView:scrollView_.contentView];
}

- (void)updateTabChrome {
  const bool dark = PaletteIsDarkAppearance();
  const PaletteChrome chrome = MakePaletteChrome(dark);
  for (MozcPaletteTabButton *tab in tabButtons_) {
    tab.isDark = dark;
    tab.isActiveTab = (tab.tag == selectedTab_);
    [tab applyTabChrome];
  }
  tabUnderline_.layer.backgroundColor = chrome.tabActive.CGColor;
  if (selectedTab_ >= 0 &&
      selectedTab_ < static_cast<NSInteger>(tabButtons_.count)) {
    MozcPaletteTabButton *active = tabButtons_[selectedTab_];
    [tabStrip_ layoutSubtreeIfNeeded];
    const NSRect frame =
        [active convertRect:active.bounds toView:tabUnderline_.superview];
    tabUnderlineLeading_.constant = NSMinX(frame);
    tabUnderlineWidth_.constant = NSWidth(frame);
  }
}

- (void)applyAppearance {
  const bool dark = PaletteIsDarkAppearance();
  const PaletteChrome chrome = MakePaletteChrome(dark);
  self.window.backgroundColor = chrome.window;
  self.window.contentView.layer.backgroundColor = chrome.window.CGColor;
  descriptionLabel_.textColor = chrome.secondary;
  descriptionLabel_.font =
      [NSFont systemFontOfSize:11.0 weight:NSFontWeightRegular];
  footerDivider_.layer.backgroundColor = chrome.divider.CGColor;
  NSMutableAttributedString *pinTitle = [[NSMutableAttributedString alloc]
      initWithString:MarinaLocalizedString(@"MM.PinPalette")];
  [pinTitle addAttribute:NSForegroundColorAttributeName
                   value:chrome.secondary
                   range:NSMakeRange(0, pinTitle.length)];
  [pinTitle addAttribute:NSFontAttributeName
                   value:[NSFont systemFontOfSize:11.0 weight:NSFontWeightRegular]
                   range:NSMakeRange(0, pinTitle.length)];
  pinCheckbox_.attributedTitle = pinTitle;
  if ([pinCheckbox_ respondsToSelector:@selector(setContentTintColor:)]) {
    pinCheckbox_.contentTintColor = chrome.tabActive;
  }
  [self updateTabChrome];
  [self rebuildGrid];
}

- (void)windowDidResize:(NSNotification *)notification {
  [self rebuildGrid];
  [self updateTabChrome];
}

- (void)selectTab:(NSInteger)tab notify:(BOOL)notify {
  if (tab < 0 || tab >= kSymbolsTabCount) return;
  selectedTab_ = tab;
  NSString *hintKey = [self hintKeyForTab:tab];
  descriptionLabel_.stringValue =
      hintKey ? MarinaLocalizedString(hintKey) : @"";
  [self updateTabChrome];
  [self rebuildGrid];
  if (notify) {
    [self saveLastTabPreference:tab];
  }
}

- (void)tabClicked:(NSButton *)sender {
  [self selectTab:sender.tag notify:YES];
  // Mouse activation should not leave a focus ring on the tab; keyboard
  // users still reach tabs via Tab / arrow keys (custom Prussian outline).
  [self.window makeFirstResponder:nil];
}

- (std::vector<std::string>)loadUserSymbols {
  return LoadUserSymbolsFromFile();
}

- (NSString *)prefsPath {
  return ToolbarPrefsPath();
}

- (NSMutableDictionary *)mutablePrefs {
  NSDictionary *existing =
      [NSDictionary dictionaryWithContentsOfFile:[self prefsPath]];
  return existing ? [existing mutableCopy] : [NSMutableDictionary dictionary];
}

- (bool)loadPinnedPreference {
  NSDictionary *dict =
      [NSDictionary dictionaryWithContentsOfFile:[self prefsPath]];
  return dict[kPrefsSymbolsPinnedKey] != nil &&
         [dict[kPrefsSymbolsPinnedKey] boolValue];
}

- (NSInteger)loadLastTabPreference {
  NSDictionary *dict =
      [NSDictionary dictionaryWithContentsOfFile:[self prefsPath]];
  if (dict[kPrefsSymbolsLastTabKey] == nil) {
    return kSymbolsTabOdoriji;
  }
  return [dict[kPrefsSymbolsLastTabKey] integerValue];
}

- (void)savePinnedPreference:(bool)pinned {
  NSMutableDictionary *dict = [self mutablePrefs];
  dict[kPrefsSymbolsPinnedKey] = @(pinned);
  [dict writeToFile:[self prefsPath] atomically:YES];
}

- (void)saveLastTabPreference:(NSInteger)index {
  NSMutableDictionary *dict = [self mutablePrefs];
  dict[kPrefsSymbolsLastTabKey] = @(index);
  [dict writeToFile:[self prefsPath] atomically:YES];
}

- (void)pinCheckboxChanged:(id)sender {
  [self savePinnedPreference:(pinCheckbox_.state == NSControlStateValueOn)];
}

- (void)symbolClicked:(NSButton *)sender {
  NSString *text = sender.title;
  if (text.length == 0) return;
  // Prefer the controller for the field that currently has focus, so a pinned
  // palette can insert into a different caret position or a different app
  // after the user clicks away. Fall back to the controller captured at open.
  id<ControllerCallback> controller =
      g_active_controller ? g_active_controller : callbackController_;
  if (!controller) return;

  // Odoriji tab: insert the clicked mark and make it the Ctrl+Shift+1 default.
  if ([sender.identifier isEqualToString:@"odorijiSymbolButton"]) {
    mozc::commands::SessionCommand insert;
    insert.set_type(mozc::commands::SessionCommand::INSERT_ODORIJI_DEFAULT);
    insert.set_id(static_cast<int32_t>(sender.tag));
    [controller sendCommand:insert];
  } else {
    mozc::commands::Output output;
    output.mutable_result()->set_value(text.UTF8String);
    [controller outputResult:output];
  }

  if (pinCheckbox_.state != NSControlStateValueOn) {
    [self dismissPalette];
  }
}

- (void)cancelOperation:(id)sender {
  [self dismissPalette];
}

- (void)windowDidResignKey:(NSNotification *)notification {
  if (pinCheckbox_.state == NSControlStateValueOn) {
    return;
  }
  [self dismissPalette];
}

- (void)dismissPalette {
  if (!g_symbols_palette_visible) return;
  g_symbols_palette_visible = false;
  [[self window] orderOut:nil];
  g_toolbar_reshow_after_palette_close = true;
  mozc::mac::MozcToolbarReshowAfterPaletteClose();
}

- (void)windowWillClose:(NSNotification *)notification {
  g_symbols_palette_visible = false;
}

- (void)forceKeepOpenForPreview {
  pinCheckbox_.state = NSControlStateValueOn;
}

@end

namespace mozc {
namespace mac {

void MozcSymbolsPalettePreviewShow() {
  static MozcSymbolsPaletteWindowController *preview_ctrl = nil;
  if (preview_ctrl == nil) {
    preview_ctrl =
        [[MozcSymbolsPaletteWindowController alloc] initWithClient:nullptr
                                                        controller:nil];
  }
  [preview_ctrl forceKeepOpenForPreview];
  g_symbols_palette_visible = true;
  NSWindow *window = [preview_ctrl window];
  [window center];
  [window makeKeyAndOrderFront:nil];
}

}  // namespace mac
}  // namespace mozc

// ---------------------------------------------------------------------------
#pragma mark - MozcToolbarView

@interface MozcToolbarView : NSView {
  NSView *backgroundView_;
  NSImageView *logoView_;
  NSButton *modeButton_;
  NSButton *tradButton_;
  NSButton *liveButton_;
  NSButton *symbolsButton_;
  NSButton *dictButton_;
  NSButton *composerButton_;

  mozc::client::ClientInterface *client_;
  mozc::commands::CompositionMode currentMode_;
  bool useTraditionalKanji_;
  bool useLiveConversion_;
  bool leftShiftDirectLock_;
  bool isDarkMode_;

  NSPoint dragStartPoint_;
  NSPoint windowStartOrigin_;
}

- (instancetype)initWithClient:(mozc::client::ClientInterface *)client
                          mode:(mozc::commands::CompositionMode)mode;
- (void)updateMode:(mozc::commands::CompositionMode)mode
            locked:(bool)locked;
- (void)updateTraditionalKanji:(bool)useTrad;
- (void)updateLiveConversion:(bool)useLive;
- (void)setClient:(mozc::client::ClientInterface *)client;
- (void)applyToolbarChrome;
- (mozc::client::ClientInterface *)mozcClient;
- (mozc::commands::CompositionMode)compositionMode;
- (bool)leftShiftDirectLocked;
- (NSPoint)loadPosition;
- (void)clampWindowToVisibleScreen;

@end

@implementation MozcToolbarView

- (instancetype)initWithClient:(mozc::client::ClientInterface *)client
                          mode:(mozc::commands::CompositionMode)mode {
  CGFloat totalWidth = kLogoWidth + kButtonWidth * 6 + 10;
  NSRect frame = NSMakeRect(0, 0, totalWidth, kToolbarHeight);
  self = [super initWithFrame:frame];
  if (!self) return nil;

  self.wantsLayer = YES;

  client_ = client;
  currentMode_ = mode;
  useTraditionalKanji_ = false;
  useLiveConversion_ = false;  // config default; real value arrives with the first update
  leftShiftDirectLock_ = false;
  isDarkMode_ = [self isDarkAppearance];

  // Solid chrome (matches Linux GTK toolbar: white / dark gray, not vibrancy).
  backgroundView_ = [[NSView alloc] initWithFrame:self.bounds];
  backgroundView_.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
  backgroundView_.wantsLayer = YES;
  backgroundView_.layer.cornerRadius = kCornerRadius;
  backgroundView_.layer.masksToBounds = YES;
  [self addSubview:backgroundView_];
  [self applyToolbarChrome];

  CGFloat x = 4;
  CGFloat iconY = (kToolbarHeight - kIconSize) / 2.0;

  // Logo
  logoView_ = [[NSImageView alloc]
      initWithFrame:NSMakeRect(x, iconY, kLogoWidth, kIconSize)];
  logoView_.imageScaling = NSImageScaleProportionallyUpOrDown;
  logoView_.imageAlignment = NSImageAlignCenter;
  logoView_.image = [self loadLogoSvg:isDarkMode_ ? @"logo_long_dark" : @"logo_long_light"];
  [self addSubview:logoView_];
  x += kLogoWidth + 2;

  // Mode indicator
  modeButton_ = [self makeButtonAt:x action:@selector(modeClicked:)];
  modeButton_.image = [self modeIconForMode:currentMode_ locked:leftShiftDirectLock_];
  [self addSubview:modeButton_];
  x += kButtonWidth;

  // Shin/Kyu toggle
  tradButton_ = [self makeButtonAt:x action:@selector(tradClicked:)];
  tradButton_.image = [self loadSvg:isDarkMode_ ? @"toolbar_shin_dark" : @"toolbar_shin_light"];
  [self addSubview:tradButton_];
  x += kButtonWidth;

  // Live conversion toggle
  liveButton_ = [self makeButtonAt:x action:@selector(liveClicked:)];
  liveButton_.image =
      [self loadSvg:isDarkMode_ ? @"toolbar_live_off_dark" : @"toolbar_live_off_light"];
  liveButton_.toolTip = MarinaLocalizedString(@"MM.LiveConversion");
  [self addSubview:liveButton_];
  x += kButtonWidth;

  // Symbols palette
  symbolsButton_ = [self makeButtonAt:x action:@selector(symbolsClicked:)];
  symbolsButton_.image =
      [self loadSvg:isDarkMode_ ? @"toolbar_symbols_dark" : @"toolbar_symbols_light"];
  [self addSubview:symbolsButton_];
  x += kButtonWidth;

  // Dict
  dictButton_ = [self makeButtonAt:x action:@selector(dictClicked:)];
  dictButton_.image = [self loadSvg:isDarkMode_ ? @"toolbar_dict_dark" : @"toolbar_dict_light"];
  [self addSubview:dictButton_];
  // Right-click menu for dictionary tool
  NSMenu *dictMenu = [[NSMenu alloc] initWithTitle:MarinaLocalizedString(@"MM.Dict")];
  NSMenuItem *dictToolItem =
      [dictMenu addItemWithTitle:MarinaLocalizedString(@"MM.DictionaryTool")
                          action:@selector(dictToolClicked:)
                   keyEquivalent:@""];
  dictToolItem.target = self;
  dictButton_.menu = dictMenu;
  x += kButtonWidth;

  // Character composer (runs CharacterComposer.app; see WatchCharacterComposer)
  composerButton_ = [self makeButtonAt:x action:@selector(composerClicked:)];
  composerButton_.image =
      [self loadSvg:isDarkMode_ ? @"toolbar_composer_dark" : @"toolbar_composer_light"];
  composerButton_.toolTip = MarinaLocalizedString(@"MM.CharacterComposer");
  [self addSubview:composerButton_];
  x += kButtonWidth;

  // Shin/kyu state is applied via MozcToolbarUpdate after the controller is active.

  return self;
}

#pragma mark - Button Factory

- (NSButton *)makeButtonAt:(CGFloat)x action:(SEL)action {
  CGFloat iconY = (kToolbarHeight - kIconSize) / 2.0;
  NSButton *btn = [[NSButton alloc]
      initWithFrame:NSMakeRect(x, iconY, kButtonWidth, kIconSize)];
  btn.bezelStyle = NSBezelStyleAccessoryBarAction;
  btn.bordered = NO;
  btn.imagePosition = NSImageOnly;
  btn.imageScaling = NSImageScaleProportionallyDown;
  btn.target = self;
  btn.action = action;
  return btn;
}

#pragma mark - Icon Loading

- (NSImage *)loadSvgFromToolbarIcons:(NSString *)name {
  NSString *resourceDir = [NSString
      stringWithUTF8String:mozc::MacUtil::GetServerDirectory().c_str()];
  NSString *path = [resourceDir stringByAppendingPathComponent:
                        [NSString stringWithFormat:@"toolbar_icons/%@.svg", name]];
  return [[NSImage alloc] initWithContentsOfFile:path];
}

- (NSImage *)loadSvg:(NSString *)name {
  NSImage *image = [self loadSvgFromToolbarIcons:name];
  if (image) {
    image.size = NSMakeSize(kIconSize, kIconSize);
  }
  return image;
}

// Wide marinaMoji logo: fit within kLogoWidth x kIconSize, preserving aspect ratio
// (matches Linux GTK toolbar: LoadSvgIcon(..., kToolbarLogoWidth, kIconSize)).
- (NSImage *)loadLogoSvg:(NSString *)name {
  NSImage *image = [self loadSvgFromToolbarIcons:name];
  if (!image) return nil;

  NSSize natural = image.size;
  if (natural.width <= 0 || natural.height <= 0) {
    image.size = NSMakeSize(kLogoWidth, kIconSize);
    return image;
  }

  const CGFloat scale =
      MIN(kLogoWidth / natural.width, kIconSize / natural.height);
  image.size =
      NSMakeSize(natural.width * scale, natural.height * scale);
  return image;
}

- (NSImage *)modeIconForMode:(mozc::commands::CompositionMode)mode
                      locked:(bool)locked {
  NSString *name;
  bool dark = isDarkMode_;
  switch (mode) {
    case mozc::commands::DIRECT:
      if (locked) {
        name = dark ? @"toolbar_roman_dark_lock" : @"toolbar_roman_light_lock";
      } else {
        name = dark ? @"toolbar_roman_dark" : @"toolbar_roman_light";
      }
      break;
    case mozc::commands::HALF_ASCII:
      name = dark ? @"toolbar_roma_half_dark" : @"toolbar_roma_half_light";
      break;
    case mozc::commands::HIRAGANA:
      if (locked) {
        name = dark ? @"toolbar_hira_dark_lock" : @"toolbar_hira_light_lock";
      } else {
        name = dark ? @"toolbar_hira_dark" : @"toolbar_hira_light";
      }
      break;
    case mozc::commands::FULL_KATAKANA:
      if (locked) {
        name = dark ? @"toolbar_kata_dark_lock" : @"toolbar_kata_light_lock";
      } else {
        name = dark ? @"toolbar_kata_dark" : @"toolbar_kata_light";
      }
      break;
    case mozc::commands::MANYOSHU:
      if (locked) {
        name = dark ? @"toolbar_kata_dark_lock" : @"toolbar_kata_light_lock";
      } else {
        name = dark ? @"toolbar_kata_dark" : @"toolbar_kata_light";
      }
      break;
    case mozc::commands::FULL_ASCII:
      name = dark ? @"toolbar_roma_full_dark" : @"toolbar_roma_full_light";
      break;
    case mozc::commands::HALF_KATAKANA:
      name = dark ? @"toolbar_kata_half_dark" : @"toolbar_kata_half_light";
      break;
    default:
      name = dark ? @"toolbar_hira_dark" : @"toolbar_hira_light";
      break;
  }
  return [self loadSvg:name];
}

- (BOOL)isDarkAppearance {
  if (@available(macOS 10.14, *)) {
    NSAppearanceName best = [NSApp.effectiveAppearance
        bestMatchFromAppearancesWithNames:@[
          NSAppearanceNameAqua, NSAppearanceNameDarkAqua
        ]];
    return [best isEqualToString:NSAppearanceNameDarkAqua];
  }
  return NO;
}

- (NSColor *)toolbarBackgroundColor {
  // Same palette as unix/ibus/mozc_toolbar.cc EnsureToolbarCSS (alpha 1.0 here).
  if (isDarkMode_) {
    return [NSColor colorWithCalibratedRed:32.0 / 255.0
                                       green:35.0 / 255.0
                                        blue:40.0 / 255.0
                                       alpha:1.0];
  }
  return [NSColor whiteColor];
}

- (void)applyToolbarChrome {
  if (!backgroundView_.layer) {
    return;
  }
  backgroundView_.layer.backgroundColor = [self toolbarBackgroundColor].CGColor;
  if (isDarkMode_) {
    backgroundView_.layer.borderColor =
        [[NSColor colorWithCalibratedWhite:1.0 alpha:0.12] CGColor];
  } else {
    backgroundView_.layer.borderColor =
        [[NSColor colorWithCalibratedWhite:0.0 alpha:0.08] CGColor];
  }
  backgroundView_.layer.borderWidth = 1.0;
}

#pragma mark - State Updates

- (void)updateMode:(mozc::commands::CompositionMode)mode locked:(bool)locked {
  currentMode_ = mode;
  leftShiftDirectLock_ = locked;
  modeButton_.image = [self modeIconForMode:mode locked:locked];
}

- (void)updateTraditionalKanji:(bool)useTrad {
  useTraditionalKanji_ = useTrad;
  [self updateTradIcon];
}

- (void)updateLiveConversion:(bool)useLive {
  useLiveConversion_ = useLive;
  [self updateLiveIcon];
}

- (void)updateTradIcon {
  NSString *iconName =
      useTraditionalKanji_
          ? (isDarkMode_ ? @"toolbar_kyu_dark" : @"toolbar_kyu_light")
          : (isDarkMode_ ? @"toolbar_shin_dark" : @"toolbar_shin_light");
  NSImage *img = [self loadSvg:iconName];
  tradButton_.image = img;
}

- (void)updateLiveIcon {
  NSString *iconName =
      useLiveConversion_
          ? (isDarkMode_ ? @"toolbar_live_on_dark" : @"toolbar_live_on_light")
          : (isDarkMode_ ? @"toolbar_live_off_dark" : @"toolbar_live_off_light");
  liveButton_.image = [self loadSvg:iconName];
}

- (void)setClient:(mozc::client::ClientInterface *)client {
  client_ = client;
}

- (mozc::client::ClientInterface *)mozcClient {
  return client_;
}

- (mozc::commands::CompositionMode)compositionMode {
  return currentMode_;
}

- (bool)leftShiftDirectLocked {
  return leftShiftDirectLock_;
}

#pragma mark - Button Actions

- (void)modeClicked:(id)sender {
  NSMenu *menu = [[NSMenu alloc] initWithTitle:MarinaLocalizedString(@"MM.Mode")];

  struct ModeEntry {
    NSString *title;
    mozc::commands::CompositionMode mode;
  };
  ModeEntry modes[] = {
      {MarinaLocalizedString(@"MM.Hiragana"), mozc::commands::HIRAGANA},
      {MarinaLocalizedString(@"MM.KatakanaManyoshu"), mozc::commands::MANYOSHU},
      {MarinaLocalizedString(@"MM.HalfWidthKatakana"), mozc::commands::HALF_KATAKANA},
      {MarinaLocalizedString(@"MM.FullWidthRoman"), mozc::commands::FULL_ASCII},
      {MarinaLocalizedString(@"MM.HalfWidthRoman"), mozc::commands::HALF_ASCII},
      {MarinaLocalizedString(@"MM.DirectInputMode"), mozc::commands::DIRECT},
  };

  for (const auto &entry : modes) {
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:entry.title
                                                 action:@selector(modeSelected:)
                                          keyEquivalent:@""];
    item.target = self;
    item.tag = static_cast<NSInteger>(entry.mode);
    if (entry.mode == currentMode_) {
      item.state = NSControlStateValueOn;
    }
    [menu addItem:item];
  }

  [menu popUpMenuPositioningItem:nil
                      atLocation:NSMakePoint(0, modeButton_.frame.size.height)
                          inView:modeButton_];
}

- (void)modeSelected:(NSMenuItem *)item {
  auto mode =
      static_cast<mozc::commands::CompositionMode>(item.tag);

  mozc::commands::SessionCommand command;
  if (mode == mozc::commands::DIRECT) {
    command.set_type(mozc::commands::SessionCommand::TURN_OFF_IME);
  } else {
    command.set_type(mozc::commands::SessionCommand::SWITCH_COMPOSITION_MODE);
    command.set_composition_mode(mode);
  }

  id<ControllerCallback> controller = g_active_controller;
  if (controller) {
    [controller sendCommand:command];
    return;
  }

  if (!client_) return;
  mozc::commands::Output output;
  if (!client_->SendCommand(command, &output)) {
    return;
  }
  currentMode_ = mode;
  modeButton_.image = [self modeIconForMode:mode locked:leftShiftDirectLock_];
}

- (void)tradClicked:(id)sender {
  (void)sender;
  // Icon updates from server output only (same as Linux toolbar), via
  // MozcToolbarUpdate or the fallback SendCommand path below.
  mozc::commands::SessionCommand command;
  command.set_type(
      mozc::commands::SessionCommand::TOGGLE_TRADITIONAL_KANJI);

  id<ControllerCallback> controller = g_active_controller;
  if (controller) {
    [controller sendCommand:command];
    return;
  }

  if (!client_) return;
  mozc::commands::Output output;
  if (client_->SendCommand(command, &output)) {
    if (output.has_config()) {
      useTraditionalKanji_ = output.config().use_traditional_kanji();
      [self updateTradIcon];
    }
  }
}

- (void)liveClicked:(id)sender {
  (void)sender;
  mozc::commands::SessionCommand command;
  command.set_type(mozc::commands::SessionCommand::TOGGLE_LIVE_CONVERSION);

  id<ControllerCallback> controller = g_active_controller;
  if (controller) {
    [controller sendCommand:command];
    return;
  }

  if (!client_) return;
  mozc::commands::Output output;
  if (client_->SendCommand(command, &output)) {
    if (output.has_config()) {
      useLiveConversion_ = output.config().use_live_conversion();
      [self updateLiveIcon];
    }
  }
}

- (void)dictClicked:(id)sender {
  if (!client_) return;
  id<ControllerCallback> controller = g_active_controller;
  if (controller) {
    [controller launchWordRegisterDialog];
    return;
  }
  // No active IMK controller: same prefill path as Ctrl+Shift+0 via session.
  mozc::commands::KeyEvent key;
  key.set_key_code('0');
  key.add_modifier_keys(mozc::commands::KeyEvent::CTRL);
  key.add_modifier_keys(mozc::commands::KeyEvent::SHIFT);
  mozc::commands::Output launch_output;
  if (client_->SendKey(key, &launch_output) &&
      launch_output.has_launch_tool_mode()) {
    mozc::mac::MozcImkNotifyToolLaunchStarting();
    client_->LaunchToolWithProtoBuf(launch_output);
    return;
  }
  mozc::mac::MozcImkNotifyToolLaunchStarting();
  client_->LaunchTool("word_register_dialog", "");
}

- (void)composerClicked:(id)sender {
  (void)sender;
  id<ControllerCallback> controller = g_active_controller;
  if (controller) {
    [controller flushCompositionForToolLaunch:controller];
  }
  // A stale result from an earlier run must not be inserted by this one.
  if (!g_character_composer_watching) {
    mozc::FileUtil::Unlink(mozc::composer_ids::DefaultResultFilePath()).IgnoreError();
  }
  NSString *directory = [NSString stringWithUTF8String:mozc::MacUtil::GetServerDirectory().c_str()];
  NSURL *appURL =
      [NSURL fileURLWithPath:[directory stringByAppendingPathComponent:@"CharacterComposer.app"]];
  NSWorkspaceOpenConfiguration *configuration = [NSWorkspaceOpenConfiguration configuration];
  configuration.activates = YES;
  [[NSWorkspace sharedWorkspace]
      openApplicationAtURL:appURL
             configuration:configuration
         completionHandler:^(NSRunningApplication *app, NSError *error) {
           if (app == nil) {
             LOG(ERROR) << "Cannot start the character composer: "
                        << (error ? error.localizedDescription.UTF8String : "unknown error");
             return;
           }
           dispatch_async(dispatch_get_main_queue(), ^{
             WatchCharacterComposer(app);
           });
         }];
}

- (void)dictToolClicked:(id)sender {
  if (!client_) return;
  id<ControllerCallback> controller = g_active_controller;
  if (controller) {
    [controller flushCompositionForToolLaunch:controller];
  }
  mozc::mac::MozcImkNotifyToolLaunchStarting();
  client_->LaunchTool("dictionary_tool", "");
}

- (void)symbolsClicked:(id)sender {
  if (!client_) return;
  mozc::mac::MozcImkNotifyToolLaunchStarting();
  MozcSymbolsPaletteWindowController *ctrl =
      [[MozcSymbolsPaletteWindowController alloc] initWithClient:client_
                                                      controller:g_active_controller];
  objc_setAssociatedObject(self, "symbolsPaletteCtrl", ctrl,
                           OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  g_symbols_palette_visible = true;
  [ctrl showWindow:nil];
  [ctrl.window makeKeyAndOrderFront:nil];
  [ctrl.window orderFrontRegardless];
}

#pragma mark - Dragging (from any non-button area)

- (void)mouseDown:(NSEvent *)event {
  dragStartPoint_ = [NSEvent mouseLocation];
  windowStartOrigin_ = self.window.frame.origin;
}

- (void)mouseDragged:(NSEvent *)event {
  NSPoint current = [NSEvent mouseLocation];
  CGFloat dx = current.x - dragStartPoint_.x;
  CGFloat dy = current.y - dragStartPoint_.y;
  NSPoint newOrigin =
      NSMakePoint(windowStartOrigin_.x + dx, windowStartOrigin_.y + dy);
  [self.window setFrameOrigin:newOrigin];
}

- (void)mouseUp:(NSEvent *)event {
  [self savePosition];
}

#pragma mark - Position Persistence

- (NSString *)prefsPath {
  return ToolbarPrefsPath();
}

- (NSMutableDictionary *)mutablePrefs {
  NSDictionary *existing =
      [NSDictionary dictionaryWithContentsOfFile:[self prefsPath]];
  return existing ? [existing mutableCopy] : [NSMutableDictionary dictionary];
}

- (NSSize)toolbarSize {
  NSSize size = self.frame.size;
  if (size.width < 1 || size.height < 1) {
    size = NSMakeSize(kLogoWidth + kButtonWidth * 6 + 10, kToolbarHeight);
  }
  return size;
}

- (NSPoint)defaultOrigin {
  NSRect screen = [NSScreen mainScreen].visibleFrame;
  NSSize size = [self toolbarSize];
  return NSMakePoint(NSMaxX(screen) - size.width - kToolbarMargin,
                     NSMinY(screen) + kToolbarMargin);
}

// Saved coordinates from a now-unplugged display would park the toolbar where
// it cannot be seen or dragged. Keep the saved origin only if a grabbable
// strip still intersects some screen's work area (same idea as Windows).
- (NSPoint)clampedOrigin:(NSPoint)origin {
  NSSize size = [self toolbarSize];
  const CGFloat minVisibleX = MIN(size.width, (CGFloat)48);
  const CGFloat minVisibleY = MIN(size.height, (CGFloat)16);
  NSRect windowRect = NSMakeRect(origin.x, origin.y, size.width, size.height);
  for (NSScreen *screen in [NSScreen screens]) {
    NSRect inter = NSIntersectionRect(windowRect, screen.visibleFrame);
    if (inter.size.width >= minVisibleX && inter.size.height >= minVisibleY) {
      return origin;
    }
  }
  return [self defaultOrigin];
}

- (void)clampWindowToVisibleScreen {
  if (!self.window) {
    return;
  }
  NSPoint clamped = [self clampedOrigin:self.window.frame.origin];
  if (!NSEqualPoints(clamped, self.window.frame.origin)) {
    [self.window setFrameOrigin:clamped];
  }
}

- (void)savePosition {
  NSPoint origin = [self clampedOrigin:self.window.frame.origin];
  NSMutableDictionary *dict = [self mutablePrefs];
  dict[@"x"] = @(origin.x);
  dict[@"y"] = @(origin.y);
  [dict writeToFile:[self prefsPath] atomically:YES];
}

- (NSPoint)loadPosition {
  NSDictionary *dict =
      [NSDictionary dictionaryWithContentsOfFile:[self prefsPath]];
  if (dict && dict[@"x"] != nil && dict[@"y"] != nil) {
    return [self clampedOrigin:NSMakePoint([dict[@"x"] doubleValue],
                                           [dict[@"y"] doubleValue])];
  }
  return [self defaultOrigin];
}

#pragma mark - Dark Mode Observation

- (void)viewDidChangeEffectiveAppearance {
  bool newDark = [self isDarkAppearance];
  if (newDark == isDarkMode_) return;
  isDarkMode_ = newDark;

  [self applyToolbarChrome];
  logoView_.image = [self loadLogoSvg:isDarkMode_ ? @"logo_long_dark" : @"logo_long_light"];
  modeButton_.image = [self modeIconForMode:currentMode_ locked:leftShiftDirectLock_];
  [self updateTradIcon];
  [self updateLiveIcon];
  symbolsButton_.image =
      [self loadSvg:isDarkMode_ ? @"toolbar_symbols_dark" : @"toolbar_symbols_light"];
  dictButton_.image = [self loadSvg:isDarkMode_ ? @"toolbar_dict_dark" : @"toolbar_dict_light"];
  composerButton_.image =
      [self loadSvg:isDarkMode_ ? @"toolbar_composer_dark" : @"toolbar_composer_light"];
}

@end

// ---------------------------------------------------------------------------
#pragma mark - Module-level state

static NSPanel *g_toolbar_panel = nil;
static MozcToolbarView *g_toolbar_view = nil;

static void EnsureToolbarPrefsDirectory() {
  NSString *path = ToolbarPrefsPath();
  [[NSFileManager defaultManager] createDirectoryAtPath:[path stringByDeletingLastPathComponent]
                            withIntermediateDirectories:YES
                                             attributes:nil
                                                  error:nil];
}

static void EnsureToolbar(mozc::client::ClientInterface *client,
                          mozc::commands::CompositionMode mode) {
  if (g_toolbar_panel) {
    [g_toolbar_view setClient:client];
    [g_toolbar_view clampWindowToVisibleScreen];
    return;
  }

  g_toolbar_view = [[MozcToolbarView alloc] initWithClient:client mode:mode];
  mozc::config::Config config;
  if (client->GetConfig(&config)) {
    [g_toolbar_view updateTraditionalKanji:config.use_traditional_kanji()];
    [g_toolbar_view updateLiveConversion:config.use_live_conversion()];
  }

  NSRect contentRect = g_toolbar_view.bounds;
  const NSUInteger styleMask =
      NSWindowStyleMaskBorderless | NSWindowStyleMaskNonactivatingPanel;
  g_toolbar_panel =
      [[NSPanel alloc] initWithContentRect:contentRect
                                 styleMask:styleMask
                                   backing:NSBackingStoreBuffered
                                     defer:YES];
  g_toolbar_panel.backgroundColor = NSColor.clearColor;
  g_toolbar_panel.opaque = NO;
  g_toolbar_panel.hasShadow = YES;
  [g_toolbar_panel setFloatingPanel:YES];
  [g_toolbar_panel setLevel:NSPopUpMenuWindowLevel];
  [g_toolbar_panel setBecomesKeyOnlyIfNeeded:YES];
  [g_toolbar_panel setHidesOnDeactivate:NO];
  [g_toolbar_panel setReleasedWhenClosed:NO];
  [g_toolbar_panel setContentView:g_toolbar_view];

  NSPoint pos = [g_toolbar_view loadPosition];
  [g_toolbar_panel setFrameOrigin:pos];
}

// ---------------------------------------------------------------------------
#pragma mark - Public C++ API

namespace mozc {
namespace mac {

static void RunOnMainThread(void (^block)(void)) {
  if ([NSThread isMainThread]) {
    block();
  } else {
    dispatch_sync(dispatch_get_main_queue(), block);
  }
}

bool MozcToolbarNeedsReshowAfterPaletteClose() {
  return g_toolbar_reshow_after_palette_close;
}

void MozcToolbarReshowAfterPaletteClose() {
  dispatch_async(dispatch_get_main_queue(), ^{
    dispatch_async(dispatch_get_main_queue(), ^{
      if (!g_active_controller || g_symbols_palette_visible || !g_toolbar_view) {
        return;
      }
      MozcToolbarShow([g_toolbar_view mozcClient], [g_toolbar_view compositionMode]);
      g_toolbar_reshow_after_palette_close = false;
    });
  });
}

void MozcToolbarShow(client::ClientInterface *client,
                     commands::CompositionMode mode) {
  if (!MozcToolbarLoadVisiblePreference()) {
    return;
  }
  if (!client) {
    return;
  }
  RunOnMainThread(^{
    if (!g_active_controller) {
      return;
    }
    EnsureToolbar(client, mode);
    [g_toolbar_view clampWindowToVisibleScreen];
    const bool locked =
        g_toolbar_view ? [g_toolbar_view leftShiftDirectLocked] : false;
    [g_toolbar_view updateMode:mode locked:locked];
    [g_toolbar_panel orderFront:nil];
    g_toolbar_reshow_after_palette_close = false;
  });
}

void MozcToolbarHide() {
  RunOnMainThread(^{
    if (g_symbols_palette_visible) {
      return;
    }
    if (g_toolbar_reshow_after_palette_close) {
      return;
    }
    [g_toolbar_panel orderOut:nil];
  });
}

void MozcToolbarSetActiveController(void *controller) {
  // Called from activateServer: on the main thread.
  g_active_controller = (__bridge id<ControllerCallback>)controller;
}

void MozcToolbarClearActiveControllerIfMatches(void *controller) {
  if (g_active_controller != (__bridge id<ControllerCallback>)controller) {
    return;
  }
  g_active_controller = nullptr;
  MozcToolbarHide();
}

void MozcImkNotifyToolLaunchStarting() {
  g_suppress_set_value_direct_until =
      [[NSDate date] timeIntervalSinceReferenceDate] + 5.0;
}

bool MozcImkShouldSuppressSetValueDirect() {
  return [[NSDate date] timeIntervalSinceReferenceDate] <
         g_suppress_set_value_direct_until;
}

void MozcToolbarUpdate(const commands::Output &output,
                       commands::CompositionMode mode) {
  commands::CompositionMode display_mode = mode;
  if (output.has_status()) {
    display_mode = output.status().activated() ? output.status().mode()
                                               : commands::DIRECT;
  }

  bool has_trad =
      (output.has_status() && output.status().has_use_traditional_kanji()) ||
      (output.has_config() && output.config().has_use_traditional_kanji());
  bool use_trad =
      output.has_status() && output.status().has_use_traditional_kanji()
          ? output.status().use_traditional_kanji()
          : (output.has_config() ? output.config().use_traditional_kanji()
                                 : false);
  bool has_live =
      (output.has_status() && output.status().has_use_live_conversion()) ||
      (output.has_config() && output.config().has_use_live_conversion());
  bool use_live =
      output.has_status() && output.status().has_use_live_conversion()
          ? output.status().use_live_conversion()
          : (output.has_config() ? output.config().use_live_conversion()
                                 : false);
  bool locked = output.has_status() && output.status().left_shift_direct_lock();

  RunOnMainThread(^{
    if (!g_toolbar_view) return;
    [g_toolbar_view updateMode:display_mode locked:locked];
    if (has_trad) {
      [g_toolbar_view updateTraditionalKanji:use_trad];
    }
    if (has_live) {
      [g_toolbar_view updateLiveConversion:use_live];
    }
  });
}

void MozcShortcutsWindowShow(client::ClientInterface *client) {
  if (client == nullptr) return;
  MozcImkNotifyToolLaunchStarting();
  if (g_shortcuts_window_controller == nil ||
      !g_shortcuts_window_controller.window.isVisible) {
    g_shortcuts_window_controller =
        [[MozcShortcutsWindowController alloc] initWithClient:client];
  }
  [g_shortcuts_window_controller showWindow:nil];
  [g_shortcuts_window_controller.window makeKeyAndOrderFront:nil];
}

bool MozcToolbarLoadVisiblePreference() {
  NSDictionary *dict =
      [NSDictionary dictionaryWithContentsOfFile:ToolbarPrefsPath()];
  if (!dict || dict[@"toolbar_visible"] == nil) {
    return true;
  }
  return [dict[@"toolbar_visible"] boolValue];
}

void MozcToolbarSaveVisiblePreference(bool visible) {
  EnsureToolbarPrefsDirectory();
  NSMutableDictionary *dict =
      [[NSDictionary dictionaryWithContentsOfFile:ToolbarPrefsPath()] mutableCopy];
  if (!dict) {
    dict = [NSMutableDictionary dictionary];
  }
  dict[@"toolbar_visible"] = @(visible);
  [dict writeToFile:ToolbarPrefsPath() atomically:YES];
}

}  // namespace mac
}  // namespace mozc
