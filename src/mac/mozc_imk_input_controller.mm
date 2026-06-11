// Copyright 2010-2021, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#import "mac/marina_localized_string.h"
#import "mac/mozc_imk_input_controller.h"

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <InputMethodKit/IMKInputController.h>
#import <InputMethodKit/IMKServer.h>

#include <unistd.h>

#include <optional>
#include <string>

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <new>
#include <set>
#include <string>
#include <utility>

#import "mac/KeyCodeMap.h"
#import "mac/renderer_receiver.h"

#include "absl/log/log.h"
#include "absl/strings/strip.h"
#include "absl/strings/string_view.h"
#include "base/const.h"
#include "base/file_util.h"
#include "base/system_util.h"
#include "base/mac/mac_process.h"
#include "base/mac/mac_util.h"
#include "base/process.h"
#include "base/util.h"
#include "client/client.h"
#include "ipc/ipc.h"
#include "protocol/candidate_window.pb.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "session/marina_number_row_bindings_util.h"
#include "sync/sync_activity.h"
#include "mac/mozc_toolbar.h"
#include "mac/sync_overlay.h"
#include "renderer/renderer_client.h"

using mozc::kProductNameInEnglish;
using mozc::MacProcess;
using mozc::commands::Capability;
using mozc::commands::CompositionMode;
using mozc::commands::KeyEvent;
using mozc::commands::Output;
using mozc::commands::Preedit;
using mozc::commands::RendererCommand;
using mozc::commands::SessionCommand;
using mozc::config::Config;
// less<> is necessary to compare between std::string and absl::string_view.
using SetOfString = std::set<std::string, std::less<>>;

namespace {
// Global object used as a singleton used as a proxy to receive messages from
// the renderer process.
RendererReceiver *gRendererReceiver = nil;

// TODO(horo): This value should be get from system configuration.
//  DoubleClickInterval can be get from NSEvent (MacOSX ver >= 10.6)
constexpr NSTimeInterval kDoubleTapInterval = 0.5;

constexpr int kMaxSurroundingLength = 20;
// In some apllications when the client's text length is large, getting the
// surrounding text takes too much time. So we set this limitation.
constexpr int kGetSurroundingTextClientLengthLimit = 1000;

constexpr absl::string_view kRomanModeId = "com.apple.inputmethod.Roman";
constexpr absl::string_view kKatakanaModeId = "com.apple.inputmethod.Japanese.Katakana";
constexpr absl::string_view kHalfWidthKanaModeId = "com.apple.inputmethod.Japanese.HalfWidthKana";
constexpr absl::string_view kFullWidthRomanModeId = "com.apple.inputmethod.Japanese.FullWidthRoman";
constexpr absl::string_view kHiraganaModeId = "com.apple.inputmethod.Japanese";

CompositionMode GetCompositionMode(absl::string_view mode_id) {
  if (mode_id.empty()) {
    LOG(ERROR) << "mode_id is initialized.";
    return mozc::commands::DIRECT;
  }
  DLOG(INFO) << mode_id;

  // The information for ID names was available at
  // /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/System/Library/Frameworks/
  // Carbon.framework/Versions/A/Frameworks/HIToolbox.framework/Versions/A/Headers/TextServices.h
  // These IDs are also defined in Info.plist.
  if (mode_id == kRomanModeId) {
    return mozc::commands::HALF_ASCII;
  }
  if (mode_id == kKatakanaModeId) {
    // Redirect Katakana selection to Manyoshu (menu still shows "Katakana").
    return mozc::commands::MANYOSHU;
  }
  if (mode_id == kHalfWidthKanaModeId) {
    return mozc::commands::HALF_KATAKANA;
  }
  if (mode_id == kFullWidthRomanModeId) {
    return mozc::commands::FULL_ASCII;
  }
  if (mode_id == kHiraganaModeId) {
    return mozc::commands::HIRAGANA;
  }

  LOG(ERROR) << "The code should not reach here.";
  return mozc::commands::DIRECT;
}

CompositionMode EffectiveCompositionMode(const Output &output,
                                         CompositionMode fallback) {
  if (output.has_status()) {
    return output.status().activated() ? output.status().mode()
                                       : mozc::commands::DIRECT;
  }
  if (output.has_mode()) {
    return output.mode();
  }
  return fallback;
}

CompositionMode NormalizeModeForEmptyHalfAscii(CompositionMode mode,
                                               const Output &output) {
  if (mode == mozc::commands::HALF_ASCII &&
      (!output.has_preedit() || output.preedit().segment_size() == 0)) {
    return mozc::commands::DIRECT;
  }
  return mode;
}

bool OutputLaunchesMozcTool(const Output &output) {
  return output.has_launch_tool_mode() &&
         output.launch_tool_mode() != Output::NO_TOOL;
}

// Server |status.activated() == false| means the converter session is off
// (e.g. focus left the host), not that the user chose DIRECT composition mode.
bool ShouldPreserveClientCompositionMode(const Output &output,
                                         CompositionMode current_mode,
                                         CompositionMode new_mode) {
  // Left Shift → direct sets output.mode() to DIRECT with consumed=false.
  if (output.has_mode() && output.mode() == mozc::commands::DIRECT) {
    return false;
  }
  return output.has_status() && !output.status().activated() &&
         current_mode != mozc::commands::DIRECT &&
         new_mode == mozc::commands::DIRECT;
}

#if 0  // Input Mode submenu disabled (see setupMarinaImeMenuIfNeeded).
// Maps server mode to the IBUS-style input-mode menu (Manyōshū → Katakana).
CompositionMode CompositionModeForImeMenu(CompositionMode mode) {
  if (mode == mozc::commands::MANYOSHU) {
    return mozc::commands::FULL_KATAKANA;
  }
  return mode;
}
#endif

absl::string_view GetModeId(CompositionMode mode) {
  switch (mode) {
    case mozc::commands::DIRECT:
    case mozc::commands::HALF_ASCII:
      return kRomanModeId;
    case mozc::commands::FULL_KATAKANA:
    case mozc::commands::MANYOSHU:
      return kKatakanaModeId;
    case mozc::commands::HALF_KATAKANA:
      return kHalfWidthKanaModeId;
    case mozc::commands::FULL_ASCII:
      return kFullWidthRomanModeId;
    case mozc::commands::HIRAGANA:
      return kHiraganaModeId;
    default:
      LOG(ERROR) << "The code should not reach here.";
      return kRomanModeId;
  }
}

bool CanOpenLink(absl::string_view bundle_id) {
  // Should not open links during screensaver.
  return bundle_id != "com.apple.securityagent";
}

bool CanSelectedRange(absl::string_view bundle_id) {
  // Do not call selectedRange: method for the following
  // applications because it could lead to application crash.
  const bool is_supported = bundle_id != "com.microsoft.Excel" &&
                            bundle_id != "com.microsoft.Powerpoint" &&
                            bundle_id != "com.microsoft.Word";
  return is_supported;
}

bool CanDisplayModeSwitch(absl::string_view bundle_id) {
  // Do not call selectInputMode: method for the following
  // applications because it could cause some unexpected behavior.
  // MS-Word: When the display mode goes to ASCII but there is no
  // compositions, it goes to direct input mode instead of Half-ASCII
  // mode.  When the first composition character is alphanumeric (such
  // like pressing Shift-A at first), that character is directly
  // inserted into application instead of composition starting "A".
  return bundle_id != "com.microsoft.Word";
}

bool CanSurroundingText(absl::string_view bundle_id) {
  // Disables the surrounding text feature for the following application
  // because calling attributedSubstringFromRange to it is very heavy.
  return bundle_id != "com.evernote.Evernote";
}

bool KeyEventHasCtrlShift(const KeyEvent &key) {
  bool ctrl = false;
  bool shift = false;
  for (int i = 0; i < key.modifier_keys_size(); ++i) {
    if (key.modifier_keys(i) == KeyEvent::CTRL) {
      ctrl = true;
    }
    if (key.modifier_keys(i) == KeyEvent::SHIFT) {
      shift = true;
    }
  }
  return ctrl && shift;
}

bool IsMarinaConfigurableNumberRowShortcut(const KeyEvent &key,
                                           const Config &config) {
  return mozc::session::FindMarinaActionForKeyEvent(config, key).has_value();
}

// Ctrl+Shift+` / ~ (ToggleAlphanumericMode in keymap).
bool IsMarinaBacktickShortcut(const KeyEvent &key) {
  if (!KeyEventHasCtrlShift(key) || !key.has_key_code()) {
    return false;
  }
  const uint32_t code = key.key_code();
  return code == static_cast<uint32_t>('`') || code == static_cast<uint32_t>('~');
}

// Ctrl+Shift+F / f (ToggleTraditionalKanji); always SessionCommand, not keymap.
bool IsMarinaTraditionalKanjiShortcut(const KeyEvent &key) {
  return KeyEventHasCtrlShift(key) && key.has_key_code() &&
         (key.key_code() == static_cast<uint32_t>('f') ||
          key.key_code() == static_cast<uint32_t>('F'));
}

bool IsMacronVowelLetter(unichar c) {
  const unichar lower = (c >= 'A' && c <= 'Z') ? (c - 'A' + 'a') : c;
  return lower == 'a' || lower == 'e' || lower == 'i' || lower == 'o' || lower == 'u';
}

bool IsCtrlOptionMacronChord(NSUInteger ns_modifiers) {
  return (ns_modifiers & NSEventModifierFlagControl) &&
         (ns_modifiers & NSEventModifierFlagOption) &&
         !(ns_modifiers & NSEventModifierFlagCommand);
}

bool IsPhysicalModifierKeyCode(unsigned short key_code) {
  switch (key_code) {
    case kVK_Control:
    case kVK_RightControl:
    case kVK_Shift:
    case kVK_RightShift:
    case kVK_Option:
    case kVK_RightOption:
    case kVK_Command:
    case kVK_CapsLock:
    case kVK_Function:
      return true;
    default:
      return false;
  }
}

bool MarinaImkTraceEnabled() {
  static int env_trace = -1;
  if (env_trace < 0) {
    env_trace = ::getenv("MARINA_IMK_TRACE") != nullptr ? 1 : 0;
  }
  if (env_trace != 0) {
    return true;
  }
  // Re-check the flag file every time: the IME process often stays alive across
  // "touch imk_trace", so a one-shot cache left trace disabled until logout.
  const char *home = ::getenv("HOME");
  if (home == nullptr) {
    return false;
  }
  const std::string flag =
      std::string(home) + "/Library/Application Support/marinaMoji/imk_trace";
  return ::access(flag.c_str(), F_OK) == 0;
}

const char *CompositionModeName(CompositionMode mode) {
  switch (mode) {
    case mozc::commands::DIRECT:
      return "DIRECT";
    case mozc::commands::HIRAGANA:
      return "HIRAGANA";
    case mozc::commands::FULL_KATAKANA:
      return "FULL_KATAKANA";
    case mozc::commands::MANYOSHU:
      return "MANYOSHU";
    case mozc::commands::HALF_KATAKANA:
      return "HALF_KATAKANA";
    case mozc::commands::FULL_ASCII:
      return "FULL_ASCII";
    case mozc::commands::HALF_ASCII:
      return "HALF_ASCII";
    default:
      return "?";
  }
}

std::optional<CompositionMode> ParseCompositionModeName(absl::string_view name) {
  name = absl::StripAsciiWhitespace(name);
  if (name == "DIRECT") {
    return mozc::commands::DIRECT;
  }
  if (name == "HIRAGANA") {
    return mozc::commands::HIRAGANA;
  }
  if (name == "FULL_KATAKANA") {
    return mozc::commands::FULL_KATAKANA;
  }
  if (name == "MANYOSHU") {
    return mozc::commands::MANYOSHU;
  }
  if (name == "HALF_KATAKANA") {
    return mozc::commands::HALF_KATAKANA;
  }
  if (name == "FULL_ASCII") {
    return mozc::commands::FULL_ASCII;
  }
  if (name == "HALF_ASCII") {
    return mozc::commands::HALF_ASCII;
  }
  return std::nullopt;
}

std::string LastCompositionModePath() {
  return mozc::FileUtil::JoinPath(mozc::SystemUtil::GetUserProfileDirectory(),
                                  "last_composition_mode.txt");
}

void PersistLastCompositionMode(CompositionMode mode) {
  mozc::FileUtil::SetContents(LastCompositionModePath(), CompositionModeName(mode))
      .IgnoreError();
}

std::optional<CompositionMode> LoadLastCompositionMode() {
  const absl::StatusOr<std::string> contents =
      mozc::FileUtil::GetContents(LastCompositionModePath());
  if (!contents.ok()) {
    return std::nullopt;
  }
  return ParseCompositionModeName(*contents);
}
}  // namespace

@interface MozcImkInputController (MarinaPrivate)
- (BOOL)isConverterSessionActivated;
- (BOOL)dispatchMarinaNumberRowShortcut:(const KeyEvent &)keyEvent client:(id)sender;
- (BOOL)dispatchRightShiftAlone:(const KeyEvent &)keyEvent client:(id)sender;
- (BOOL)dispatchLeftShiftAlone:(const KeyEvent &)keyEvent client:(id)sender;
- (BOOL)dispatchCtrlLeftShiftModeLock:(const KeyEvent &)keyEvent client:(id)sender;
- (BOOL)tryMacronVowelChord:(NSEvent *)event client:(id)sender;
- (void)setupMarinaImeMenuIfNeeded;
- (void)updateImeMenuState:(const Output *)output;
- (void)syncCandidatesWithOutput:(const Output *)output;
- (void)cancelPendingCandidateUpdate;
- (void)applyCommitAndPreeditFromOutput:(const Output *)output
                                 client:(id)sender
                allowClearWithoutPreedit:(BOOL)allowClearWithoutPreedit;
- (void)flushCompositionBeforeDeactivate:(id)sender;
@end

@implementation MozcImkInputController
#pragma mark accessors for testing
@synthesize keyCodeMap = keyCodeMap_;
@synthesize yenSignCharacter = yenSignCharacter_;
@synthesize mode = mode_;
@synthesize rendererCommand = rendererCommand_;
@synthesize replacementRange = replacementRange_;
@synthesize imkClientForTest = imkClientForTest_;
@synthesize imeServerActive = imeServerActive_;
- (mozc::client::ClientInterface *)mozcClient {
  return mozcClient_.get();
}
- (void)setMozcClient:(std::unique_ptr<mozc::client::ClientInterface>)newMozcClient {
  mozcClient_ = std::move(newMozcClient);
}
- (mozc::renderer::RendererInterface *)renderer {
  return mozcRenderer_.get();
}
- (void)setRenderer:(std::unique_ptr<mozc::renderer::RendererInterface>)newRenderer {
  mozcRenderer_ = std::move(newRenderer);
}

#pragma mark object init/dealloc
// Initializer designated in IMKInputController. see:
// https://developer.apple.com/documentation/inputmethodkit/imkinputcontroller?language=objc

- (id)initWithServer:(IMKServer *)server delegate:(id)delegate client:(id)inputClient {
  // If server is nil, we are in the unit test environment.
  if (server == nil) {
    self = [super init];
    imkClientForTest_ = inputClient;
  } else {
    self = [super initWithServer:server delegate:delegate client:inputClient];
    imkClientForTest_ = nil;
  }
  if (!self) {
    return self;
  }
  keyCodeMap_ = [[KeyCodeMap alloc] init];
  replacementRange_ = NSMakeRange(NSNotFound, 0);
  originalString_ = [[NSMutableString alloc] init];
  composedString_ = [[NSMutableAttributedString alloc] init];
  cursorPosition_ = -1;
  mode_ = mozc::commands::DIRECT;
  suppressSuggestion_ = false;
  spotlightHost_ = false;
  syncingDisplayMode_ = false;
  handlingKeyboardEvent_ = false;
  suppressSetValueUntil_ = 0;
  processOutputDepth_ = 0;
  imeServerActive_ = false;
  cachedMenuConfigValid_ = false;
  cachedUseTraditionalKanji_ = false;
  cachedPrivacyMode_ = false;
  yenSignCharacter_ = mozc::config::Config::YEN_SIGN;
  mozcRenderer_ = mozc::renderer::RendererClient::Create();
  mozcClient_ = mozc::client::ClientFactory::NewClient();
  lastKeyDownTime_ = 0;
  lastKeyCode_ = 0;

  // We don't check the return value of NSBundle because it fails during tests.
  [[NSBundle mainBundle] loadNibNamed:@"Config" owner:self topLevelObjects:nil];
  [self setupMarinaImeMenuIfNeeded];
  if (!originalString_ || !composedString_ || !mozcRenderer_ || !mozcClient_) {
    self = nil;
  } else {
    DLOG(INFO) << [[NSString
        stringWithFormat:@"initWithServer: %@ %@ %@", server, delegate, inputClient] UTF8String];
    if (!mozcRenderer_->Activate()) {
      LOG(ERROR) << "Cannot activate renderer";
      mozcRenderer_.reset();
    }
    [self setupClientBundle:inputClient];
    [self setupCapability];
    RendererCommand::ApplicationInfo *applicationInfo = rendererCommand_.mutable_application_info();
    applicationInfo->set_process_id(::getpid());
    // thread_id and receiver_handle are not used currently in Mac but
    // set some values to prevent warning.
    applicationInfo->set_thread_id(0);
    applicationInfo->set_receiver_handle(0);
    if (MarinaImkTraceEnabled()) {
      LOG(INFO) << "[marinaImk] trace enabled pid=" << ::getpid();
    }
  }

  return self;
}

- (void)dealloc {
  keyCodeMap_ = nil;
  originalString_ = nil;
  composedString_ = nil;
  imkClientForTest_ = nil;
  mozcRenderer_.reset();
  mozcClient_.reset();
  DLOG(INFO) << "dealloc server";
}

- (id)client {
  if (imkClientForTest_) {
    return imkClientForTest_;
  }
  return [super client];
}

- (void)setupMarinaImeMenuIfNeeded {
  if (!menu_ || traditionalKanjiMenuItem_ != nil) {
    return;
  }

  // IBUS panel order: Input Mode, Traditional kanji, Odoriji, Toolbar (property_handler.cc).
  NSInteger insertIndex = 0;

  // Input Mode submenu disabled: mode changes from the IME menu no longer work
  // reliably after M1n mode-persistence (setValue ignores composition resync).
  // Use toolbar or keyboard shortcuts until menu switching is fixed.
#if 0
  struct ModeMenuEntry {
    NSString *titleKey;
    CompositionMode mode;
  };
  ModeMenuEntry kModeEntries[] = {
      {@"MM.DirectInput", mozc::commands::DIRECT},
      {@"MM.Hiragana", mozc::commands::HIRAGANA},
      {@"MM.Katakana", mozc::commands::FULL_KATAKANA},
      {@"MM.Latin", mozc::commands::HALF_ASCII},
      {@"MM.WideLatin", mozc::commands::FULL_ASCII},
      {@"MM.HalfWidthKatakana", mozc::commands::HALF_KATAKANA},
  };

  NSMenu *modeMenu =
      [[NSMenu alloc] initWithTitle:MarinaLocalizedString(@"MM.InputMode")];
  NSMutableArray<NSMenuItem *> *modeItems = [NSMutableArray array];
  for (const ModeMenuEntry &entry : kModeEntries) {
    NSMenuItem *item = [[NSMenuItem alloc]
        initWithTitle:MarinaLocalizedString(entry.titleKey)
               action:@selector(inputModeMenuClicked:)
        keyEquivalent:@""];
    item.target = self;
    item.tag = static_cast<NSInteger>(entry.mode);
    [modeMenu addItem:item];
    [modeItems addObject:item];
  }
  inputModeMenuItems_ = [modeItems copy];

  NSMenuItem *modeMenuItem = [[NSMenuItem alloc]
      initWithTitle:MarinaLocalizedString(@"MM.InputMode")
             action:nil
      keyEquivalent:@""];
  modeMenuItem.submenu = modeMenu;
  [menu_ insertItem:modeMenuItem atIndex:insertIndex++];
  [menu_ insertItem:[NSMenuItem separatorItem] atIndex:insertIndex++];
#endif

  traditionalKanjiMenuItem_ = [[NSMenuItem alloc]
      initWithTitle:MarinaLocalizedString(@"MM.TraditionalKanji")
             action:@selector(traditionalKanjiMenuClicked:)
      keyEquivalent:@""];
  traditionalKanjiMenuItem_.target = self;
  [menu_ insertItem:traditionalKanjiMenuItem_ atIndex:insertIndex++];

  NSMenuItem *odorijiItem = [[NSMenuItem alloc]
      initWithTitle:MarinaLocalizedString(@"MM.Odoriji")
             action:@selector(odorijiPaletteMenuClicked:)
      keyEquivalent:@""];
  odorijiItem.target = self;
  [menu_ insertItem:odorijiItem atIndex:insertIndex++];

  if (toolbarMenuItem_ != nil) {
    [menu_ removeItem:toolbarMenuItem_];
  }
  toolbarMenuItem_ = [[NSMenuItem alloc]
      initWithTitle:MarinaLocalizedString(@"MM.Toolbar")
             action:@selector(toolbarVisibilityMenuClicked:)
      keyEquivalent:@""];
  toolbarMenuItem_.target = self;
  [menu_ insertItem:toolbarMenuItem_ atIndex:insertIndex++];

  privacyModeMenuItem_ = [[NSMenuItem alloc]
      initWithTitle:MarinaLocalizedString(@"MM.PrivacyMode")
             action:@selector(privacyModeMenuClicked:)
      keyEquivalent:@""];
  privacyModeMenuItem_.target = self;
  [menu_ insertItem:privacyModeMenuItem_ atIndex:insertIndex++];

  [menu_ insertItem:[NSMenuItem separatorItem] atIndex:insertIndex];
  [self updateImeMenuState:nullptr];
}

- (void)updateImeMenuState:(const Output *)output {
  if (toolbarMenuItem_) {
    const bool visible = mozc::mac::MozcToolbarLoadVisiblePreference();
    toolbarMenuItem_.state =
        visible ? NSControlStateValueOn : NSControlStateValueOff;
  }

  if (traditionalKanjiMenuItem_) {
    bool use_trad = false;
    if (output != nullptr && output->has_config()) {
      use_trad = output->config().use_traditional_kanji();
      cachedMenuConfigValid_ = true;
      cachedUseTraditionalKanji_ = use_trad;
      if (output->config().has_incognito_mode()) {
        cachedPrivacyMode_ = output->config().incognito_mode();
      }
    } else if (cachedMenuConfigValid_) {
      use_trad = cachedUseTraditionalKanji_;
    } else if (mozcClient_ != nullptr) {
      Config config;
      if (mozcClient_->GetConfig(&config)) {
        use_trad = config.use_traditional_kanji();
        cachedMenuConfigValid_ = true;
        cachedUseTraditionalKanji_ = use_trad;
        cachedPrivacyMode_ = config.incognito_mode();
      }
    }
    traditionalKanjiMenuItem_.state =
        use_trad ? NSControlStateValueOn : NSControlStateValueOff;
  }

  if (privacyModeMenuItem_) {
    bool privacy_on = false;
    if (output != nullptr && output->has_config()) {
      privacy_on = output->config().incognito_mode();
      cachedMenuConfigValid_ = true;
      cachedPrivacyMode_ = privacy_on;
      if (output->config().has_use_traditional_kanji()) {
        cachedUseTraditionalKanji_ = output->config().use_traditional_kanji();
      }
    } else if (cachedMenuConfigValid_) {
      privacy_on = cachedPrivacyMode_;
    } else if (mozcClient_ != nullptr) {
      Config config;
      if (mozcClient_->GetConfig(&config)) {
        privacy_on = config.incognito_mode();
        cachedMenuConfigValid_ = true;
        cachedPrivacyMode_ = privacy_on;
        cachedUseTraditionalKanji_ = config.use_traditional_kanji();
      }
    }
    privacyModeMenuItem_.state =
        privacy_on ? NSControlStateValueOn : NSControlStateValueOff;
  }

#if 0  // Input Mode submenu disabled (see setupMarinaImeMenuIfNeeded).
  if (!inputModeMenuItems_) {
    return;
  }

  CompositionMode display_mode = mode_;
  if (output != nullptr && (output->has_status() || output->has_mode())) {
    display_mode = NormalizeModeForEmptyHalfAscii(
        EffectiveCompositionMode(*output, mode_), *output);
  }
  const CompositionMode menu_mode = CompositionModeForImeMenu(display_mode);
  for (NSMenuItem *item in inputModeMenuItems_) {
    const auto item_mode = static_cast<CompositionMode>(item.tag);
    item.state = (item_mode == menu_mode) ? NSControlStateValueOn : NSControlStateValueOff;
  }
#endif
}

- (NSMenu *)menu {
  [self setupMarinaImeMenuIfNeeded];
  [self updateImeMenuState:nullptr];
  return menu_;
}

#pragma mark IMKStateSetting Protocol
// Currently it just ignores the following methods:
//   Modes, showPreferences, valueForTag
// They are described at
// https://developer.apple.com/documentation/inputmethodkit/imkstatesetting?language=objc

- (void)activateServer:(id)sender {
  if (imkClientForTest_) {
    return;
  }
  [super activateServer:sender];
  imeServerActive_ = true;
  [self setupClientBundle:sender];
  if (rendererCommand_.visible() && mozcRenderer_) {
    mozcRenderer_->ExecCommand(rendererCommand_);
  }
  [self handleConfig];

  // Sets this controller as the active controller to receive messages from the renderer process.
  [gRendererReceiver setCurrentController:self];

  std::string window_name, window_owner;
  spotlightHost_ = false;
  if (mozc::MacUtil::GetFrontmostWindowNameAndOwner(&window_name, &window_owner)) {
    DLOG(INFO) << "frontmost window name: \"" << window_name << "\" " << "owner: \"" << window_owner
               << "\"";
    suppressSuggestion_ = mozc::MacUtil::IsSuppressSuggestionWindow(window_name, window_owner);
    spotlightHost_ =
        mozc::MacUtil::IsSpotlightLikeHost(clientBundle_, window_name);
  } else {
    spotlightHost_ = mozc::MacUtil::IsSpotlightLikeHost(clientBundle_, "");
  }
  if (spotlightHost_) {
    // Spotlight: suppress suggestion UI only; keep Japanese composition and toolbar.
    suppressSuggestion_ = true;
  }

  mozc::mac::MozcToolbarSetActiveController((__bridge void *)self);
  if (const std::optional<CompositionMode> persisted = LoadLastCompositionMode();
      persisted.has_value()) {
    mode_ = *persisted;
  }
  [self refreshModeFromServer:sender];
  [self syncServerActivationIfNeeded:sender];
  if (mozc::mac::MozcToolbarNeedsReshowAfterPaletteClose()) {
    mozc::mac::MozcToolbarShow(mozcClient_.get(), mode_);
  }

  DLOG(INFO) << kProductNameInEnglish << " client (" << self << "): activated for " << sender;
  DLOG(INFO) << "sender bundleID: " << clientBundle_;
}

- (void)flushCompositionBeforeDeactivate:(id)sender {
  if (imkClientForTest_ || [composedString_ length] == 0) {
    return;
  }
  if ([self isConverterSessionActivated]) {
    KeyEvent keyEvent;
    Output output;
    keyEvent.set_special_key(mozc::commands::KeyEvent::ESCAPE);
    if (mozcClient_->SendKey(keyEvent, &output)) {
      [self processOutput:&output client:sender];
    }
  }
  if ([composedString_ length] > 0) {
    [self updateComposedString:nullptr];
  }
}

- (void)deactivateServer:(id)sender {
  if (imkClientForTest_) {
    return;
  }
  [self flushCompositionBeforeDeactivate:sender];
  imeServerActive_ = false;
  mozc::mac::MozcToolbarClearActiveControllerIfMatches((__bridge void *)self);
  spotlightHost_ = false;
  if ([composedString_ length] > 0) {
    [self updateComposedString:nullptr];
  }
  // Sync local |rendererCommand_| and cancel a queued |-delayedUpdateCandidates| (typing
  // schedules it on the next run loop; switching to Dvorak can run it after deactivate).
  [self clearCandidates];
  DLOG(INFO) << kProductNameInEnglish << " client (" << self << "): deactivated";
  DLOG(INFO) << "sender bundleID: " << clientBundle_;
  mozc::sync::RecordImeDeactivated();
  [super deactivateServer:sender];
}

- (NSUInteger)recognizedEvents:(id)sender {
  // Because we want to handle single Shift key pressing later, now I
  // turned on NSFlagsChanged also.
  return NSEventMaskKeyDown | NSEventMaskFlagsChanged;
}

// This method is called when a user changes the input mode.
- (void)setValue:(id)value forTag:(long)tag client:(id)sender {
  if (imkClientForTest_) {
    return;
  }

  // Called by macOS when |-switchDisplayMode| uses |selectInputMode:|.  Do not
  // push mode changes back to the server or call |-switchDisplayMode| again.
  const NSTimeInterval now = [[NSDate date] timeIntervalSinceReferenceDate];
  if (syncingDisplayMode_ || handlingKeyboardEvent_ || now < suppressSetValueUntil_) {
    if (MarinaImkTraceEnabled()) {
      LOG(INFO) << "[marinaImk] setValue skipped"
                << " sync=" << syncingDisplayMode_ << " key=" << handlingKeyboardEvent_
                << " suppress=" << (now < suppressSetValueUntil_);
    }
    [super setValue:value forTag:tag client:sender];
    return;
  }

  // macOS calls this on focus changes and the input-mode picker.  Honour only
  // transitions to DIRECT (IME off / 英数).  Ignore composition-mode resyncs
  // (e.g. Spotlight refocus forcing hiragana) so |mode_| stays where the user
  // left it via toolbar or shortcuts (see macOS_mode_persistence.md).
  CompositionMode new_mode = [value isKindOfClass:[NSString class]]
                                 ? GetCompositionMode([value UTF8String])
                                 : mozc::commands::DIRECT;
  if (new_mode == mozc::commands::HALF_ASCII && [composedString_ length] == 0) {
    new_mode = mozc::commands::DIRECT;
  }
  if (new_mode != mozc::commands::DIRECT) {
    if (MarinaImkTraceEnabled() && new_mode != mode_) {
      LOG(INFO) << "[marinaImk] setValue ignored (composition resync) "
                << CompositionModeName(mode_) << " <- "
                << CompositionModeName(new_mode);
    }
    [super setValue:value forTag:tag client:sender];
    return;
  }
  if (mozc::mac::MozcImkShouldSuppressSetValueDirect()) {
    [super setValue:value forTag:tag client:sender];
    return;
  }
  if (mode_ != mozc::commands::DIRECT) {
    if (MarinaImkTraceEnabled()) {
      LOG(INFO) << "[marinaImk] setValue mode " << CompositionModeName(mode_)
                << " -> DIRECT";
    }
    mode_ = mozc::commands::DIRECT;
    PersistLastCompositionMode(mode_);
    mozc::mac::MozcToolbarShow(mozcClient_.get(), mode_);
    [self syncServerDeactivationIfNeeded:sender];
  }
  // Do not call |-handleConfig| here: |-overrideKeyboardWithKeyboardNamed:| can
  // re-enter the input system and contributed to post-shortcut freezes.
  [super setValue:value forTag:tag client:sender];
}

#pragma mark internal methods

- (void)refreshModeFromServer:(id)sender {
  SessionCommand command;
  command.set_type(SessionCommand::GET_STATUS);
  Output output;
  if (!mozcClient_->SendCommand(command, &output)) {
    mozc::mac::MozcToolbarShow(mozcClient_.get(), mode_);
    return;
  }

  if (mode_ == mozc::commands::DIRECT) {
    if (output.has_status() && output.status().activated()) {
      [self syncServerDeactivationIfNeeded:sender];
    }
    mozc::mac::MozcToolbarShow(mozcClient_.get(), mode_);
    mozc::mac::MozcToolbarUpdate(output, mode_);
    [self updateImeMenuState:&output];
    return;
  }

  CompositionMode new_mode = NormalizeModeForEmptyHalfAscii(
      EffectiveCompositionMode(output, mode_), output);
  if (ShouldPreserveClientCompositionMode(output, mode_, new_mode)) {
    new_mode = mode_;
  }
  mode_ = new_mode;
  mozc::mac::MozcToolbarShow(mozcClient_.get(), mode_);
  mozc::mac::MozcToolbarUpdate(output, mode_);
  [self updateImeMenuState:&output];
}

- (BOOL)isConverterSessionActivated {
  SessionCommand command;
  command.set_type(SessionCommand::GET_STATUS);
  Output output;
  if (!mozcClient_->SendCommand(command, &output) || !output.has_status()) {
    return NO;
  }
  return output.status().activated();
}

- (BOOL)ensureConverterActivated:(id)sender context:(mozc::commands::Context *)context {
  (void)context;
  if ([self isConverterSessionActivated]) {
    return YES;
  }
  SessionCommand command;
  command.set_type(SessionCommand::TURN_ON_IME);
  const CompositionMode on_mode =
      mode_ == mozc::commands::DIRECT ? mozc::commands::HIRAGANA : mode_;
  command.set_composition_mode(on_mode);
  Output output;
  if (!mozcClient_->SendCommand(command, &output)) {
    return NO;
  }
  [self processOutput:&output client:sender];
  if (MarinaImkTraceEnabled()) {
    LOG(INFO) << "[marinaImk] ensureConverterActivated TURN_ON_IME"
              << " mode_=" << CompositionModeName(mode_);
  }
  return YES;
}

- (void)syncServerActivationIfNeeded:(id)sender {
  if (mode_ == mozc::commands::DIRECT) {
    return;
  }
  mozc::commands::Context context;
  [self ensureConverterActivated:sender context:&context];
}

- (void)syncServerDeactivationIfNeeded:(id)sender {
  if (mode_ != mozc::commands::DIRECT || ![self isConverterSessionActivated]) {
    return;
  }
  SessionCommand command;
  command.set_type(SessionCommand::TURN_OFF_IME);
  Output output;
  if (!mozcClient_->SendCommand(command, &output)) {
    return;
  }
  [self processOutput:&output client:sender];
  if (MarinaImkTraceEnabled()) {
    LOG(INFO) << "[marinaImk] syncServerDeactivation TURN_OFF_IME";
  }
}

- (BOOL)dispatchMarinaNumberRowShortcut:(const KeyEvent &)keyEvent client:(id)sender {
  (void)sender;
  Config config;
  if (!mozcClient_->GetConfig(&config)) {
    return NO;
  }
  const std::optional<mozc::config::MarinaNumberRowAction> action =
      mozc::session::FindMarinaActionForKeyEvent(config, keyEvent);
  if (!action.has_value()) {
    return NO;
  }

  if (MarinaImkTraceEnabled() && keyEvent.has_key_code()) {
    LOG(INFO) << "[marinaImk] dispatch marina action=" << static_cast<int>(*action)
              << " key=" << static_cast<char>(keyEvent.key_code())
              << " mode_=" << CompositionModeName(mode_);
  }

  SessionCommand command;
  switch (*action) {
    case mozc::config::MARINA_NR_HIRAGANA_DIRECT:
      if ([self isConverterSessionActivated]) {
        command.set_type(SessionCommand::TURN_OFF_IME);
      } else {
        command.set_type(SessionCommand::TURN_ON_IME);
        command.set_composition_mode(mozc::commands::HIRAGANA);
      }
      [self sendCommand:command];
      return YES;

    case mozc::config::MARINA_NR_MANYOSHU_HIRAGANA:
      if (![self isConverterSessionActivated]) {
        SessionCommand on_command;
        on_command.set_type(SessionCommand::TURN_ON_IME);
        const CompositionMode on_mode =
            mode_ == mozc::commands::DIRECT ? mozc::commands::HIRAGANA : mode_;
        on_command.set_composition_mode(on_mode);
        [self sendCommand:on_command];
      }
      command.set_type(SessionCommand::SWITCH_COMPOSITION_MODE);
      if (mode_ == mozc::commands::MANYOSHU) {
        command.set_composition_mode(mozc::commands::HIRAGANA);
      } else {
        command.set_composition_mode(mozc::commands::MANYOSHU);
      }
      [self sendCommand:command];
      return YES;

    case mozc::config::MARINA_NR_ODORIJI_DEFAULT:
      if (![self isConverterSessionActivated]) {
        SessionCommand on_command;
        on_command.set_type(SessionCommand::TURN_ON_IME);
        const CompositionMode on_mode =
            mode_ == mozc::commands::DIRECT ? mozc::commands::HIRAGANA : mode_;
        on_command.set_composition_mode(on_mode);
        [self sendCommand:on_command];
      }
      command.set_type(SessionCommand::INSERT_ODORIJI_DEFAULT);
      [self sendCommand:command];
      return YES;

    case mozc::config::MARINA_NR_ODORIJI_PALETTE:
      if (![self isConverterSessionActivated]) {
        SessionCommand on_command;
        on_command.set_type(SessionCommand::TURN_ON_IME);
        const CompositionMode on_mode =
            mode_ == mozc::commands::DIRECT ? mozc::commands::HIRAGANA : mode_;
        on_command.set_composition_mode(on_mode);
        [self sendCommand:on_command];
      }
      command.set_type(SessionCommand::SHOW_ODORIJI_PALETTE);
      [self sendCommand:command];
      return YES;

    case mozc::config::MARINA_NR_TRADITIONAL_KANJI:
      command.set_type(SessionCommand::TOGGLE_TRADITIONAL_KANJI);
      [self sendCommand:command];
      return YES;

    case mozc::config::MARINA_NR_WORD_REGISTER:
      command.set_type(SessionCommand::LAUNCH_WORD_REGISTER_DIALOG);
      [self sendCommand:command];
      return YES;

    default:
      return NO;
  }
}

- (BOOL)dispatchMarinaTraditionalKanjiShortcut:(id)sender {
  (void)sender;
  SessionCommand command;
  command.set_type(SessionCommand::TOGGLE_TRADITIONAL_KANJI);
  [self sendCommand:command];
  return YES;
}

- (BOOL)dispatchRightShiftAlone:(const KeyEvent &)keyEvent client:(id)sender {
  // Direct input: Right Shift is just a modifier key (Linux IBus parity).
  if (mode_ == mozc::commands::DIRECT) {
    return NO;
  }

  if (![self isConverterSessionActivated]) {
    if (![self ensureConverterActivated:sender context:nullptr]) {
      return NO;
    }
  }

  KeyEvent key = keyEvent;
  key.set_mode(mode_);
  Output output;
  if (!mozcClient_->SendKey(key, &output)) {
    return NO;
  }
  [self processOutput:&output client:sender];
  // Session leaves consumed=false so Shift modifier state clears in the app.
  return output.consumed() ? YES : NO;
}

- (BOOL)dispatchLeftShiftAlone:(const KeyEvent &)keyEvent client:(id)sender {
  const BOOL serverActivated = [self isConverterSessionActivated];
  const BOOL inDirect =
      (mode_ == mozc::commands::DIRECT) || !serverActivated;
  if (!inDirect) {
    PersistLastCompositionMode(mode_);
  }

  KeyEvent key = keyEvent;
  key.set_mode(mode_);
  Output output;
  if (!mozcClient_->SendKey(key, &output)) {
    return NO;
  }
  [self processOutput:&output client:sender];

  // ToggleLeftShiftDirect leaves consumed=false when going direct so the app
  // receives Shift key-up. ShouldPreserveClientCompositionMode then blocks
  // mode_ from updating; sync client mode only (no syncServerDeactivation here —
  // nested TURN_OFF_IME during handleEvent caused IME switch stalls).
  if (output.has_mode() && output.mode() == mozc::commands::DIRECT &&
      mode_ != mozc::commands::DIRECT) {
    mode_ = mozc::commands::DIRECT;
    PersistLastCompositionMode(mode_);
    mozc::mac::MozcToolbarShow(mozcClient_.get(), mode_);
  } else if (output.consumed() && output.has_status() &&
             output.status().activated() &&
             mode_ == mozc::commands::DIRECT) {
    CompositionMode new_mode = EffectiveCompositionMode(output, mode_);
    if (new_mode != mozc::commands::DIRECT) {
      mode_ = new_mode;
      PersistLastCompositionMode(mode_);
      mozc::mac::MozcToolbarShow(mozcClient_.get(), mode_);
    }
  }
  return output.consumed() ? YES : NO;
}

- (BOOL)dispatchCtrlLeftShiftModeLock:(const KeyEvent &)keyEvent client:(id)sender {
  KeyEvent key = keyEvent;
  key.set_mode(mode_);
  Output output;
  if (!mozcClient_->SendKey(key, &output)) {
    return NO;
  }
  [self processOutput:&output client:sender];
  return output.consumed() ? YES : NO;
}

- (BOOL)dispatchMarinaBacktickShortcut:(const KeyEvent &)keyEvent client:(id)sender {
  // Keymap: ToggleAlphanumericMode (hiragana ↔ half-width). ⌃⇧` after ⌃⇧5 (direct)
  // must turn the IME back on; use client mode_, not a separate GET_STATUS round-trip.
  if (mode_ == mozc::commands::DIRECT) {
    SessionCommand command;
    command.set_type(SessionCommand::TURN_ON_IME);
    command.set_composition_mode(mozc::commands::HIRAGANA);
    [self sendCommand:command];
    return YES;
  }

  KeyEvent key = keyEvent;
  key.set_mode(mode_);
  Output output;
  if (!mozcClient_->SendKey(key, &output)) {
    return NO;
  }
  [self processOutput:&output client:sender];
  return YES;
}

- (BOOL)tryMacronVowelChord:(NSEvent *)event client:(id)sender {
  if ([event type] != NSEventTypeKeyDown) {
    return NO;
  }

  NSUInteger ns_modifiers = [event modifierFlags];
  ns_modifiers &= (~NSEventModifierFlagCapsLock & NSEventModifierFlagDeviceIndependentFlagsMask);
  if (!IsCtrlOptionMacronChord(ns_modifiers)) {
    return NO;
  }

  const bool want_upper = (ns_modifiers & NSEventModifierFlagShift) != 0;
  NSString *chars = [event characters];
  NSString *raw = [event charactersIgnoringModifiers];
  unichar c = 0;

  // Use whatever the active keyboard layout reports (AZERTY 'a' is on Q, etc.).
  if (want_upper && chars != nil && [chars length] > 0) {
    c = [chars characterAtIndex:0];
  } else if (raw != nil && [raw length] > 0) {
    c = [raw characterAtIndex:0];
  } else if (chars != nil && [chars length] > 0) {
    c = [chars characterAtIndex:0];
  } else if ([event keyCode] == kVK_JIS_Kana) {
    // AZERTY Ctrl+Alt can emit a Hiragana keysym with no printable char (IBus does
    // the same); default to ā slot only in that edge case.
    c = want_upper ? 'A' : 'a';
  } else {
    return NO;
  }

  if (!IsMacronVowelLetter(c)) {
    return NO;
  }

  unichar lower = c;
  if (lower >= 'A' && lower <= 'Z') {
    lower = static_cast<unichar>(lower - 'A' + 'a');
  }
  const unichar key_char = want_upper ? static_cast<unichar>(lower - 'a' + 'A') : lower;

  SessionCommand command;
  command.set_type(SessionCommand::INSERT_MACRON_VOWEL);
  command.set_text(std::string(1, static_cast<char>(key_char)));
  [self sendCommand:command];

  if (MarinaImkTraceEnabled()) {
    LOG(INFO) << "[marinaImk] macron vowel=" << static_cast<char>(key_char)
              << " keyCode=" << [event keyCode] << " mode_=" << CompositionModeName(mode_);
  }
  return YES;
}

- (void)handleConfig {
  // Get the config and set client-side behaviors
  Config config;
  if (!mozcClient_->GetConfig(&config)) {
    LOG(ERROR) << "Cannot obtain the current config";
    return;
  }
  cachedMenuConfigValid_ = true;
  cachedUseTraditionalKanji_ = config.use_traditional_kanji();
  cachedPrivacyMode_ = config.incognito_mode();

  InputMode input_mode = ASCII;
  if (config.preedit_method() == Config::KANA) {
    input_mode = KANA;
  }
  [keyCodeMap_ setInputMode:input_mode];
  yenSignCharacter_ = config.yen_sign_character();

  if (config.use_japanese_layout()) {
    // Apple does not have "Japanese" layout actually -- here sets
    // "US" layout, which means US-ASCII layout or JIS layout
    // depending on which type of keyboard is actually connected.
    [[self client] overrideKeyboardWithKeyboardNamed:@"com.apple.keylayout.US"];
  }
}

- (void)setupClientBundle:(id)sender {
  NSString *bundleIdentifier = [sender bundleIdentifier];
  if (bundleIdentifier != nil && [bundleIdentifier length] > 0) {
    clientBundle_.assign([bundleIdentifier UTF8String]);
  }
}

- (void)setupCapability {
  Capability capability;

  if (CanSelectedRange(clientBundle_)) {
    capability.set_text_deletion(Capability::DELETE_PRECEDING_TEXT);
  } else {
    capability.set_text_deletion(Capability::NO_TEXT_DELETION_CAPABILITY);
  }

  mozcClient_->set_client_capability(capability);
}

// Mode changes to direct and clean up the status.
- (void)switchModeToDirect:(id)sender {
  mode_ = mozc::commands::DIRECT;
  PersistLastCompositionMode(mode_);
  DLOG(INFO) << "Mode switch: HIRAGANA, KATAKANA, etc. -> DIRECT";
  KeyEvent keyEvent;
  Output output;
  keyEvent.set_special_key(mozc::commands::KeyEvent::OFF);
  mozcClient_->SendKey(keyEvent, &output);
  if (output.has_result()) {
    [self commitText:output.result().value().c_str() client:sender];
  }
  if ([composedString_ length] > 0) {
    [self updateComposedString:nullptr];
    [self clearCandidates];
  }
}

- (void)switchMode:(CompositionMode)new_mode client:(id)sender {
  if (mode_ == new_mode) {
    return;
  }

  if (new_mode == mozc::commands::DIRECT) {
    // Turn off the IME and commit the composing text.
    DLOG(INFO) << "Mode switch: HIRAGANA, KATAKANA, etc. -> DIRECT";
    KeyEvent keyEvent;
    Output output;
    keyEvent.set_special_key(mozc::commands::KeyEvent::OFF);
    mozcClient_->SendKey(keyEvent, &output);
    if (output.has_result()) {
      [self commitText:output.result().value().c_str() client:sender];
    }
    if ([composedString_ length] > 0) {
      [self updateComposedString:nullptr];
      [self clearCandidates];
    }
    mode_ = mozc::commands::DIRECT;
    PersistLastCompositionMode(mode_);
    return;
  }

  if (mode_ == mozc::commands::DIRECT) {
    // Turn on the IME as the mode is changed from DIRECT to an active mode.
    DLOG(INFO) << "Mode switch: DIRECT -> HIRAGANA, KATAKANA, etc.";
    KeyEvent keyEvent;
    Output output;
    keyEvent.set_special_key(mozc::commands::KeyEvent::ON);
    mozcClient_->SendKey(keyEvent, &output);
  }

  // Switch composition mode.
  DLOG(INFO) << "Switch composition mode.";
  SessionCommand command;
  command.set_type(mozc::commands::SessionCommand::SWITCH_COMPOSITION_MODE);
  command.set_composition_mode(new_mode);
  Output output;
  mozcClient_->SendCommand(command, &output);
  mode_ = new_mode;
  PersistLastCompositionMode(mode_);
}

- (void)switchDisplayMode {
  if (!CanDisplayModeSwitch(clientBundle_)) {
    return;
  }

  absl::string_view mode_id = GetModeId(mode_);
  if (mode_id == lastDisplayModeId_) {
    return;
  }

  lastDisplayModeId_.assign(mode_id.data(), mode_id.size());
  syncingDisplayMode_ = true;
  [[self client] selectInputMode:[NSString stringWithUTF8String:mode_id.data()]];
  // |selectInputMode:| may call |-setValue:forTag:client:| asynchronously; keep
  // the guard set until the current event finishes.
  dispatch_async(dispatch_get_main_queue(), ^{
    syncingDisplayMode_ = false;
  });
}

- (void)commitText:(const char *)text client:(id)sender {
  if (text == nullptr) {
    return;
  }

  [sender insertText:[NSString stringWithUTF8String:text] replacementRange:replacementRange_];
  replacementRange_ = NSMakeRange(NSNotFound, 0);
}

- (void)launchWordRegisterDialog {
  [self launchWordRegisterTool:[self client] output:nullptr];
}

- (void)launchWordRegisterTool:(id)client {
  [self launchWordRegisterTool:client output:nullptr];
}

- (void)flushCompositionForToolLaunch:(id)sender {
  if (!imkClientForTest_) {
    mozc::commands::KeyEvent keyEvent;
    mozc::commands::Output output;
    keyEvent.set_special_key(mozc::commands::KeyEvent::ESCAPE);
    if (mozcClient_->SendKey(keyEvent, &output)) {
      [self processOutput:&output client:sender];
    }
  }
  [self updateComposedString:nullptr];
  [self clearCandidates];
  [originalString_ setString:@""];
  cursorPosition_ = -1;
}

- (void)clearClientCompositionUI {
  [self updateComposedString:nullptr];
  [self clearCandidates];
  [originalString_ setString:@""];
  cursorPosition_ = -1;
}

- (void)launchWordRegisterTool:(id)client
                        output:(const mozc::commands::Output *)output {
  mozc::mac::MozcImkNotifyToolLaunchStarting();

  // Toolbar, menu, and Ctrl+Shift+0 must share one path: ask the session for
  // prefill while converter state is still active. Do not flush (Escape) first.
  mozc::commands::Output launch_output;
  const mozc::commands::Output *prefill_output = nullptr;
  if (output != nullptr && output->has_launch_tool_mode() &&
      output->launch_tool_mode() == mozc::commands::Output::WORD_REGISTER_DIALOG &&
      (output->has_word_register_expression() ||
       output->word_register_reading_candidates_size() > 0)) {
    prefill_output = output;
  } else {
    mozc::commands::KeyEvent key;
    key.set_key_code('0');
    key.add_modifier_keys(mozc::commands::KeyEvent::CTRL);
    key.add_modifier_keys(mozc::commands::KeyEvent::SHIFT);
    if (mozcClient_->SendKey(key, &launch_output) &&
        launch_output.has_launch_tool_mode()) {
      prefill_output = &launch_output;
    }
  }

  if (prefill_output != nullptr) {
    [self clearClientCompositionUI];
    mozcClient_->LaunchToolWithProtoBuf(*prefill_output);
    return;
  }

  // Fallback when IME has no active prefill: use document selection.
  if (CanSelectedRange(clientBundle_)) {
    NSRange selectedRange = [client selectedRange];
    if (selectedRange.location != NSNotFound && selectedRange.length != NSNotFound &&
        selectedRange.length > 0) {
      NSString *text = [[client attributedSubstringFromRange:selectedRange] string];
      if (text != nil && [text length] > 0) {
        mozc::commands::Output selection_output;
        selection_output.set_launch_tool_mode(
            mozc::commands::Output::WORD_REGISTER_DIALOG);
        selection_output.set_word_register_expression([text UTF8String]);
        [self clearClientCompositionUI];
        mozcClient_->LaunchToolWithProtoBuf(selection_output);
        return;
      }
    }
  }

  [self clearClientCompositionUI];
  MacProcess::LaunchMozcTool("word_register_dialog");
}

- (void)invokeReconvert:(const SessionCommand *)command client:(id)sender {
  if (!CanSelectedRange(clientBundle_)) {
    return;
  }

  NSRange selectedRange = [sender selectedRange];
  if (selectedRange.location == NSNotFound || selectedRange.length == NSNotFound) {
    // the application does not support reconversion.
    return;
  }

  DLOG(INFO) << selectedRange.location << ", " << selectedRange.length;
  SessionCommand sending_command;
  Output output;
  sending_command = *command;

  if (selectedRange.length == 0) {
    // Currently no range is selected for reconversion.  Tries to
    // invoke UNDO instead.
    [self invokeUndo:sender];
    return;
  }

  if (!sending_command.has_text()) {
    NSString *text = [[sender attributedSubstringFromRange:selectedRange] string];
    if (!text) {
      return;
    }
    sending_command.set_text([text UTF8String]);
  }

  if (mozcClient_->SendCommand(sending_command, &output)) {
    replacementRange_ = selectedRange;
    [self processOutput:&output client:sender];
  }
}

- (void)invokeUndo:(id)sender {
  if (!CanSelectedRange(clientBundle_)) {
    return;
  }

  NSRange selectedRange = [sender selectedRange];
  if (selectedRange.location == NSNotFound || selectedRange.length == NSNotFound ||
      // Some applications such like iTunes does not return NSNotFound
      // range but (0, 0).  However, the range starting with negative
      // location has to be invalid, then we can reject such apps.
      selectedRange.location == 0) {
    return;
  }

  DLOG(INFO) << selectedRange.location << ", " << selectedRange.length;
  SessionCommand command;
  Output output;
  command.set_type(SessionCommand::UNDO);
  if (mozcClient_->SendCommand(command, &output)) {
    [self processOutput:&output client:sender];
  }
}

namespace {
// Shin/kyū, privacy mode, etc. return config without touching the candidate list.
bool IsConfigOnlySessionOutput(const Output &output) {
  return output.has_config() && !output.has_result() && !output.has_candidate_window();
}
}  // namespace

- (void)applyCommitAndPreeditFromOutput:(const Output *)output
                                 client:(id)sender
                allowClearWithoutPreedit:(BOOL)allowClearWithoutPreedit {
  if (output == nullptr) {
    return;
  }
  if (output->has_result()) {
    [self commitText:output->result().value().c_str() client:sender];
  }
  if (output->has_preedit()) {
    if (output->preedit().segment_size() == 0) {
      [self updateComposedString:nullptr];
    } else {
      [self updateComposedString:&(output->preedit())];
    }
  } else if (output->has_result()) {
    // Server commit with no preedit field: clear marked text. Otherwise switching
    // input (e.g. to Dvorak) can flush composedString_ and insert a duplicate.
    [self updateComposedString:nullptr];
    [self clearCandidates];
  } else if (allowClearWithoutPreedit && !IsConfigOnlySessionOutput(*output)) {
    // Escape/Cancel (consumed): server clears composition but often omits preedit;
    // drop stale marked text so Word does not keep a ghost character on IME off.
    // Do not run this for consumed=false echo-back (e.g. Precomposition Backspace
    // → Revert): upstream Mozc leaves marked text alone and returns NO from
    // handleEvent so one character is removed, not the whole preedit.
    [self updateComposedString:nullptr];
  }
}

- (void)syncCandidatesWithOutput:(const Output *)output {
  if (output == nullptr) {
    [self clearCandidates];
    return;
  }
  // IME off (e.g. after commit then Ctrl+Shift+5): server may still attach zero-query
  // candidates in the same output; always hide the renderer.
  if (output->has_status() && !output->status().activated()) {
    [self clearCandidates];
    return;
  }
  if (output->has_mode() && output->mode() == mozc::commands::DIRECT) {
    [self clearCandidates];
    return;
  }
  if (output->has_candidate_window()) {
    [self updateCandidates:output];
    return;
  }
  // Shin/kyū and similar: do not dismiss an open conversion list.
  if (IsConfigOnlySessionOutput(*output)) {
    return;
  }
  // Commit, Escape, delete-to-empty: response omits candidate_window — hide stale UI.
  [self clearCandidates];
}

- (void)processOutput:(const mozc::commands::Output *)output client:(id)sender {
  if (output == nullptr) {
    return;
  }
  if (processOutputDepth_ > 12) {
    LOG(ERROR) << "[marinaImk] processOutput depth limit exceeded; dropping";
    return;
  }
  ++processOutputDepth_;
  if (MarinaImkTraceEnabled()) {
    LOG(INFO) << "[marinaImk] processOutput depth=" << processOutputDepth_
              << " consumed=" << output->consumed() << " mode_=" << CompositionModeName(mode_);
  }
  // LaunchWordRegisterDialog returns DoNothing + Output(); applying that here
  // clears marked text without committing and leaves candidates stuck open.
  if (output->consumed() && output->has_launch_tool_mode() &&
      output->launch_tool_mode() == mozc::commands::Output::WORD_REGISTER_DIALOG) {
    [self launchWordRegisterTool:sender output:output];
    --processOutputDepth_;
    return;
  }

  if (output->consumed() && output->has_launch_tool_mode() &&
      output->launch_tool_mode() == mozc::commands::Output::DICTIONARY_TOOL) {
    [self flushCompositionForToolLaunch:sender];
  }

  const bool launching_tool = OutputLaunchesMozcTool(*output);
  if (launching_tool) {
    mozc::mac::MozcImkNotifyToolLaunchStarting();
  }

  if (!output->consumed()) {
    [self applyCommitAndPreeditFromOutput:output
                                   client:sender
                  allowClearWithoutPreedit:NO];
    if (!launching_tool && (output->has_status() || output->has_mode())) {
      CompositionMode new_mode = NormalizeModeForEmptyHalfAscii(
          EffectiveCompositionMode(*output, mode_), *output);
      if (ShouldPreserveClientCompositionMode(*output, mode_, new_mode)) {
        new_mode = mode_;
      }
      mode_ = new_mode;
      mozc::mac::MozcToolbarShow(mozcClient_.get(), mode_);
    }
    [self syncCandidatesWithOutput:output];
    if (launching_tool) {
      mozc::mac::MozcToolbarShow(mozcClient_.get(), mode_);
    } else {
      mozc::mac::MozcToolbarUpdate(*output, mode_);
    }
    [self updateImeMenuState:output];
    --processOutputDepth_;
    return;
  }

  DLOG(INFO) << output->Utf8DebugString();
  if (output->has_url()) {
    NSString *url = [NSString stringWithUTF8String:output->url().c_str()];
    [self openLink:[NSURL URLWithString:url]];
  }

  [self applyCommitAndPreeditFromOutput:output
                                 client:sender
                allowClearWithoutPreedit:YES];

  // Handles deletion range.  We do not even handle it for some
  // applications to prevent application crashes.
  if (output->has_deletion_range() && CanSelectedRange(clientBundle_)) {
    if ([composedString_ length] == 0 && replacementRange_.location == NSNotFound) {
      NSRange selectedRange = [sender selectedRange];
      const mozc::commands::DeletionRange &deletion_range = output->deletion_range();
      if (selectedRange.location != NSNotFound || selectedRange.length != NSNotFound ||
          selectedRange.location + deletion_range.offset() > 0) {
        // The offset is a negative value.  See protocol/commands.proto for
        // the details.
        selectedRange.location += deletion_range.offset();
        selectedRange.length += deletion_range.length();
        replacementRange_ = selectedRange;
      }
    } else {
      // We have to consider the case that there is already
      // composition and/or we already set the position of the
      // composition by replacementRange_.  We do nothing here at this
      // time because we already found that it will involve several
      // buggy behaviors with Carbon apps and MS Office.
      // TODO(mukai): find the right behavior.
    }
  }

  [self syncCandidatesWithOutput:output];

  if (!launching_tool && (output->has_mode() || output->has_status())) {
    CompositionMode new_mode = NormalizeModeForEmptyHalfAscii(
        EffectiveCompositionMode(*output, mode_), *output);
    if (ShouldPreserveClientCompositionMode(*output, mode_, new_mode)) {
      new_mode = mode_;
    }
    // Do not allow HALF_ASCII with empty composition.  This should be
    // handled in the converter, but just in case.
    if (new_mode != mode_) {
      if (MarinaImkTraceEnabled()) {
        LOG(INFO) << "[marinaImk] processOutput mode " << CompositionModeName(mode_)
                  << " -> " << CompositionModeName(new_mode);
      }
      mode_ = new_mode;
      PersistLastCompositionMode(mode_);
      if (handlingKeyboardEvent_) {
        suppressSetValueUntil_ = [[NSDate date] timeIntervalSinceReferenceDate] + 0.2;
      }
      // Do not call |-switchDisplayMode| here.  Keyboard-driven mode changes
      // (e.g. Ctrl+Shift+5 / ToggleHiraganaDirect) used to call
      // |selectInputMode:|, which re-entered |-setValue:forTag:client:| and
      // could freeze the session.  Toolbar + |mode_| are updated below; the
      // system menu icon stays on the visible marinaMoji entry (see M8).
    }
  }

  if (launching_tool) {
    switch (output->launch_tool_mode()) {
      case mozc::commands::Output::CONFIG_DIALOG:
        MacProcess::LaunchMozcTool("config_dialog");
        break;
      case mozc::commands::Output::DICTIONARY_TOOL:
        MacProcess::LaunchMozcTool("dictionary_tool");
        break;
      case mozc::commands::Output::WORD_REGISTER_DIALOG:
        // Handled above before applyCommitAndPreedit.
        break;
      default:
        // do nothing
        break;
    }
  }

  if (launching_tool) {
    mozc::mac::MozcToolbarShow(mozcClient_.get(), mode_);
  } else {
    mozc::mac::MozcToolbarUpdate(*output, mode_);
  }
  [self updateImeMenuState:output];

  // Handle callbacks.
  if (output->has_callback() && output->callback().has_session_command()) {
    const SessionCommand &callback_command = output->callback().session_command();
    if (callback_command.type() == SessionCommand::CONVERT_REVERSE) {
      [self invokeReconvert:&callback_command client:sender];
    } else if (callback_command.type() == SessionCommand::UNDO) {
      [self invokeUndo:sender];
    } else {
      Output output_for_callback;
      if (mozcClient_->SendCommand(callback_command, &output_for_callback)) {
        [self processOutput:&output_for_callback client:sender];
      }
    }
  }

  --processOutputDepth_;
}

#pragma mark Mozc Server methods

#pragma mark IMKServerInput Protocol
// Currently MozcImkInputController uses handleEvent:client:
// method to handle key events.  It does not support inputText:client:
// nor inputText:key:modifiers:client:.
// Because MozcImkInputController does not use IMKCandidates,
// the following methods are not needed to implement:
//   candidates
//
// The meaning of these methods are described at:
// https://developer.apple.com/documentation/inputmethodkit/imkserverinput?language=objc

- (id)originalString:(id)sender {
  return originalString_;
}

- (void)updateComposedString:(const Preedit *)preedit {
  // If the last and the current composed string length is 0,
  // we don't update the composition.
  if (([composedString_ length] == 0) && ((preedit == nullptr || preedit->segment_size() == 0))) {
    return;
  }

  [composedString_ deleteCharactersInRange:NSMakeRange(0, [composedString_ length])];
  cursorPosition_ = -1;
  if (preedit != nullptr) {
    cursorPosition_ = preedit->cursor();
    for (size_t i = 0; i < preedit->segment_size(); ++i) {
      NSDictionary *highlightAttributes = [self markForStyle:kTSMHiliteSelectedConvertedText
                                                     atRange:NSMakeRange(NSNotFound, 0)];
      NSDictionary *underlineAttributes = [self markForStyle:kTSMHiliteConvertedText
                                                     atRange:NSMakeRange(NSNotFound, 0)];
      const Preedit::Segment &seg = preedit->segment(static_cast<int32_t>(i));
      NSDictionary *attr = (seg.annotation() == Preedit::Segment::HIGHLIGHT) ? highlightAttributes
                                                                             : underlineAttributes;
      NSString *seg_string = [NSString stringWithUTF8String:seg.value().c_str()];
      NSAttributedString *seg_attributed_string =
          [[NSAttributedString alloc] initWithString:seg_string attributes:attr];
      [composedString_ appendAttributedString:seg_attributed_string];
    }
  }
  if ([composedString_ length] == 0) {
    mozc::sync::RecordCompositionEnd();
    [originalString_ setString:@""];
    replacementRange_ = NSMakeRange(NSNotFound, 0);
  }

  // Update the composed string of the client applications.
  [[self client] setMarkedText:composedString_
                selectionRange:[self selectionRange]
              replacementRange:replacementRange_];
}

- (void)commitComposition:(id)sender {
  if ([composedString_ length] == 0) {
    DLOG(INFO) << "Nothing is committed.";
    return;
  }
  [self commitText:[[composedString_ string] UTF8String] client:sender];

  SessionCommand command;
  Output output;
  command.set_type(SessionCommand::SUBMIT);
  mozcClient_->SendCommand(command, &output);
  [self clearCandidates];
  [self updateComposedString:nullptr];
}

- (id)composedString:(id)sender {
  return composedString_;
}

- (void)cancelPendingCandidateUpdate {
  [NSObject cancelPreviousPerformRequestsWithTarget:self
                                           selector:@selector(delayedUpdateCandidates)
                                             object:nil];
}

- (void)clearCandidates {
  [self cancelPendingCandidateUpdate];
  rendererCommand_.set_type(RendererCommand::UPDATE);
  rendererCommand_.set_visible(false);
  rendererCommand_.clear_output();
  if (mozcRenderer_) {
    mozcRenderer_->ExecCommand(rendererCommand_);
  }
}

// |selecrionRange| method is defined at IMKInputController class and
// means the position of cursor actually.
- (NSRange)selectionRange {
  if (imkClientForTest_) {
    return NSMakeRange(cursorPosition_, 0);
  }

  return (cursorPosition_ == -1)
             ? [super selectionRange]  // default behavior defined at super class
             : NSMakeRange(cursorPosition_, 0);
}

- (void)delayedUpdateCandidates {
  if (!imeServerActive_ || !mozcRenderer_) {
    return;
  }

  // If there is no candidate, the candidate window is closed.
  if (rendererCommand_.output().candidate_window().candidate_size() == 0) {
    rendererCommand_.set_visible(false);
    mozcRenderer_->ExecCommand(rendererCommand_);
    return;
  }

  // The candidate window position is not recalculated if the
  // candidate already appears on the screen.  Therefore, if a user
  // moves client application window by mouse, candidate window won't
  // follow the move of window.  This is done because:
  //  - some applications like Emacs or Google Chrome don't return the
  //    cursor position correctly.  The candidate window moves
  //    frequently with those application, which irritates users.
  //  - Kotoeri does this too.
  if (rendererCommand_.visible()) {
    // Call ExecCommand anyway to update other information like candidate words.
    mozcRenderer_->ExecCommand(rendererCommand_);
    return;
  }

  rendererCommand_.set_visible(true);

  NSRect preeditRect = NSZeroRect;
  const int32_t position = rendererCommand_.output().candidate_window().position();
  // Some applications throws error when we call attributesForCharacterIndex.
  DLOG(INFO) << "attributesForCharacterIndex: " << position;
  @try {
    NSDictionary *clientData = [[self client] attributesForCharacterIndex:position
                                                      lineHeightRectangle:&preeditRect];

    // IMKBaseline: Left-bottom of the composition.
    NSPoint baseline = [clientData[@"IMKBaseline"] pointValue];
    // IMKTextOrientation: 0: vertical writing, 1: horizontal writing.
    // IMKLineHeight: Height of the composition (in horizontal writing).
    // NSFont: Font information of the composition.
    // IMKLineAscent: Not sure. A float number. (e.g. 9.240234)

    const int right_offset = preeditRect.size.width;
    const int top_offset = -preeditRect.size.height;

    RendererCommand::Rectangle *rect = rendererCommand_.mutable_preedit_rectangle();
    rect->set_left(baseline.x);
    rect->set_right(baseline.x + right_offset);
    rect->set_top(baseline.y + top_offset);
    rect->set_bottom(baseline.y);

  } @catch (NSException *exception) {
    LOG(ERROR) << "Exception from [" << clientBundle_ << "] " << [[exception name] UTF8String]
               << "," << [[exception reason] UTF8String];
  }

  mozcRenderer_->ExecCommand(rendererCommand_);
}

- (void)updateCandidates:(const Output *)output {
  if (output == nullptr) {
    [self clearCandidates];
    return;
  }

  rendererCommand_.set_type(RendererCommand::UPDATE);
  *rendererCommand_.mutable_output() = *output;

  // Runs delayedUpdateCandidates in the next event loop.
  // This is because some applications like Google Docs with Chrome returns
  // incorrect cursor position if we call attributesForCharacterIndex here.
  [self cancelPendingCandidateUpdate];
  [self performSelector:@selector(delayedUpdateCandidates) withObject:nil afterDelay:0];
}

- (void)openLink:(NSURL *)url {
  // Open a link specified by |url|.  Any opening link behavior should
  // call this method because it checks the capability of application.
  // On some application like login window of screensaver, opening
  // link behavior should not happen because it can cause some
  // security issues.
  if (CanOpenLink(clientBundle_)) {
    [[NSWorkspace sharedWorkspace] openURL:url];
  }
}

- (BOOL)fillSurroundingContext:(mozc::commands::Context *)context client:(id<IMKTextInput>)client {
  NSInteger totalLength = [client length];
  if (totalLength == 0 || totalLength == NSNotFound ||
      totalLength > kGetSurroundingTextClientLengthLimit) {
    return false;
  }
  NSRange selectedRange = [client selectedRange];
  if (selectedRange.location == NSNotFound || selectedRange.length == NSNotFound) {
    return false;
  }
  NSRange precedingRange = NSMakeRange(0, selectedRange.location);
  if (selectedRange.location > kMaxSurroundingLength) {
    precedingRange =
        NSMakeRange(selectedRange.location - kMaxSurroundingLength, kMaxSurroundingLength);
  }
  NSString *precedingString = [[client attributedSubstringFromRange:precedingRange] string];
  if (precedingString) {
    context->set_preceding_text([precedingString UTF8String]);
    DLOG(INFO) << "preceding_text: \"" << context->preceding_text() << "\"";
  }
  return true;
}

- (BOOL)handleEvent:(NSEvent *)event client:(id)sender {
  if (event == nullptr || [event isEqual:[NSNull null]]) {
    return NO;
  }

  handlingKeyboardEvent_ = true;
  BOOL handled = NO;
  @try {
    handled = [self handleEventBody:event client:sender];
  } @finally {
    handlingKeyboardEvent_ = false;
  }
  return handled;
}

- (BOOL)handleEventBody:(NSEvent *)event client:(id)sender {
  if (mozc::mac::SyncOverlayIsActive()) {
    if (rendererCommand_.visible() && mozcRenderer_) {
      rendererCommand_.set_visible(false);
      mozcRenderer_->ExecCommand(rendererCommand_);
    }
    mozc::mac::SyncOverlayFlashBlockedInput();
    return YES;
  }
  if ([event type] == NSEventTypeCursorUpdate) {
    [[self client] setMarkedText:composedString_
                  selectionRange:[self selectionRange]
                replacementRange:replacementRange_];
    return NO;
  }
  if ([event type] != NSEventTypeKeyDown && [event type] != NSEventTypeFlagsChanged) {
    return NO;
  }

  // Handle KANA key and EISU key.  We explicitly handles this here
  // for mode switch because some text area such like iPhoto person
  // name editor does not call setValue:forTag:client: method.
  // see:
  // http://www.google.com/support/forum/p/ime/thread?tid=3aafb74ff71a1a69&hl=ja&fid=3aafb74ff71a1a690004aa3383bc9f5d
  if ([event type] == NSEventTypeKeyDown) {
    NSTimeInterval currentTime = [[NSDate date] timeIntervalSince1970];
    const NSTimeInterval elapsedTime = currentTime - lastKeyDownTime_;
    const bool isDoubleTap =
        ([event keyCode] == lastKeyCode_) && (elapsedTime < kDoubleTapInterval);
    lastKeyDownTime_ = currentTime;
    lastKeyCode_ = [event keyCode];

    // these calling of switchMode: can be duplicated if the
    // application sends the setValue:forTag:client: and handleEvent:
    // at the same key event, but that's okay because switchMode:
    // method does nothing if the new mode is same as the current
    // mode.
    if ([event keyCode] == kVK_JIS_Kana) {
      [self switchMode:mozc::commands::HIRAGANA client:sender];
      mozc::mac::MozcToolbarShow(mozcClient_.get(), mode_);
      if (isDoubleTap) {
        SessionCommand command;
        command.set_type(SessionCommand::CONVERT_REVERSE);
        [self invokeReconvert:&command client:sender];
      }
    } else if ([event keyCode] == kVK_JIS_Eisu) {
      if (isDoubleTap) {
        SessionCommand command;
        command.set_type(SessionCommand::COMMIT_RAW_TEXT);
        [self sendCommand:command];
      }
      CompositionMode new_mode =
          ([composedString_ length] == 0) ? mozc::commands::DIRECT : mozc::commands::HALF_ASCII;
      [self switchMode:new_mode client:sender];
      mozc::mac::MozcToolbarShow(mozcClient_.get(), mode_);
    }
  }

  if ([keyCodeMap_ isModeSwitchingKey:event]) {
    // Special hack for Eisu/Kana keys.  Sometimes those key events
    // come to this method but we should ignore them because some
    // applications like PhotoShop is stuck.
    return YES;
  }

  // Ctrl+Left Shift alone → ToggleLeftShiftModeLock (Linux IBus parity).
  if ([event type] == NSEventTypeFlagsChanged) {
    KeyEvent ctrlLeftShiftKey;
    if ([keyCodeMap_ tryCtrlLeftShiftModeLockFromEvent:event
                                       toMozcKeyEvent:&ctrlLeftShiftKey]) {
      return [self dispatchCtrlLeftShiftModeLock:ctrlLeftShiftKey client:sender];
    }
  }

  // Left Shift alone on release → ToggleLeftShiftDirect (Linux IBus parity).
  if ([event keyCode] == kVK_Shift && [event type] == NSEventTypeFlagsChanged) {
    KeyEvent leftShiftKey;
    if ([keyCodeMap_ tryLeftShiftAloneKeyFromEvent:event toMozcKeyEvent:&leftShiftKey]) {
      return [self dispatchLeftShiftAlone:leftShiftKey client:sender];
    }
    return YES;
  }

  // Right Shift alone on release → ToggleManyoshuHiragana (Linux IBus parity).
  if ([event keyCode] == kVK_RightShift && [event type] == NSEventTypeFlagsChanged) {
    KeyEvent rightShiftKey;
    if ([keyCodeMap_ tryRightShiftAloneKeyFromEvent:event toMozcKeyEvent:&rightShiftKey]) {
      return [self dispatchRightShiftAlone:rightShiftKey client:sender];
    }
    return YES;
  }

  // AZERTY/Dvorak: macOS may deliver separate KeyDown / FlagsChanged events for
  // Control, Shift, Command, etc. Swallow them so the app does not beep.
  if (IsPhysicalModifierKeyCode([event keyCode]) &&
      ([event type] == NSEventTypeKeyDown || [event type] == NSEventTypeFlagsChanged)) {
    return YES;
  }

  // Ctrl+Option(+Shift)+vowel: layout-aware macron (AZERTY/Dvorak); bypass KeyCodeMap.
  if ([self tryMacronVowelChord:event client:sender]) {
    return YES;
  }

  // Get the Mozc key event
  KeyEvent keyEvent;
  if (![keyCodeMap_ getMozcKeyCodeFromKeyEvent:event toMozcKeyEvent:&keyEvent]) {
    // Modifier flags change (not submitted to the server yet), or
    // unsupported key pressed.
    if (MarinaImkTraceEnabled()) {
      LOG(INFO) << "[marinaImk] handleEvent keyCode=" << [event keyCode]
                << " no mozc mapping (beep)";
    }
    return NO;
  }

  if (MarinaImkTraceEnabled() && keyEvent.has_key_code()) {
    LOG(INFO) << "[marinaImk] handleEvent keyCode=" << [event keyCode]
              << " mozc=" << static_cast<char>(keyEvent.key_code())
              << " mode_=" << CompositionModeName(mode_);
  }

  mozc::commands::Context context;
  if (suppressSuggestion_) {
    context.add_experimental_features("google_search_box");
  }

  // Swallow autorepeat for marina Ctrl+Shift shortcuts (shin/kyū, odoriji, etc.).
  // Otherwise one key hold fires ToggleTraditionalKanji many times (rapid flip / freeze).
  if ([event isARepeat] &&
      (KeyEventHasCtrlShift(keyEvent) ||
       mozc::session::KeyEventHasCtrlOnly(keyEvent))) {
    return YES;
  }

  if (IsMarinaTraditionalKanjiShortcut(keyEvent)) {
    return [self dispatchMarinaTraditionalKanjiShortcut:sender];
  }

  if (IsMarinaBacktickShortcut(keyEvent)) {
    return [self dispatchMarinaBacktickShortcut:keyEvent client:sender];
  }

  {
    Config marina_config;
    if (mozcClient_->GetConfig(&marina_config) &&
        IsMarinaConfigurableNumberRowShortcut(keyEvent, marina_config)) {
      return [self dispatchMarinaNumberRowShortcut:keyEvent client:sender];
    }
  }

  // If the key event is turn on event, the key event has to be sent
  // to the server anyway.
  if (mode_ == mozc::commands::DIRECT && !mozcClient_->IsDirectModeCommand(keyEvent)) {
    // Yen sign special hack: although the current mode is DIRECT,
    // backslash is sent instead of yen sign for JIS yen key with no
    // modifiers.  This behavior is based on the configuration.
    if ([event keyCode] == kVK_JIS_Yen && [event modifierFlags] == 0 &&
        yenSignCharacter_ == mozc::config::Config::BACKSLASH) {
      [self commitText:"\\" client:sender];
      return YES;
    }
    return NO;
  }

  // Send the key event to the server actually
  Output output;

  if (isprint(keyEvent.key_code())) {
    [originalString_ appendFormat:@"%c", keyEvent.key_code()];
  }

  keyEvent.set_mode(mode_);

  if (mode_ != mozc::commands::DIRECT && ![self isConverterSessionActivated]) {
    if (![self ensureConverterActivated:sender context:&context]) {
      return NO;
    }
  }

  if ([composedString_ length] == 0 && CanSelectedRange(clientBundle_) &&
      CanSurroundingText(clientBundle_)) {
    [self fillSurroundingContext:&context client:sender];
  }
  if (!mozcClient_->SendKeyWithContext(keyEvent, context, &output)) {
    return NO;
  }

  [self processOutput:&output client:sender];
  return output.consumed();
}

#pragma mark ControllerCallback
- (void)sendCommand:(const SessionCommand &)command {
  Output output;
  if (!mozcClient_->SendCommand(command, &output)) {
    return;
  }
  [self processOutput:&output client:[self client]];
}

- (void)outputResult:(const mozc::commands::Output &)output {
  if (!output.has_result()) {
    return;
  }
  [self commitText:output.result().value().c_str() client:[self client]];
}

#pragma mark callbacks
- (IBAction)reconversionClicked:(id)sender {
  SessionCommand command;
  command.set_type(SessionCommand::CONVERT_REVERSE);
  [self invokeReconvert:&command client:[self client]];
}

- (IBAction)configClicked:(id)sender {
  mozc::mac::MozcImkNotifyToolLaunchStarting();
  MacProcess::LaunchMozcTool("config_dialog");
}

- (IBAction)dictionaryToolClicked:(id)sender {
  mozc::mac::MozcImkNotifyToolLaunchStarting();
  [self flushCompositionForToolLaunch:[self client]];
  MacProcess::LaunchMozcTool("dictionary_tool");
}

- (IBAction)registerWordClicked:(id)sender {
  [self launchWordRegisterTool:[self client]];
}

- (IBAction)aboutDialogClicked:(id)sender {
  MacProcess::LaunchMozcTool("about_dialog");
}

- (IBAction)toolbarVisibilityMenuClicked:(id)sender {
  (void)sender;
  const bool visible = !mozc::mac::MozcToolbarLoadVisiblePreference();
  mozc::mac::MozcToolbarSaveVisiblePreference(visible);
  if (visible) {
    mozc::mac::MozcToolbarShow(mozcClient_.get(), mode_);
  } else {
    mozc::mac::MozcToolbarHide();
  }
  [self updateImeMenuState:nullptr];
}

- (IBAction)inputModeMenuClicked:(NSMenuItem *)sender {
#if 0  // Input Mode submenu disabled (see setupMarinaImeMenuIfNeeded).
  const auto mode = static_cast<CompositionMode>(sender.tag);
  SessionCommand command;
  if (mode == mozc::commands::DIRECT) {
    command.set_type(SessionCommand::TURN_OFF_IME);
  } else {
    command.set_type(SessionCommand::SWITCH_COMPOSITION_MODE);
    CompositionMode server_mode = mode;
    if (server_mode == mozc::commands::FULL_KATAKANA) {
      server_mode = mozc::commands::MANYOSHU;
    }
    command.set_composition_mode(server_mode);
  }
  [self sendCommand:command];
#endif
  (void)sender;
}

- (IBAction)traditionalKanjiMenuClicked:(id)sender {
  (void)sender;
  SessionCommand command;
  command.set_type(SessionCommand::TOGGLE_TRADITIONAL_KANJI);
  [self sendCommand:command];
}

- (IBAction)odorijiPaletteMenuClicked:(id)sender {
  (void)sender;
  SessionCommand command;
  command.set_type(SessionCommand::SHOW_ODORIJI_PALETTE);
  [self sendCommand:command];
}

- (IBAction)privacyModeMenuClicked:(id)sender {
  (void)sender;
  SessionCommand command;
  command.set_type(SessionCommand::TOGGLE_PRIVACY_MODE);
  [self sendCommand:command];
}

+ (void)setGlobalRendererReceiver:(RendererReceiver *)rendererReceiver {
  gRendererReceiver = rendererReceiver;
}
@end

// An alias of MozcImkInputController for backward compatibility.
@implementation GoogleJapaneseInputController
@end
