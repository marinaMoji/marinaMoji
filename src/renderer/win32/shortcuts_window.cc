// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0

#include "renderer/win32/shortcuts_window.h"

#include <commctrl.h>
#include <uxtheme.h>
#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "base/win32/wide_char.h"
#include "protocol/commands.pb.h"
#include "protocol/renderer_command.pb.h"
#include "renderer/win32/marina_localized_string.h"
#include "renderer/win32/win32_dpi_util.h"

namespace mozc {
namespace renderer {
namespace win32 {

namespace {

constexpr int kNumTabs = static_cast<int>(3);

// Logical (96 DPI) metrics; scaled per-monitor like the palette's.
constexpr int kWindowWidthLogical = 440;
constexpr int kWindowHeightLogical = 400;
constexpr int kMinWindowWidthLogical = 320;
constexpr int kMinWindowHeightLogical = 200;
constexpr int kMarginLogical = 10;
constexpr int kTabStripHeightLogical = 26;

constexpr int kTabControlId = 1101;
constexpr int kListViewIdBase = 1200;

const wchar_t* TabLabelKey(int tab) {
  switch (tab) {
    case 0:
      return L"MM.Script";
    case 1:
      return L"MM.Composition";
    default:
      return L"MM.Kaeriten";
  }
}

// The Kaeriten tab is the other way round from the other two: its left column
// is the glyph produced and its right column the input that produces it.
const wchar_t* LeftColumnKey(int tab) {
  return tab == 2 ? L"MM.Result" : L"MM.Function";
}

const wchar_t* RightColumnKey(int tab) {
  return tab == 2 ? L"MM.Input" : L"MM.Keys";
}

bool IsDarkTheme() {
  HKEY key = nullptr;
  if (::RegOpenKeyExW(
          HKEY_CURRENT_USER,
          L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
          0, KEY_READ, &key) != ERROR_SUCCESS) {
    return false;
  }
  DWORD value = 1;
  DWORD size = sizeof(value);
  const LSTATUS status =
      ::RegQueryValueExW(key, L"AppsUseLightTheme", nullptr, nullptr,
                         reinterpret_cast<BYTE*>(&value), &size);
  ::RegCloseKey(key);
  return status == ERROR_SUCCESS && value == 0;
}

}  // namespace

ShortcutsWindow::ShortcutsWindow()
    : send_command_interface_(nullptr),
      tab_control_(nullptr),
      list_views_{},
      active_tab_(0),
      has_shown_once_(false),
      dpi_(USER_DEFAULT_SCREEN_DPI),
      is_dark_theme_(false),
      ui_font_(nullptr),
      background_brush_(nullptr) {}

ShortcutsWindow::~ShortcutsWindow() {}

void ShortcutsWindow::Initialize() {
  INITCOMMONCONTROLSEX icc = {sizeof(icc),
                              ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES |
                                  ICC_STANDARD_CLASSES};
  ::InitCommonControlsEx(&icc);
  Create(nullptr);
  ShowWindow(SW_HIDE);
}

void ShortcutsWindow::Destroy() {
  if (IsWindow()) {
    DestroyWindow();
  }
}

void ShortcutsWindow::SetSendCommandInterface(
    client::SendCommandInterface* send_command_interface) {
  send_command_interface_ = send_command_interface;
}

void ShortcutsWindow::Hide() { ShowWindow(SW_HIDE); }

int ShortcutsWindow::Scaled(int logical_value) const {
  return std::max(
      1, static_cast<int>(std::lround(logical_value *
                                      GetDPIScalingFactor(dpi_))));
}

LRESULT ShortcutsWindow::OnCreate(LPCREATESTRUCT create_struct) {
  dpi_ = GetDpiForPoint(0, 0);
  is_dark_theme_ = IsDarkTheme();

  SetWindowTextW(MarinaLocalizedString(L"MM.KeyboardShortcuts"));
  CreateFonts();
  CreateBackgroundBrush();
  CreateTabControl();
  CreateListViews();
  return 0;
}

void ShortcutsWindow::OnDestroy() {
  DeleteFonts();
  if (background_brush_ != nullptr) {
    ::DeleteObject(background_brush_);
    background_brush_ = nullptr;
  }
}

void ShortcutsWindow::CreateFonts() {
  DeleteFonts();
  NONCLIENTMETRICSW metrics = {};
  metrics.cbSize = sizeof(metrics);
  if (::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics),
                              &metrics, 0)) {
    LOGFONTW log_font = metrics.lfMessageFont;
    const double system_scale = GetDPIScalingFactor(GetDpiForPoint(0, 0));
    const double window_scale = GetDPIScalingFactor(dpi_);
    if (system_scale > 0.0) {
      log_font.lfHeight = static_cast<LONG>(
          std::lround(log_font.lfHeight * window_scale / system_scale));
    }
    ui_font_ = ::CreateFontIndirectW(&log_font);
  }
  if (ui_font_ == nullptr) {
    ui_font_ = static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
    owns_ui_font_ = false;
  } else {
    owns_ui_font_ = true;
  }
}

void ShortcutsWindow::DeleteFonts() {
  if (owns_ui_font_ && ui_font_ != nullptr) {
    ::DeleteObject(ui_font_);
  }
  ui_font_ = nullptr;
  owns_ui_font_ = false;
}

void ShortcutsWindow::ApplyFont(HWND control, HFONT font) {
  if (control != nullptr && font != nullptr) {
    ::SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font),
                   MAKELPARAM(TRUE, 0));
  }
}

void ShortcutsWindow::ApplyDarkModeTheme(HWND control) {
  if (control == nullptr) {
    return;
  }
  ::SetWindowTheme(control, is_dark_theme_ ? L"DarkMode_Explorer" : nullptr,
                   nullptr);
}

void ShortcutsWindow::CreateBackgroundBrush() {
  if (background_brush_ != nullptr) {
    ::DeleteObject(background_brush_);
  }
  background_brush_ = ::CreateSolidBrush(BackgroundColor());
}

COLORREF ShortcutsWindow::BackgroundColor() const {
  return is_dark_theme_ ? RGB(32, 35, 40) : ::GetSysColor(COLOR_WINDOW);
}

COLORREF ShortcutsWindow::TextColor() const {
  return is_dark_theme_ ? RGB(235, 235, 235) : ::GetSysColor(COLOR_WINDOWTEXT);
}

void ShortcutsWindow::CreateTabControl() {
  tab_control_ = ::CreateWindowExW(
      0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, m_hWnd,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTabControlId)), nullptr,
      nullptr);
  if (tab_control_ == nullptr) {
    return;
  }
  ApplyFont(tab_control_, ui_font_);
  ApplyDarkModeTheme(tab_control_);
  for (int i = 0; i < kNumTabs; ++i) {
    TCITEMW item = {};
    item.mask = TCIF_TEXT;
    item.pszText = const_cast<wchar_t*>(MarinaLocalizedString(TabLabelKey(i)));
    TabCtrl_InsertItem(tab_control_, i, &item);
  }
  TabCtrl_SetCurSel(tab_control_, active_tab_);
}

void ShortcutsWindow::CreateListViews() {
  for (int i = 0; i < kNumTabs; ++i) {
    HWND list = ::CreateWindowExW(
        0, WC_LISTVIEWW, L"",
        WS_CHILD | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS |
            LVS_NOSORTHEADER,
        0, 0, 0, 0, m_hWnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kListViewIdBase + i)),
        nullptr, nullptr);
    list_views_[i] = list;
    if (list == nullptr) {
      continue;
    }
    ListView_SetExtendedListViewStyle(
        list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    ApplyFont(list, ui_font_);
    ApplyDarkModeTheme(list);
    if (is_dark_theme_) {
      ListView_SetBkColor(list, BackgroundColor());
      ListView_SetTextBkColor(list, BackgroundColor());
      ListView_SetTextColor(list, TextColor());
    }

    LVCOLUMNW column = {};
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    column.iSubItem = 0;
    column.cx = Scaled(200);
    column.pszText = const_cast<wchar_t*>(MarinaLocalizedString(
        LeftColumnKey(i)));
    ListView_InsertColumn(list, 0, &column);

    column.iSubItem = 1;
    column.pszText = const_cast<wchar_t*>(MarinaLocalizedString(
        RightColumnKey(i)));
    ListView_InsertColumn(list, 1, &column);
  }
  ShowOnlyActiveTab();
}

void ShortcutsWindow::SetRowsForTab(Tab tab, const std::vector<Row>& rows) {
  const size_t idx = static_cast<size_t>(tab);
  if (rows_[idx] == rows) {
    return;  // unchanged; don't rebuild the list on every update
  }
  rows_[idx] = rows;

  HWND list = list_views_[idx];
  if (list == nullptr) {
    return;
  }
  ListView_DeleteAllItems(list);
  for (size_t i = 0; i < rows.size(); ++i) {
    LVITEMW item = {};
    item.mask = LVIF_TEXT;
    item.iItem = static_cast<int>(i);
    item.iSubItem = 0;
    item.pszText = const_cast<wchar_t*>(rows[i].function.c_str());
    ListView_InsertItem(list, &item);
    ListView_SetItemText(list, static_cast<int>(i), 1,
                         const_cast<wchar_t*>(rows[i].keys.c_str()));
  }
  UpdateColumnWidths(tab);
}

void ShortcutsWindow::UpdateColumnWidths(Tab tab) {
  HWND list = list_views_[static_cast<size_t>(tab)];
  if (list == nullptr) {
    return;
  }
  CRect client_rect;
  GetClientRect(&client_rect);
  const int available =
      std::max(Scaled(kMinWindowWidthLogical),
               client_rect.Width() - Scaled(kMarginLogical) * 2 -
                   ::GetSystemMetrics(SM_CXVSCROLL));
  // Give the key column a little more room than the command column: command
  // names are long but predictable, key lists grow with every extra binding.
  ListView_SetColumnWidth(list, 0, available * 45 / 100);
  ListView_SetColumnWidth(list, 1, available - available * 45 / 100);
}

void ShortcutsWindow::ShowOnlyActiveTab() {
  for (int i = 0; i < kNumTabs; ++i) {
    HWND list = list_views_[i];
    if (list != nullptr) {
      ::ShowWindow(list, i == active_tab_ ? SW_SHOW : SW_HIDE);
    }
  }
}

void ShortcutsWindow::LayoutChildren() {
  CRect client_rect;
  GetClientRect(&client_rect);
  const int margin = Scaled(kMarginLogical);
  const int tab_height = Scaled(kTabStripHeightLogical);

  if (tab_control_ != nullptr) {
    ::SetWindowPos(tab_control_, nullptr, margin, margin,
                   std::max(0, client_rect.Width() - margin * 2),
                   std::max(0, client_rect.Height() - margin * 2),
                   SWP_NOZORDER | SWP_NOACTIVATE);
  }

  // The list views sit inside the tab control's display area.
  RECT display = {margin, margin, client_rect.right - margin,
                  client_rect.bottom - margin};
  if (tab_control_ != nullptr) {
    TabCtrl_AdjustRect(tab_control_, FALSE, &display);
  } else {
    display.top += tab_height;
  }
  for (int i = 0; i < kNumTabs; ++i) {
    HWND list = list_views_[i];
    if (list == nullptr) {
      continue;
    }
    ::SetWindowPos(list, HWND_TOP, display.left, display.top,
                   std::max(0, static_cast<int>(display.right - display.left)),
                   std::max(0, static_cast<int>(display.bottom - display.top)),
                   SWP_NOACTIVATE);
  }
  for (int i = 0; i < kNumTabs; ++i) {
    UpdateColumnWidths(static_cast<Tab>(i));
  }
}

LRESULT ShortcutsWindow::OnSize(UINT msg_id, WPARAM wparam, LPARAM lparam,
                                BOOL& handled) {
  LayoutChildren();
  handled = TRUE;
  return 0;
}

LRESULT ShortcutsWindow::OnGetMinMaxInfo(UINT msg_id, WPARAM wparam,
                                         LPARAM lparam, BOOL& handled) {
  auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
  if (info != nullptr) {
    info->ptMinTrackSize.x = Scaled(kMinWindowWidthLogical);
    info->ptMinTrackSize.y = Scaled(kMinWindowHeightLogical);
    handled = TRUE;
  }
  return 0;
}

LRESULT ShortcutsWindow::OnNotify(UINT msg_id, WPARAM wparam, LPARAM lparam,
                                  BOOL& handled) {
  const auto* header = reinterpret_cast<const NMHDR*>(lparam);
  if (header != nullptr && header->hwndFrom == tab_control_ &&
      header->code == TCN_SELCHANGE) {
    active_tab_ = TabCtrl_GetCurSel(tab_control_);
    ShowOnlyActiveTab();
    handled = TRUE;
    return 0;
  }
  handled = FALSE;
  return 0;
}

LRESULT ShortcutsWindow::OnCloseMessage(UINT msg_id, WPARAM wparam,
                                        LPARAM lparam, BOOL& handled) {
  handled = TRUE;
  SendHideSignal();
  Hide();
  return 0;
}

LRESULT ShortcutsWindow::OnEraseBackground(UINT msg_id, WPARAM wparam,
                                           LPARAM lparam, BOOL& handled) {
  auto dc = reinterpret_cast<HDC>(wparam);
  if (dc == nullptr || background_brush_ == nullptr) {
    handled = FALSE;
    return 0;
  }
  CRect client_rect;
  GetClientRect(&client_rect);
  ::FillRect(dc, &client_rect, background_brush_);
  handled = TRUE;
  return 1;
}

LRESULT ShortcutsWindow::OnCtlColor(UINT msg_id, WPARAM wparam, LPARAM lparam,
                                    BOOL& handled) {
  auto dc = reinterpret_cast<HDC>(wparam);
  if (dc == nullptr || background_brush_ == nullptr) {
    handled = FALSE;
    return 0;
  }
  ::SetTextColor(dc, TextColor());
  ::SetBkColor(dc, BackgroundColor());
  handled = TRUE;
  return reinterpret_cast<LRESULT>(background_brush_);
}

LRESULT ShortcutsWindow::OnDpiChanged(UINT msg_id, WPARAM wparam,
                                      LPARAM lparam, BOOL& handled) {
  const uint32_t new_dpi = static_cast<uint32_t>(LOWORD(wparam));
  if (new_dpi != 0 && new_dpi != dpi_) {
    dpi_ = new_dpi;
    CreateFonts();
    ApplyFont(tab_control_, ui_font_);
    for (HWND list : list_views_) {
      ApplyFont(list, ui_font_);
    }
    // Windows suggests a rect scaled for the new monitor; honour it and let
    // WM_SIZE re-lay the children out.
    const auto* suggested = reinterpret_cast<const RECT*>(lparam);
    if (suggested != nullptr) {
      SetWindowPos(nullptr, suggested->left, suggested->top,
                   suggested->right - suggested->left,
                   suggested->bottom - suggested->top,
                   SWP_NOZORDER | SWP_NOACTIVATE);
    } else {
      LayoutChildren();
    }
  }
  handled = TRUE;
  return 0;
}

LRESULT ShortcutsWindow::OnSettingChange(UINT msg_id, WPARAM wparam,
                                         LPARAM lparam, BOOL& handled) {
  const bool dark = IsDarkTheme();
  if (dark != is_dark_theme_) {
    is_dark_theme_ = dark;
    CreateBackgroundBrush();
    ApplyDarkModeTheme(tab_control_);
    for (HWND list : list_views_) {
      ApplyDarkModeTheme(list);
      if (list == nullptr) {
        continue;
      }
      ListView_SetBkColor(list, BackgroundColor());
      ListView_SetTextBkColor(list, BackgroundColor());
      ListView_SetTextColor(list, TextColor());
    }
    Invalidate(TRUE);
  }
  handled = FALSE;
  return 0;
}

void ShortcutsWindow::SendHideSignal() {
  if (send_command_interface_ == nullptr) {
    return;
  }
  commands::SessionCommand command;
  command.set_type(commands::SessionCommand::HIDE_SHORTCUTS_WINDOW);
  commands::Output output;
  send_command_interface_->SendCommand(command, &output);
}

void ShortcutsWindow::OnUpdate(const commands::RendererCommand& command) {
  if (!command.has_application_info() ||
      !command.application_info().has_shortcuts_info()) {
    // TIP has closed the window (e.g. focus loss) -- follow suit.
    if (IsWindowVisible()) {
      Hide();
    }
    return;
  }

  const auto& info = command.application_info().shortcuts_info();

  const auto to_rows =
      [](const auto& proto_rows) {
        std::vector<Row> rows;
        rows.reserve(proto_rows.size());
        for (const auto& proto_row : proto_rows) {
          rows.push_back(Row{mozc::win32::Utf8ToWide(proto_row.function()),
                             mozc::win32::Utf8ToWide(proto_row.keys())});
        }
        return rows;
      };
  SetRowsForTab(Tab::kScript, to_rows(info.script()));
  SetRowsForTab(Tab::kComposition, to_rows(info.composition()));
  SetRowsForTab(Tab::kKaeriten, to_rows(info.kaeriten()));

  if (!has_shown_once_) {
    RECT work_area = {0, 0, 1920, 1080};
    ::SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);
    const int width = Scaled(kWindowWidthLogical);
    const int height = Scaled(kWindowHeightLogical);
    const int x =
        work_area.left + (work_area.right - work_area.left - width) / 2;
    const int y =
        work_area.top + (work_area.bottom - work_area.top - height) / 2;
    SetWindowPos(nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    has_shown_once_ = true;
  }

  if (!IsWindowVisible()) {
    ShowWindow(SW_SHOWNA);
    LayoutChildren();
  }
}

}  // namespace win32
}  // namespace renderer
}  // namespace mozc
