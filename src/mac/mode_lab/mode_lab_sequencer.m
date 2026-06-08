// Copyright 2026 marinaMoji contributors.
// Automated signal sequencer — sends spaced TIS/IME/focus/inject steps and writes a report.

#import "mac/mode_lab/mode_lab_sequencer.h"

#import "mac/mode_lab/mode_lab_command.h"
#import "mac/mode_lab/mode_lab_event.h"
#import "mac/mode_lab/mode_lab_log.h"
#import "mac/mode_lab/mode_lab_mode.h"
#import "mac/mode_lab/mode_lab_policy.h"
#import "mac/mode_lab/mode_lab_tis.h"

// Declared in mode_lab_host.m (same Bazel target).
@interface ModeLabHostWindowController : NSWindowController
@property(nonatomic, readonly) NSTextView *textView;
- (void)focusTextField;
- (void)appendLogLine:(NSString *)line;
- (void)setSequenceStatus:(NSString *)status;
@end

@protocol ModeLabHostDelegate <NSObject>
- (void)ensureSecondaryWindow;
- (ModeLabHostWindowController *)secondaryHost;
@end

@interface ModeLabSequencer ()
@property(nonatomic, assign, getter=isRunning) BOOL running;
@property(nonatomic, assign) NSInteger currentStep;
@property(nonatomic, assign) NSInteger totalSteps;
@property(nonatomic, weak) ModeLabHostWindowController *primaryHost;
@property(nonatomic, assign) NSTimeInterval intervalSeconds;
@property(nonatomic, strong) NSArray<NSDictionary *> *steps;
@property(nonatomic, assign) NSInteger stepIndex;
@property(nonatomic, copy) NSString *runId;
@end

@implementation ModeLabSequencer

+ (instancetype)shared {
  static ModeLabSequencer *instance;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    instance = [[ModeLabSequencer alloc] init];
  });
  return instance;
}

static NSDictionary *ModeLabSnapshot(void) {
  NSURL *base = [NSFileManager.defaultManager URLsForDirectory:NSApplicationSupportDirectory
                                                     inDomains:NSUserDomainMask].firstObject;
  NSURL *stateURL = [[base URLByAppendingPathComponent:@"marinaMoji" isDirectory:YES]
      URLByAppendingPathComponent:@"mode_lab_state.json"];
  NSData *data = [NSData dataWithContentsOfURL:stateURL];
  NSDictionary *state = data ? [NSJSONSerialization JSONObjectWithData:data options:0 error:nil] : nil;
  return @{
    @"imeMode" : state[@"mode"] ?: @"(unknown)",
    @"tisMode" : [ModeLabTIS currentModeId] ?: @"(unknown)",
    @"tisSource" : [ModeLabTIS currentSourceId] ?: @"(unknown)",
    @"lastEvent" : state[@"lastEvent"] ?: @"",
    @"controllerCount" : state[@"controllerCount"] ?: @0,
  };
}

static NSArray<NSDictionary *> *BuildStepPlan(void) {
  return @[
    @{@"id" : @"001", @"action" : @"SNAPSHOT", @"label" : @"baseline"},
    @{
      @"id" : @"002",
      @"action" : @"TIS_SELECT",
      @"mode_id" : @"com.apple.inputmethod.Japanese",
      @"expectedTis" : @"Japanese",
    },
    @{
      @"id" : @"003",
      @"action" : @"TIS_SELECT",
      @"mode_id" : @"com.apple.inputmethod.Japanese.Katakana",
      @"expectedTis" : @"Katakana",
    },
    @{
      @"id" : @"004",
      @"action" : @"TIS_SELECT",
      @"mode_id" : @"com.apple.inputmethod.Roman",
      @"expectedTis" : @"Roman",
    },
    @{
      @"id" : @"005",
      @"action" : @"TIS_SELECT",
      @"mode_id" : @"com.apple.inputmethod.Japanese.HalfWidthKana",
      @"expectedTis" : @"HalfWidthKana",
    },
    @{
      @"id" : @"006",
      @"action" : @"TIS_SELECT",
      @"mode_id" : @"com.apple.inputmethod.Japanese.FullWidthRoman",
      @"expectedTis" : @"FullWidthRoman",
    },
    @{
      @"id" : @"007",
      @"action" : @"IME_SWITCH",
      @"mode" : @"HIRAGANA",
      @"expectedIme" : @"HIRAGANA",
    },
    @{
      @"id" : @"008",
      @"action" : @"IME_SWITCH",
      @"mode" : @"KATAKANA",
      @"expectedIme" : @"KATAKANA",
    },
    @{
      @"id" : @"009",
      @"action" : @"IME_SWITCH",
      @"mode" : @"DIRECT",
      @"expectedIme" : @"DIRECT",
    },
    @{
      @"id" : @"010",
      @"action" : @"IME_SWITCH",
      @"mode" : @"HIRAGANA",
      @"expectedIme" : @"HIRAGANA",
    },
    @{
      @"id" : @"011",
      @"action" : @"IME_SYNC_DISPLAY",
      @"expectedTis" : @"Japanese",
    },
    @{
      @"id" : @"012",
      @"action" : @"IME_SWITCH",
      @"mode" : @"KATAKANA",
      @"expectedIme" : @"KATAKANA",
    },
    @{
      @"id" : @"013",
      @"action" : @"IME_SYNC_DISPLAY",
      @"expectedTis" : @"Katakana",
    },
    @{
      @"id" : @"014",
      @"action" : @"FOCUS_OTHER",
    },
    @{
      @"id" : @"015",
      @"action" : @"FOCUS_PRIMARY",
    },
    @{
      @"id" : @"016",
      @"action" : @"INJECT_SETVALUE",
      @"mode_id" : @"com.apple.inputmethod.Japanese.Katakana",
      @"label" : @"resync katakana while hiragana",
    },
    @{
      @"id" : @"017",
      @"action" : @"INJECT_SETVALUE",
      @"mode_id" : @"com.apple.inputmethod.Japanese",
      @"label" : @"resync hiragana",
    },
    @{
      @"id" : @"018",
      @"action" : @"INJECT_SETVALUE",
      @"mode_id" : @"com.apple.inputmethod.Roman",
      @"label" : @"direct via roman",
      @"expectedIme" : @"DIRECT",
    },
    @{
      @"id" : @"019",
      @"action" : @"IME_SWITCH",
      @"mode" : @"HIRAGANA",
      @"expectedIme" : @"HIRAGANA",
    },
    @{
      @"id" : @"020",
      @"action" : @"RAPID_BURST",
    },
    @{
      @"id" : @"021",
      @"action" : @"IME_SWITCH",
      @"mode" : @"KATAKANA",
      @"expectedIme" : @"KATAKANA",
    },
    @{
      @"id" : @"022",
      @"action" : @"IME_SYNC_DISPLAY",
      @"expectedTis" : @"Katakana",
    },
    @{
      @"id" : @"023",
      @"action" : @"FOCUS_OTHER",
    },
    @{
      @"id" : @"024",
      @"action" : @"FOCUS_PRIMARY",
    },
    @{@"id" : @"025", @"action" : @"SNAPSHOT", @"label" : @"final"},
  ];
}

- (void)startWithPrimaryHost:(ModeLabHostWindowController *)primary
                    interval:(NSTimeInterval)intervalSeconds {
  if (self.isRunning) {
    return;
  }
  self.running = YES;
  self.primaryHost = primary;
  self.intervalSeconds = intervalSeconds > 0 ? intervalSeconds : 2.0;
  self.steps = BuildStepPlan();
  self.totalSteps = self.steps.count;
  self.stepIndex = 0;
  self.currentStep = 0;

  NSDateFormatter *fmt = [[NSDateFormatter alloc] init];
  fmt.dateFormat = @"yyyy-MM-dd'T'HH:mm:ss";
  self.runId = [fmt stringFromDate:NSDate.date];

  [ModeLabTIS selectModeLabBase];
  [self prepareForIMECommand];
  ModeLabEventBeginRun(self.runId, ModeLabPolicy.shared.summary);
  [primary setSequenceStatus:[NSString stringWithFormat:@"Sequence running (0/%ld)...",
                                                        (long)self.totalSteps]];
  [primary appendLogLine:[NSString stringWithFormat:@"\n=== SEQ BEGIN %@ interval=%.1fs ===\n",
                                                    self.runId, self.intervalSeconds]];
  [self runNextStep];
}

- (void)cancel {
  if (!self.isRunning) {
    return;
  }
  self.running = NO;
  ModeLabEventEndRun();
  [self.primaryHost setSequenceStatus:@"Sequence cancelled."];
  [self.primaryHost appendLogLine:@"=== SEQ CANCELLED ===\n"];
}

- (void)runNextStep {
  if (!self.isRunning || self.stepIndex >= self.steps.count) {
    [self finishRun];
    return;
  }
  NSDictionary *step = self.steps[self.stepIndex];
  self.currentStep = self.stepIndex + 1;
  NSString *label = step[@"label"] ?: step[@"action"];
  [self.primaryHost
      setSequenceStatus:[NSString stringWithFormat:@"Step %ld/%ld: %@ (%@)", (long)self.currentStep,
                                                                 (long)self.totalSteps, step[@"id"],
                                                                 label]];

  NSDictionary *before = ModeLabSnapshot();
  ModeLabEventBeginStep();
  [self executeStep:step];
  NSTimeInterval wait = self.intervalSeconds;
  if ([step[@"action"] isEqualToString:@"RAPID_BURST"]) {
    wait = 3.0;
  }

  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(wait * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), ^{
                   if (!self.isRunning) {
                     return;
                   }
                   NSDictionary *after = ModeLabSnapshot();
                   NSArray<NSString *> *responses = ModeLabEventTakeStepResponses();
                   NSMutableDictionary *extra = [NSMutableDictionary dictionary];
                   for (NSString *key in @[ @"expectedTis", @"expectedIme", @"label", @"mode_id" ]) {
                     if (step[key]) {
                       extra[key] = step[key];
                     }
                   }
                   ModeLabEventRecordStep(step[@"id"], step[@"action"], before, after, responses,
                                          extra);
                   [self.primaryHost
                       appendLogLine:[NSString
                                          stringWithFormat:@"SEQ step %@ %@ done (ime=%@ tis=%@ src=%@)\n",
                                                           step[@"id"], step[@"action"],
                                                           after[@"imeMode"], after[@"tisMode"],
                                                           after[@"tisSource"]]];
                   ++self.stepIndex;
                   [self runNextStep];
                 });
}

- (void)prepareForIMECommand {
  if (![ModeLabTIS isCurrentModeLab]) {
    [ModeLabTIS selectModeLabBase];
  }
  [self.primaryHost focusTextField];
  ModeLabPostCommand(@"ping", nil);
}

- (void)executeStep:(NSDictionary *)step {
  NSString *action = step[@"action"];
  ModeLabLog([NSString stringWithFormat:@"SEQ execute %@ %@", step[@"id"], action]);

  if ([action isEqualToString:@"SNAPSHOT"]) {
    return;
  }
  if ([action isEqualToString:@"TIS_SELECT"]) {
    BOOL ok = [ModeLabTIS selectModeId:step[@"mode_id"]];
    if (!ok) {
      ModeLabLog([NSString stringWithFormat:@"SEQ TIS_SELECT failed for %@", step[@"mode_id"]]);
    }
    [self.primaryHost focusTextField];
    return;
  }
  if ([action isEqualToString:@"IME_SWITCH"]) {
    [self prepareForIMECommand];
    ModeLabPostCommand(@"switch_mode", @{@"mode" : step[@"mode"]});
    return;
  }
  if ([action isEqualToString:@"IME_SYNC_DISPLAY"]) {
    [self prepareForIMECommand];
    ModeLabPostCommand(@"sync_display", nil);
    return;
  }
  if ([action isEqualToString:@"INJECT_SETVALUE"]) {
    [self prepareForIMECommand];
    ModeLabPostCommand(@"inject_set_value", @{@"mode_id" : step[@"mode_id"]});
    return;
  }
  if ([action isEqualToString:@"FOCUS_OTHER"]) {
    id<ModeLabHostDelegate> delegate =
        (id<ModeLabHostDelegate>)NSApplication.sharedApplication.delegate;
    [delegate ensureSecondaryWindow];
    ModeLabHostWindowController *secondary = delegate.secondaryHost;
    if (secondary) {
      [secondary focusTextField];
    } else {
      ModeLabLog(@"SEQ FOCUS_OTHER skipped (no secondary window)");
    }
    return;
  }
  if ([action isEqualToString:@"FOCUS_PRIMARY"]) {
    [self.primaryHost focusTextField];
    return;
  }
  if ([action isEqualToString:@"RAPID_BURST"]) {
    [self runRapidBurst];
    return;
  }
}

- (void)runRapidBurst {
  [self runRapidBurstAtIndex:0];
}

- (void)runRapidBurstAtIndex:(NSInteger)index {
  NSArray<NSDictionary *> *burst = @[
    @{@"action" : @"TIS_SELECT", @"mode_id" : @"com.apple.inputmethod.Japanese.Katakana"},
    @{@"action" : @"IME_SWITCH", @"mode" : @"HIRAGANA"},
    @{@"action" : @"TIS_SELECT", @"mode_id" : @"com.apple.inputmethod.Roman"},
    @{@"action" : @"IME_SWITCH", @"mode" : @"KATAKANA"},
    @{@"action" : @"sync_display"},
  ];
  if (index >= burst.count) {
    return;
  }
  NSDictionary *item = burst[index];
  NSString *a = item[@"action"];
  if ([a isEqualToString:@"TIS_SELECT"]) {
    [ModeLabTIS selectModeId:item[@"mode_id"]];
    [self.primaryHost focusTextField];
  } else if ([a isEqualToString:@"IME_SWITCH"]) {
    [self prepareForIMECommand];
    ModeLabPostCommand(@"switch_mode", @{@"mode" : item[@"mode"]});
  } else if ([a isEqualToString:@"sync_display"]) {
    [self prepareForIMECommand];
    ModeLabPostCommand(@"sync_display", nil);
  }
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.4 * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), ^{
                   [self runRapidBurstAtIndex:index + 1];
                 });
}

- (void)finishRun {
  self.running = NO;
  ModeLabEventEndRun();
  NSString *reportPath = ModeLabEventReportURL().path;
  [self.primaryHost
      setSequenceStatus:[NSString stringWithFormat:@"Sequence complete. Report: %@", reportPath]];
  [self.primaryHost appendLogLine:[NSString stringWithFormat:@"\n=== SEQ END report: %@ ===\n\n",
                                                             reportPath]];
  NSString *report = [NSString stringWithContentsOfFile:reportPath
                                               encoding:NSUTF8StringEncoding
                                                  error:nil];
  if (report.length > 0) {
    [self.primaryHost appendLogLine:report];
  }
}

@end
