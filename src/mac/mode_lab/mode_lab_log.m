// Copyright 2026 marinaMoji contributors.

#import "mac/mode_lab/mode_lab_log.h"

#import "mac/mode_lab/mode_lab_event.h"

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

static NSURL *ModeLabLogFileURL(void) {
  NSURL *logs = [NSFileManager.defaultManager URLsForDirectory:NSLibraryDirectory
                                                     inDomains:NSUserDomainMask].firstObject;
  NSURL *dir = [[logs URLByAppendingPathComponent:@"Logs" isDirectory:YES]
      URLByAppendingPathComponent:@"marinaMoji" isDirectory:YES];
  [NSFileManager.defaultManager createDirectoryAtURL:dir
                           withIntermediateDirectories:YES
                                            attributes:nil
                                                 error:nil];
  return [dir URLByAppendingPathComponent:@"mode_lab.log"];
}

void ModeLabLog(NSString *message) {
  NSDateFormatter *fmt = [[NSDateFormatter alloc] init];
  fmt.dateFormat = @"yyyy-MM-dd HH:mm:ss.SSS";
  NSString *line =
      [NSString stringWithFormat:@"[%@] %@\n", [fmt stringFromDate:NSDate.date], message];
  NSData *data = [line dataUsingEncoding:NSUTF8StringEncoding];
  NSURL *url = ModeLabLogFileURL();
  if (![NSFileManager.defaultManager fileExistsAtPath:url.path]) {
    [data writeToURL:url atomically:YES];
    return;
  }
  NSFileHandle *handle = [NSFileHandle fileHandleForWritingToURL:url error:nil];
  if (!handle) {
    return;
  }
  [handle seekToEndOfFile];
  [handle writeData:data];
  [handle closeFile];
  fputs(line.UTF8String, stderr);
  if (ModeLabEventRunActive()) {
    ModeLabEventRecordResponse(message);
  }
}

void ModeLabWriteState(ModeLabMode mode, NSString *lastEvent, int controllerCount,
                       NSString *_Nullable activeController) {
  NSDictionary *state = @{
    @"mode" : ModeLabModeName(mode),
    @"lastEvent" : lastEvent ?: @"",
    @"controllerCount" : @(controllerCount),
    @"activeController" : activeController ?: @"",
    @"timestamp" : @([[NSDate date] timeIntervalSince1970]),
  };
  NSData *json = [NSJSONSerialization dataWithJSONObject:state options:NSJSONWritingPrettyPrinted
                                                 error:nil];
  if (!json) {
    return;
  }
  NSURL *url = [ModeLabSupportDirectory() URLByAppendingPathComponent:@"mode_lab_state.json"];
  [json writeToURL:url atomically:YES];
  [[NSNotificationCenter defaultCenter] postNotificationName:@"ModeLabStateDidChange"
                                                      object:nil
                                                    userInfo:state];
}
