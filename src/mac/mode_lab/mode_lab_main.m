// Copyright 2026 marinaMoji contributors.

#import <Cocoa/Cocoa.h>
#import <InputMethodKit/InputMethodKit.h>

#import "mac/mode_lab/mode_lab_command.h"
#import "mac/mode_lab/mode_lab_input_controller.h"
#import "mac/mode_lab/mode_lab_log.h"

int main(int argc, const char *argv[]) {
  @autoreleasepool {
    NSBundle *bundle = NSBundle.mainBundle;
    NSDictionary *info = bundle.infoDictionary;
    NSString *connectionName = info[@"InputMethodConnectionName"];
    IMKServer *server =
        [[IMKServer alloc] initWithName:connectionName bundleIdentifier:bundle.bundleIdentifier];
    if (!server) {
      ModeLabLog(@"FATAL: IMKServer init failed");
      return 1;
    }
    ModeLabSetCommandHandler(^(NSDictionary *command) {
      [ModeLabInputController handleRemoteCommand:command];
    });
    ModeLabLog(@"Mode Lab IME started");
    ModeLabStartCommandListener();
    return NSApplicationMain(argc, argv);
  }
}
