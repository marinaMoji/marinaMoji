// Copyright 2010-2021, Google Inc.
// All rights reserved.
//
// marinaMoji: Windows Symbols Palette window (Odoriji / Kaeriten / Symbols /
// User tabs). Mirrors src/mac/mozc_toolbar.mm's
// MozcSymbolsPaletteWindowController and the Linux GTK palette
// (src/unix/ibus/mozc_toolbar.cc) in functionality. Unlike ToolbarWindow,
// this uses native Win32 common controls (SysTabControl32 + BUTTON children)
// rather than custom GDI compositing, since symbol buttons are just
// system-font glyphs, not icons.

#ifndef MOZC_RENDERER_WIN32_SYMBOLS_PALETTE_WINDOW_H_
#define MOZC_RENDERER_WIN32_SYMBOLS_PALETTE_WINDOW_H_

#include <atlbase.h>
#include <atltypes.h>
#include <atlwin.h>
#include <windows.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "base/const.h"
#include "client/client_interface.h"
#include "protocol/renderer_command.pb.h"

namespace mozc {
namespace renderer {
namespace win32 {

// WS_EX_NOACTIVATE keeps this from stealing focus from the target
// application's text field when it's shown/clicked, matching mac's
// NSWindowStyleMaskNonactivatingPanel. Unlike ToolbarWindow this has normal
// window chrome (caption + close box), so WS_POPUPWINDOW | WS_CAPTION rather
// than WS_POPUP alone. WS_VSCROLL is present for the tabs whose symbol list
// is taller than the window (a long user list); the bar is hidden on the
// tabs that fit.
typedef ATL::CWinTraits<WS_POPUPWINDOW | WS_CAPTION | WS_VSCROLL,
                        // The toolbar is topmost, so the palette must be as
                        // well. Otherwise SW_SHOWNA succeeds but the palette
                        // is immediately covered by the focused application.
                        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE>
    SymbolsPaletteWindowTraits;

class SymbolsPaletteWindow
    : public ATL::CWindowImpl<SymbolsPaletteWindow, ATL::CWindow,
                              SymbolsPaletteWindowTraits> {
 public:
  DECLARE_WND_CLASS_EX(kSymbolsPaletteWindowClassName, 0, COLOR_WINDOW);

  BEGIN_MSG_MAP(SymbolsPaletteWindow)
  MESSAGE_HANDLER(WM_CREATE, OnCreate)
  MESSAGE_HANDLER(WM_DESTROY, OnDestroyMessage)
  MESSAGE_HANDLER(WM_COMMAND, OnCommand)
  MESSAGE_HANDLER(WM_NOTIFY, OnNotify)
  MESSAGE_HANDLER(WM_CLOSE, OnCloseMessage)
  MESSAGE_HANDLER(WM_VSCROLL, OnVScroll)
  MESSAGE_HANDLER(WM_MOUSEWHEEL, OnMouseWheel)
  MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBackground)
  MESSAGE_HANDLER(WM_CTLCOLORSTATIC, OnCtlColor)
  MESSAGE_HANDLER(WM_CTLCOLORBTN, OnCtlColor)
  MESSAGE_HANDLER(WM_DPICHANGED, OnDpiChanged)
  MESSAGE_HANDLER(WM_SETTINGCHANGE, OnSettingChange)
  END_MSG_MAP()

  SymbolsPaletteWindow();
  SymbolsPaletteWindow(const SymbolsPaletteWindow&) = delete;
  SymbolsPaletteWindow& operator=(const SymbolsPaletteWindow&) = delete;
  ~SymbolsPaletteWindow();

  void Initialize();
  void Destroy();

  // Updates the Kaeriten/User tabs from SymbolsPaletteInfo (if present) and
  // shows/hides the window based on whether the palette is wanted. Call from
  // WindowManager::UpdateLayout.
  void OnUpdate(const commands::RendererCommand& command);

  void Hide();
  void SetSendCommandInterface(
      client::SendCommandInterface* send_command_interface);

 private:
  enum class Tab {
    kOdoriji = 0,
    kKaeriten,
    kSymbols,
    kUser,
    kNumTabs,
  };

  LRESULT OnCreate(LPCREATESTRUCT create_struct);
  void OnDestroy();
  LRESULT OnCommand(UINT msg_id, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnNotify(UINT msg_id, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnCloseMessage(UINT msg_id, WPARAM wparam, LPARAM lparam,
                        BOOL& handled);
  LRESULT OnVScroll(UINT msg_id, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnMouseWheel(UINT msg_id, WPARAM wparam, LPARAM lparam,
                       BOOL& handled);
  LRESULT OnEraseBackground(UINT msg_id, WPARAM wparam, LPARAM lparam,
                            BOOL& handled);
  LRESULT OnCtlColor(UINT msg_id, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnDpiChanged(UINT msg_id, WPARAM wparam, LPARAM lparam,
                       BOOL& handled);
  LRESULT OnSettingChange(UINT msg_id, WPARAM wparam, LPARAM lparam,
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

  void CreateTabControl();
  void RebuildButtonsForTab(Tab tab, const std::vector<std::wstring>& symbols);
  void OnSymbolClicked(Tab tab, int index);
  void OnPinToggled();

  // Recomputes the window size for the active tab's content and repositions
  // every child. Called whenever the tab, its contents, or the DPI changes.
  void Relayout();
  void LayoutButtons();
  void UpdateScrollBar();
  void ScrollTo(int offset);
  int MaxScrollOffset() const;
  int RowCountForTab(int tab) const;

  // Client-area top of the symbol grid (below the tab strip and, on tabs
  // that have one, the hint label).
  int SymbolAreaTop() const;
  CSize ComputeClientSize() const;

  // |logical_value| (a 96 DPI measurement) in pixels at the current DPI.
  int Scaled(int logical_value) const;

  void CreateFonts();
  void DeleteFonts();
  void ApplyFont(HWND control, HFONT font);
  void ApplyDarkModeTheme(HWND control);
  void CreateBackgroundBrush();
  COLORREF BackgroundColor() const;
  COLORREF TextColor() const;

  void SendShowOdorijiPaletteAndSubmit(int index);
  void SendInsertSymbolText(const std::wstring& symbol);
  void SendHidePaletteSignal();

  std::string ConfigFilePath() const;
  void LoadPreferences();
  void SavePreferences() const;

  client::SendCommandInterface* send_command_interface_;
  HWND tab_control_;
  HWND pin_checkbox_;
  HWND hint_label_;
  int active_tab_;
  bool pinned_;
  bool has_shown_once_;

  // DPI of the monitor this window is on. All layout metrics are logical
  // (96 DPI) constants scaled by this -- the renderer is PerMonitorV2, so
  // nothing here is scaled for us.
  uint32_t dpi_;
  bool is_dark_theme_;

  // Pixels the symbol grid is scrolled down by, for tabs whose list is
  // taller than the window.
  int scroll_offset_;

  // Shell UI font and the larger face used for symbol glyphs. Owned unless
  // the corresponding owns_*_ flag is false (stock-object fallback).
  HFONT ui_font_;
  HFONT symbol_font_;
  bool owns_ui_font_ = false;
  bool owns_symbol_font_ = false;

  // Background fill, owned. The class brush is COLOR_WINDOW, which does not
  // follow the dark theme.
  HBRUSH background_brush_;

  // Per-tab symbol text and their child BUTTON HWNDs. Odoriji/Symbols are
  // populated once at Initialize() (static lists); Kaeriten/User are rebuilt
  // by OnUpdate() whenever SymbolsPaletteInfo changes.
  std::array<std::vector<std::wstring>, static_cast<size_t>(Tab::kNumTabs)>
      tab_symbols_;
  std::array<std::vector<HWND>, static_cast<size_t>(Tab::kNumTabs)>
      tab_buttons_;
};

}  // namespace win32
}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_WIN32_SYMBOLS_PALETTE_WINDOW_H_
