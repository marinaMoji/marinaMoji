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
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/file_util.h"
#include "base/mac/mac_util.h"
#include "composer/kaeriten_table_util.h"
#include "session/marina_number_row_bindings_util.h"
#include "base/system_util.h"
#include "client/client_interface.h"
#include "mac/common.h"
#include "mac/marina_localized_string.h"
#include "mac/mozc_toolbar.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"

// Declared before @implementation MozcToolbarView.
static __weak id<ControllerCallback> g_active_controller = nil;
static NSTimeInterval g_suppress_set_value_direct_until = 0;
static bool g_symbols_palette_visible = false;
static bool g_toolbar_reshow_after_palette_close = false;

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
    "ToggleManyoshuHiragana", "ToggleHiraganaKatakana", "ConvertToFullKatakana",
    "ConvertToHalfWidth",
    "ConvertToFullAlphanumeric", "ConvertToHiragana", nullptr};
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
      {"Ctrl 0", "LaunchWordRegisterDialog"},
      {"Ctrl k", "SegmentWidthShrink"},
      {"Shift Left", "SegmentWidthShrink"},
      {"Ctrl l", "SegmentWidthExpand"},
      {"Shift Right", "SegmentWidthExpand"},
  };
  for (const auto &p : kDefault) composition->emplace_back(p.first, p.second);
}

void FillDefaultKaeritenShortcuts(std::vector<ShortcutEntry> *kaeriten) {
  if (!kaeriten->empty()) return;
  const std::pair<const char *, const char *> kDefault[] = {
      {";te", "\xe3\x86\x9d"}, {";ti", "\xe3\x86\x9e"},
      {";ji", "\xe3\x86\x9f"}, {";r", "\xe3\x86\x91"},
      {";1", "\xe3\x86\x92"},  {";2", "\xe3\x86\x93"},
      {";3", "\xe3\x86\x94"},  {";4", "\xe3\x86\x95"},
      {";u", "\xe3\x86\x96"},  {";m", "\xe3\x86\x97"},
      {";d", "\xe3\x86\x98"},  {";k", "\xe3\x86\x99"},
      {";o", "\xe3\x86\x9a"},  {";h", "\xe3\x86\x9b"},
      {";t", "\xe3\x86\x9c"},  {";.", "\xe3\x83\xbb"},
      {";,", "\xe3\x80\x81"},
  };
  for (const auto &p : kDefault) kaeriten->emplace_back(p.first, p.second);
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
  for (const auto &pair : pairs) {
    kaeriten->emplace_back(pair.first, pair.second);
  }
  if (kaeriten->empty()) {
    FillDefaultKaeritenShortcuts(kaeriten);
  }
}

std::string GetKeymapPath(const std::string &filename);

std::vector<std::string> BuildDefaultOdorijiSymbols() {
  return {"々", "ゝ", "ゞ", "ヽ", "ヾ", "〻", "〱", "〲"};
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
          "▲", "▽", "…", "—"};
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
    <NSTabViewDelegate, NSTableViewDataSource, NSTableViewDelegate> {
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
  NSWindow *window =
      [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 440, 400)
                                  styleMask:NSWindowStyleMaskTitled |
                                            NSWindowStyleMaskClosable |
                                            NSWindowStyleMaskResizable
                                    backing:NSBackingStoreBuffered
                                      defer:YES];
  window.title = MarinaLocalizedString(@"MM.KeyboardShortcuts");
  window.releasedWhenClosed = NO;
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

@end

// ---------------------------------------------------------------------------
#pragma mark - MozcSymbolsPaletteWindowController

@interface MozcSymbolsPaletteWindowController : NSWindowController
    <NSTabViewDelegate, NSWindowDelegate> {
  mozc::client::ClientInterface *client_;
  __weak id<ControllerCallback> callbackController_;
  NSTabView *tabView_;
  NSButton *pinCheckbox_;
  NSStackView *odorijiStack_;
  NSStackView *kaeritenStack_;
  NSStackView *symbolsStack_;
  NSStackView *userStack_;
}
- (instancetype)initWithClient:(mozc::client::ClientInterface *)client
                    controller:(id<ControllerCallback>)controller;
@end

@implementation MozcSymbolsPaletteWindowController

- (instancetype)initWithClient:(mozc::client::ClientInterface *)client
                    controller:(id<ControllerCallback>)controller {
  NSPanel *window =
      [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 520, 380)
                                 styleMask:NSWindowStyleMaskTitled |
                                           NSWindowStyleMaskClosable |
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
  [window center];

  self = [super initWithWindow:window];
  if (!self) return nil;
  client_ = client;
  callbackController_ = controller;
  window.delegate = self;

  NSView *content = window.contentView;
  tabView_ = [[NSTabView alloc] initWithFrame:NSMakeRect(10, 50, 500, 320)];
  tabView_.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
  tabView_.delegate = self;
  [content addSubview:tabView_];

  pinCheckbox_ =
      [[NSButton alloc] initWithFrame:NSMakeRect(12, 14, 200, 24)];
  pinCheckbox_.buttonType = NSButtonTypeSwitch;
  pinCheckbox_.title = MarinaLocalizedString(@"MM.PinPalette");
  pinCheckbox_.target = self;
  pinCheckbox_.action = @selector(pinCheckboxChanged:);
  pinCheckbox_.state = [self loadPinnedPreference] ? NSControlStateValueOn
                                                   : NSControlStateValueOff;
  [content addSubview:pinCheckbox_];

  [self addSymbolsTabWithIdentifier:@"MM.Odoriji"
                              title:MarinaLocalizedString(@"MM.Odoriji")
                            symbols:BuildDefaultOdorijiSymbols()
                        hintMessage:MarinaLocalizedString(@"MM.OdorijiHint")
                            outView:&odorijiStack_];
  [self addSymbolsTabWithIdentifier:@"MM.Kaeriten"
                              title:MarinaLocalizedString(@"MM.Kaeriten")
                            symbols:BuildKaeritenSymbols(client_)
                        hintMessage:MarinaLocalizedString(@"MM.KaeritenHint")
                            outView:&kaeritenStack_];
  [self addSymbolsTabWithIdentifier:@"MM.Symbols"
                              title:MarinaLocalizedString(@"MM.Symbols")
                            symbols:BuildDefaultGeneralSymbols()
                        hintMessage:nil
                            outView:&symbolsStack_];
  [self addSymbolsTabWithIdentifier:@"MM.User"
                              title:MarinaLocalizedString(@"MM.User")
                            symbols:[self loadUserSymbols]
                        hintMessage:MarinaLocalizedString(@"MM.UserSymbolsHint")
                            outView:&userStack_];

  NSInteger saved_tab = [self loadLastTabPreference];
  if (saved_tab >= 0 && saved_tab < tabView_.numberOfTabViewItems) {
    [tabView_ selectTabViewItemAtIndex:saved_tab];
  } else {
    [tabView_ selectTabViewItemAtIndex:kSymbolsTabOdoriji];
  }

  return self;
}

- (void)addSymbolsTabWithIdentifier:(NSString *)identifier
                              title:(NSString *)title
                            symbols:(const std::vector<std::string> &)symbols
                        hintMessage:(NSString *)hintMessage
                            outView:(NSStackView * __strong *)outView {
  NSTabViewItem *item = [[NSTabViewItem alloc] initWithIdentifier:identifier];
  item.label = title;

  NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSZeroRect];
  scroll.hasVerticalScroller = YES;
  scroll.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

  NSView *container = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 460, 300)];
  NSStackView *stack = [[NSStackView alloc] initWithFrame:NSMakeRect(0, 0, 460, 300)];
  stack.orientation = NSUserInterfaceLayoutOrientationVertical;
  stack.alignment = NSLayoutAttributeLeading;
  stack.spacing = 8.0;
  stack.edgeInsets = NSEdgeInsetsMake(8, 8, 8, 8);
  stack.translatesAutoresizingMaskIntoConstraints = NO;
  [container addSubview:stack];
  [NSLayoutConstraint activateConstraints:@[
    [stack.leadingAnchor constraintEqualToAnchor:container.leadingAnchor],
    [stack.trailingAnchor constraintEqualToAnchor:container.trailingAnchor],
    [stack.topAnchor constraintEqualToAnchor:container.topAnchor]
  ]];

  if (hintMessage.length > 0) {
    NSTextField *hint = [NSTextField labelWithString:hintMessage];
    hint.lineBreakMode = NSLineBreakByWordWrapping;
    hint.maximumNumberOfLines = 2;
    [stack addArrangedSubview:hint];
  }

  NSStackView *row = nil;
  constexpr NSInteger kColumns = 8;
  NSInteger col = 0;
  NSInteger symbolIndex = 0;
  for (const std::string &value : symbols) {
    if (value.empty()) continue;
    if (row == nil || col == 0) {
      row = [[NSStackView alloc] initWithFrame:NSZeroRect];
      row.orientation = NSUserInterfaceLayoutOrientationHorizontal;
      row.alignment = NSLayoutAttributeCenterY;
      row.spacing = 6.0;
      [stack addArrangedSubview:row];
    }
    NSString *text = [NSString stringWithUTF8String:value.c_str()];
    NSButton *button = [NSButton buttonWithTitle:text
                                          target:self
                                          action:@selector(symbolClicked:)];
    if ([identifier isEqualToString:@"MM.Odoriji"]) {
      button.identifier = @"odorijiSymbolButton";
      button.tag = symbolIndex;
    }
    button.bezelStyle = NSBezelStyleRounded;
    button.controlSize = NSControlSizeRegular;
    button.font = [NSFont systemFontOfSize:18.0];
    [button.widthAnchor constraintGreaterThanOrEqualToConstant:40.0].active = YES;
    [button.heightAnchor constraintEqualToConstant:32.0].active = YES;
    button.contentTintColor = nil;
    [row addArrangedSubview:button];
    col = (col + 1) % kColumns;
    ++symbolIndex;
  }

  [stack addArrangedSubview:[NSView new]];
  scroll.documentView = container;
  item.view = scroll;
  [tabView_ addTabViewItem:item];
  if (outView) {
    *outView = stack;
  }
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

- (void)tabView:(NSTabView *)tabView didSelectTabViewItem:(NSTabViewItem *)item {
  NSInteger index = [tabView indexOfTabViewItem:item];
  [self saveLastTabPreference:index];
}

- (void)symbolClicked:(NSButton *)sender {
  NSString *text = sender.title;
  if (text.length == 0) return;
  id<ControllerCallback> controller = callbackController_ ? callbackController_ : g_active_controller;
  if (!controller) return;

  // Odoriji tab uses the same session flow as the main odoriji palette so the
  // selected symbol becomes the new default odoriji.
  if ([sender.identifier isEqualToString:@"odorijiSymbolButton"]) {
    mozc::commands::SessionCommand show;
    show.set_type(mozc::commands::SessionCommand::SHOW_ODORIJI_PALETTE);
    [controller sendCommand:show];

    mozc::commands::SessionCommand submit;
    submit.set_type(mozc::commands::SessionCommand::SUBMIT_CANDIDATE);
    submit.set_id(static_cast<int32_t>(sender.tag));
    [controller sendCommand:submit];
  } else {
    mozc::commands::Output output;
    output.mutable_result()->set_value(text.UTF8String);
    [controller outputResult:output];
  }

  if (pinCheckbox_.state != NSControlStateValueOn) {
    g_symbols_palette_visible = false;
    [[self window] orderOut:nil];
    g_toolbar_reshow_after_palette_close = true;
    mozc::mac::MozcToolbarReshowAfterPaletteClose();
  }
}

- (void)windowWillClose:(NSNotification *)notification {
  g_symbols_palette_visible = false;
}

@end

// ---------------------------------------------------------------------------
#pragma mark - MozcToolbarView

@interface MozcToolbarView : NSView {
  NSView *backgroundView_;
  NSImageView *logoView_;
  NSButton *modeButton_;
  NSButton *tradButton_;
  NSButton *symbolsButton_;
  NSButton *dictButton_;
  NSButton *shortcutsButton_;

  mozc::client::ClientInterface *client_;
  mozc::commands::CompositionMode currentMode_;
  bool useTraditionalKanji_;
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
- (void)setClient:(mozc::client::ClientInterface *)client;
- (void)applyToolbarChrome;
- (mozc::client::ClientInterface *)mozcClient;
- (mozc::commands::CompositionMode)compositionMode;
- (bool)leftShiftDirectLocked;

@end

@implementation MozcToolbarView

- (instancetype)initWithClient:(mozc::client::ClientInterface *)client
                          mode:(mozc::commands::CompositionMode)mode {
  CGFloat totalWidth = kLogoWidth + kButtonWidth * 5 + 10;
  NSRect frame = NSMakeRect(0, 0, totalWidth, kToolbarHeight);
  self = [super initWithFrame:frame];
  if (!self) return nil;

  self.wantsLayer = YES;

  client_ = client;
  currentMode_ = mode;
  useTraditionalKanji_ = false;
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

  // Shortcuts
  shortcutsButton_ = [self makeButtonAt:x action:@selector(shortcutsClicked:)];
  shortcutsButton_.image = [self loadSvg:isDarkMode_ ? @"toolbar_shortcuts_dark" : @"toolbar_shortcuts_light"];
  [self addSubview:shortcutsButton_];

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

- (void)updateTradIcon {
  NSString *iconName =
      useTraditionalKanji_
          ? (isDarkMode_ ? @"toolbar_kyu_dark" : @"toolbar_kyu_light")
          : (isDarkMode_ ? @"toolbar_shin_dark" : @"toolbar_shin_light");
  NSImage *img = [self loadSvg:iconName];
  tradButton_.image = img;
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

- (void)dictToolClicked:(id)sender {
  if (!client_) return;
  id<ControllerCallback> controller = g_active_controller;
  if (controller) {
    [controller flushCompositionForToolLaunch:controller];
  }
  mozc::mac::MozcImkNotifyToolLaunchStarting();
  client_->LaunchTool("dictionary_tool", "");
}

- (void)shortcutsClicked:(id)sender {
  if (!client_) return;
  mozc::mac::MozcImkNotifyToolLaunchStarting();
  MozcShortcutsWindowController *ctrl =
      [[MozcShortcutsWindowController alloc] initWithClient:client_];
  [ctrl showWindow:nil];
  // Keep a strong reference so the window stays alive.
  objc_setAssociatedObject(self, "shortcutsCtrl", ctrl,
                           OBJC_ASSOCIATION_RETAIN_NONATOMIC);
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

- (void)savePosition {
  NSPoint origin = self.window.frame.origin;
  NSMutableDictionary *dict = [self mutablePrefs];
  dict[@"x"] = @(origin.x);
  dict[@"y"] = @(origin.y);
  [dict writeToFile:[self prefsPath] atomically:YES];
}

- (NSPoint)loadPosition {
  NSDictionary *dict =
      [NSDictionary dictionaryWithContentsOfFile:[self prefsPath]];
  if (dict && dict[@"x"] != nil && dict[@"y"] != nil) {
    return NSMakePoint([dict[@"x"] doubleValue], [dict[@"y"] doubleValue]);
  }
  // Default: bottom-right of main screen
  NSRect screen = [NSScreen mainScreen].visibleFrame;
  return NSMakePoint(
      NSMaxX(screen) - self.frame.size.width - kToolbarMargin,
      NSMinY(screen) + kToolbarMargin);
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
  symbolsButton_.image =
      [self loadSvg:isDarkMode_ ? @"toolbar_symbols_dark" : @"toolbar_symbols_light"];
  dictButton_.image = [self loadSvg:isDarkMode_ ? @"toolbar_dict_dark" : @"toolbar_dict_light"];
  shortcutsButton_.image = [self loadSvg:isDarkMode_ ? @"toolbar_shortcuts_dark" : @"toolbar_shortcuts_light"];
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
    return;
  }

  g_toolbar_view = [[MozcToolbarView alloc] initWithClient:client mode:mode];
  mozc::config::Config config;
  if (client->GetConfig(&config)) {
    [g_toolbar_view updateTraditionalKanji:config.use_traditional_kanji()];
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
      output.has_config() && output.config().has_use_traditional_kanji();
  bool use_trad =
      has_trad ? output.config().use_traditional_kanji() : false;
  bool locked = output.has_status() && output.status().left_shift_direct_lock();

  RunOnMainThread(^{
    if (!g_toolbar_view) return;
    [g_toolbar_view updateMode:display_mode locked:locked];
    if (has_trad) {
      [g_toolbar_view updateTraditionalKanji:use_trad];
    }
  });
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
