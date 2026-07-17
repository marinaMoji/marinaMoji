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
// than WS_POPUP alone.
typedef ATL::CWinTraits<WS_POPUPWINDOW | WS_CAPTION,
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
  MESSAGE_HANDLER(WM_COMMAND, OnCommand)
  MESSAGE_HANDLER(WM_NOTIFY, OnNotify)
  MESSAGE_HANDLER(WM_CLOSE, OnCloseMessage)
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
  LRESULT OnCommand(UINT msg_id, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnNotify(UINT msg_id, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnCloseMessage(UINT msg_id, WPARAM wparam, LPARAM lparam,
                        BOOL& handled);

  inline LRESULT OnCreate(UINT msg_id, WPARAM wparam, LPARAM lparam,
                          BOOL& handled) {
    return OnCreate(reinterpret_cast<LPCREATESTRUCT>(lparam));
  }

  void CreateTabControl();
  void RebuildButtonsForTab(Tab tab, const std::vector<std::wstring>& symbols);
  void ShowOnlyActiveTab();
  void OnSymbolClicked(Tab tab, int index);
  void OnPinToggled();

  void SendShowOdorijiPaletteAndSubmit(int index);
  void SendInsertSymbolText(const std::wstring& symbol);
  void SendHidePaletteSignal();

  std::string ConfigFilePath() const;
  void LoadPreferences();
  void SavePreferences() const;

  client::SendCommandInterface* send_command_interface_;
  HWND tab_control_;
  HWND pin_checkbox_;
  int active_tab_;
  bool pinned_;
  bool has_shown_once_;

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
