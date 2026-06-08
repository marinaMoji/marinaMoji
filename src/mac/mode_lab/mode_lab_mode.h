// Copyright 2026 marinaMoji contributors.
// Mode Lab — macOS IMK mode-sync test harness (mirrors marinaMoji mode IDs).

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, ModeLabMode) {
  ModeLabModeDirect = 0,
  ModeLabModeHiragana,
  ModeLabModeKatakana,
  ModeLabModeHalfKatakana,
  ModeLabModeHalfAscii,
  ModeLabModeFullAscii,
};

NSString *ModeLabModeName(ModeLabMode mode);
ModeLabMode ModeLabModeFromId(NSString *modeId);
ModeLabMode ModeLabModeFromName(NSString *name);
NSString *ModeLabModeId(ModeLabMode mode);

NS_ASSUME_NONNULL_END
