// Copyright 2026 marinaMoji contributors.
// Runtime policy toggles for Mode Lab experiments (mirrors marinaMoji M1b/M1n flags).

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_OPTIONS(NSUInteger, ModeLabPolicyFlags) {
  // M1n: ignore composition resyncs in setValue: (honour DIRECT only).
  ModeLabPolicyIgnoreCompositionResync = 1 << 0,
  // M1b opposite: call selectInputMode: when mode changes from IME side.
  ModeLabPolicySyncDisplayOnChange = 1 << 1,
  // Honour every setValue: from macOS (pre-M1n behaviour).
  ModeLabPolicyHonorAllSetValue = 1 << 2,
  // Persist last mode across activate/deactivate (global profile).
  ModeLabPolicyPersistLastMode = 1 << 3,
};

@interface ModeLabPolicy : NSObject

+ (instancetype)shared;

@property(nonatomic) ModeLabPolicyFlags flags;
@property(nonatomic) NSTimeInterval suppressSetValueSeconds;

- (void)reload;
- (void)save;
- (NSString *)summary;

@end

NS_ASSUME_NONNULL_END
