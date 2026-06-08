// Copyright 2026 marinaMoji contributors.
// Structured event log + automated run report for Mode Lab.

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

void ModeLabEventBeginRun(NSString *runId, NSString *policySummary);
void ModeLabEventBeginStep(void);
NSArray<NSString *> *ModeLabEventTakeStepResponses(void);
void ModeLabEventRecordResponse(NSString *line);
void ModeLabEventRecordStep(NSString *stepId, NSString *action, NSDictionary *before,
                            NSDictionary *after, NSArray<NSString *> *responses,
                            NSDictionary *_Nullable extra);
void ModeLabEventEndRun(void);
BOOL ModeLabEventRunActive(void);
NSString *_Nullable ModeLabEventCurrentRunId(void);
NSURL *ModeLabEventReportURL(void);
NSURL *ModeLabEventJsonlURL(void);

NS_ASSUME_NONNULL_END
