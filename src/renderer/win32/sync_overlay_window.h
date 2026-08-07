// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0
//
// marinaMoji: "synchronising…" overlay shown while marinaMojiSync holds the
// session lock and keystrokes are being dropped. Mirrors
// mac/sync_overlay.mm and unix/ibus/sync_overlay.cc.
//
// Those two live in the IME process; on Windows the TIP DLL is loaded into
// every application, so a window owned there would be created per-process.
// The renderer is the natural single-instance host, exactly as for the
// floating toolbar. It also means no IPC is needed: the window polls
// sync.status.json itself on a timer, the same way the mac and Linux
// watchers do, so the overlay appears for the whole duration of a sync
// rather than only when a key is pressed. The actual key blocking is
// independent of this window and lives in the TIP
// (win32/base/sync_lock_util.h).

#ifndef MOZC_RENDERER_WIN32_SYNC_OVERLAY_WINDOW_H_
#define MOZC_RENDERER_WIN32_SYNC_OVERLAY_WINDOW_H_

#include <atlbase.h>
#include <atltypes.h>
#include <atlwin.h>
#include <windows.h>

#include <cstdint>
#include <string>

#include "base/const.h"

namespace mozc {
namespace renderer {
namespace win32 {

// Borderless, click-through, always-on-top and never focusable: it is a
// status indicator layered over whatever the user is working in, and it must
// not disturb the focus of the application whose keystrokes are being held.
typedef ATL::CWinTraits<WS_POPUP, WS_EX_LAYERED | WS_EX_TOOLWINDOW |
                                      WS_EX_TOPMOST | WS_EX_NOACTIVATE |
                                      WS_EX_TRANSPARENT>
    SyncOverlayWindowTraits;

class SyncOverlayWindow
    : public ATL::CWindowImpl<SyncOverlayWindow, ATL::CWindow,
                              SyncOverlayWindowTraits> {
 public:
  DECLARE_WND_CLASS_EX(kSyncOverlayWindowClassName, 0, COLOR_WINDOW);

  BEGIN_MSG_MAP(SyncOverlayWindow)
  MESSAGE_HANDLER(WM_CREATE, OnCreate)
  MESSAGE_HANDLER(WM_DESTROY, OnDestroyMessage)
  MESSAGE_HANDLER(WM_TIMER, OnTimer)
  MESSAGE_HANDLER(WM_PAINT, OnPaint)
  MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBackground)
  MESSAGE_HANDLER(WM_DISPLAYCHANGE, OnDisplayChange)
  END_MSG_MAP()

  SyncOverlayWindow();
  SyncOverlayWindow(const SyncOverlayWindow&) = delete;
  SyncOverlayWindow& operator=(const SyncOverlayWindow&) = delete;
  ~SyncOverlayWindow();

  // Creates the window and starts the status poll. The overlay stays hidden
  // until a sync actually reports state=running.
  void Initialize();
  void Destroy();

 private:
  static constexpr UINT_PTR kPollTimerId = 1;

  LRESULT OnCreate(LPCREATESTRUCT create_struct);
  void OnDestroy();
  LRESULT OnTimer(UINT msg_id, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnPaint(UINT msg_id, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnEraseBackground(UINT msg_id, WPARAM wparam, LPARAM lparam,
                            BOOL& handled);
  LRESULT OnDisplayChange(UINT msg_id, WPARAM wparam, LPARAM lparam,
                          BOOL& handled);

  inline LRESULT OnCreate(UINT msg_id, WPARAM wparam, LPARAM lparam,
                          BOOL& handled) {
    return OnCreate(reinterpret_cast<LPCREATESTRUCT>(lparam));
  }
  inline LRESULT OnDestroyMessage(UINT msg_id, WPARAM wparam, LPARAM lparam,
                                  BOOL& handled) {
    OnDestroy();
    handled = FALSE;
    return 0;
  }

  // Reads sync.status.json and shows/hides/relabels the overlay to match.
  void PollStatus();

  // Resizes to fit |message_| and centres on the primary monitor's work
  // area, then repaints.
  void UpdateLayout();

  int Scaled(int logical_value) const;
  void CreateFont();
  void DeleteFont();

  uint32_t dpi_;
  HFONT font_;
  bool owns_font_ = false;

  // Text currently displayed; empty while hidden.
  std::wstring message_;

  // True while sync.status.json reports state=running.
  bool active_;
};

}  // namespace win32
}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_WIN32_SYNC_OVERLAY_WINDOW_H_
