// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0
//
// Standalone preview for the redesigned Symbols Palette.
// Build: bazel build //mac:symbols_palette_preview --config=macos
// Run:   bazel run //mac:symbols_palette_preview --config=macos
// Env:   MARINAMOJI_FORCE_COLOR_SCHEME=dark|light (optional)

#import <Cocoa/Cocoa.h>

#include <cstdlib>
#include <cstring>

#include "mac/mozc_toolbar.h"

@interface SymbolsPalettePreviewAppDelegate : NSObject <NSApplicationDelegate>
@end

@implementation SymbolsPalettePreviewAppDelegate
- (void)applicationDidFinishLaunching:(NSNotification *)notification {
  const char *scheme = getenv("MARINAMOJI_FORCE_COLOR_SCHEME");
  if (scheme != nullptr) {
    if (strcmp(scheme, "dark") == 0) {
      NSApp.appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
    } else if (strcmp(scheme, "light") == 0) {
      NSApp.appearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];
    }
  }
  mozc::mac::MozcSymbolsPalettePreviewShow();
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
  return YES;
}
@end

int main(int argc, char **argv) {
  @autoreleasepool {
    [NSApplication sharedApplication];
    NSApp.activationPolicy = NSApplicationActivationPolicyRegular;
    SymbolsPalettePreviewAppDelegate *delegate =
        [[SymbolsPalettePreviewAppDelegate alloc] init];
    NSApp.delegate = delegate;
    [NSApp activateIgnoringOtherApps:YES];
    [NSApp run];
  }
  return 0;
}
