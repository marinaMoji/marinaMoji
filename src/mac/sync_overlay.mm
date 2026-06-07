#import <Cocoa/Cocoa.h>

#include "mac/sync_overlay.h"
#include "sync/sync_status.h"

namespace mozc {
namespace mac {
namespace {

static NSPanel* g_sync_panel = nil;
static NSTextField* g_sync_label = nil;
static bool g_sync_active = false;
static dispatch_source_t g_status_timer = nil;
static NSTimeInterval g_last_flash_time = 0;

static void EnsureOverlayOnMainQueue() {
  if (g_sync_panel) {
    return;
  }
  const NSScreen* screen = [NSScreen mainScreen];
  const NSRect frame = screen ? [screen frame] : NSMakeRect(0, 0, 800, 600);
  const CGFloat width = 360;
  const CGFloat height = 56;
  const NSRect panelRect = NSMakeRect(
      NSMidX(frame) - width / 2, NSMidY(frame) - height / 2, width, height);

  g_sync_panel = [[NSPanel alloc] initWithContentRect:panelRect
                                            styleMask:NSWindowStyleMaskBorderless
                                              backing:NSBackingStoreBuffered
                                                defer:YES];
  g_sync_panel.backgroundColor = [[NSColor blackColor] colorWithAlphaComponent:0.55];
  g_sync_panel.opaque = NO;
  g_sync_panel.hasShadow = YES;
  [g_sync_panel setFloatingPanel:YES];
  [g_sync_panel setLevel:NSPopUpMenuWindowLevel + 1];
  [g_sync_panel setBecomesKeyOnlyIfNeeded:YES];
  [g_sync_panel setHidesOnDeactivate:NO];
  [g_sync_panel setReleasedWhenClosed:NO];

  g_sync_label = [[NSTextField alloc] initWithFrame:NSMakeRect(16, 12, width - 32, height - 24)];
  g_sync_label.bezeled = NO;
  g_sync_label.editable = NO;
  g_sync_label.selectable = NO;
  g_sync_label.drawsBackground = NO;
  g_sync_label.textColor = [NSColor whiteColor];
  g_sync_label.font = [NSFont boldSystemFontOfSize:16];
  g_sync_label.alignment = NSTextAlignmentCenter;
  g_sync_label.stringValue = @"marinaMoji synchronising…";
  [g_sync_panel setContentView:g_sync_label];
}

static void UpdateFromStatusOnMainQueue() {
  const auto status_or = sync::ReadSyncStatus();
  const bool running =
      status_or.ok() && status_or->state == "running";
  g_sync_active = running;
  if (!running && g_sync_panel) {
    [g_sync_panel orderOut:nil];
  }
}

static void PollStatusTimer() {
  dispatch_async(dispatch_get_main_queue(), ^{
    UpdateFromStatusOnMainQueue();
  });
}

}  // namespace

void SyncOverlaySetVisible(bool visible) {
  dispatch_async(dispatch_get_main_queue(), ^{
    g_sync_active = visible;
    if (visible) {
      EnsureOverlayOnMainQueue();
      [g_sync_panel orderFront:nil];
    } else if (g_sync_panel) {
      [g_sync_panel orderOut:nil];
    }
  });
}

void SyncOverlayFlashBlockedInput() {
  dispatch_async(dispatch_get_main_queue(), ^{
    const NSTimeInterval now = [[NSDate date] timeIntervalSince1970];
    if (now - g_last_flash_time < 1.0) {
      NSBeep();
      return;
    }
    g_last_flash_time = now;
    NSBeep();
    EnsureOverlayOnMainQueue();
    const auto status_or = sync::ReadSyncStatus();
    if (status_or.ok() && !status_or->message.empty()) {
      g_sync_label.stringValue =
          [NSString stringWithUTF8String:status_or->message.c_str()];
    } else {
      g_sync_label.stringValue = @"marinaMoji synchronising…";
    }
    [g_sync_panel orderFront:nil];
    dispatch_after(
        dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.8 * NSEC_PER_SEC)),
        dispatch_get_main_queue(), ^{
          if (g_sync_panel) {
            [g_sync_panel orderOut:nil];
          }
        });
  });
}

bool SyncOverlayIsActive() { return g_sync_active; }

void SyncOverlayStartWatcher() {
  dispatch_async(dispatch_get_main_queue(), ^{
    if (g_status_timer) {
      return;
    }
    g_status_timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0,
                                            dispatch_get_main_queue());
    dispatch_source_set_timer(g_status_timer, DISPATCH_TIME_NOW,
                              (uint64_t)(0.25 * NSEC_PER_SEC),
                              (uint64_t)(0.05 * NSEC_PER_SEC));
    dispatch_source_set_event_handler(g_status_timer, ^{
      PollStatusTimer();
    });
    dispatch_resume(g_status_timer);
    UpdateFromStatusOnMainQueue();
  });
}

void SyncOverlayStopWatcher() {
  dispatch_async(dispatch_get_main_queue(), ^{
    if (g_status_timer) {
      dispatch_source_cancel(g_status_timer);
      g_status_timer = nil;
    }
    g_sync_active = false;
    if (g_sync_panel) {
      [g_sync_panel orderOut:nil];
    }
  });
}

}  // namespace mac
}  // namespace mozc
