// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0
//
// marinaMoji: Windows Keyboard Shortcuts window (Script / Composition /
// Kaeriten tabs). Mirrors src/mac/mozc_toolbar.mm's
// MozcShortcutsWindowController. Like SymbolsPaletteWindow this is built from
// native common controls -- a SysTabControl32 plus one report-mode
// SysListView32 per tab -- rather than custom GDI compositing, since it is
// all text.
//
// The rows themselves are computed TIP-side (session/marina_shortcut_list_
// util.cc) and arrive as RendererCommand::ShortcutsInfo: the renderer
// deliberately links no config or keymap parsing.

#ifndef MOZC_RENDERER_WIN32_SHORTCUTS_WINDOW_H_
#define MOZC_RENDERER_WIN32_SHORTCUTS_WINDOW_H_

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

// Unlike the toolbar and the palette this window is a normal, resizable,
// *activatable* one: it shows reference text the user reads and scrolls, so
// taking focus is correct here -- and unlike the palette, nothing about it
// commits text into the application, so there is no context to preserve.
typedef ATL::CWinTraits<WS_OVERLAPPEDWINDOW, WS_EX_TOOLWINDOW>
    ShortcutsWindowTraits;

class ShortcutsWindow
    : public ATL::CWindowImpl<ShortcutsWindow, ATL::CWindow,
                              ShortcutsWindowTraits> {
 public:
  DECLARE_WND_CLASS_EX(kShortcutsWindowClassName, 0, COLOR_WINDOW);

  BEGIN_MSG_MAP(ShortcutsWindow)
  MESSAGE_HANDLER(WM_CREATE, OnCreate)
  MESSAGE_HANDLER(WM_DESTROY, OnDestroyMessage)
  MESSAGE_HANDLER(WM_SIZE, OnSize)
  MESSAGE_HANDLER(WM_NOTIFY, OnNotify)
  MESSAGE_HANDLER(WM_CLOSE, OnCloseMessage)
  MESSAGE_HANDLER(WM_GETMINMAXINFO, OnGetMinMaxInfo)
  MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBackground)
  MESSAGE_HANDLER(WM_CTLCOLORSTATIC, OnCtlColor)
  MESSAGE_HANDLER(WM_DPICHANGED, OnDpiChanged)
  MESSAGE_HANDLER(WM_SETTINGCHANGE, OnSettingChange)
  END_MSG_MAP()

  ShortcutsWindow();
  ShortcutsWindow(const ShortcutsWindow&) = delete;
  ShortcutsWindow& operator=(const ShortcutsWindow&) = delete;
  ~ShortcutsWindow();

  void Initialize();
  void Destroy();

  // Fills the tabs from ShortcutsInfo (if present) and shows/hides the
  // window accordingly. Call from WindowManager::UpdateLayout.
  void OnUpdate(const commands::RendererCommand& command);

  void Hide();
  void SetSendCommandInterface(
      client::SendCommandInterface* send_command_interface);

 private:
  enum class Tab {
    kScript = 0,
    kComposition,
    kKaeriten,
    kNumTabs,
  };

  struct Row {
    std::wstring function;
    std::wstring keys;

    friend bool operator==(const Row& lhs, const Row& rhs) {
      return lhs.function == rhs.function && lhs.keys == rhs.keys;
    }
  };

  LRESULT OnCreate(LPCREATESTRUCT create_struct);
  void OnDestroy();
  LRESULT OnSize(UINT msg_id, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnNotify(UINT msg_id, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnCloseMessage(UINT msg_id, WPARAM wparam, LPARAM lparam,
                         BOOL& handled);
  LRESULT OnGetMinMaxInfo(UINT msg_id, WPARAM wparam, LPARAM lparam,
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
  void CreateListViews();
  void SetRowsForTab(Tab tab, const std::vector<Row>& rows);
  void ShowOnlyActiveTab();
  void LayoutChildren();
  void UpdateColumnWidths(Tab tab);

  int Scaled(int logical_value) const;
  void CreateFonts();
  void DeleteFonts();
  void ApplyFont(HWND control, HFONT font);
  void ApplyDarkModeTheme(HWND control);
  void CreateBackgroundBrush();
  COLORREF BackgroundColor() const;
  COLORREF TextColor() const;

  void SendHideSignal();

  client::SendCommandInterface* send_command_interface_;
  HWND tab_control_;
  std::array<HWND, static_cast<size_t>(Tab::kNumTabs)> list_views_;
  std::array<std::vector<Row>, static_cast<size_t>(Tab::kNumTabs)> rows_;
  int active_tab_;
  bool has_shown_once_;

  uint32_t dpi_;
  bool is_dark_theme_;
  HFONT ui_font_;
  bool owns_ui_font_ = false;
  HBRUSH background_brush_;
};

}  // namespace win32
}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_WIN32_SHORTCUTS_WINDOW_H_
