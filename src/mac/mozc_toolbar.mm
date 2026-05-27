// Copyright 2010-2021, Google Inc.
// All rights reserved.
//
// marinaMozc: macOS floating toolbar.  A non-activating NSPanel with branded
// SVG icons for mode switching, shin/kyu toggle, odoriji, dictionary, and
// keyboard shortcuts.  Mirrors the Linux GTK toolbar in functionality.

#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/mac/mac_util.h"
#include "client/client_interface.h"
#include "mac/common.h"
#include "mac/mozc_toolbar.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"

// Declared before @implementation MozcToolbarView (used in -odorijiClicked:).
static __weak id<ControllerCallback> g_active_controller = nil;

namespace {

constexpr int kIconSize = 24;
constexpr int kLogoWidth = 120;
constexpr int kToolbarHeight = 36;
constexpr int kButtonWidth = 36;
constexpr int kToolbarMargin = 20;
constexpr CGFloat kCornerRadius = 10.0;

using ShortcutEntry = std::pair<std::string, std::string>;
using GroupedShortcutRow = std::pair<std::string, std::string>;

const char *const kScriptCommands[] = {
    "ToggleAlphanumericMode", "ToggleHiraganaDirect", "ToggleTraditionalKanji",
    "ToggleManyoshuHiragana", "ConvertToFullKatakana",  "ConvertToHalfWidth",
    "ConvertToFullAlphanumeric", "ConvertToHiragana", nullptr};
const char *const kCompositionCommands[] = {
    "Commit", "LaunchWordRegisterDialog", "SegmentWidthShrink",
    "SegmentWidthExpand", nullptr};

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

void ParseKaeritenTsv(const std::string &path,
                      std::vector<ShortcutEntry> *kaeriten) {
  kaeriten->clear();
  std::ifstream f(path);
  if (!f) return;
  std::string line;
  while (std::getline(f, line)) {
    if (line.empty() || line[0] == '#') continue;
    size_t t1 = line.find('\t');
    if (t1 == std::string::npos) continue;
    size_t t2 = line.find('\t', t1 + 1);
    std::string input = line.substr(0, t1);
    std::string result = (t2 != std::string::npos)
                             ? line.substr(t1 + 1, t2 - (t1 + 1))
                             : line.substr(t1 + 1);
    while (!result.empty() && (result.back() == '\r' || result.back() == ' '))
      result.pop_back();
    if (!input.empty()) kaeriten->emplace_back(input, result);
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
  window.title = @"Keyboard Shortcuts";
  window.releasedWhenClosed = NO;
  [window center];

  self = [super initWithWindow:window];
  if (!self) return nil;

  [self loadDataWithClient:client];

  tabView_ = [[NSTabView alloc] initWithFrame:NSMakeRect(10, 10, 420, 380)];
  tabView_.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

  NSTabViewItem *scriptTab = [self makeTabWithTitle:@"Script"
                                        funcHeader:@"Function"
                                         keyHeader:@"Keys"];
  scriptTable_ = ((NSScrollView *)scriptTab.view).documentView;
  [tabView_ addTabViewItem:scriptTab];

  NSTabViewItem *compTab = [self makeTabWithTitle:@"Composition"
                                      funcHeader:@"Function"
                                       keyHeader:@"Keys"];
  compositionTable_ = ((NSScrollView *)compTab.view).documentView;
  [tabView_ addTabViewItem:compTab];

  NSTabViewItem *kaeritenTab = [self makeTabWithTitle:@"Kaeriten"
                                          funcHeader:@"Result"
                                           keyHeader:@"Input"];
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

  ParseKaeritenTsv(GetKeymapPath("kaeriten.tsv"), &kaeriten_entries);
  FillDefaultKaeritenShortcuts(&kaeriten_entries);

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
#pragma mark - MozcToolbarView

@interface MozcToolbarView : NSView {
  NSImageView *logoView_;
  NSButton *modeButton_;
  NSButton *tradButton_;
  NSButton *odorijiButton_;
  NSButton *dictButton_;
  NSButton *shortcutsButton_;

  mozc::client::ClientInterface *client_;
  mozc::commands::CompositionMode currentMode_;
  bool useTraditionalKanji_;
  bool isDarkMode_;

  NSPoint dragStartPoint_;
  NSPoint windowStartOrigin_;
}

- (instancetype)initWithClient:(mozc::client::ClientInterface *)client
                          mode:(mozc::commands::CompositionMode)mode;
- (void)updateMode:(mozc::commands::CompositionMode)mode;
- (void)updateTraditionalKanji:(bool)useTrad;
- (void)setClient:(mozc::client::ClientInterface *)client;

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
  isDarkMode_ = [self isDarkAppearance];

  NSVisualEffectView *vibrancy =
      [[NSVisualEffectView alloc] initWithFrame:self.bounds];
  vibrancy.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
  vibrancy.blendingMode = NSVisualEffectBlendingModeBehindWindow;
  vibrancy.material = NSVisualEffectMaterialHUDWindow;
  vibrancy.state = NSVisualEffectStateActive;
  vibrancy.wantsLayer = YES;
  vibrancy.layer.cornerRadius = kCornerRadius;
  vibrancy.layer.masksToBounds = YES;
  [self addSubview:vibrancy];

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
  modeButton_.image = [self modeIconForMode:currentMode_];
  [self addSubview:modeButton_];
  x += kButtonWidth;

  // Shin/Kyu toggle
  tradButton_ = [self makeButtonAt:x action:@selector(tradClicked:)];
  tradButton_.image = [self loadSvg:isDarkMode_ ? @"toolbar_shin_dark" : @"toolbar_shin_light"];
  [self addSubview:tradButton_];
  x += kButtonWidth;

  // Odoriji
  odorijiButton_ = [self makeButtonAt:x action:@selector(odorijiClicked:)];
  odorijiButton_.image = [self loadSvg:isDarkMode_ ? @"toolbar_marks_dark" : @"toolbar_marks_light"];
  [self addSubview:odorijiButton_];
  x += kButtonWidth;

  // Dict
  dictButton_ = [self makeButtonAt:x action:@selector(dictClicked:)];
  dictButton_.image = [self loadSvg:isDarkMode_ ? @"toolbar_dict_dark" : @"toolbar_dict_light"];
  [self addSubview:dictButton_];
  // Right-click menu for dictionary tool
  NSMenu *dictMenu = [[NSMenu alloc] initWithTitle:@"Dict"];
  [dictMenu addItemWithTitle:@"Dictionary Tool..."
                      action:@selector(dictToolClicked:)
               keyEquivalent:@""];
  dictMenu.itemArray.lastObject.target = self;
  dictButton_.menu = dictMenu;
  x += kButtonWidth;

  // Shortcuts
  shortcutsButton_ = [self makeButtonAt:x action:@selector(shortcutsClicked:)];
  shortcutsButton_.image = [self loadSvg:isDarkMode_ ? @"toolbar_shortcuts_dark" : @"toolbar_shortcuts_light"];
  [self addSubview:shortcutsButton_];

  // Fetch initial shin/kyu state
  if (client_) {
    mozc::config::Config config;
    if (client_->GetConfig(&config)) {
      useTraditionalKanji_ = config.use_traditional_kanji();
      [self updateTradIcon];
    }
  }

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

- (NSImage *)modeIconForMode:(mozc::commands::CompositionMode)mode {
  NSString *name;
  bool dark = isDarkMode_;
  switch (mode) {
    case mozc::commands::DIRECT:
    case mozc::commands::HALF_ASCII:
      name = dark ? @"toolbar_roma_half_dark" : @"toolbar_roma_half_light";
      break;
    case mozc::commands::HIRAGANA:
      name = dark ? @"toolbar_hira_dark" : @"toolbar_hira_light";
      break;
    case mozc::commands::FULL_KATAKANA:
    case mozc::commands::MANYOSHU:
      name = dark ? @"toolbar_kata_dark" : @"toolbar_kata_light";
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

#pragma mark - State Updates

- (void)updateMode:(mozc::commands::CompositionMode)mode {
  currentMode_ = mode;
  modeButton_.image = [self modeIconForMode:mode];
}

- (void)updateTraditionalKanji:(bool)useTrad {
  useTraditionalKanji_ = useTrad;
  [self updateTradIcon];
}

- (void)updateTradIcon {
  if (useTraditionalKanji_) {
    tradButton_.image = [self loadSvg:isDarkMode_ ? @"toolbar_kyu_dark" : @"toolbar_kyu_light"];
  } else {
    tradButton_.image = [self loadSvg:isDarkMode_ ? @"toolbar_shin_dark" : @"toolbar_shin_light"];
  }
}

- (void)setClient:(mozc::client::ClientInterface *)client {
  client_ = client;
}

#pragma mark - Button Actions

- (void)modeClicked:(id)sender {
  NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Mode"];

  struct ModeEntry {
    NSString *title;
    mozc::commands::CompositionMode mode;
  };
  ModeEntry modes[] = {
      {@"Hiragana", mozc::commands::HIRAGANA},
      {@"Katakana (Manyōshū)", mozc::commands::MANYOSHU},
      {@"Half-width Katakana", mozc::commands::HALF_KATAKANA},
      {@"Full-width Roman", mozc::commands::FULL_ASCII},
      {@"Half-width Roman", mozc::commands::HALF_ASCII},
      {@"Direct Input", mozc::commands::DIRECT},
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
  if (!client_) return;
  auto mode =
      static_cast<mozc::commands::CompositionMode>(item.tag);

  if (mode == mozc::commands::DIRECT) {
    mozc::commands::KeyEvent key_event;
    key_event.set_special_key(mozc::commands::KeyEvent::OFF);
    mozc::commands::Output output;
    client_->SendKey(key_event, &output);
  } else {
    mozc::commands::SessionCommand command;
    command.set_type(mozc::commands::SessionCommand::SWITCH_COMPOSITION_MODE);
    command.set_composition_mode(mode);
    mozc::commands::Output output;
    client_->SendCommand(command, &output);
  }
  currentMode_ = mode;
  modeButton_.image = [self modeIconForMode:mode];
}

- (void)tradClicked:(id)sender {
  if (!client_) return;
  mozc::commands::SessionCommand command;
  command.set_type(
      mozc::commands::SessionCommand::TOGGLE_TRADITIONAL_KANJI);
  mozc::commands::Output output;
  if (client_->SendCommand(command, &output)) {
    if (output.has_config()) {
      useTraditionalKanji_ = output.config().use_traditional_kanji();
      [self updateTradIcon];
    }
  }
}

- (void)odorijiClicked:(id)sender {
  mozc::commands::SessionCommand command;
  command.set_type(mozc::commands::SessionCommand::SHOW_ODORIJI_PALETTE);
  // Route through the active IMK controller so processOutput/updateCandidates runs
  // (same as Linux MozcEngine::SendToolbarSessionCommand → UpdateAll).
  id<ControllerCallback> controller = g_active_controller;
  if (controller) {
    [controller sendCommand:command];
    return;
  }
  if (!client_) return;
  mozc::commands::Output output;
  client_->SendCommand(command, &output);
}

- (void)dictClicked:(id)sender {
  if (!client_) return;
  client_->LaunchTool("word_register_dialog", "");
}

- (void)dictToolClicked:(id)sender {
  if (!client_) return;
  client_->LaunchTool("dictionary_tool", "");
}

- (void)shortcutsClicked:(id)sender {
  if (!client_) return;
  MozcShortcutsWindowController *ctrl =
      [[MozcShortcutsWindowController alloc] initWithClient:client_];
  [ctrl showWindow:nil];
  // Keep a strong reference so the window stays alive.
  objc_setAssociatedObject(self, "shortcutsCtrl", ctrl,
                           OBJC_ASSOCIATION_RETAIN_NONATOMIC);
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
  NSString *dir = [NSString
      stringWithUTF8String:mozc::MacUtil::GetApplicationSupportDirectory().c_str()];
  return [dir stringByAppendingPathComponent:@"toolbar.conf"];
}

- (void)savePosition {
  NSPoint origin = self.window.frame.origin;
  NSDictionary *dict = @{
    @"x" : @(origin.x),
    @"y" : @(origin.y),
  };
  [dict writeToFile:[self prefsPath] atomically:YES];
}

- (NSPoint)loadPosition {
  NSDictionary *dict =
      [NSDictionary dictionaryWithContentsOfFile:[self prefsPath]];
  if (dict) {
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

  logoView_.image = [self loadLogoSvg:isDarkMode_ ? @"logo_long_dark" : @"logo_long_light"];
  modeButton_.image = [self modeIconForMode:currentMode_];
  [self updateTradIcon];
  odorijiButton_.image = [self loadSvg:isDarkMode_ ? @"toolbar_marks_dark" : @"toolbar_marks_light"];
  dictButton_.image = [self loadSvg:isDarkMode_ ? @"toolbar_dict_dark" : @"toolbar_dict_light"];
  shortcutsButton_.image = [self loadSvg:isDarkMode_ ? @"toolbar_shortcuts_dark" : @"toolbar_shortcuts_light"];
}

@end

// ---------------------------------------------------------------------------
#pragma mark - Module-level state

static NSPanel *g_toolbar_panel = nil;
static MozcToolbarView *g_toolbar_view = nil;

static void EnsureToolbar(mozc::client::ClientInterface *client,
                          mozc::commands::CompositionMode mode) {
  if (g_toolbar_panel) {
    [g_toolbar_view setClient:client];
    return;
  }

  g_toolbar_view = [[MozcToolbarView alloc] initWithClient:client mode:mode];

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

void MozcToolbarShow(client::ClientInterface *client,
                     commands::CompositionMode mode) {
  dispatch_async(dispatch_get_main_queue(), ^{
    EnsureToolbar(client, mode);
    [g_toolbar_view updateMode:mode];
    [g_toolbar_panel orderFront:nil];
  });
}

void MozcToolbarHide() {
  dispatch_async(dispatch_get_main_queue(), ^{
    [g_toolbar_panel orderOut:nil];
  });
}

void MozcToolbarSetActiveController(void *controller) {
  // Called from activateServer:/deactivateServer: on the main thread.
  g_active_controller = (__bridge id<ControllerCallback>)controller;
}

void MozcToolbarUpdate(const commands::Output &output,
                       commands::CompositionMode mode) {
  bool has_trad =
      output.has_config() && output.config().has_use_traditional_kanji();
  bool use_trad =
      has_trad ? output.config().use_traditional_kanji() : false;

  dispatch_async(dispatch_get_main_queue(), ^{
    if (!g_toolbar_view) return;
    [g_toolbar_view updateMode:mode];
    if (has_trad) {
      [g_toolbar_view updateTraditionalKanji:use_trad];
    }
  });
}

}  // namespace mac
}  // namespace mozc
