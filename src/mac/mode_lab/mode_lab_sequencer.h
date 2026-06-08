// Copyright 2026 marinaMoji contributors.

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@class ModeLabHostWindowController;

@interface ModeLabSequencer : NSObject

@property(nonatomic, readonly, getter=isRunning) BOOL running;
@property(nonatomic, readonly) NSInteger currentStep;
@property(nonatomic, readonly) NSInteger totalSteps;

+ (instancetype)shared;

- (void)startWithPrimaryHost:(ModeLabHostWindowController *)primary
                    interval:(NSTimeInterval)intervalSeconds;

- (void)cancel;

@end

NS_ASSUME_NONNULL_END
