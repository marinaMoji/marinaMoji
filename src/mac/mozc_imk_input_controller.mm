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

#import "mac/mozc_imk_input_controller.h"

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <InputMethodKit/IMKInputController.h>
#import <InputMethodKit/IMKServer.h>

#include <unistd.h>

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
#include "absl/strings/string_view.h"
#include "base/const.h"
#include "base/mac/mac_process.h"
#include "base/mac/mac_util.h"
#include "base/process.h"
#include "base/util.h"
#include "client/client.h"
#include "ipc/ipc.h"
#include "protocol/candidate_window.pb.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "mac/mozc_toolbar.h"
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

// Maps server mode to the IBUS-style input-mode menu (Manyōshū → Katakana).
CompositionMode CompositionModeForImeMenu(CompositionMode mode) {
  if (mode == mozc::commands::MANYOSHU) {
    return mozc::commands::FULL_KATAKANA;
  }
  return mode;
}

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

// Ctrl+Shift+US number-row slot (1..5) after KeyCodeMap physical-key normalization.
bool IsMarinaNumberRowSlot(const KeyEvent &key, char slot_digit) {
  return KeyEventHasCtrlShift(key) && key.has_key_code() &&
         key.key_code() == static_cast<uint32_t>(slot_digit);
}

bool IsMarinaNumberRowShortcut(const KeyEvent &key) {
  return IsMarinaNumberRowSlot(key, '1') || IsMarinaNumberRowSlot(key, '2') ||
         IsMarinaNumberRowSlot(key, '3') || IsMarinaNumberRowSlot(key, '4') ||
         IsMarinaNumberRowSlot(key, '5');
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
}  // namespace

@interface MozcImkInputController (MarinaPrivate)
- (BOOL)isConverterSessionActivated;
- (BOOL)dispatchMarinaNumberRowShortcut:(const KeyEvent &)keyEvent client:(id)sender;
- (BOOL)tryMacronVowelChord:(NSEvent *)event client:(id)sender;
- (void)setupMarinaImeMenuIfNeeded;
- (void)updateImeMenuState:(const Output *)output;
@end

@implementation MozcImkInputController
#pragma mark accessors for testing
@synthesize keyCodeMap = keyCodeMap_;
@synthesize yenSignCharacter = yenSignCharacter_;
@synthesize mode = mode_;
@synthesize rendererCommand = rendererCommand_;
@synthesize replacementRange = replacementRange_;
@synthesize imkClientForTest = imkClientForTest_;
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
  syncingDisplayMode_ = false;
  handlingKeyboardEvent_ = false;
  suppressSetValueUntil_ = 0;
  processOutputDepth_ = 0;
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

- (NSString *)imeMenuTitle:(const char *)english {
  static NSDictionary<NSString *, NSString *> *kJapaneseTitles;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    kJapaneseTitles = @{
      @"Input Mode" : @"入力モード",
      @"Direct input" : @"直接入力",
      @"Hiragana" : @"ひらがな",
      @"Katakana" : @"カタカナ",
      @"Latin" : @"半角英数",
      @"Wide Latin" : @"全角英数",
      @"Half width katakana" : @"半角カタカナ",
      @"Traditional kanji (Kyūjitai)" : @"伝統漢字（旧字体）",
      @"Odoriji (iteration marks)" : @"踊り字（繰り返し記号）",
      @"Toolbar" : @"ツールバー",
      @"Privacy mode" : @"プライバシーモード",
    };
  });
  NSString *key = [NSString stringWithUTF8String:english];
  if ([[[NSLocale currentLocale] languageCode] isEqualToString:@"ja"]) {
    NSString *localized = kJapaneseTitles[key];
    if (localized != nil) {
      return localized;
    }
  }
  return key;
}

- (void)setupMarinaImeMenuIfNeeded {
  if (!menu_ || traditionalKanjiMenuItem_ != nil) {
    return;
  }

  // IBUS panel order: Input Mode, Traditional kanji, Odoriji, Toolbar (property_handler.cc).
  NSInteger insertIndex = 0;

  struct ModeMenuEntry {
    const char *title;
    CompositionMode mode;
  };
  constexpr ModeMenuEntry kModeEntries[] = {
      {"Direct input", mozc::commands::DIRECT},
      {"Hiragana", mozc::commands::HIRAGANA},
      {"Katakana", mozc::commands::FULL_KATAKANA},
      {"Latin", mozc::commands::HALF_ASCII},
      {"Wide Latin", mozc::commands::FULL_ASCII},
      {"Half width katakana", mozc::commands::HALF_KATAKANA},
  };

  NSMenu *modeMenu =
      [[NSMenu alloc] initWithTitle:[self imeMenuTitle:"Input Mode"]];
  NSMutableArray<NSMenuItem *> *modeItems = [NSMutableArray array];
  for (const ModeMenuEntry &entry : kModeEntries) {
    NSMenuItem *item = [[NSMenuItem alloc]
        initWithTitle:[self imeMenuTitle:entry.title]
               action:@selector(inputModeMenuClicked:)
        keyEquivalent:@""];
    item.target = self;
    item.tag = static_cast<NSInteger>(entry.mode);
    [modeMenu addItem:item];
    [modeItems addObject:item];
  }
  inputModeMenuItems_ = [modeItems copy];

  NSMenuItem *modeMenuItem = [[NSMenuItem alloc]
      initWithTitle:[self imeMenuTitle:"Input Mode"]
             action:nil
      keyEquivalent:@""];
  modeMenuItem.submenu = modeMenu;
  [menu_ insertItem:modeMenuItem atIndex:insertIndex++];
  [menu_ insertItem:[NSMenuItem separatorItem] atIndex:insertIndex++];

  traditionalKanjiMenuItem_ = [[NSMenuItem alloc]
      initWithTitle:[self imeMenuTitle:"Traditional kanji (Kyūjitai)"]
             action:@selector(traditionalKanjiMenuClicked:)
      keyEquivalent:@""];
  traditionalKanjiMenuItem_.target = self;
  [menu_ insertItem:traditionalKanjiMenuItem_ atIndex:insertIndex++];

  NSMenuItem *odorijiItem = [[NSMenuItem alloc]
      initWithTitle:[self imeMenuTitle:"Odoriji (iteration marks)"]
             action:@selector(odorijiPaletteMenuClicked:)
      keyEquivalent:@""];
  odorijiItem.target = self;
  [menu_ insertItem:odorijiItem atIndex:insertIndex++];

  if (toolbarMenuItem_ != nil) {
    [menu_ removeItem:toolbarMenuItem_];
  }
  toolbarMenuItem_ = [[NSMenuItem alloc]
      initWithTitle:[self imeMenuTitle:"Toolbar"]
             action:@selector(toolbarVisibilityMenuClicked:)
      keyEquivalent:@""];
  toolbarMenuItem_.target = self;
  [menu_ insertItem:toolbarMenuItem_ atIndex:insertIndex++];

  privacyModeMenuItem_ = [[NSMenuItem alloc]
      initWithTitle:[self imeMenuTitle:"Privacy mode"]
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
    } else if (mozcClient_ != nullptr) {
      Config config;
      if (mozcClient_->GetConfig(&config)) {
        use_trad = config.use_traditional_kanji();
      }
    }
    traditionalKanjiMenuItem_.state =
        use_trad ? NSControlStateValueOn : NSControlStateValueOff;
  }

  if (privacyModeMenuItem_) {
    bool privacy_on = false;
    if (output != nullptr && output->has_config()) {
      privacy_on = output->config().incognito_mode();
    } else if (mozcClient_ != nullptr) {
      Config config;
      if (mozcClient_->GetConfig(&config)) {
        privacy_on = config.incognito_mode();
      }
    }
    privacyModeMenuItem_.state =
        privacy_on ? NSControlStateValueOn : NSControlStateValueOff;
  }

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
  [self setupClientBundle:sender];
  if (rendererCommand_.visible() && mozcRenderer_) {
    mozcRenderer_->ExecCommand(rendererCommand_);
  }
  [self handleConfig];

  // Sets this controller as the active controller to receive messages from the renderer process.
  [gRendererReceiver setCurrentController:self];

  std::string window_name, window_owner;
  if (mozc::MacUtil::GetFrontmostWindowNameAndOwner(&window_name, &window_owner)) {
    DLOG(INFO) << "frontmost window name: \"" << window_name << "\" " << "owner: \"" << window_owner
               << "\"";
    suppressSuggestion_ = mozc::MacUtil::IsSuppressSuggestionWindow(window_name, window_owner);
  }

  mozc::mac::MozcToolbarSetActiveController((__bridge void *)self);
  [self refreshModeFromServer:sender];
  [self syncServerActivationIfNeeded:sender];

  DLOG(INFO) << kProductNameInEnglish << " client (" << self << "): activated for " << sender;
  DLOG(INFO) << "sender bundleID: " << clientBundle_;
}

- (void)deactivateServer:(id)sender {
  if (imkClientForTest_) {
    return;
  }
  mozc::mac::MozcToolbarSetActiveController(nullptr);
  mozc::mac::MozcToolbarHide();

  RendererCommand clearCommand;
  clearCommand.set_type(RendererCommand::UPDATE);
  clearCommand.set_visible(false);
  clearCommand.clear_output();
  if (mozcRenderer_) {
    mozcRenderer_->ExecCommand(clearCommand);
  }
  DLOG(INFO) << kProductNameInEnglish << " client (" << self << "): deactivated";
  DLOG(INFO) << "sender bundleID: " << clientBundle_;
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

  // macOS calls this when the input-mode picker changes.  Do not call the full
  // |-switchMode:| / |-handleConfig| path (freeze risk).  Do sync IME ON/OFF with
  // the converter when the user picks Hiragana vs Direct — otherwise mode_ says
  // HIRAGANA while the session stays DIRECT and Ctrl+Shift+1..4 return
  // consumed=false (beep) even though the user turned hiragana on.
  CompositionMode new_mode = [value isKindOfClass:[NSString class]]
                                 ? GetCompositionMode([value UTF8String])
                                 : mozc::commands::DIRECT;
  if (new_mode == mozc::commands::HALF_ASCII && [composedString_ length] == 0) {
    new_mode = mozc::commands::DIRECT;
  }
  if (new_mode != mode_) {
    if (MarinaImkTraceEnabled()) {
      LOG(INFO) << "[marinaImk] setValue mode " << CompositionModeName(mode_) << " -> "
                << CompositionModeName(new_mode);
    }
    mode_ = new_mode;
    mozc::mac::MozcToolbarShow(mozcClient_.get(), mode_);
    if (new_mode == mozc::commands::DIRECT) {
      [self syncServerDeactivationIfNeeded:sender];
    } else {
      [self syncServerActivationIfNeeded:sender];
    }
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

  CompositionMode new_mode = NormalizeModeForEmptyHalfAscii(
      EffectiveCompositionMode(output, mode_), output);
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
  if (MarinaImkTraceEnabled() && keyEvent.has_key_code()) {
    LOG(INFO) << "[marinaImk] dispatch shortcut slot="
              << static_cast<char>(keyEvent.key_code())
              << " mode_=" << CompositionModeName(mode_);
  }

  // Slot 5: same as toolbar Direct ↔ Hiragana (SessionCommand, not keymap).
  if (IsMarinaNumberRowSlot(keyEvent, '5')) {
    SessionCommand command;
    if ([self isConverterSessionActivated]) {
      command.set_type(SessionCommand::TURN_OFF_IME);
    } else {
      command.set_type(SessionCommand::TURN_ON_IME);
      command.set_composition_mode(mozc::commands::HIRAGANA);
    }
    [self sendCommand:command];
    return YES;
  }

  if (![self isConverterSessionActivated]) {
    SessionCommand command;
    command.set_type(SessionCommand::TURN_ON_IME);
    const CompositionMode on_mode =
        mode_ == mozc::commands::DIRECT ? mozc::commands::HIRAGANA : mode_;
    command.set_composition_mode(on_mode);
    [self sendCommand:command];
  }

  if (IsMarinaNumberRowSlot(keyEvent, '2')) {
    SessionCommand command;
    command.set_type(SessionCommand::SHOW_ODORIJI_PALETTE);
    [self sendCommand:command];
    return YES;
  }
  if (IsMarinaNumberRowSlot(keyEvent, '3')) {
    SessionCommand command;
    command.set_type(SessionCommand::TOGGLE_TRADITIONAL_KANJI);
    [self sendCommand:command];
    return YES;
  }
  if (IsMarinaNumberRowSlot(keyEvent, '1')) {
    SessionCommand command;
    command.set_type(SessionCommand::INSERT_ODORIJI_DEFAULT);
    [self sendCommand:command];
    return YES;
  }
  if (IsMarinaNumberRowSlot(keyEvent, '4')) {
    SessionCommand command;
    command.set_type(SessionCommand::SWITCH_COMPOSITION_MODE);
    if (mode_ == mozc::commands::MANYOSHU) {
      command.set_composition_mode(mozc::commands::HIRAGANA);
    } else {
      command.set_composition_mode(mozc::commands::MANYOSHU);
    }
    [self sendCommand:command];
    return YES;
  }

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

- (void)launchWordRegisterTool:(id)client {
  ::setenv(mozc::kWordRegisterEnvironmentName, "", 1);
  if (CanSelectedRange(clientBundle_)) {
    NSRange selectedRange = [client selectedRange];
    if (selectedRange.location != NSNotFound && selectedRange.length != NSNotFound &&
        selectedRange.length > 0) {
      NSString *text = [[client attributedSubstringFromRange:selectedRange] string];
      if (text != nil) {
        ::setenv(mozc::kWordRegisterEnvironmentName, [text UTF8String], 1);
      }
    }
  }
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

  if (!output->consumed()) {
    if (output->has_result()) {
      [self commitText:output->result().value().c_str() client:sender];
    }
    if (output->has_status() || output->has_mode()) {
      const CompositionMode new_mode = NormalizeModeForEmptyHalfAscii(
          EffectiveCompositionMode(*output, mode_), *output);
      mode_ = new_mode;
      mozc::mac::MozcToolbarShow(mozcClient_.get(), mode_);
    }
    if (output->has_preedit()) {
      [self updateComposedString:&(output->preedit())];
    }
    [self updateCandidates:output];
    mozc::mac::MozcToolbarUpdate(*output, mode_);
    [self updateImeMenuState:output];
    --processOutputDepth_;
    return;
  }

  DLOG(INFO) << output->Utf8DebugString();
  if (output->has_url()) {
    NSString *url = [NSString stringWithUTF8String:output->url().c_str()];
    [self openLink:[NSURL URLWithString:url]];
  }

  if (output->has_result()) {
    [self commitText:output->result().value().c_str() client:sender];
  }

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

  [self updateComposedString:&(output->preedit())];
  [self updateCandidates:output];

  if (output->has_mode() || output->has_status()) {
    CompositionMode new_mode = NormalizeModeForEmptyHalfAscii(
        EffectiveCompositionMode(*output, mode_), *output);
    // Do not allow HALF_ASCII with empty composition.  This should be
    // handled in the converter, but just in case.
    if (new_mode != mode_) {
      if (MarinaImkTraceEnabled()) {
        LOG(INFO) << "[marinaImk] processOutput mode " << CompositionModeName(mode_)
                  << " -> " << CompositionModeName(new_mode);
      }
      mode_ = new_mode;
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

  if (output->has_launch_tool_mode()) {
    switch (output->launch_tool_mode()) {
      case mozc::commands::Output::CONFIG_DIALOG:
        MacProcess::LaunchMozcTool("config_dialog");
        break;
      case mozc::commands::Output::DICTIONARY_TOOL:
        MacProcess::LaunchMozcTool("dictionary_tool");
        break;
      case mozc::commands::Output::WORD_REGISTER_DIALOG:
        [self launchWordRegisterTool:sender];
        break;
      default:
        // do nothing
        break;
    }
  }

  mozc::mac::MozcToolbarUpdate(*output, mode_);
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

- (void)clearCandidates {
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
  if (!mozcRenderer_) {
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

  if (IsMarinaNumberRowShortcut(keyEvent)) {
    return [self dispatchMarinaNumberRowShortcut:keyEvent client:sender];
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
  MacProcess::LaunchMozcTool("config_dialog");
}

- (IBAction)dictionaryToolClicked:(id)sender {
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
