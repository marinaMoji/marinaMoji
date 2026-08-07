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

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
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

  // Posted by the MSAA object (toolbar_accessible.cc) so a screen reader's
  // accDoDefaultAction runs the button on this window's thread. MSAA
  // delivers that call on an RPC thread, and some button actions (the mode
  // menu's TrackPopupMenuEx) are only valid on the thread owning the window.
  static constexpr UINT kActivateButtonMessage = WM_APP + 1;

  BEGIN_MSG_MAP(ToolbarWindow)
  MESSAGE_HANDLER(kActivateButtonMessage, OnActivateButtonMessage)
  MESSAGE_HANDLER(WM_CREATE, OnCreate)
  MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
  MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
  MESSAGE_HANDLER(WM_LBUTTONUP, OnLButtonUp)
  MESSAGE_HANDLER(WM_RBUTTONUP, OnRButtonUp)
  MESSAGE_HANDLER(WM_NCRBUTTONUP, OnNcRButtonUp)
  MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
  MESSAGE_HANDLER(WM_MOUSELEAVE, OnMouseLeave)
  MESSAGE_HANDLER(WM_NCHITTEST, OnNcHitTest)
  MESSAGE_HANDLER(WM_EXITSIZEMOVE, OnExitSizeMove)
  MESSAGE_HANDLER(WM_SETTINGCHANGE, OnSettingChange)
  MESSAGE_HANDLER(WM_DISPLAYCHANGE, OnDisplayChange)
  MESSAGE_HANDLER(WM_DPICHANGED, OnDpiChanged)
  MESSAGE_HANDLER(WM_GETOBJECT, OnGetObject)
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

  // Screen-coordinate rect of |button|, for the MSAA accessible object.
  // Returns false before the first Redraw() has laid the buttons out.
  bool GetButtonScreenRect(ButtonId button, CRect* out_rect) const;

  // Localized accessible name / tooltip text for |button|.
  static const wchar_t* GetButtonName(ButtonId button);

  // Performs |button|'s default action, as if it had been clicked. Used by
  // both the mouse path and the accessible object's accDoDefaultAction.
  void ActivateButton(ButtonId button);

 private:
  LRESULT OnCreate(LPCREATESTRUCT create_struct);
  void OnDestroy();
  void OnLButtonDown(UINT flags, CPoint point);
  void OnLButtonUp(UINT flags, CPoint point);
  void OnRButtonUp(UINT flags, CPoint point);
  void OnNcRButtonUp(UINT hit_test, CPoint screen_point);
  void OnMouseMove(UINT flags, CPoint point);
  void OnMouseLeave();
  LRESULT OnNcHitTest(CPoint point);
  void OnExitSizeMove();
  void OnSettingChange(UINT flags, LPCTSTR section);
  void OnDisplayChange();
  void OnDpiChanged(uint32_t dpi, const RECT* suggested_rect);
  LRESULT HandleGetObject(WPARAM wparam, LPARAM lparam, BOOL& handled);

  inline LRESULT OnCreate(UINT msg_id, WPARAM wparam, LPARAM lparam,
                          BOOL& handled) {
    return static_cast<LRESULT>(
        OnCreate(reinterpret_cast<LPCREATESTRUCT>(lparam)));
  }
  inline LRESULT OnDestroy(UINT msg_id, WPARAM wparam, LPARAM lparam,
                           BOOL& handled) {
    OnDestroy();
    handled = FALSE;  // let ATL/DefWindowProc finish the teardown too
    return 0;
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
  inline LRESULT OnRButtonUp(UINT msg_id, WPARAM wparam, LPARAM lparam,
                             BOOL& handled) {
    OnRButtonUp(static_cast<UINT>(wparam),
                CPoint(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)));
    return 0;
  }
  inline LRESULT OnNcRButtonUp(UINT msg_id, WPARAM wparam, LPARAM lparam,
                               BOOL& handled) {
    OnNcRButtonUp(static_cast<UINT>(wparam),
                  CPoint(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)));
    return 0;
  }
  inline LRESULT OnMouseMove(UINT msg_id, WPARAM wparam, LPARAM lparam,
                             BOOL& handled) {
    OnMouseMove(static_cast<UINT>(wparam),
                CPoint(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)));
    return 0;
  }
  inline LRESULT OnMouseLeave(UINT msg_id, WPARAM wparam, LPARAM lparam,
                              BOOL& handled) {
    OnMouseLeave();
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
  inline LRESULT OnDpiChanged(UINT msg_id, WPARAM wparam, LPARAM lparam,
                              BOOL& handled) {
    OnDpiChanged(static_cast<uint32_t>(LOWORD(wparam)),
                 reinterpret_cast<const RECT*>(lparam));
    return 0;
  }
  inline LRESULT OnGetObject(UINT msg_id, WPARAM wparam, LPARAM lparam,
                             BOOL& handled) {
    return HandleGetObject(wparam, lparam, handled);
  }
  inline LRESULT OnActivateButtonMessage(UINT msg_id, WPARAM wparam,
                                         LPARAM lparam, BOOL& handled) {
    const int index = static_cast<int>(wparam);
    if (index >= 0 && index < static_cast<int>(ButtonId::kNumButtons)) {
      ActivateButton(static_cast<ButtonId>(index));
    }
    return 0;
  }

  // Redraws the composited logo+button bitmap and pushes it via
  // UpdateLayeredWindow. Called whenever visible state changes.
  void Redraw();

  // (Re)loads the icon bitmaps for the current DPI/theme/mode/lock/trad-kanji
  // state into icon_cache_.
  void LoadIcons();

  // Returns the decoded bitmap for toolbar icon |name| at |size|, loading
  // and WIC-decoding it from disk only on the first request for that exact
  // (name, size) pair -- LoadIcons() runs on every mode/theme change (and
  // every re-show) while the same handful of PNGs cycle in and out, so
  // without this cache a mode toggle re-hits the disk for icons that were
  // already showing a moment ago. Returned HBITMAP is owned by
  // |icon_disk_cache_| and stays valid for the lifetime of the window.
  HBITMAP GetOrLoadCachedIcon(const std::string& name, int size,
                             CSize* out_size);

  bool IsDarkTheme() const;

  // Returns the button index hit by a client-coordinate |point|, or -1.
  int HitTestButton(const CPoint& point) const;

  void ShowModeMenu();
  void ShowContextMenu(const CPoint& client_point);
  void SendSwitchCompositionMode(commands::CompositionMode mode);
  void SendTurnOffIme();
  void SendToggleTraditionalKanji();
  void SendLaunchWordRegisterDialog();
  void SendLaunchDictionaryTool();
  void SendLaunchConfigDialog();
  void SendToggleSymbolsPalette();
  void SendToggleShortcutsWindow();
  void SendHideToolbar();

  // Runs |menu| at |screen_point| and returns the chosen command id (0 when
  // dismissed), handling the shared bookkeeping both menus need: hide
  // suppression while the modal loop runs, clearing the hover/press
  // highlight the loop leaves stale, and applying any hide that arrived
  // meanwhile. See the comment on the implementation for why the owner
  // window is deliberately left non-foreground.
  int TrackMenu(HMENU menu, const CPoint& screen_point);

  // Creates the tooltip control (once) and syncs one tool rect per button to
  // the current layout. Called from Redraw(), since the rects move with DPI.
  void UpdateTooltips();

  // Re-reads the DPI for the monitor the window currently sits on; reloads
  // icons and redraws when it changed. Called on WM_DPICHANGED, after a drag
  // (WM_EXITSIZEMOVE) and on WM_DISPLAYCHANGE.
  bool SyncDpiForCurrentMonitor();

  std::string ConfigFilePath() const;
  void LoadSavedPosition(CPoint* out_position, const CSize& window_size) const;
  void SavePosition() const;
  CPoint DefaultWindowOrigin(const CSize& window_size) const;

  // Returns |origin| moved back onto a monitor if a window of |window_size|
  // placed there would be (almost) entirely off every current display -- e.g.
  // a position saved while a since-unplugged monitor was attached, which
  // would otherwise strand the toolbar somewhere it can't be dragged from.
  CPoint ClampToVisibleArea(const CPoint& origin,
                            const CSize& window_size) const;

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
  bool shortcuts_window_visible_;

  // Pixel size the square icons are drawn at for the current DPI, i.e.
  // round(24 * scale). The loaded bitmap is whatever tier is nearest and is
  // resampled to this, so 125% / 175% scaling gets correctly sized icons
  // rather than the tier's raw pixels.
  int icon_draw_size_;

  // Pixel size the logo is drawn at: |icon_draw_size_| tall, with the
  // bitmap's natural aspect ratio.
  CSize logo_draw_size_;

  // Top-left corner of the window in screen coordinates, restored from
  // toolbar.conf (or defaulted to bottom-right of the primary monitor) on
  // first show, and updated by OnExitSizeMove() after a drag. Redraw() uses
  // this directly rather than GetWindowRect(), since the window's initial
  // OS-assigned rect (from Create(nullptr)) is (0,0,0,0) and not meaningful.
  CPoint window_origin_;

  // Icon bitmaps for the current theme/DPI. Index matches ButtonId. Not
  // owned -- these are borrowed from |icon_disk_cache_|, which outlives any
  // single LoadIcons() call.
  std::vector<HBITMAP> icon_cache_;
  HBITMAP logo_cache_ = nullptr;

  // Every distinct (name, size) icon decoded so far, keyed by the on-disk
  // path (see GetOrLoadCachedIcon()). Small and unbounded on purpose: the
  // icon set is fixed (kButtonCount + logo, times 2 themes, times at most a
  // couple of DPI tiers actually seen on one monitor setup), so this holds
  // at most a few dozen bitmaps for the life of the renderer process.
  std::map<std::string, std::pair<wil::unique_hbitmap, CSize>>
      icon_disk_cache_;

  // Client-coordinate rects for hit-testing, recomputed by Redraw(). A fixed
  // array rather than a vector because the MSAA object reads it from an RPC
  // thread: a stale coordinate there is harmless, a reallocation underneath
  // the reader would not be.
  std::array<CRect, static_cast<size_t>(ButtonId::kNumButtons)> button_rects_;

  // False until the first Redraw() has filled in |button_rects_|.
  bool has_layout_;

  // Index of the button WM_LBUTTONDOWN landed on, so WM_LBUTTONUP only fires
  // the action when release lands back on the same button (standard button
  // press/release semantics). -1 when no button is currently pressed.
  int pressed_button_;

  // Index of the button the cursor is currently over, or -1. Drives the hover
  // highlight; kept in sync by WM_MOUSEMOVE / WM_MOUSELEAVE.
  int hovered_button_;

  // True once TrackMouseEvent(TME_LEAVE) has been armed for the current
  // hover, so WM_MOUSEMOVE doesn't re-arm it on every pixel of movement.
  bool tracking_mouse_leave_;

  // True while TrackMenu()'s TrackPopupMenuEx modal loop is running. The loop
  // can dispatch a focus-loss hide before the selected command reaches the
  // TIP, so Hide() is suppressed while this is set.
  bool menu_open_;

  // Set when Hide() was suppressed because |menu_open_| was true, so the menu
  // teardown can apply the hide it swallowed. Without this the toolbar can
  // stay on screen indefinitely after focus moves away while a menu is up --
  // no further RendererCommand is guaranteed to arrive once unfocused.
  bool hide_deferred_by_menu_;

  // Tooltip control (one tool per button), owned. Created lazily on the first
  // Redraw() so it exists only for a toolbar that is actually shown.
  HWND tooltip_window_;

  // |button_rects_[0]| as of the last tooltip rect sync, so Redraw()'s
  // hover-driven calls don't re-push unchanged rects.
  CRect tooltip_layout_rect_;

  // MSAA object for this window, created on the first WM_GETOBJECT and
  // released in OnDestroy(). Void to keep <oleacc.h> out of this header.
  void* accessible_;
};

}  // namespace win32
}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_WIN32_TOOLBAR_WINDOW_H_
