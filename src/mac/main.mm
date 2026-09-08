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
#import <ServiceManagement/ServiceManagement.h>
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

// The converter and the renderer are launchd jobs, not plain child processes:
// their plists declare MachServices, and mozc's IPC obtains the server port with
// bootstrap_check_in(), which only succeeds for a name launchd already knows.
// They therefore cannot simply be spawned.
//
// Registering them with SMAppService keeps launchd in the loop while moving the
// plists inside the app bundle, so macOS attributes the background activity to
// marinaMoji rather than to the signing identity of whoever built it.
void RegisterLaunchAgents() {
  NSArray<NSString *> *plists = @[
    @"org.mozc.inputmethod.Japanese.Converter.plist",
    @"org.mozc.inputmethod.Japanese.Renderer.plist",
    @"org.mozc.inputmethod.Japanese.Sync.plist",
  ];
  for (NSString *plist in plists) {
    SMAppService *service = [SMAppService agentServiceWithPlistName:plist];
    if (service.status == SMAppServiceStatusEnabled) {
      continue;
    }
    if (service.status == SMAppServiceStatusRequiresApproval) {
      // The user has switched the item off in Login Items. Registering again
      // will not override that, and the converter cannot start without it, so
      // record why input will not work rather than failing silently.
      LOG(ERROR) << "marinaMoji background item awaiting approval in System "
                 << "Settings > General > Login Items: "
                 << [plist UTF8String];
      continue;
    }
    NSError *error = nil;
    if (![service registerAndReturnError:&error]) {
      LOG(ERROR) << "SMAppService registration failed for " << [plist UTF8String]
                 << ": " << [[error localizedDescription] UTF8String];
    }
  }
}

NSString *InputSourceID(TISInputSourceRef source) {
  return (__bridge NSString *)(TISGetInputSourceProperty(source, kTISPropertyInputSourceID));
}

// Returns 1 for true, 0 for false, -1 when the property is absent.
int TISBool(TISInputSourceRef source, CFStringRef key) {
  const void *value = TISGetInputSourceProperty(source, key);
  if (value == nullptr) {
    return -1;
  }
  return CFBooleanGetValue((CFBooleanRef)value) ? 1 : 0;
}

// Prints one line per input source belonging to |bundleID| and returns how many
// there were.
//
// The properties matter because the count alone proves nothing.
// TISCreateInputSourceList(nullptr, true) reports what is installed on disk, not
// what the current login session will offer: when an earlier install happened in
// the same session, macOS keeps listing these sources here while System Settings
// shows none of them. Callers must not read a non-zero count as "the user can
// see it" -- see mac/vm_trial_marinamoji.sh for the reproduction.
int ReportInputSources(NSString *bundleID) {
  CFArrayRef sourceList = TISCreateInputSourceList(nullptr, true);
  if (sourceList == nullptr) {
    fprintf(stderr, "ERROR: TISCreateInputSourceList failed\n");
    return -1;
  }

  int count = 0;
  for (CFIndex i = 0; i < CFArrayGetCount(sourceList); ++i) {
    TISInputSourceRef source = (TISInputSourceRef)(CFArrayGetValueAtIndex(sourceList, i));
    NSString *sourceID = InputSourceID(source);
    if (![sourceID hasPrefix:bundleID]) {
      continue;
    }
    ++count;
    fprintf(stderr, "  %s enabled=%d selected=%d enable_capable=%d select_capable=%d\n",
            [sourceID UTF8String], TISBool(source, kTISPropertyInputSourceIsEnabled),
            TISBool(source, kTISPropertyInputSourceIsSelected),
            TISBool(source, kTISPropertyInputSourceIsEnableCapable),
            TISBool(source, kTISPropertyInputSourceIsSelectCapable));
  }
  CFRelease(sourceList);
  return count;
}

// Registers the running bundle with Text Input Services. |select| additionally
// enables every mode and selects the base one.
//
// Exit status reports only whether the registration call itself succeeded and
// left the sources listed. It deliberately does NOT claim the sources are
// visible in the current login session, because no API checked here can tell:
// in the failure this was written for, TISRegisterInputSource returns noErr and
// all modes stay listed while System Settings offers none of them. Only a
// logout is known to resolve that, so callers must phrase success accordingly.
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

  if (select) {
    CFArrayRef sourceList = TISCreateInputSourceList(nullptr, true);
    if (sourceList != nullptr) {
      for (CFIndex i = 0; i < CFArrayGetCount(sourceList); ++i) {
        TISInputSourceRef source = (TISInputSourceRef)(CFArrayGetValueAtIndex(sourceList, i));
        NSString *sourceID = InputSourceID(source);
        if (![sourceID hasPrefix:bundleID]) {
          continue;
        }
        TISEnableInputSource(source);
        if ([sourceID isEqualToString:[bundleID stringByAppendingString:@".base"]] ||
            [sourceID isEqualToString:bundleID]) {
          TISSelectInputSource(source);
        }
      }
      CFRelease(sourceList);
    }
  }

  const int count = ReportInputSources(bundleID);
  if (count <= 0) {
    fprintf(stderr, "ERROR: no %s input sources listed after registration\n",
            [bundleID UTF8String]);
    return 1;
  }
  fprintf(stderr,
          "%d input sources listed. This does NOT prove they are visible in this\n"
          "login session; if they are missing from System Settings, log out.\n",
          count);
  printf("%d\n", count);
  return 0;
}

// Reports the input source state without registering anything, so that a broken
// and a working session can be compared property by property.
int VerifyInputSource() {
  NSString *bundleID = [[NSBundle mainBundle] bundleIdentifier];
  if (bundleID == nil) {
    fprintf(stderr, "ERROR: cannot resolve the running bundle\n");
    return 1;
  }
  const int count = ReportInputSources(bundleID);
  if (count <= 0) {
    fprintf(stderr, "no %s input sources listed\n", [bundleID UTF8String]);
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
    if (std::strcmp(argv[i], "--verify_input_source") == 0) {
      return VerifyInputSource();
    }
  }

  if (!mozc::RunLevel::IsValidClientRunLevel()) {
    return -1;
  }

  mozc::InitMozc(argv[0], &argc, &argv);

  // Registered before the IMK server starts, so that the converter's Mach
  // service is available by the time the first key event arrives.
  RegisterLaunchAgents();

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
