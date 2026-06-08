// Copyright 2026 marinaMoji contributors.

#import <Carbon/Carbon.h>
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface ModeLabTIS : NSObject

+ (NSArray<NSDictionary *> *)listModeLabSources;
+ (NSString *)currentModeId;
+ (NSString *)currentSourceId;
+ (BOOL)isCurrentModeLab;
/// Select a Mode Lab TIS source. Accepts canonical mode IDs (com.apple.inputmethod.*)
/// or Mode Lab source IDs (org.mozc.inputmethod.ModeLab.*). Ignores marinaMoji/system matches.
+ (BOOL)selectModeId:(NSString *)modeId;
+ (BOOL)selectModeLabBase;

@end

NS_ASSUME_NONNULL_END
