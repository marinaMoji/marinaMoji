// Copyright 2026 marinaMoji contributors.

#import "mac/mode_lab/mode_lab_event.h"

#import "mac/mode_lab/mode_lab_log.h"

static NSURL *ModeLabSupportDirectory(void) {
  NSURL *base = [NSFileManager.defaultManager URLsForDirectory:NSApplicationSupportDirectory
                                                     inDomains:NSUserDomainMask].firstObject;
  NSURL *dir = [base URLByAppendingPathComponent:@"marinaMoji" isDirectory:YES];
  [NSFileManager.defaultManager createDirectoryAtURL:dir
                           withIntermediateDirectories:YES
                                            attributes:nil
                                                 error:nil];
  return dir;
}

static NSMutableArray<NSDictionary *> *gSteps = nil;
static NSMutableArray<NSString *> *gPendingResponses = nil;
static NSString *gRunId = nil;
static NSString *gPolicySummary = nil;
static NSDate *gRunStart = nil;
static BOOL gRunActive = NO;

NSURL *ModeLabEventReportURL(void) {
  return [ModeLabSupportDirectory() URLByAppendingPathComponent:@"mode_lab_run_report.txt"];
}

NSURL *ModeLabEventJsonlURL(void) {
  return [ModeLabSupportDirectory() URLByAppendingPathComponent:@"mode_lab_run.jsonl"];
}

static void AppendJsonl(NSDictionary *obj) {
  NSMutableDictionary *row = [obj mutableCopy];
  row[@"runId"] = gRunId ?: @"";
  row[@"timestamp"] = @([NSDate.date timeIntervalSince1970]);
  NSData *data = [NSJSONSerialization dataWithJSONObject:row options:0 error:nil];
  if (!data) {
    return;
  }
  NSMutableData *line = [data mutableCopy];
  [line appendData:[@"\n" dataUsingEncoding:NSUTF8StringEncoding]];
  NSURL *url = ModeLabEventJsonlURL();
  if (![NSFileManager.defaultManager fileExistsAtPath:url.path]) {
    [line writeToURL:url atomically:YES];
    return;
  }
  NSFileHandle *handle = [NSFileHandle fileHandleForWritingToURL:url error:nil];
  if (!handle) {
    return;
  }
  [handle seekToEndOfFile];
  [handle writeData:line];
  [handle closeFile];
}

void ModeLabEventBeginRun(NSString *runId, NSString *policySummary) {
  gRunId = [runId copy];
  gPolicySummary = [policySummary copy];
  gRunStart = NSDate.date;
  gRunActive = YES;
  gSteps = [NSMutableArray array];
  gPendingResponses = [NSMutableArray array];
  [[NSFileManager defaultManager] removeItemAtURL:ModeLabEventReportURL() error:nil];
  [[NSFileManager defaultManager] removeItemAtURL:ModeLabEventJsonlURL() error:nil];
  AppendJsonl(@{
    @"type" : @"run_begin",
    @"policy" : policySummary ?: @"",
  });
  ModeLabLog([NSString stringWithFormat:@"SEQ BEGIN run=%@ policy=%@", runId, policySummary]);
}

void ModeLabEventRecordResponse(NSString *line) {
  if (!gRunActive || line.length == 0) {
    return;
  }
  [gPendingResponses addObject:line];
  AppendJsonl(@{@"type" : @"response", @"line" : line});
}

void ModeLabEventBeginStep(void) {
  [gPendingResponses removeAllObjects];
}

NSArray<NSString *> *ModeLabEventTakeStepResponses(void) {
  NSArray<NSString *> *copy = [gPendingResponses copy];
  [gPendingResponses removeAllObjects];
  return copy;
}

void ModeLabEventRecordStep(NSString *stepId, NSString *action, NSDictionary *before,
                            NSDictionary *after, NSArray<NSString *> *responses,
                            NSDictionary *_Nullable extra) {
  if (!gRunActive) {
    return;
  }
  NSMutableDictionary *step = [@{
    @"stepId" : stepId ?: @"",
    @"action" : action ?: @"",
    @"before" : before ?: @{},
    @"after" : after ?: @{},
    @"responses" : responses ?: @[],
  } mutableCopy];
  if (extra) {
    [step addEntriesFromDictionary:extra];
  }
  [gSteps addObject:step];
  NSMutableDictionary *row = [@{@"type" : @"step"} mutableCopy];
  [row addEntriesFromDictionary:step];
  AppendJsonl(row);
}

static NSArray<NSString *> *DetectAnomalies(NSDictionary *step) {
  NSMutableArray<NSString *> *anomalies = [NSMutableArray array];
  NSDictionary *before = step[@"before"];
  NSDictionary *after = step[@"after"];
  NSString *action = step[@"action"];
  NSString *beforeIme = before[@"imeMode"];
  NSString *afterIme = after[@"imeMode"];
  NSString *afterTis = after[@"tisMode"];
  NSArray<NSString *> *responses = step[@"responses"];

  if ([action hasPrefix:@"TIS_SELECT"]) {
    NSString *expected = step[@"expectedTis"];
    if (expected.length > 0 && ![afterTis containsString:expected] &&
        ![afterTis isEqualToString:expected]) {
      [anomalies addObject:[NSString stringWithFormat:@"tis_not_selected: expected %@ got %@",
                                                      expected, afterTis]];
    }
  }

  if ([action hasPrefix:@"IME_SWITCH"]) {
    NSString *expected = step[@"expectedIme"];
    if (expected.length > 0 && ![afterIme isEqualToString:expected]) {
      [anomalies addObject:[NSString stringWithFormat:@"ime_mode_unchanged: expected %@ got %@",
                                                      expected, afterIme]];
    }
  }

  if ([action isEqualToString:@"IME_SYNC_DISPLAY"]) {
    NSString *expectedTis = step[@"expectedTis"];
    if (expectedTis.length > 0 && ![afterTis containsString:expectedTis]) {
      [anomalies addObject:[NSString stringWithFormat:@"tis_ime_mismatch: ime=%@ tis=%@ expectedTis~%@",
                                                      afterIme, afterTis, expectedTis]];
    }
  }

  if ([action isEqualToString:@"FOCUS_OTHER"] || [action isEqualToString:@"FOCUS_PRIMARY"]) {
    for (NSString *line in responses) {
      if ([line containsString:@"setValue IGNORED resync"]) {
        [anomalies addObject:@"focus_resync_ignored (expected with M1n policy)"];
      }
      if ([line containsString:@"setValue ACCEPTED"]) {
        [anomalies addObject:@"unexpected_setValue_accept_on_focus"];
      }
    }
  }

  if ([action hasPrefix:@"INJECT_SETVALUE"]) {
    NSString *expected = step[@"expectedIme"];
    if (expected.length > 0 && ![afterIme isEqualToString:expected] &&
        ![afterIme isEqualToString:beforeIme]) {
      // mode changed but not to expected - only flag if inject should have been ignored
      for (NSString *line in responses) {
        if ([line containsString:@"IGNORED"] && ![afterIme isEqualToString:beforeIme]) {
          [anomalies addObject:@"inject_ignored_but_mode_changed"];
        }
      }
    }
  }

  if ([action isEqualToString:@"RAPID_BURST"]) {
    NSInteger setValueCount = 0;
    for (NSString *line in responses) {
      if ([line containsString:@"setValue"]) {
        ++setValueCount;
      }
    }
    if (setValueCount > 6) {
      [anomalies addObject:[NSString stringWithFormat:@"rapid_setValue_storm: %ld events",
                                                      (long)setValueCount]];
    }
  }

  for (NSString *line in responses) {
    if ([line containsString:@"setValue SKIPPED sync=1"]) {
      [anomalies addObject:@"setValue_reentry_during_sync (M1b pattern)"];
    }
  }

  return anomalies;
}

void ModeLabEventEndRun(void) {
  if (!gRunActive) {
    return;
  }
  gRunActive = NO;
  NSTimeInterval duration = gRunStart ? [NSDate.date timeIntervalSinceDate:gRunStart] : 0;

  NSMutableArray<NSString *> *allAnomalies = [NSMutableArray array];
  NSMutableString *report = [NSMutableString string];
  [report appendFormat:@"Mode Lab automated run report\n"];
  [report appendFormat:@"Run ID: %@\n", gRunId];
  [report appendFormat:@"Policy: %@\n", gPolicySummary];
  [report appendFormat:@"Duration: %.1fs\n", duration];
  [report appendFormat:@"Steps: %lu\n\n", (unsigned long)gSteps.count];

  for (NSDictionary *step in gSteps) {
    NSMutableDictionary *mutableStep = [step mutableCopy];
    NSArray<NSString *> *anomalies = DetectAnomalies(step);
    if (anomalies.count > 0) {
      mutableStep[@"anomalies"] = anomalies;
      [allAnomalies addObjectsFromArray:anomalies];
    }
    [report appendFormat:@"--- STEP %@ %@ ---\n", step[@"stepId"], step[@"action"]];
    [report appendFormat:@"  before: ime=%@ tis=%@\n", step[@"before"][@"imeMode"],
                          step[@"before"][@"tisMode"]];
    [report appendFormat:@"  after:  ime=%@ tis=%@\n", step[@"after"][@"imeMode"],
                          step[@"after"][@"tisMode"]];
    for (NSString *line in step[@"responses"]) {
      [report appendFormat:@"  response: %@\n", line];
    }
    for (NSString *a in anomalies) {
      [report appendFormat:@"  ANOMALY: %@\n", a];
    }
    [report appendString:@"\n"];
  }

  [report appendFormat:@"Summary: %lu anomal%s detected\n", (unsigned long)allAnomalies.count,
                        allAnomalies.count == 1 ? "y" : "ies"];
  if (allAnomalies.count == 0) {
    [report appendString:@"  (none — all steps completed without flagged issues)\n"];
  } else {
    NSCountedSet *counts = [[NSCountedSet alloc] initWithArray:allAnomalies];
    for (NSString *a in counts) {
      [report appendFormat:@"  - %@ (x%lu)\n", a, (unsigned long)[counts countForObject:a]];
    }
  }

  [report writeToURL:ModeLabEventReportURL()
          atomically:YES
            encoding:NSUTF8StringEncoding
               error:nil];
  AppendJsonl(@{
    @"type" : @"run_end",
    @"duration" : @(duration),
    @"anomalyCount" : @(allAnomalies.count),
  });
  ModeLabLog([NSString stringWithFormat:@"SEQ END run=%@ duration=%.1fs anomalies=%lu report=%@",
                                        gRunId, duration, (unsigned long)allAnomalies.count,
                                        ModeLabEventReportURL().path]);
  gSteps = nil;
  gPendingResponses = nil;
}

BOOL ModeLabEventRunActive(void) { return gRunActive; }

NSString *ModeLabEventCurrentRunId(void) { return gRunId; }
