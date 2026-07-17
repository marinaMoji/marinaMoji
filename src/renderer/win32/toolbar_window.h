// Copyright 2010-2021, Google Inc.
// All rights reserved.
//
// marinaMoji: Windows floating toolbar. A non-activating, always-on-top,
// draggable popup with the marinaMoji logo and buttons for mode switching,
// shin/kyu toggle, symbols palette, dictionary, settings, and shortcuts.
// Mirrors src/mac/mozc_toolbar.mm and src/unix/ibus/mozc_toolbar.cc in
// functionality (see docs/GTK_TOOLBAR.md), implemented with raw Win32/GDI
// (no Qt/WinUI) as the renderer's other windows already are.

#ifndef MOZC_RENDERER_WIN32_TOOLBAR_WINDOW_H_
#define MOZC_RENDERER_WIN32_TOOLBAR_WINDOW_H_

#include <atlbase.h>
#include <atltypes.h>
#include <atlwin.h>
#include <wil/resource.h>
#include <windows.h>

// atlbase.h pulls in <shlwapi.h>, which #defines StrCat to StrCatW (a real
// WinAPI path-string function) under UNICODE builds. Left unchecked, this
// silently rewrites every absl::StrCat(...) call in any translation unit
// that includes this header into absl::StrCatW(...), which doesn't exist
// and fails at link time instead of compile time.
#undef StrCat

#include <cstdint>
#include <string>
#include <vector>

#include "base/const.h"
#include "base/coordinates.h"
#include "client/client_interface.h"
#include "protocol/commands.pb.h"
#include "protocol/renderer_command.pb.h"

namespace mozc {
namespace renderer {
namespace win32 {

// Toolbar is interactive (unlike IndicatorWindow), so unlike
// CandidateWindow/IndicatorWindow it must NOT carry WS_DISABLED. It is
// layered (like IndicatorWindow, unlike CandidateWindow) so that rounded
// corners can be composited with true per-pixel alpha instead of relying on
// a DWM region/attribute whose fidelity varies by Windows version.
typedef ATL::CWinTraits<WS_POPUP, WS_EX_LAYERED | WS_EX_TOOLWINDOW |
                                     WS_EX_TOPMOST | WS_EX_NOACTIVATE>
    ToolbarWindowTraits;

class ToolbarWindow : public ATL::CWindowImpl<ToolbarWindow, ATL::CWindow,
                                              ToolbarWindowTraits> {
 public:
  DECLARE_WND_CLASS_EX(kToolbarWindowClassName, 0, COLOR_WINDOW);

  BEGIN_MSG_MAP(ToolbarWindow)
  MESSAGE_HANDLER(WM_CREATE, OnCreate)
  MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
  MESSAGE_HANDLER(WM_LBUTTONUP, OnLButtonUp)
  MESSAGE_HANDLER(WM_NCHITTEST, OnNcHitTest)
  MESSAGE_HANDLER(WM_EXITSIZEMOVE, OnExitSizeMove)
  MESSAGE_HANDLER(WM_SETTINGCHANGE, OnSettingChange)
  MESSAGE_HANDLER(WM_DISPLAYCHANGE, OnDisplayChange)
  END_MSG_MAP()

  ToolbarWindow();
  ToolbarWindow(const ToolbarWindow&) = delete;
  ToolbarWindow& operator=(const ToolbarWindow&) = delete;
  ~ToolbarWindow();

  void Initialize();
  void Destroy();

  // Updates visibility/state from |command| (reads the ShowToolbar bit in
  // ui_visibilities and the top-level Output for mode/shin-kyu) and redraws
  // if visible. Call from WindowManager::UpdateLayout.
  void OnUpdate(const commands::RendererCommand& command);

  // Hides the toolbar unconditionally (e.g. when the renderer is told to
  // hide all windows).
  void Hide();

  void SetSendCommandInterface(
      client::SendCommandInterface* send_command_interface);

  enum class ButtonId {
    kMode = 0,
    kTraditionalKanji,
    kSymbols,
    kDictionary,
    kSettings,
    kShortcuts,
    kNumButtons,
  };

 private:
  LRESULT OnCreate(LPCREATESTRUCT create_struct);
  void OnLButtonDown(UINT flags, CPoint point);
  void OnLButtonUp(UINT flags, CPoint point);
  LRESULT OnNcHitTest(CPoint point);
  void OnExitSizeMove();
  void OnSettingChange(UINT flags, LPCTSTR section);
  void OnDisplayChange();

  inline LRESULT OnCreate(UINT msg_id, WPARAM wparam, LPARAM lparam,
                          BOOL& handled) {
    return static_cast<LRESULT>(
        OnCreate(reinterpret_cast<LPCREATESTRUCT>(lparam)));
  }
  inline LRESULT OnLButtonDown(UINT msg_id, WPARAM wparam, LPARAM lparam,
                               BOOL& handled) {
    OnLButtonDown(static_cast<UINT>(wparam),
                  CPoint(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)));
    return 0;
  }
  inline LRESULT OnLButtonUp(UINT msg_id, WPARAM wparam, LPARAM lparam,
                             BOOL& handled) {
    OnLButtonUp(static_cast<UINT>(wparam),
                CPoint(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)));
    return 0;
  }
  inline LRESULT OnNcHitTest(UINT msg_id, WPARAM wparam, LPARAM lparam,
                             BOOL& handled) {
    return OnNcHitTest(CPoint(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)));
  }
  inline LRESULT OnExitSizeMove(UINT msg_id, WPARAM wparam, LPARAM lparam,
                                BOOL& handled) {
    OnExitSizeMove();
    return 0;
  }
  inline LRESULT OnSettingChange(UINT msg_id, WPARAM wparam, LPARAM lparam,
                                 BOOL& handled) {
    OnSettingChange(static_cast<UINT>(wparam),
                    reinterpret_cast<LPCTSTR>(lparam));
    return 0;
  }
  inline LRESULT OnDisplayChange(UINT msg_id, WPARAM wparam, LPARAM lparam,
                                 BOOL& handled) {
    OnDisplayChange();
    return 0;
  }

  // Redraws the composited logo+button bitmap and pushes it via
  // UpdateLayeredWindow. Called whenever visible state changes.
  void Redraw();

  // (Re)loads the icon bitmaps for the current DPI/theme/mode/lock/trad-kanji
  // state into icon_cache_.
  void LoadIcons();

  bool IsDarkTheme() const;

  // Returns the button index hit by a client-coordinate |point|, or -1.
  int HitTestButton(const CPoint& point) const;

  void ShowModeMenu();
  void SendSwitchCompositionMode(commands::CompositionMode mode);
  void SendTurnOffIme();
  void SendToggleTraditionalKanji();
  void SendLaunchWordRegisterDialog();
  void SendLaunchConfigDialog();
  void SendToggleSymbolsPalette();

  std::string ConfigFilePath() const;
  void LoadSavedPosition(CPoint* out_position, const CSize& window_size) const;
  void SavePosition() const;
  CPoint DefaultWindowOrigin(const CSize& window_size) const;

  // Width in pixels reserved for the logo: the loaded logo bitmap's actual
  // width, or kLogoWidthLogical scaled by |scale| before icons are loaded.
  int LogoWidth(double scale) const;

  // Window size for the current DPI and loaded logo. Falls back to the
  // logical logo width when called before the first LoadIcons().
  CSize ComputeWindowSize() const;

  client::SendCommandInterface* send_command_interface_;
  uint32_t dpi_;
  bool is_dark_theme_;
  bool has_state_;
  commands::CompositionMode current_mode_;
  bool activated_;
  bool left_shift_direct_lock_;
  bool use_traditional_kanji_;
  bool symbols_palette_visible_;

  // Top-left corner of the window in screen coordinates, restored from
  // toolbar.conf (or defaulted to bottom-right of the primary monitor) on
  // first show, and updated by OnExitSizeMove() after a drag. Redraw() uses
  // this directly rather than GetWindowRect(), since the window's initial
  // OS-assigned rect (from Create(nullptr)) is (0,0,0,0) and not meaningful.
  CPoint window_origin_;

  // Icon bitmaps for the current theme/DPI, owned. Index matches ButtonId,
  // plus the logo at the end.
  std::vector<wil::unique_hbitmap> icon_cache_;
  wil::unique_hbitmap logo_cache_;
  CSize logo_size_;

  // Client-coordinate rects for hit-testing, recomputed by Redraw().
  std::vector<CRect> button_rects_;

  // Index of the button WM_LBUTTONDOWN landed on, so WM_LBUTTONUP only fires
  // the action when release lands back on the same button (standard button
  // press/release semantics). -1 when no button is currently pressed.
  int pressed_button_;

  // True while ShowModeMenu()'s TrackPopupMenuEx modal loop is running. The
  // loop can dispatch a focus-loss hide before the selected mode command
  // reaches the TIP, so Hide() is suppressed while this is set.
  bool mode_menu_open_;
};

}  // namespace win32
}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_WIN32_TOOLBAR_WINDOW_H_
