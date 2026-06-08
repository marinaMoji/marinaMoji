// Copyright 2026 marinaMoji contributors.

#import "mac/mode_lab/mode_lab_policy.h"

static NSString *const kPolicyFileName = @"mode_lab_policy.plist";

static NSURL *ModeLabPolicyFileURL(void) {
  NSURL *base = [NSFileManager.defaultManager URLsForDirectory:NSApplicationSupportDirectory
                                                     inDomains:NSUserDomainMask].firstObject;
  NSURL *dir = [base URLByAppendingPathComponent:@"marinaMoji" isDirectory:YES];
  [NSFileManager.defaultManager createDirectoryAtURL:dir
                           withIntermediateDirectories:YES
                                            attributes:nil
                                                 error:nil];
  return [dir URLByAppendingPathComponent:kPolicyFileName];
}

@implementation ModeLabPolicy

+ (instancetype)shared {
  static ModeLabPolicy *instance;
  static dispatch_once_t once;
  dispatch_once(&once, ^{
    instance = [[ModeLabPolicy alloc] init];
  });
  return instance;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    // Defaults match current marinaMoji production behaviour.
    _flags = ModeLabPolicyIgnoreCompositionResync | ModeLabPolicyPersistLastMode;
    _suppressSetValueSeconds = 0.2;
    [self reload];
  }
  return self;
}

- (void)reload {
  NSURL *url = ModeLabPolicyFileURL();
  NSDictionary *dict = [NSDictionary dictionaryWithContentsOfURL:url];
  if (!dict) {
    return;
  }
  NSNumber *flags = dict[@"flags"];
  if (flags) {
    _flags = flags.unsignedIntegerValue;
  }
  NSNumber *suppress = dict[@"suppressSetValueSeconds"];
  if (suppress) {
    _suppressSetValueSeconds = suppress.doubleValue;
  }
}

- (void)save {
  NSDictionary *dict = @{
    @"flags" : @(_flags),
    @"suppressSetValueSeconds" : @(_suppressSetValueSeconds),
  };
  [dict writeToURL:ModeLabPolicyFileURL() atomically:YES];
}

- (NSString *)summary {
  NSMutableArray<NSString *> *parts = [NSMutableArray array];
  if (_flags & ModeLabPolicyIgnoreCompositionResync) {
    [parts addObject:@"ignoreResync"];
  }
  if (_flags & ModeLabPolicySyncDisplayOnChange) {
    [parts addObject:@"syncDisplay"];
  }
  if (_flags & ModeLabPolicyHonorAllSetValue) {
    [parts addObject:@"honorAllSetValue"];
  }
  if (_flags & ModeLabPolicyPersistLastMode) {
    [parts addObject:@"persistMode"];
  }
  return [NSString stringWithFormat:@"%@ suppress=%.2fs", [parts componentsJoinedByString:@","],
                                    _suppressSetValueSeconds];
}

@end
