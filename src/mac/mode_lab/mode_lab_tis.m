// Copyright 2026 marinaMoji contributors.

#import "mac/mode_lab/mode_lab_tis.h"

static NSString *const kModeLabBundlePrefix = @"org.mozc.inputmethod.ModeLab";
static NSString *const kModeLabBaseSourceId = @"org.mozc.inputmethod.ModeLab.base";

static NSString *_Nullable TISString(TISInputSourceRef src, CFStringRef key) {
  CFTypeRef raw = TISGetInputSourceProperty(src, key);
  if (!raw) {
    return nil;
  }
  return (__bridge NSString *)raw;
}

static BOOL SourceIsModeLab(TISInputSourceRef src) {
  NSString *sourceId = TISString(src, kTISPropertyInputSourceID);
  return [sourceId hasPrefix:kModeLabBundlePrefix];
}

@implementation ModeLabTIS

+ (NSArray<NSDictionary *> *)listModeLabSources {
  NSMutableArray<NSDictionary *> *result = [NSMutableArray array];
  CFArrayRef list = TISCreateInputSourceList(NULL, true);
  if (!list) {
    return result;
  }
  CFIndex count = CFArrayGetCount(list);
  for (CFIndex i = 0; i < count; ++i) {
    TISInputSourceRef src = (TISInputSourceRef)CFArrayGetValueAtIndex(list, i);
    if (!SourceIsModeLab(src)) {
      continue;
    }
    NSString *sourceId = TISString(src, kTISPropertyInputSourceID);
    [result addObject:@{
      @"id" : sourceId ?: @"",
      @"name" : TISString(src, kTISPropertyLocalizedName) ?: sourceId,
    }];
  }
  CFRelease(list);
  return result;
}

+ (NSString *)currentModeId {
  TISInputSourceRef current = TISCopyCurrentKeyboardInputSource();
  if (!current) {
    return @"";
  }
  NSString *modeId = TISString(current, kTISPropertyInputModeID);
  if (modeId.length == 0) {
    modeId = TISString(current, kTISPropertyInputSourceID) ?: @"";
  }
  CFRelease(current);
  return modeId;
}

+ (NSString *)currentSourceId {
  TISInputSourceRef current = TISCopyCurrentKeyboardInputSource();
  if (!current) {
    return @"";
  }
  NSString *sourceId = TISString(current, kTISPropertyInputSourceID) ?: @"";
  CFRelease(current);
  return sourceId;
}

+ (BOOL)isCurrentModeLab {
  return [[self currentSourceId] hasPrefix:kModeLabBundlePrefix];
}

+ (BOOL)selectModeLabBase {
  return [self selectModeId:kModeLabBaseSourceId];
}

+ (BOOL)selectModeId:(NSString *)modeId {
  if (modeId.length == 0) {
    return NO;
  }

  CFArrayRef list = TISCreateInputSourceList(NULL, true);
  if (!list) {
    return NO;
  }

  BOOL found = NO;
  CFIndex count = CFArrayGetCount(list);
  for (CFIndex i = 0; i < count; ++i) {
    TISInputSourceRef src = (TISInputSourceRef)CFArrayGetValueAtIndex(list, i);
    if (!SourceIsModeLab(src)) {
      continue;
    }
    NSString *inputModeId = TISString(src, kTISPropertyInputModeID);
    NSString *sourceId = TISString(src, kTISPropertyInputSourceID);
    BOOL matches = NO;
    if ([modeId hasPrefix:kModeLabBundlePrefix]) {
      matches = [sourceId isEqualToString:modeId];
    } else {
      matches = [inputModeId isEqualToString:modeId];
    }
    if (!matches) {
      continue;
    }
    OSStatus status = TISSelectInputSource(src);
    found = (status == noErr);
    break;
  }
  CFRelease(list);
  return found;
}

@end
