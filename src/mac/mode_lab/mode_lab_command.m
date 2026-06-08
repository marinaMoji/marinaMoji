// Copyright 2026 marinaMoji contributors.

#import "mac/mode_lab/mode_lab_command.h"

#import "mac/mode_lab/mode_lab_log.h"

NSString *const ModeLabCommandNotification = @"org.mozc.inputmethod.ModeLab.Command";

static ModeLabCommandHandler gHandler = nil;

void ModeLabSetCommandHandler(ModeLabCommandHandler handler) { gHandler = [handler copy]; }

void ModeLabPostCommand(NSString *action, NSDictionary *_Nullable params) {
  NSMutableDictionary *info = [NSMutableDictionary dictionary];
  info[@"action"] = action ?: @"";
  if (params) {
    [info addEntriesFromDictionary:params];
  }
  info[@"timestamp"] = @([NSDate.date timeIntervalSince1970]);
  [[NSDistributedNotificationCenter defaultCenter]
      postNotificationName:ModeLabCommandNotification
                    object:nil
                  userInfo:info
        deliverImmediately:YES];
  ModeLabLog([NSString stringWithFormat:@"CMD post action=%@ params=%@", action, params]);
}

void ModeLabStartCommandListener(void) {
  [[NSDistributedNotificationCenter defaultCenter]
      addObserverForName:ModeLabCommandNotification
                  object:nil
                   queue:NSOperationQueue.mainQueue
              usingBlock:^(NSNotification *note) {
                if (gHandler) {
                  gHandler(note.userInfo);
                } else {
                  ModeLabLog([NSString stringWithFormat:@"CMD dropped (no handler): %@",
                                                        note.userInfo]);
                }
              }];
  ModeLabLog(@"CMD listener started");
}
