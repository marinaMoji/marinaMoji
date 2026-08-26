// Copyright 2010-2021, Google Inc.
// All rights reserved.

#include "renderer/win32/symbols_palette_window.h"

#include <commctrl.h>
#include <uxtheme.h>
#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "base/file_util.h"
#include "base/system_util.h"
#include "base/win32/wide_char.h"
#include "base/win32/win_util.h"
#include "protocol/commands.pb.h"
#include "protocol/renderer_command.pb.h"
#include "renderer/win32/marina_localized_string.h"
#include "renderer/win32/win32_dpi_util.h"

namespace mozc {
namespace renderer {
namespace win32 {

namespace {

constexpr int kNumTabs = 4;
constexpr int kColumns = 8;

// Logical (96 DPI) metrics. Every use scales these by the window's current
// DPI -- unlike the toolbar this window is made of ordinary child controls,
// which Windows does not scale for us in a PerMonitorV2 process.
constexpr int kButtonWidthLogical = 40;
constexpr int kButtonHeightLogical = 32;
constexpr int kGapLogical = 4;
constexpr int kMarginLogical = 8;
constexpr int kTabStripHeightLogical = 28;
constexpr int kPinCheckboxHeightLogical = 24;
constexpr int kHintHeightLogical = 32;
// Symbol glyphs (々 ゝ ㆐ …) need a larger face than the shell UI default to
// be legible and unambiguous at a glance; mac uses an 18pt system font.
constexpr int kSymbolFontHeightLogical = 20;
// Rows visible before the symbol area starts scrolling. Four rows fits the
// stock Odoriji/Kaeriten/Symbols lists; a long user list scrolls.
constexpr int kMaxVisibleRows = 4;

// marinaMoji: mirrors src/session/odoriji_palette.cc's kOdorijiChars and
// mac's BuildDefaultOdorijiSymbols()/BuildDefaultGeneralSymbols(). Kept as a
// Windows-local copy for scope containment; sharing one source across
// platforms is a nice-to-have follow-up.
const wchar_t* const kOdorijiSymbols[] = {
    L"\x3005", L"\x309D", L"\x309E", L"\x30FD",
    L"\x30FE", L"\x303B", L"\x3031", L"\x3032",
};

const wchar_t* const kGeneralSymbols[] = {
    L"\x3014", L"\x3015", L"\xFF3B", L"\xFF3D", L"\x3010", L"\x3011",
    L"\x3008", L"\x3009", L"\x300A", L"\x300B", L"\xFF08", L"\xFF09",
    L"\xFF5B", L"\xFF5D", L"\x25A1", L"\x25A0", L"\x25CB", L"\x25B3",
    L"\xFF0D", L"\x203B", L"\x3013", L"\x25C6", L"\x25C7", L"\x25CE",
    L"\x25B2", L"\x25BD", L"\x2026", L"\x2014", L"\x30F6",
};

std::vector<std::wstring> BuildOdorijiSymbols() {
  return std::vector<std::wstring>(std::begin(kOdorijiSymbols),
                                   std::end(kOdorijiSymbols));
}

std::vector<std::wstring> BuildGeneralSymbols() {
  return std::vector<std::wstring>(std::begin(kGeneralSymbols),
                                   std::end(kGeneralSymbols));
}

constexpr int kTabControlId = 1001;
constexpr int kPinCheckboxId = 1002;
constexpr int kHintLabelId = 1003;
constexpr int kButtonIdBase = 2000;
constexpr int kButtonsPerTab = 1000;  // must exceed any tab's symbol count

const wchar_t* HintKeyForTab(int tab) {
  switch (tab) {
    case 0:
      return L"MM.OdorijiHint";
    case 1:
      return L"MM.KaeritenHint";
    case 3:
      return L"MM.UserSymbolsHint";
    default:
      return nullptr;  // the general Symbols tab needs no explanation
  }
}

// AdjustWindowRectExForDpi is Windows 10 1607+, but the renderer manifest
// still claims support back to Vista, so resolve it dynamically and fall back
// to the system-DPI variant rather than taking a hard import that would stop
// the process loading on an older OS.
BOOL AdjustWindowRectForDpi(RECT* rect, DWORD style, DWORD ex_style,
                            uint32_t dpi) {
  using AdjustForDpiFunc = BOOL(WINAPI*)(LPRECT, DWORD, BOOL, DWORD, UINT);
  static const AdjustForDpiFunc adjust_for_dpi = []() -> AdjustForDpiFunc {
    const HMODULE user32 = ::GetModuleHandleW(L"user32.dll");
    if (user32 == nullptr) {
      return nullptr;
    }
    return reinterpret_cast<AdjustForDpiFunc>(
        ::GetProcAddress(user32, "AdjustWindowRectExForDpi"));
  }();
  if (adjust_for_dpi != nullptr) {
    return adjust_for_dpi(rect, style, FALSE, ex_style, dpi);
  }
  return ::AdjustWindowRectEx(rect, style, FALSE, ex_style);
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

// marinaMoji: SPI_GETWORKAREA reports the *primary* monitor's work area, which
// is not necessarily the monitor the user is typing on -- centring on it drops
// the palette onto another screen entirely. Anchor to the monitor holding the
// application being typed into, falling back to the cursor's monitor and only
// then to the primary. Mirrors shortcuts_window.cc's copy of this, and the
// "keep it where the user is looking" rule the toolbar already follows.
RECT CurrentMonitorWorkArea(const commands::RendererCommand& command) {
  HMONITOR monitor = nullptr;
  if (command.has_application_info() &&
      command.application_info().has_target_window_handle()) {
    const HWND target = ::mozc::WinUtil::DecodeWindowHandle(
        command.application_info().target_window_handle());
    if (target != nullptr) {
      monitor = ::MonitorFromWindow(target, MONITOR_DEFAULTTONULL);
    }
  }
  if (monitor == nullptr) {
    POINT cursor = {0, 0};
    if (::GetCursorPos(&cursor)) {
      monitor = ::MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    }
  }
  MONITORINFO info = {};
  info.cbSize = sizeof(info);
  if (monitor != nullptr && ::GetMonitorInfoW(monitor, &info)) {
    return info.rcWork;
  }
  RECT work_area = {0, 0, 1920, 1080};
  ::SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);
  return work_area;
}

}  // namespace

SymbolsPaletteWindow::SymbolsPaletteWindow()
    : send_command_interface_(nullptr),
      tab_control_(nullptr),
      pin_checkbox_(nullptr),
      hint_label_(nullptr),
      active_tab_(0),
      pinned_(false),
      has_shown_once_(false),
      initial_work_area_{0, 0, 0, 0},
      dpi_(USER_DEFAULT_SCREEN_DPI),
      is_dark_theme_(false),
      scroll_offset_(0),
      ui_font_(nullptr),
      symbol_font_(nullptr),
      background_brush_(nullptr) {}

SymbolsPaletteWindow::~SymbolsPaletteWindow() {}

void SymbolsPaletteWindow::Initialize() {
  INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_TAB_CLASSES | ICC_STANDARD_CLASSES};
  ::InitCommonControlsEx(&icc);

  // Must run before Create() so OnCreate()'s CreateTabControl() picks up the
  // persisted |active_tab_| when it calls TabCtrl_SetCurSel.
  LoadPreferences();

  Create(nullptr);

  RebuildButtonsForTab(Tab::kOdoriji, BuildOdorijiSymbols());
  RebuildButtonsForTab(Tab::kSymbols, BuildGeneralSymbols());
  Relayout();

  ShowWindow(SW_HIDE);
}

void SymbolsPaletteWindow::Destroy() {
  if (IsWindow()) {
    DestroyWindow();
  }
}

void SymbolsPaletteWindow::SetSendCommandInterface(
    client::SendCommandInterface* send_command_interface) {
  send_command_interface_ = send_command_interface;
}

void SymbolsPaletteWindow::Hide() { ShowWindow(SW_HIDE); }

int SymbolsPaletteWindow::Scaled(int logical_value) const {
  return std::max(
      1, static_cast<int>(std::lround(logical_value *
                                      GetDPIScalingFactor(dpi_))));
}

LRESULT SymbolsPaletteWindow::OnCreate(LPCREATESTRUCT create_struct) {
  // The window has no meaningful on-screen rect yet (Create(nullptr)); this
  // is the primary monitor's DPI, corrected by WM_DPICHANGED once Relayout()
  // has actually placed the window.
  dpi_ = GetDpiForPoint(0, 0);
  is_dark_theme_ = IsDarkTheme();

  SetWindowTextW(MarinaLocalizedString(L"MM.SymbolsPalette"));
  CreateFonts();
  CreateBackgroundBrush();
  CreateTabControl();

  pin_checkbox_ = ::CreateWindowExW(
      0, L"BUTTON", MarinaLocalizedString(L"MM.PinPalette"),
      WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0, 0, 0, 0, m_hWnd,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPinCheckboxId)), nullptr,
      nullptr);
  if (pin_checkbox_ != nullptr) {
    Button_SetCheck(pin_checkbox_, pinned_ ? BST_CHECKED : BST_UNCHECKED);
    ApplyFont(pin_checkbox_, ui_font_);
    ApplyDarkModeTheme(pin_checkbox_);
  }

  hint_label_ = ::CreateWindowExW(
      0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_EDITCONTROL,
      0, 0, 0, 0, m_hWnd,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kHintLabelId)), nullptr,
      nullptr);
  ApplyFont(hint_label_, ui_font_);
  ApplyDarkModeTheme(hint_label_);

  return 0;
}

void SymbolsPaletteWindow::OnDestroy() {
  DeleteFonts();
  if (background_brush_ != nullptr) {
    ::DeleteObject(background_brush_);
    background_brush_ = nullptr;
  }
}

void SymbolsPaletteWindow::CreateFonts() {
  DeleteFonts();

  // NONCLIENTMETRICS gives the shell UI font (Segoe UI / Yu Gothic UI /
  // Meiryo UI depending on the system language) at the *system* DPI; scale it
  // to this window's DPI. Without an explicit WM_SETFONT every child control
  // would silently fall back to the legacy bitmap SYSTEM_FONT, which renders
  // CJK poorly or not at all.
  NONCLIENTMETRICSW metrics = {};
  metrics.cbSize = sizeof(metrics);
  if (::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics),
                              &metrics, 0)) {
    LOGFONTW ui_log_font = metrics.lfMessageFont;
    const double system_scale =
        GetDPIScalingFactor(GetDpiForPoint(0, 0));
    const double window_scale = GetDPIScalingFactor(dpi_);
    if (system_scale > 0.0) {
      ui_log_font.lfHeight = static_cast<LONG>(
          std::lround(ui_log_font.lfHeight * window_scale / system_scale));
    }
    ui_font_ = ::CreateFontIndirectW(&ui_log_font);

    LOGFONTW symbol_log_font = ui_log_font;
    symbol_log_font.lfHeight = -Scaled(kSymbolFontHeightLogical);
    symbol_log_font.lfWidth = 0;
    symbol_font_ = ::CreateFontIndirectW(&symbol_log_font);
  }
  if (ui_font_ == nullptr) {
    ui_font_ = static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
    owns_ui_font_ = false;
  } else {
    owns_ui_font_ = true;
  }
  if (symbol_font_ == nullptr) {
    symbol_font_ = ui_font_;
    owns_symbol_font_ = false;
  } else {
    owns_symbol_font_ = true;
  }
}

void SymbolsPaletteWindow::DeleteFonts() {
  if (owns_symbol_font_ && symbol_font_ != nullptr) {
    ::DeleteObject(symbol_font_);
  }
  symbol_font_ = nullptr;
  owns_symbol_font_ = false;
  if (owns_ui_font_ && ui_font_ != nullptr) {
    ::DeleteObject(ui_font_);
  }
  ui_font_ = nullptr;
  owns_ui_font_ = false;
}

void SymbolsPaletteWindow::CreateBackgroundBrush() {
  if (background_brush_ != nullptr) {
    ::DeleteObject(background_brush_);
  }
  // The window class brush is COLOR_WINDOW, which stays white in dark mode --
  // common controls have no automatic dark theme. Paint the background (and
  // the static/checkbox text below, via WM_CTLCOLOR*) ourselves so the
  // palette matches the toolbar that opened it.
  background_brush_ = ::CreateSolidBrush(BackgroundColor());
}

COLORREF SymbolsPaletteWindow::BackgroundColor() const {
  return is_dark_theme_ ? RGB(32, 35, 40) : ::GetSysColor(COLOR_WINDOW);
}

COLORREF SymbolsPaletteWindow::TextColor() const {
  return is_dark_theme_ ? RGB(235, 235, 235) : ::GetSysColor(COLOR_WINDOWTEXT);
}

void SymbolsPaletteWindow::ApplyFont(HWND control, HFONT font) {
  if (control != nullptr && font != nullptr) {
    ::SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font),
                   MAKELPARAM(TRUE, 0));
  }
}

void SymbolsPaletteWindow::ApplyDarkModeTheme(HWND control) {
  if (control == nullptr) {
    return;
  }
  // Themed common controls draw their own light background regardless of
  // WM_CTLCOLOR*. Opting them into the shell's dark theme class is what
  // Explorer itself does; on Windows versions that don't know the class this
  // simply fails and the control keeps its light appearance, which is why no
  // version check is needed.
  ::SetWindowTheme(control, is_dark_theme_ ? L"DarkMode_Explorer" : nullptr,
                   nullptr);
}

void SymbolsPaletteWindow::CreateTabControl() {
  tab_control_ = ::CreateWindowExW(
      0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, m_hWnd,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTabControlId)), nullptr,
      nullptr);
  if (tab_control_ == nullptr) {
    return;
  }
  ApplyFont(tab_control_, ui_font_);
  ApplyDarkModeTheme(tab_control_);

  const wchar_t* const label_keys[] = {L"MM.Odoriji", L"MM.Kaeriten",
                                       L"MM.Symbols", L"MM.User"};
  for (int i = 0; i < kNumTabs; ++i) {
    TCITEMW item = {};
    item.mask = TCIF_TEXT;
    item.pszText = const_cast<wchar_t*>(MarinaLocalizedString(label_keys[i]));
    TabCtrl_InsertItem(tab_control_, i, &item);
  }
  TabCtrl_SetCurSel(tab_control_, active_tab_);
}

int SymbolsPaletteWindow::RowCountForTab(int tab) const {
  const size_t count = tab_symbols_[static_cast<size_t>(tab)].size();
  if (count == 0) {
    return 0;
  }
  return static_cast<int>((count + kColumns - 1) / kColumns);
}

int SymbolsPaletteWindow::SymbolAreaTop() const {
  int top = Scaled(kTabStripHeightLogical) + Scaled(kMarginLogical);
  if (HintKeyForTab(active_tab_) != nullptr) {
    top += Scaled(kHintHeightLogical);
  }
  return top;
}

CSize SymbolsPaletteWindow::ComputeClientSize() const {
  int width = Scaled(kMarginLogical) * 2 +
              kColumns * Scaled(kButtonWidthLogical) +
              (kColumns - 1) * Scaled(kGapLogical);
  if (MaxScrollOffset() > 0) {
    // A visible scrollbar eats client width; widen so the last column keeps
    // its margin rather than being clipped.
    width += ::GetSystemMetrics(SM_CXVSCROLL);
  }
  const int visible_rows =
      std::clamp(RowCountForTab(active_tab_), 1, kMaxVisibleRows);
  const int height = SymbolAreaTop() +
                     visible_rows * (Scaled(kButtonHeightLogical) +
                                     Scaled(kGapLogical)) +
                     Scaled(kMarginLogical) +
                     Scaled(kPinCheckboxHeightLogical) +
                     Scaled(kMarginLogical);
  return CSize(width, height);
}

void SymbolsPaletteWindow::Relayout() {
  const CSize client_size = ComputeClientSize();

  // SetWindowPos takes the *window* size, not the client size: without this
  // the caption and borders eat into the layout and clip the bottom row of
  // symbols. AdjustWindowRectEx converts one to the other for our styles.
  RECT desired = {0, 0, client_size.cx, client_size.cy};
  const DWORD style = static_cast<DWORD>(::GetWindowLongW(m_hWnd, GWL_STYLE));
  const DWORD ex_style =
      static_cast<DWORD>(::GetWindowLongW(m_hWnd, GWL_EXSTYLE));
  AdjustWindowRectForDpi(&desired, style, ex_style, dpi_);
  const int window_width = desired.right - desired.left;
  const int window_height = desired.bottom - desired.top;

  const int max_scroll = MaxScrollOffset();
  scroll_offset_ = std::clamp(scroll_offset_, 0, max_scroll);

  const int margin = Scaled(kMarginLogical);
  const int tab_height = Scaled(kTabStripHeightLogical);
  if (tab_control_ != nullptr) {
    ::SetWindowPos(tab_control_, nullptr, 0, 0, client_size.cx, tab_height,
                   SWP_NOZORDER | SWP_NOACTIVATE);
  }

  if (hint_label_ != nullptr) {
    const wchar_t* hint_key = HintKeyForTab(active_tab_);
    if (hint_key != nullptr) {
      ::SetWindowTextW(hint_label_, MarinaLocalizedString(hint_key));
      ::SetWindowPos(hint_label_, nullptr, margin, tab_height + margin,
                     client_size.cx - margin * 2, Scaled(kHintHeightLogical),
                     SWP_NOZORDER | SWP_NOACTIVATE);
      ::ShowWindow(hint_label_, SW_SHOW);
    } else {
      ::ShowWindow(hint_label_, SW_HIDE);
    }
  }

  if (pin_checkbox_ != nullptr) {
    ::SetWindowPos(pin_checkbox_, nullptr, margin,
                   client_size.cy - Scaled(kPinCheckboxHeightLogical) - margin,
                   client_size.cx - margin * 2,
                   Scaled(kPinCheckboxHeightLogical),
                   SWP_NOZORDER | SWP_NOACTIVATE);
  }

  LayoutButtons();
  UpdateScrollBar();

  const UINT flags = SWP_NOZORDER | SWP_NOACTIVATE |
                     (has_shown_once_ ? SWP_NOMOVE : 0);
  int x = 0;
  int y = 0;
  if (!has_shown_once_) {
    // marinaMoji: mirrors mac's [window center] -- the palette isn't anchored
    // to the toolbar's position, unlike the toolbar's own bottom-right
    // default. Centred on the monitor the user is typing on, not the primary
    // one (see CurrentMonitorWorkArea).
    RECT work_area = initial_work_area_;
    if (work_area.right <= work_area.left ||
        work_area.bottom <= work_area.top) {
      // Relayout is also reached from tab/scroll handling, which can run
      // before any OnUpdate has captured a monitor. Fall back rather than
      // centring inside an empty rect at negative coordinates.
      ::SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);
    }
    x = work_area.left + (work_area.right - work_area.left - window_width) / 2;
    y = work_area.top + (work_area.bottom - work_area.top - window_height) / 2;
  }
  SetWindowPos(HWND_TOPMOST, x, y, window_width, window_height, flags);
  Invalidate(TRUE);
}

void SymbolsPaletteWindow::LayoutButtons() {
  const int button_w = Scaled(kButtonWidthLogical);
  const int button_h = Scaled(kButtonHeightLogical);
  const int gap = Scaled(kGapLogical);
  const int margin = Scaled(kMarginLogical);
  const int area_top = SymbolAreaTop();
  const int area_bottom = area_top + std::clamp(RowCountForTab(active_tab_), 1,
                                                kMaxVisibleRows) *
                                         (button_h + gap);

  for (int tab = 0; tab < kNumTabs; ++tab) {
    const auto& buttons = tab_buttons_[static_cast<size_t>(tab)];
    const bool tab_visible = (tab == active_tab_);
    for (size_t i = 0; i < buttons.size(); ++i) {
      HWND button = buttons[i];
      if (button == nullptr) {
        continue;
      }
      if (!tab_visible) {
        ::ShowWindow(button, SW_HIDE);
        continue;
      }
      const int row = static_cast<int>(i) / kColumns;
      const int col = static_cast<int>(i) % kColumns;
      const int x = margin + col * (button_w + gap);
      const int y = area_top + row * (button_h + gap) - scroll_offset_;
      ::SetWindowPos(button, nullptr, x, y, button_w, button_h,
                     SWP_NOZORDER | SWP_NOACTIVATE);
      // Rows scrolled out of the symbol area are hidden rather than clipped:
      // these are plain child controls with no clipping parent of their own.
      const bool row_visible = (y + button_h > area_top) && (y < area_bottom);
      ::ShowWindow(button, row_visible ? SW_SHOW : SW_HIDE);
    }
  }
}

int SymbolsPaletteWindow::MaxScrollOffset() const {
  const int rows = RowCountForTab(active_tab_);
  if (rows <= kMaxVisibleRows) {
    return 0;
  }
  return (rows - kMaxVisibleRows) *
         (Scaled(kButtonHeightLogical) + Scaled(kGapLogical));
}

void SymbolsPaletteWindow::UpdateScrollBar() {
  const int rows = RowCountForTab(active_tab_);
  const int row_height = Scaled(kButtonHeightLogical) + Scaled(kGapLogical);
  SCROLLINFO info = {};
  info.cbSize = sizeof(info);
  info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
  info.nMin = 0;
  info.nMax = std::max(0, rows * row_height - 1);
  info.nPage = static_cast<UINT>(kMaxVisibleRows * row_height);
  info.nPos = scroll_offset_;
  SetScrollInfo(SB_VERT, &info, TRUE);
  ShowScrollBar(SB_VERT, MaxScrollOffset() > 0);
}

void SymbolsPaletteWindow::ScrollTo(int offset) {
  const int clamped = std::clamp(offset, 0, MaxScrollOffset());
  if (clamped == scroll_offset_) {
    return;
  }
  scroll_offset_ = clamped;
  LayoutButtons();
  SetScrollPos(SB_VERT, scroll_offset_, TRUE);
  Invalidate(TRUE);
}

LRESULT SymbolsPaletteWindow::OnVScroll(UINT msg_id, WPARAM wparam,
                                        LPARAM lparam, BOOL& handled) {
  const int row_height = Scaled(kButtonHeightLogical) + Scaled(kGapLogical);
  switch (LOWORD(wparam)) {
    case SB_LINEUP:
      ScrollTo(scroll_offset_ - row_height);
      break;
    case SB_LINEDOWN:
      ScrollTo(scroll_offset_ + row_height);
      break;
    case SB_PAGEUP:
      ScrollTo(scroll_offset_ - row_height * kMaxVisibleRows);
      break;
    case SB_PAGEDOWN:
      ScrollTo(scroll_offset_ + row_height * kMaxVisibleRows);
      break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION: {
      SCROLLINFO info = {};
      info.cbSize = sizeof(info);
      info.fMask = SIF_TRACKPOS;
      if (GetScrollInfo(SB_VERT, &info)) {
        ScrollTo(info.nTrackPos);
      }
      break;
    }
    default:
      break;
  }
  handled = TRUE;
  return 0;
}

LRESULT SymbolsPaletteWindow::OnMouseWheel(UINT msg_id, WPARAM wparam,
                                           LPARAM lparam, BOOL& handled) {
  const int delta = GET_WHEEL_DELTA_WPARAM(wparam);
  const int row_height = Scaled(kButtonHeightLogical) + Scaled(kGapLogical);
  ScrollTo(scroll_offset_ - delta * row_height / WHEEL_DELTA);
  handled = TRUE;
  return 0;
}

LRESULT SymbolsPaletteWindow::OnEraseBackground(UINT msg_id, WPARAM wparam,
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

LRESULT SymbolsPaletteWindow::OnCtlColor(UINT msg_id, WPARAM wparam,
                                         LPARAM lparam, BOOL& handled) {
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

LRESULT SymbolsPaletteWindow::OnDpiChanged(UINT msg_id, WPARAM wparam,
                                           LPARAM lparam, BOOL& handled) {
  const uint32_t new_dpi = static_cast<uint32_t>(LOWORD(wparam));
  if (new_dpi != 0 && new_dpi != dpi_) {
    dpi_ = new_dpi;
    CreateFonts();
    ApplyFont(tab_control_, ui_font_);
    ApplyFont(pin_checkbox_, ui_font_);
    ApplyFont(hint_label_, ui_font_);
    for (const auto& buttons : tab_buttons_) {
      for (HWND button : buttons) {
        ApplyFont(button, symbol_font_);
      }
    }
    Relayout();
  }
  handled = TRUE;
  return 0;
}

LRESULT SymbolsPaletteWindow::OnSettingChange(UINT msg_id, WPARAM wparam,
                                              LPARAM lparam, BOOL& handled) {
  const bool dark = IsDarkTheme();
  if (dark != is_dark_theme_) {
    is_dark_theme_ = dark;
    CreateBackgroundBrush();
    ApplyDarkModeTheme(tab_control_);
    ApplyDarkModeTheme(pin_checkbox_);
    ApplyDarkModeTheme(hint_label_);
    for (const auto& buttons : tab_buttons_) {
      for (HWND button : buttons) {
        ApplyDarkModeTheme(button);
      }
    }
    Invalidate(TRUE);
  }
  handled = FALSE;
  return 0;
}

void SymbolsPaletteWindow::RebuildButtonsForTab(
    Tab tab, const std::vector<std::wstring>& symbols) {
  const size_t idx = static_cast<size_t>(tab);
  if (tab_symbols_[idx] == symbols && !tab_buttons_[idx].empty()) {
    return;  // unchanged; don't churn HWNDs on every update
  }
  for (HWND button : tab_buttons_[idx]) {
    if (button != nullptr) {
      ::DestroyWindow(button);
    }
  }
  tab_buttons_[idx].clear();
  tab_symbols_[idx] = symbols;

  for (size_t i = 0; i < symbols.size(); ++i) {
    const int control_id =
        kButtonIdBase + static_cast<int>(idx) * kButtonsPerTab +
        static_cast<int>(i);
    HWND button = ::CreateWindowExW(
        0, L"BUTTON", symbols[i].c_str(), WS_CHILD, 0, 0, 0, 0, m_hWnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(control_id)), nullptr,
        nullptr);
    ApplyFont(button, symbol_font_);
    ApplyDarkModeTheme(button);
    tab_buttons_[idx].push_back(button);
  }
}

LRESULT SymbolsPaletteWindow::OnNotify(UINT msg_id, WPARAM wparam,
                                       LPARAM lparam, BOOL& handled) {
  const auto* header = reinterpret_cast<const NMHDR*>(lparam);
  if (header != nullptr && header->hwndFrom == tab_control_ &&
      header->code == TCN_SELCHANGE) {
    active_tab_ = TabCtrl_GetCurSel(tab_control_);
    scroll_offset_ = 0;
    // Tabs differ in row count (and whether they carry a hint), so the whole
    // window is resized, not just the button rows.
    Relayout();
    SavePreferences();
    handled = TRUE;
    return 0;
  }
  handled = FALSE;
  return 0;
}

LRESULT SymbolsPaletteWindow::OnCommand(UINT msg_id, WPARAM wparam,
                                        LPARAM lparam, BOOL& handled) {
  const int control_id = LOWORD(wparam);
  const int notification = HIWORD(wparam);
  handled = TRUE;

  if (control_id == kPinCheckboxId && notification == BN_CLICKED) {
    OnPinToggled();
    return 0;
  }

  if (control_id >= kButtonIdBase && notification == BN_CLICKED) {
    const int offset = control_id - kButtonIdBase;
    const int tab = offset / kButtonsPerTab;
    const int index = offset % kButtonsPerTab;
    if (tab >= 0 && tab < kNumTabs) {
      OnSymbolClicked(static_cast<Tab>(tab), index);
    }
    return 0;
  }

  handled = FALSE;
  return 0;
}

void SymbolsPaletteWindow::OnSymbolClicked(Tab tab, int index) {
  const auto& symbols = tab_symbols_[static_cast<size_t>(tab)];
  if (index < 0 || static_cast<size_t>(index) >= symbols.size()) {
    return;
  }

  if (tab == Tab::kOdoriji) {
    SendShowOdorijiPaletteAndSubmit(index);
  } else {
    SendInsertSymbolText(symbols[index]);
  }

  if (!pinned_) {
    SendHidePaletteSignal();
    Hide();
  }
}

void SymbolsPaletteWindow::OnPinToggled() {
  pinned_ = (Button_GetCheck(pin_checkbox_) == BST_CHECKED);
  SavePreferences();
}

LRESULT SymbolsPaletteWindow::OnCloseMessage(UINT msg_id, WPARAM wparam,
                                             LPARAM lparam, BOOL& handled) {
  handled = TRUE;
  SendHidePaletteSignal();
  Hide();
  return 0;
}

void SymbolsPaletteWindow::SendShowOdorijiPaletteAndSubmit(int index) {
  if (send_command_interface_ == nullptr) {
    return;
  }
  commands::SessionCommand show;
  show.set_type(commands::SessionCommand::SHOW_ODORIJI_PALETTE);
  commands::Output show_output;
  send_command_interface_->SendCommand(show, &show_output);

  commands::SessionCommand submit;
  submit.set_type(commands::SessionCommand::SUBMIT_CANDIDATE);
  submit.set_id(index);
  commands::Output submit_output;
  send_command_interface_->SendCommand(submit, &submit_output);
}

void SymbolsPaletteWindow::SendInsertSymbolText(const std::wstring& symbol) {
  if (send_command_interface_ == nullptr) {
    return;
  }
  commands::SessionCommand command;
  command.set_type(commands::SessionCommand::INSERT_SYMBOL_TEXT);
  command.set_text(mozc::win32::WideToUtf8(symbol));
  commands::Output output;
  send_command_interface_->SendCommand(command, &output);
}

void SymbolsPaletteWindow::SendHidePaletteSignal() {
  if (send_command_interface_ == nullptr) {
    return;
  }
  commands::SessionCommand command;
  command.set_type(commands::SessionCommand::HIDE_SYMBOLS_PALETTE);
  commands::Output output;
  send_command_interface_->SendCommand(command, &output);
}

void SymbolsPaletteWindow::OnUpdate(const commands::RendererCommand& command) {
  if (!command.has_application_info() ||
      !command.application_info().has_symbols_palette_info()) {
    // TIP has closed the palette (e.g. focus loss) -- follow suit.
    if (IsWindowVisible()) {
      Hide();
    }
    return;
  }

  const auto& info = command.application_info().symbols_palette_info();

  if (!has_shown_once_) {
    initial_work_area_ = CurrentMonitorWorkArea(command);
  }

  std::vector<std::wstring> kaeriten;
  for (const std::string& s : info.kaeriten_symbols()) {
    kaeriten.push_back(mozc::win32::Utf8ToWide(s));
  }
  RebuildButtonsForTab(Tab::kKaeriten, kaeriten);

  std::vector<std::wstring> user;
  for (const std::string& s : info.user_symbols()) {
    user.push_back(mozc::win32::Utf8ToWide(s));
  }
  RebuildButtonsForTab(Tab::kUser, user);

  Relayout();
  has_shown_once_ = true;

  if (!IsWindowVisible()) {
    ShowWindow(SW_SHOWNA);
  }
}

std::string SymbolsPaletteWindow::ConfigFilePath() const {
  return FileUtil::JoinPath(
      {SystemUtil::GetUserProfileDirectory(), "symbols_palette.conf"});
}

void SymbolsPaletteWindow::LoadPreferences() {
  pinned_ = false;
  active_tab_ = 0;
  const auto contents = FileUtil::GetContents(ConfigFilePath());
  if (!contents.ok()) {
    return;
  }
  const std::string& text = *contents;
  size_t pos = 0;
  while (pos < text.size()) {
    const size_t eol = text.find('\n', pos);
    const std::string line = text.substr(
        pos, eol == std::string::npos ? std::string::npos : eol - pos);
    const size_t eq = line.find('=');
    if (eq != std::string::npos) {
      const std::string key = line.substr(0, eq);
      const std::string value = line.substr(eq + 1);
      if (key == "pinned") {
        pinned_ = (value == "1");
      } else if (key == "last_tab") {
        const int tab = std::atoi(value.c_str());
        if (tab >= 0 && tab < kNumTabs) {
          active_tab_ = tab;
        }
      }
    }
    if (eol == std::string::npos) {
      break;
    }
    pos = eol + 1;
  }
}

void SymbolsPaletteWindow::SavePreferences() const {
  const std::string content = absl::StrCat(
      "pinned=", pinned_ ? "1" : "0", "\nlast_tab=", active_tab_, "\n");
  const std::string path = ConfigFilePath();
  (void)FileUtil::CreateDirectory(FileUtil::Dirname(path));
  (void)FileUtil::SetContents(path, content);
}

}  // namespace win32
}  // namespace renderer
}  // namespace mozc
