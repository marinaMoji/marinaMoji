// Copyright 2026 marinaMoji contributors.
// Minimal IMKInputController for debugging macOS input-mode sync (setValue / selectInputMode).

#import "mac/mode_lab/mode_lab_input_controller.h"

#import <Carbon/Carbon.h>

#import "mac/mode_lab/mode_lab_log.h"
#import "mac/mode_lab/mode_lab_policy.h"

static int gControllerCount = 0;
static __weak ModeLabInputController *gActiveController = nil;

static NSURL *ModeLabLastModeURL(void) {
  NSURL *base = [NSFileManager.defaultManager URLsForDirectory:NSApplicationSupportDirectory
                                                     inDomains:NSUserDomainMask].firstObject;
  NSURL *dir = [base URLByAppendingPathComponent:@"marinaMoji" isDirectory:YES];
  return [dir URLByAppendingPathComponent:@"mode_lab_last_mode.txt"];
}

static void PersistLastMode(ModeLabMode mode) {
  [ModeLabModeName(mode) writeToURL:ModeLabLastModeURL()
                           atomically:YES
                             encoding:NSUTF8StringEncoding
                                error:nil];
}

static ModeLabMode LoadLastMode(void) {
  NSString *text = [NSString stringWithContentsOfURL:ModeLabLastModeURL()
                                            encoding:NSUTF8StringEncoding
                                               error:nil];
  if (text.length == 0) {
    return ModeLabModeDirect;
  }
  for (NSInteger i = ModeLabModeDirect; i <= ModeLabModeFullAscii; ++i) {
    if ([text isEqualToString:ModeLabModeName((ModeLabMode)i)]) {
      return (ModeLabMode)i;
    }
  }
  return ModeLabModeDirect;
}

@interface ModeLabInputController ()
@property(nonatomic, assign) BOOL syncingDisplayMode;
@property(nonatomic, assign) BOOL handlingKeyboardEvent;
@property(nonatomic, assign) NSTimeInterval suppressSetValueUntil;
@property(nonatomic, copy) NSString *lastDisplayModeId;
@property(nonatomic, copy) NSString *clientBundle;
@property(nonatomic, assign) BOOL imeServerActive;
@property(nonatomic, strong) NSMenu *imeMenu;
@property(nonatomic, strong) NSArray<NSMenuItem *> *modeMenuItems;
@end

@implementation ModeLabInputController

- (id)initWithServer:(IMKServer *)server delegate:(id)delegate client:(id)inputClient {
  self = [super initWithServer:server delegate:delegate client:inputClient];
  if (!self) {
    return nil;
  }
  ++gControllerCount;
  _mode = ModeLabModeDirect;
  _syncingDisplayMode = NO;
  _handlingKeyboardEvent = NO;
  _suppressSetValueUntil = 0;
  _lastDisplayModeId = @"";
  _clientBundle = @"";
  _imeServerActive = NO;
  [self setupMenuIfNeeded];
  ModeLabLog([NSString stringWithFormat:@"controller init %p count=%d", self, gControllerCount]);
  return self;
}

- (void)dealloc {
  --gControllerCount;
  if (gActiveController == self) {
    gActiveController = nil;
  }
  ModeLabLog([NSString stringWithFormat:@"controller dealloc %p count=%d", self, gControllerCount]);
}

- (void)noteEvent:(NSString * _Nonnull)event {
  NSString *controller = [NSString stringWithFormat:@"%p", self];
  ModeLabWriteState(_mode, event, gControllerCount, controller);
  ModeLabLog([NSString stringWithFormat:@"[%@ bundle=%@ mode=%@ policy=%@] %@",
                                        controller, _clientBundle, ModeLabModeName(_mode),
                                        ModeLabPolicy.shared.summary, event]);
}

#pragma mark IMKStateSetting

- (void)activateServer:(id)sender {
  [super activateServer:sender];
  _imeServerActive = YES;
  gActiveController = self;
  if ([sender respondsToSelector:@selector(bundleIdentifier)]) {
    _clientBundle = [sender performSelector:@selector(bundleIdentifier)] ?: @"";
  }
  if (ModeLabPolicy.shared.flags & ModeLabPolicyPersistLastMode) {
    _mode = LoadLastMode();
  }
  [self noteEvent:@"activateServer"];
  [self updateMenuState];
}

- (void)deactivateServer:(id)sender {
  _imeServerActive = NO;
  if (gActiveController == self) {
    gActiveController = nil;
  }
  [self noteEvent:@"deactivateServer"];
  [super deactivateServer:sender];
}

- (NSUInteger)recognizedEvents:(id)sender {
  (void)sender;
  return NSEventMaskKeyDown | NSEventMaskFlagsChanged;
}

- (void)setValue:(id)value forTag:(long)tag client:(id)sender {
  (void)tag;
  [ModeLabPolicy.shared reload];
  const NSTimeInterval now = [NSDate.date timeIntervalSinceReferenceDate];
  if (_syncingDisplayMode || _handlingKeyboardEvent || now < _suppressSetValueUntil) {
    [self noteEvent:[NSString stringWithFormat:@"setValue SKIPPED sync=%d key=%d suppress=%d value=%@",
                                               _syncingDisplayMode, _handlingKeyboardEvent,
                                               now < _suppressSetValueUntil, value]];
    [super setValue:value forTag:tag client:sender];
    return;
  }

  ModeLabPolicy *policy = ModeLabPolicy.shared;
  NSString *modeId = [value isKindOfClass:[NSString class]] ? value : @"";
  ModeLabMode newMode = ModeLabModeFromId(modeId);
  if (newMode == ModeLabModeHalfAscii) {
    newMode = ModeLabModeDirect;
  }

  if (policy.flags & ModeLabPolicyHonorAllSetValue) {
    if (newMode == ModeLabModeHalfAscii) {
      newMode = ModeLabModeDirect;
    }
    _mode = newMode;
    PersistLastMode(_mode);
    [self noteEvent:[NSString stringWithFormat:@"setValue HONOR_ALL %@ -> %@",
                                               ModeLabModeName(_mode), modeId]];
    [self updateMenuState];
    [super setValue:value forTag:tag client:sender];
    return;
  }

  if ((policy.flags & ModeLabPolicyIgnoreCompositionResync) && newMode != ModeLabModeDirect) {
    [self noteEvent:[NSString stringWithFormat:@"setValue IGNORED resync %@ <- %@ (keeping %@)",
                                               ModeLabModeName(newMode), modeId,
                                               ModeLabModeName(_mode)]];
    [super setValue:value forTag:tag client:sender];
    return;
  }

  if (newMode != _mode) {
    [self noteEvent:[NSString stringWithFormat:@"setValue ACCEPTED %@ -> %@",
                                               ModeLabModeName(_mode), ModeLabModeName(newMode)]];
    _mode = newMode;
    PersistLastMode(_mode);
    [self updateMenuState];
  }
  [super setValue:value forTag:tag client:sender];
}

#pragma mark Mode switching

- (void)switchMode:(ModeLabMode)newMode client:(id)sender {
  (void)sender;
  [ModeLabPolicy.shared reload];
  if (newMode == _mode) {
    return;
  }
  _handlingKeyboardEvent = YES;
  _mode = newMode;
  PersistLastMode(_mode);
  _suppressSetValueUntil =
      [NSDate.date timeIntervalSinceReferenceDate] + ModeLabPolicy.shared.suppressSetValueSeconds;
  [self noteEvent:[NSString stringWithFormat:@"switchMode -> %@", ModeLabModeName(_mode)]];
  [self updateMenuState];

  if (ModeLabPolicy.shared.flags & ModeLabPolicySyncDisplayOnChange) {
    [self switchDisplayMode];
  }
  _handlingKeyboardEvent = NO;
}

- (void)switchDisplayMode {
  NSString *modeId = ModeLabModeId(_mode);
  if ([modeId isEqualToString:_lastDisplayModeId]) {
    return;
  }
  _lastDisplayModeId = [modeId copy];
  _syncingDisplayMode = YES;
  [self noteEvent:[NSString stringWithFormat:@"switchDisplayMode selectInputMode:%@", modeId]];
  [[self client] selectInputMode:modeId];
  dispatch_async(dispatch_get_main_queue(), ^{
    self.syncingDisplayMode = NO;
  });
}

- (void)switchDisplayModeForce {
  _lastDisplayModeId = @"";
  [self switchDisplayMode];
}

+ (void)handleRemoteCommand:(NSDictionary *)command {
  if (gActiveController == nil) {
    ModeLabLog([NSString stringWithFormat:@"CMD dropped (no active controller): %@", command]);
    return;
  }
  ModeLabInputController *controller = gActiveController;
  NSString *action = command[@"action"];
  if ([action isEqualToString:@"switch_mode"]) {
    ModeLabMode mode = ModeLabModeFromName(command[@"mode"]);
    [controller switchMode:mode client:controller.client];
  } else if ([action isEqualToString:@"sync_display"]) {
    [controller switchDisplayModeForce];
  } else if ([action isEqualToString:@"inject_set_value"]) {
    NSString *modeId = command[@"mode_id"];
    [controller setValue:modeId forTag:0 client:controller.client];
  } else if ([action isEqualToString:@"ping"]) {
    [controller noteEvent:@"ping"];
  } else {
    ModeLabLog([NSString stringWithFormat:@"CMD unknown action: %@", action]);
  }
}

#pragma mark Events

- (BOOL)handleEvent:(NSEvent *)event client:(id)sender {
  if (!_imeServerActive) {
    return NO;
  }
  if (event.type != NSEventTypeKeyDown) {
    return NO;
  }
  const NSUInteger mods = event.modifierFlags & NSEventModifierFlagDeviceIndependentFlagsMask;
  const BOOL ctrlShift = (mods & NSEventModifierFlagControl) &&
                         (mods & NSEventModifierFlagShift) &&
                         !(mods & NSEventModifierFlagCommand) &&
                         !(mods & NSEventModifierFlagOption);
  if (!ctrlShift) {
    return NO;
  }
  switch (event.keyCode) {
    case kVK_ANSI_1:
      [self switchMode:ModeLabModeDirect client:sender];
      return YES;
    case kVK_ANSI_2:
      [self switchMode:ModeLabModeHiragana client:sender];
      return YES;
    case kVK_ANSI_3:
      [self switchMode:ModeLabModeKatakana client:sender];
      return YES;
    case kVK_ANSI_4:
      [self switchMode:ModeLabModeHalfAscii client:sender];
      return YES;
    case kVK_ANSI_5:
      [self switchMode:ModeLabModeFullAscii client:sender];
      return YES;
    default:
      break;
  }
  return NO;
}

#pragma mark Menu

- (void)setupMenuIfNeeded {
  if (_imeMenu) {
    return;
  }
  _imeMenu = [[NSMenu alloc] initWithTitle:@"Mode Lab"];
  NSArray<NSDictionary *> *entries = @[
    @{@"title" : @"Direct", @"mode" : @(ModeLabModeDirect)},
    @{@"title" : @"Hiragana", @"mode" : @(ModeLabModeHiragana)},
    @{@"title" : @"Katakana", @"mode" : @(ModeLabModeKatakana)},
    @{@"title" : @"Half-width kana", @"mode" : @(ModeLabModeHalfKatakana)},
    @{@"title" : @"Latin", @"mode" : @(ModeLabModeHalfAscii)},
    @{@"title" : @"Wide Latin", @"mode" : @(ModeLabModeFullAscii)},
  ];
  NSMenu *modeMenu = [[NSMenu alloc] initWithTitle:@"Input Mode"];
  NSMutableArray<NSMenuItem *> *items = [NSMutableArray array];
  for (NSDictionary *entry in entries) {
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:entry[@"title"]
                                                  action:@selector(modeMenuClicked:)
                                           keyEquivalent:@""];
    item.target = self;
    item.tag = [entry[@"mode"] integerValue];
    [modeMenu addItem:item];
    [items addObject:item];
  }
  _modeMenuItems = [items copy];
  NSMenuItem *modeMenuItem = [[NSMenuItem alloc] initWithTitle:@"Input Mode"
                                                        action:nil
                                                 keyEquivalent:@""];
  modeMenuItem.submenu = modeMenu;
  [_imeMenu addItem:modeMenuItem];
  [_imeMenu addItem:[NSMenuItem separatorItem]];
  NSMenuItem *syncItem = [[NSMenuItem alloc] initWithTitle:@"Sync display mode now"
                                                    action:@selector(syncDisplayMenuClicked:)
                                             keyEquivalent:@""];
  syncItem.target = self;
  [_imeMenu addItem:syncItem];
}

- (void)updateMenuState {
  for (NSMenuItem *item in _modeMenuItems) {
    item.state = (item.tag == _mode) ? NSControlStateValueOn : NSControlStateValueOff;
  }
}

- (NSMenu *)menu {
  [self updateMenuState];
  return _imeMenu;
}

- (IBAction)modeMenuClicked:(NSMenuItem *)sender {
  [self switchMode:(ModeLabMode)sender.tag client:self.client];
}

- (IBAction)syncDisplayMenuClicked:(id)sender {
  (void)sender;
  [self switchDisplayMode];
}

@end
