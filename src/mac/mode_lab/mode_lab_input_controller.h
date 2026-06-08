// Copyright 2026 marinaMoji contributors.

#import <Cocoa/Cocoa.h>
#import <InputMethodKit/InputMethodKit.h>

#import "mac/mode_lab/mode_lab_mode.h"

NS_ASSUME_NONNULL_BEGIN

@interface ModeLabInputController : IMKInputController

@property(nonatomic, assign) ModeLabMode mode;

- (void)switchMode:(ModeLabMode)newMode client:(id)sender;
- (void)switchDisplayMode;
- (void)switchDisplayModeForce;

+ (void)handleRemoteCommand:(NSDictionary *)command;

@end

NS_ASSUME_NONNULL_END
