// Copyright 2026 marinaMoji contributors.

#import <Foundation/Foundation.h>

#import "mac/mode_lab/mode_lab_mode.h"

NS_ASSUME_NONNULL_BEGIN

void ModeLabLog(NSString *message);
void ModeLabWriteState(ModeLabMode mode, NSString *lastEvent, int controllerCount,
                       NSString * _Nullable activeController);

NS_ASSUME_NONNULL_END
