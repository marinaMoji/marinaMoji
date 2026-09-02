// Copyright 2010-2021, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#import <InputMethodKit/InputMethodKit.h>

#import "mac/mozc_imk_input_controller.h"
#import "mac/renderer_receiver.h"
#import "mac/sync_overlay.h"

#include <cstdio>
#include <cstring>
#include <memory>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "base/const.h"
#include "base/init_mozc.h"
#include "base/run_level.h"
#include "client/client.h"
#include "config/stats_config_util.h"

namespace {

// Text Input Services registration is per-user and per-login-session, so it
// cannot be done from the .pkg postinstall script, which runs as root. The
// installer instead re-executes this binary in the console user's session with
// --register_input_source (or --select_input_source). Keeping the logic here
// avoids shipping a second signed executable and avoids the Swift toolchain
// that mac/register_marinamoji.sh used to need, which is absent on the
// machines this matters for.

NSString *InputSourceID(TISInputSourceRef source) {
  return (__bridge NSString *)(TISGetInputSourceProperty(source, kTISPropertyInputSourceID));
}

// Registers the running bundle and reports how many of its input sources macOS
// now lists. |select| additionally enables every mode and selects the base one.
// Returns 0 when at least one input source is visible to Text Input Services.
int RegisterInputSource(bool select) {
  NSBundle *bundle = [NSBundle mainBundle];
  NSString *bundleID = [bundle bundleIdentifier];
  NSURL *bundleURL = [bundle bundleURL];
  if (bundleID == nil || bundleURL == nil) {
    fprintf(stderr, "ERROR: cannot resolve the running bundle\n");
    return 1;
  }

  // paramErr is expected when the bundle is already known to the system, so the
  // status is reported but never treated as fatal: the listing below decides.
  const OSStatus status = TISRegisterInputSource((__bridge CFURLRef)bundleURL);
  fprintf(stderr, "TISRegisterInputSource(%s): %d\n", [[bundleURL path] UTF8String],
          static_cast<int>(status));

  CFArrayRef sourceList = TISCreateInputSourceList(nullptr, true);
  if (sourceList == nullptr) {
    fprintf(stderr, "ERROR: TISCreateInputSourceList failed\n");
    return 1;
  }

  int count = 0;
  for (CFIndex i = 0; i < CFArrayGetCount(sourceList); ++i) {
    TISInputSourceRef source = (TISInputSourceRef)(CFArrayGetValueAtIndex(sourceList, i));
    NSString *sourceID = InputSourceID(source);
    if (![sourceID hasPrefix:bundleID]) {
      continue;
    }
    ++count;
    if (!select) {
      continue;
    }
    TISEnableInputSource(source);
    if ([sourceID isEqualToString:[bundleID stringByAppendingString:@".base"]] ||
        [sourceID isEqualToString:bundleID]) {
      TISSelectInputSource(source);
    }
  }
  CFRelease(sourceList);

  if (count == 0) {
    fprintf(stderr, "ERROR: no %s input sources listed after registration\n",
            [bundleID UTF8String]);
    return 1;
  }
  printf("%d\n", count);
  return 0;
}

}  // namespace

int main(int argc, char *argv[]) {
  // Handled before anything else: these modes only touch Text Input Services
  // and exit, so they must not start the IMK server or the converter.
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--register_input_source") == 0) {
      return RegisterInputSource(false);
    }
    if (std::strcmp(argv[i], "--select_input_source") == 0) {
      return RegisterInputSource(true);
    }
  }

  if (!mozc::RunLevel::IsValidClientRunLevel()) {
    return -1;
  }

  mozc::InitMozc(argv[0], &argc, &argv);

  // Initialize imkServer
  NSBundle *bundle = [NSBundle mainBundle];
  NSDictionary *infoDictionary = [bundle infoDictionary];
  NSString *connectionName = [infoDictionary objectForKey:@"InputMethodConnectionName"];
  IMKServer *imkServer = [[IMKServer alloc] initWithName:connectionName
                                        bundleIdentifier:[bundle bundleIdentifier]];
  if (!imkServer) {
    LOG(FATAL) << mozc::kProductNameInEnglish << " failed to initialize";
    return -1;
  }
  DLOG(INFO) << mozc::kProductNameInEnglish << " initialized";

  NSString *rendererConnectionName = @kProductPrefix "_Renderer_Connection";
  RendererReceiver *rendererReceiver =
      [[RendererReceiver alloc] initWithName:rendererConnectionName];
  [MozcImkInputController setGlobalRendererReceiver:rendererReceiver];

  // Start the converter server at this time explicitly to prevent the
  // slow-down of the response for initial key event.
  {
    std::unique_ptr<mozc::client::Client> client(new mozc::client::Client);
    client->PingServer();
  }
  mozc::mac::SyncOverlayStartWatcher();
  NSApplicationMain(argc, (const char **)argv);
  return 0;
}
