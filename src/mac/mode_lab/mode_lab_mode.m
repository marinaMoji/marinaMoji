// Copyright 2026 marinaMoji contributors.

#import "mac/mode_lab/mode_lab_mode.h"

// Apple canonical input-mode IDs (same strings marinaMoji uses in setValue:).
static NSString *const kRomanModeId = @"com.apple.inputmethod.Roman";
static NSString *const kHiraganaModeId = @"com.apple.inputmethod.Japanese";
static NSString *const kKatakanaModeId = @"com.apple.inputmethod.Japanese.Katakana";
static NSString *const kHalfWidthKanaModeId = @"com.apple.inputmethod.Japanese.HalfWidthKana";
static NSString *const kFullWidthRomanModeId = @"com.apple.inputmethod.Japanese.FullWidthRoman";

NSString *ModeLabModeName(ModeLabMode mode) {
  switch (mode) {
    case ModeLabModeDirect:
      return @"DIRECT";
    case ModeLabModeHiragana:
      return @"HIRAGANA";
    case ModeLabModeKatakana:
      return @"KATAKANA";
    case ModeLabModeHalfKatakana:
      return @"HALF_KATAKANA";
    case ModeLabModeHalfAscii:
      return @"HALF_ASCII";
    case ModeLabModeFullAscii:
      return @"FULL_ASCII";
  }
  return @"UNKNOWN";
}

ModeLabMode ModeLabModeFromId(NSString *modeId) {
  if (modeId.length == 0) {
    return ModeLabModeDirect;
  }
  if ([modeId isEqualToString:kRomanModeId]) {
    return ModeLabModeHalfAscii;
  }
  if ([modeId isEqualToString:kKatakanaModeId]) {
    return ModeLabModeKatakana;
  }
  if ([modeId isEqualToString:kHalfWidthKanaModeId]) {
    return ModeLabModeHalfKatakana;
  }
  if ([modeId isEqualToString:kFullWidthRomanModeId]) {
    return ModeLabModeFullAscii;
  }
  if ([modeId isEqualToString:kHiraganaModeId]) {
    return ModeLabModeHiragana;
  }
  return ModeLabModeDirect;
}

NSString *ModeLabModeId(ModeLabMode mode) {
  switch (mode) {
    case ModeLabModeDirect:
    case ModeLabModeHalfAscii:
      return kRomanModeId;
    case ModeLabModeKatakana:
      return kKatakanaModeId;
    case ModeLabModeHalfKatakana:
      return kHalfWidthKanaModeId;
    case ModeLabModeFullAscii:
      return kFullWidthRomanModeId;
    case ModeLabModeHiragana:
      return kHiraganaModeId;
  }
  return kRomanModeId;
}

ModeLabMode ModeLabModeFromName(NSString *name) {
  if (name.length == 0) {
    return ModeLabModeDirect;
  }
  for (NSInteger i = ModeLabModeDirect; i <= ModeLabModeFullAscii; ++i) {
    if ([name isEqualToString:ModeLabModeName((ModeLabMode)i)]) {
      return (ModeLabMode)i;
    }
  }
  return ModeLabModeDirect;
}
