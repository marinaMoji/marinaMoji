// Copyright 2026 marinaMoji contributors.
// Host app for Mode Lab — text fields, policy toggles, TIS mode buttons, live log.

#import <Cocoa/Cocoa.h>

#import "mac/mode_lab/mode_lab_event.h"
#import "mac/mode_lab/mode_lab_mode.h"
#import "mac/mode_lab/mode_lab_policy.h"
#import "mac/mode_lab/mode_lab_sequencer.h"
#import "mac/mode_lab/mode_lab_tis.h"

@class ModeLabHostAppDelegate;

@protocol ModeLabHostDelegate <NSObject>
- (void)ensureSecondaryWindow;
- (ModeLabHostWindowController *)secondaryHost;
@end

@interface ModeLabHostWindowController : NSWindowController <NSTextViewDelegate>
@property(nonatomic, strong) NSTextView *textView;
@property(nonatomic, strong) NSTextView *logView;
@property(nonatomic, strong) NSTextField *statusField;
@property(nonatomic, strong) NSTextField *tisField;
@property(nonatomic, strong) NSTextField *sequenceStatusField;
@property(nonatomic, strong) NSButton *ignoreResyncButton;
@property(nonatomic, strong) NSButton *syncDisplayButton;
@property(nonatomic, strong) NSButton *honorAllButton;
@property(nonatomic, strong) NSButton *persistButton;
@property(nonatomic, strong) NSButton *runSequenceButton;
@property(nonatomic, assign) unsigned long long logOffset;

- (void)focusTextField;
- (void)appendLogLine:(NSString *)line;
- (void)setSequenceStatus:(NSString *)status;
@end

@implementation ModeLabHostWindowController

- (instancetype)init {
  NSWindow *window = [[NSWindow alloc]
      initWithContentRect:NSMakeRect(0, 0, 900, 680)
                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                          NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
                  backing:NSBackingStoreBuffered
                    defer:NO];
  self = [super initWithWindow:window];
  if (!self) {
    return nil;
  }
  window.title = @"Mode Lab Host";
  [self buildUI:window.contentView];
  [window center];
  return self;
}

- (NSButton *)makeCheckbox:(NSString *)title action:(SEL)action {
  NSButton *button = [[NSButton alloc] initWithFrame:NSZeroRect];
  button.buttonType = NSButtonTypeSwitch;
  button.title = title;
  button.target = self;
  button.action = action;
  return button;
}

- (NSButton *)makeButton:(NSString *)title action:(SEL)action {
  NSButton *button = [[NSButton alloc] initWithFrame:NSZeroRect];
  button.bezelStyle = NSBezelStyleRounded;
  button.title = title;
  button.target = self;
  button.action = action;
  return button;
}

- (void)buildUI:(NSView *)root {
  root.translatesAutoresizingMaskIntoConstraints = NO;

  NSTextField *hint = [NSTextField labelWithString:
      @"Type here with Mode Lab IME selected. Use Run Automated Sequence to exercise all "
      @"TIS/IME/focus/inject signals (~50s). A second window opens for focus tests."];
  hint.translatesAutoresizingMaskIntoConstraints = NO;
  hint.lineBreakMode = NSLineBreakByWordWrapping;
  hint.maximumNumberOfLines = 0;
  [root addSubview:hint];

  _textView = [[NSTextView alloc] initWithFrame:NSZeroRect];
  _textView.translatesAutoresizingMaskIntoConstraints = NO;
  _textView.font = [NSFont systemFontOfSize:16];
  _textView.delegate = self;
  NSScrollView *textScroll = [[NSScrollView alloc] initWithFrame:NSZeroRect];
  textScroll.translatesAutoresizingMaskIntoConstraints = NO;
  textScroll.documentView = _textView;
  textScroll.hasVerticalScroller = YES;
  [root addSubview:textScroll];

  _statusField = [NSTextField labelWithString:@"IME state: (waiting)"];
  _statusField.translatesAutoresizingMaskIntoConstraints = NO;
  [root addSubview:_statusField];

  _tisField = [NSTextField labelWithString:@"TIS mode: (waiting)"];
  _tisField.translatesAutoresizingMaskIntoConstraints = NO;
  [root addSubview:_tisField];

  NSStackView *sequenceRow = [NSStackView stackViewWithViews:@[
    _runSequenceButton = [self makeButton:@"Run Automated Sequence"
                                     action:@selector(runSequenceClicked:)],
    [self makeButton:@"Cancel Sequence" action:@selector(cancelSequenceClicked:)],
    [self makeButton:@"Open Report" action:@selector(openReportClicked:)],
  ]];
  sequenceRow.translatesAutoresizingMaskIntoConstraints = NO;
  sequenceRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
  sequenceRow.spacing = 8;
  [root addSubview:sequenceRow];

  _sequenceStatusField = [NSTextField labelWithString:@"Sequence: idle"];
  _sequenceStatusField.translatesAutoresizingMaskIntoConstraints = NO;
  [root addSubview:_sequenceStatusField];

  NSStackView *policyRow = [NSStackView stackViewWithViews:@[
    _ignoreResyncButton = [self makeCheckbox:@"Ignore composition resync (M1n)"
                                      action:@selector(policyChanged:)],
    _syncDisplayButton = [self makeCheckbox:@"Sync display on IME change (M1b test)"
                                     action:@selector(policyChanged:)],
    _honorAllButton = [self makeCheckbox:@"Honor all setValue"
                                  action:@selector(policyChanged:)],
    _persistButton = [self makeCheckbox:@"Persist last mode"
                                 action:@selector(policyChanged:)],
  ]];
  policyRow.translatesAutoresizingMaskIntoConstraints = NO;
  policyRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
  policyRow.spacing = 12;
  [root addSubview:policyRow];

  NSMutableArray<NSButton *> *modeButtons = [NSMutableArray array];
  NSArray<NSDictionary *> *modes = @[
    @{@"title" : @"Hiragana", @"id" : @"com.apple.inputmethod.Japanese"},
    @{@"title" : @"Katakana", @"id" : @"com.apple.inputmethod.Japanese.Katakana"},
    @{@"title" : @"Half kana", @"id" : @"com.apple.inputmethod.Japanese.HalfWidthKana"},
    @{@"title" : @"Latin", @"id" : @"com.apple.inputmethod.Roman"},
    @{@"title" : @"Wide Latin", @"id" : @"com.apple.inputmethod.Japanese.FullWidthRoman"},
  ];
  for (NSDictionary *mode in modes) {
    NSButton *button = [self makeButton:mode[@"title"] action:@selector(tisModeClicked:)];
    button.identifier = mode[@"id"];
    [modeButtons addObject:button];
  }
  NSStackView *modeRow = [NSStackView stackViewWithViews:modeButtons];
  modeRow.translatesAutoresizingMaskIntoConstraints = NO;
  modeRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
  modeRow.spacing = 8;
  [root addSubview:modeRow];

  _logView = [[NSTextView alloc] initWithFrame:NSZeroRect];
  _logView.translatesAutoresizingMaskIntoConstraints = NO;
  _logView.editable = NO;
  _logView.font = [NSFont fontWithName:@"Menlo" size:11];
  NSScrollView *logScroll = [[NSScrollView alloc] initWithFrame:NSZeroRect];
  logScroll.translatesAutoresizingMaskIntoConstraints = NO;
  logScroll.documentView = _logView;
  logScroll.hasVerticalScroller = YES;
  [root addSubview:logScroll];

  NSDictionary *views = @{
    @"hint" : hint,
    @"text" : textScroll,
    @"status" : _statusField,
    @"tis" : _tisField,
    @"sequence" : sequenceRow,
    @"seqstatus" : _sequenceStatusField,
    @"policy" : policyRow,
    @"modes" : modeRow,
    @"log" : logScroll,
  };
  [root addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|-12-[hint]-12-|"
                                                               options:0
                                                               metrics:nil
                                                                 views:views]];
  [root addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|-12-[text]-12-|"
                                                               options:0
                                                               metrics:nil
                                                                 views:views]];
  [root addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|-12-[status]-12-|"
                                                               options:0
                                                               metrics:nil
                                                                 views:views]];
  [root addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|-12-[tis]-12-|"
                                                               options:0
                                                               metrics:nil
                                                                 views:views]];
  [root addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|-12-[sequence]-12-|"
                                                               options:0
                                                               metrics:nil
                                                                 views:views]];
  [root addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|-12-[seqstatus]-12-|"
                                                               options:0
                                                               metrics:nil
                                                                 views:views]];
  [root addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|-12-[policy]-12-|"
                                                               options:0
                                                               metrics:nil
                                                                 views:views]];
  [root addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|-12-[modes]-12-|"
                                                               options:0
                                                               metrics:nil
                                                                 views:views]];
  [root addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|-12-[log]-12-|"
                                                               options:0
                                                               metrics:nil
                                                                 views:views]];
  [root addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:
                                    @"V:|-12-[hint]-8-[text(160)]-8-[status]-4-[tis]-8-[sequence]-4-[seqstatus]-8-[policy]-8-[modes]-8-[log]-12-|"
                                                               options:0
                                                               metrics:nil
                                                                 views:views]];

  [self loadPolicyUI];
  _logOffset = 0;
  [NSTimer scheduledTimerWithTimeInterval:0.5
                                  target:self
                                selector:@selector(refreshStatus)
                                userInfo:nil
                                 repeats:YES];
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(refreshStatus)
                                               name:@"ModeLabStateDidChange"
                                             object:nil];
}

- (void)loadPolicyUI {
  ModeLabPolicy *policy = ModeLabPolicy.shared;
  _ignoreResyncButton.state =
      (policy.flags & ModeLabPolicyIgnoreCompositionResync) ? NSControlStateValueOn
                                                            : NSControlStateValueOff;
  _syncDisplayButton.state = (policy.flags & ModeLabPolicySyncDisplayOnChange)
                                 ? NSControlStateValueOn
                                 : NSControlStateValueOff;
  _honorAllButton.state =
      (policy.flags & ModeLabPolicyHonorAllSetValue) ? NSControlStateValueOn : NSControlStateValueOff;
  _persistButton.state =
      (policy.flags & ModeLabPolicyPersistLastMode) ? NSControlStateValueOn : NSControlStateValueOff;
}

- (void)policyChanged:(id)sender {
  (void)sender;
  ModeLabPolicy *policy = ModeLabPolicy.shared;
  policy.flags = 0;
  if (_ignoreResyncButton.state == NSControlStateValueOn) {
    policy.flags |= ModeLabPolicyIgnoreCompositionResync;
  }
  if (_syncDisplayButton.state == NSControlStateValueOn) {
    policy.flags |= ModeLabPolicySyncDisplayOnChange;
  }
  if (_honorAllButton.state == NSControlStateValueOn) {
    policy.flags |= ModeLabPolicyHonorAllSetValue;
  }
  if (_persistButton.state == NSControlStateValueOn) {
    policy.flags |= ModeLabPolicyPersistLastMode;
  }
  [policy save];
  ModeLabPolicy.shared.flags = policy.flags;
}

- (void)tisModeClicked:(NSButton *)sender {
  NSString *modeId = sender.identifier;
  BOOL ok = [ModeLabTIS selectModeId:modeId];
  if (!ok) {
    NSBeep();
  }
  [self refreshStatus];
}

- (NSURL *)stateFileURL {
  NSURL *base = [NSFileManager.defaultManager URLsForDirectory:NSApplicationSupportDirectory
                                                     inDomains:NSUserDomainMask].firstObject;
  return [[base URLByAppendingPathComponent:@"marinaMoji" isDirectory:YES]
      URLByAppendingPathComponent:@"mode_lab_state.json"];
}

- (NSURL *)logFileURL {
  NSURL *logs = [NSFileManager.defaultManager URLsForDirectory:NSLibraryDirectory
                                                     inDomains:NSUserDomainMask].firstObject;
  return [[[logs URLByAppendingPathComponent:@"Logs" isDirectory:YES]
      URLByAppendingPathComponent:@"marinaMoji" isDirectory:YES]
      URLByAppendingPathComponent:@"mode_lab.log"];
}

- (void)refreshStatus {
  NSData *data = [NSData dataWithContentsOfURL:self.stateFileURL];
  if (data) {
    NSDictionary *state = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
    _statusField.stringValue =
        [NSString stringWithFormat:@"IME state: mode=%@ event=%@ controllers=%@ active=%@",
                                   state[@"mode"], state[@"lastEvent"], state[@"controllerCount"],
                                   state[@"activeController"]];
  }
  _tisField.stringValue =
      [NSString stringWithFormat:@"TIS mode: %@", [ModeLabTIS currentModeId]];

  NSURL *logURL = self.logFileURL;
  NSDictionary *attrs = [NSFileManager.defaultManager attributesOfItemAtPath:logURL.path error:nil];
  unsigned long long size = [attrs fileSize];
  if (size < _logOffset) {
    _logOffset = 0;
  }
  NSFileHandle *handle = [NSFileHandle fileHandleForReadingAtPath:logURL.path];
  if (!handle) {
    return;
  }
  [handle seekToFileOffset:_logOffset];
  NSData *chunk = [handle readDataToEndOfFile];
  [handle closeFile];
  if (chunk.length == 0) {
    return;
  }
  _logOffset += chunk.length;
  NSString *text = [[NSString alloc] initWithData:chunk encoding:NSUTF8StringEncoding];
  [_logView.textStorage appendAttributedString:[[NSAttributedString alloc] initWithString:text]];
  [_logView scrollToEndOfDocument:self];
}

- (void)focusTextField {
  [self.window makeKeyAndOrderFront:nil];
  [self.window makeFirstResponder:self.textView];
}

- (void)appendLogLine:(NSString *)line {
  if (line.length == 0) {
    return;
  }
  [_logView.textStorage appendAttributedString:[[NSAttributedString alloc] initWithString:line]];
  [_logView scrollToEndOfDocument:self];
}

- (void)setSequenceStatus:(NSString *)status {
  _sequenceStatusField.stringValue = [NSString stringWithFormat:@"Sequence: %@", status];
}

- (void)runSequenceClicked:(id)sender {
  (void)sender;
  if (ModeLabSequencer.shared.isRunning) {
    return;
  }
  [self policyChanged:nil];
  [ModeLabSequencer.shared startWithPrimaryHost:self interval:2.0];
}

- (void)cancelSequenceClicked:(id)sender {
  (void)sender;
  [ModeLabSequencer.shared cancel];
}

- (void)openReportClicked:(id)sender {
  (void)sender;
  NSURL *url = ModeLabEventReportURL();
  if ([NSFileManager.defaultManager fileExistsAtPath:url.path]) {
    [NSWorkspace.sharedWorkspace openURL:url];
  } else {
    NSBeep();
  }
}

@end

@interface ModeLabHostAppDelegate : NSObject <NSApplicationDelegate, ModeLabHostDelegate>
@property(nonatomic, strong) NSMutableArray<ModeLabHostWindowController *> *windows;
@property(nonatomic, strong) ModeLabHostWindowController *secondaryHost;
@end

@implementation ModeLabHostAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
  (void)notification;
  _windows = [NSMutableArray array];
  [self openWindow];
}

- (void)openWindow {
  ModeLabHostWindowController *controller = [[ModeLabHostWindowController alloc] init];
  [_windows addObject:controller];
  [controller showWindow:self];
  [controller.window makeKeyAndOrderFront:self];
}

- (void)ensureSecondaryWindow {
  if (self.secondaryHost != nil) {
    return;
  }
  ModeLabHostWindowController *controller = [[ModeLabHostWindowController alloc] init];
  controller.window.title = @"Mode Lab Host (secondary)";
  [_windows addObject:controller];
  self.secondaryHost = controller;
  [controller showWindow:self];
  NSRect frame = controller.window.frame;
  frame.origin.x += 40;
  frame.origin.y -= 40;
  [controller.window setFrame:frame display:YES];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
  (void)sender;
  return YES;
}

- (IBAction)newWindow:(id)sender {
  (void)sender;
  [self openWindow];
}

@end

int main(int argc, const char *argv[]) {
  @autoreleasepool {
    NSApplication *app = NSApplication.sharedApplication;
    ModeLabHostAppDelegate *delegate = [[ModeLabHostAppDelegate alloc] init];
    app.delegate = delegate;

    NSMenu *mainMenu = [[NSMenu alloc] init];
    NSMenuItem *appItem = [[NSMenuItem alloc] init];
    NSMenu *appMenu = [[NSMenu alloc] initWithTitle:@"Mode Lab Host"];
    [appMenu addItemWithTitle:@"Quit Mode Lab Host"
                       action:@selector(terminate:)
                keyEquivalent:@"q"];
    appItem.submenu = appMenu;
    [mainMenu addItem:appItem];

    NSMenuItem *windowItem = [[NSMenuItem alloc] init];
    NSMenu *windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];
    [windowMenu addItemWithTitle:@"New Window"
                          action:@selector(newWindow:)
                   keyEquivalent:@"n"];
    windowItem.submenu = windowMenu;
    [mainMenu addItem:windowItem];
    app.mainMenu = mainMenu;

    return NSApplicationMain(argc, argv);
  }
}
