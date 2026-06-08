// Copyright 2026 marinaMoji contributors.

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

extern NSString *const ModeLabCommandNotification;

typedef void (^ModeLabCommandHandler)(NSDictionary *command);

void ModeLabSetCommandHandler(ModeLabCommandHandler _Nullable handler);
void ModeLabPostCommand(NSString *action, NSDictionary *_Nullable params);
void ModeLabStartCommandListener(void);

NS_ASSUME_NONNULL_END
