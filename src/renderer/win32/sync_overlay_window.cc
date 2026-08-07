// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0

#include "renderer/win32/sync_overlay_window.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <string>

#include "base/win32/wide_char.h"
#include "renderer/win32/win32_dpi_util.h"
#include "renderer/win32/win32_renderer_util.h"
#include "sync/sync_status.h"

namespace mozc {
namespace renderer {
namespace win32 {

namespace {

// Same 250 ms cadence as the mac and Linux watchers.
constexpr UINT kPollIntervalMsec = 250;

// Logical (96 DPI) metrics.
constexpr int kPaddingXLogical = 24;
constexpr int kPaddingYLogical = 16;
constexpr int kFontHeightLogical = 16;
constexpr int kCornerRadiusLogical = 8;
constexpr int kMaxWidthLogical = 520;

// Uniform window alpha. mac and Linux paint an opaque label over a 55%-alpha
// backdrop, which needs true per-pixel alpha; doing that here would mean
// compositing text into a premultiplied DIB, and GDI text drawing does not
// maintain the alpha channel. A single LWA_ALPHA value over the whole window
// is the robust equivalent -- slightly more opaque than 55% so the text stays
// legible at the same translucency.
constexpr BYTE kWindowAlpha = 220;

constexpr COLORREF kBackgroundColor = RGB(0, 0, 0);
constexpr COLORREF kTextColor = RGB(255, 255, 255);

// Shown when the status file carries no message of its own; matches the
// wording used by the mac and Linux overlays.
constexpr wchar_t kDefaultMessage[] = L"marinaMoji synchronising…";

}  // namespace

SyncOverlayWindow::SyncOverlayWindow()
    : dpi_(USER_DEFAULT_SCREEN_DPI), font_(nullptr), active_(false) {}

SyncOverlayWindow::~SyncOverlayWindow() {}

void SyncOverlayWindow::Initialize() {
  Create(nullptr);
  ShowWindow(SW_HIDE);
}

void SyncOverlayWindow::Destroy() {
  if (IsWindow()) {
    DestroyWindow();
  }
}

int SyncOverlayWindow::Scaled(int logical_value) const {
  return std::max(
      1, static_cast<int>(std::lround(logical_value *
                                      GetDPIScalingFactor(dpi_))));
}

LRESULT SyncOverlayWindow::OnCreate(LPCREATESTRUCT create_struct) {
  const POINT target = TargetScreenPoint();
  dpi_ = GetDpiForPoint(target.x, target.y);
  CreateFont();
  ::SetLayeredWindowAttributes(m_hWnd, 0, kWindowAlpha, LWA_ALPHA);
  SetTimer(kPollTimerId, kPollIntervalMsec, nullptr);
  return 0;
}

POINT SyncOverlayWindow::TargetScreenPoint() const {
  const HWND foreground = ::GetForegroundWindow();
  RECT rect = {};
  if (foreground != nullptr && ::GetWindowRect(foreground, &rect)) {
    return POINT{(rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2};
  }
  return POINT{0, 0};
}

void SyncOverlayWindow::OnDestroy() {
  KillTimer(kPollTimerId);
  DeleteFont();
}

void SyncOverlayWindow::CreateFont() {
  DeleteFont();
  NONCLIENTMETRICSW metrics = {};
  metrics.cbSize = sizeof(metrics);
  if (::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics),
                              &metrics, 0)) {
    LOGFONTW log_font = metrics.lfMessageFont;
    log_font.lfHeight = -Scaled(kFontHeightLogical);
    log_font.lfWidth = 0;
    log_font.lfWeight = FW_BOLD;  // matches the bold label on mac/Linux
    font_ = ::CreateFontIndirectW(&log_font);
  }
  if (font_ == nullptr) {
    font_ = static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
    owns_font_ = false;
  } else {
    owns_font_ = true;
  }
}

void SyncOverlayWindow::DeleteFont() {
  if (owns_font_ && font_ != nullptr) {
    ::DeleteObject(font_);
  }
  font_ = nullptr;
  owns_font_ = false;
}

void SyncOverlayWindow::PollStatus() {
  const auto status_or = mozc::sync::ReadSyncStatus();
  const bool running = status_or.ok() && status_or->state == "running";

  std::wstring message = kDefaultMessage;
  if (running && !status_or->message.empty()) {
    message = mozc::win32::Utf8ToWide(status_or->message);
  }

  if (running == active_ && message == message_) {
    return;  // nothing changed; don't repaint or re-place the window
  }
  active_ = running;
  message_ = message;

  if (!active_) {
    ShowWindow(SW_HIDE);
    return;
  }
  UpdateLayout();
  if (!IsWindowVisible()) {
    // SW_SHOWNA: the application whose keys are being held must keep focus.
    ShowWindow(SW_SHOWNA);
  }
}

void SyncOverlayWindow::UpdateLayout() {
  // Re-target to whatever monitor the foreground application is on, since
  // that can change while the overlay is up (or between one sync and the
  // next) -- Alt+Tab during a sync, or a status poll landing on a freshly
  // shown overlay after the user switched monitors. Refresh DPI/font first
  // so the Scaled() calls below and the text measurement use the right
  // monitor's scale factor, not a stale one.
  const POINT target = TargetScreenPoint();
  const uint32_t new_dpi = GetDpiForPoint(target.x, target.y);
  if (new_dpi != dpi_) {
    dpi_ = new_dpi;
    CreateFont();
  }

  const int padding_x = Scaled(kPaddingXLogical);
  const int padding_y = Scaled(kPaddingYLogical);

  // Measure the text so the box fits it, wrapping at a sensible maximum.
  int width = Scaled(200);
  int height = Scaled(24);
  if (HDC dc = ::GetDC(m_hWnd); dc != nullptr) {
    HGDIOBJ old_font = ::SelectObject(dc, font_);
    RECT text_rect = {0, 0, Scaled(kMaxWidthLogical), 0};
    ::DrawTextW(dc, message_.c_str(), -1, &text_rect,
                DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    width = text_rect.right - text_rect.left;
    height = text_rect.bottom - text_rect.top;
    ::SelectObject(dc, old_font);
    ::ReleaseDC(m_hWnd, dc);
  }
  width += padding_x * 2;
  height += padding_y * 2;

  RECT work_area = {0, 0, 1920, 1080};
  GetWorkingAreaFromPoint(target, &work_area);
  const int x =
      work_area.left + (work_area.right - work_area.left - width) / 2;
  const int y =
      work_area.top + (work_area.bottom - work_area.top - height) / 2;

  SetWindowPos(HWND_TOPMOST, x, y, width, height,
               SWP_NOACTIVATE | SWP_NOREDRAW);

  // Rounded corners. A region is enough here (unlike the toolbar, which needs
  // per-pixel alpha for antialiased corners) because the whole window is
  // uniformly translucent anyway; the region edge is hidden by the very dark
  // fill against arbitrary content.
  const int radius = Scaled(kCornerRadiusLogical);
  if (HRGN region = ::CreateRoundRectRgn(0, 0, width + 1, height + 1,
                                         radius * 2, radius * 2);
      region != nullptr) {
    // The window takes ownership of the region; do not delete it here.
    SetWindowRgn(region, FALSE);
  }
  Invalidate(TRUE);
}

LRESULT SyncOverlayWindow::OnTimer(UINT msg_id, WPARAM wparam, LPARAM lparam,
                                   BOOL& handled) {
  if (wparam == kPollTimerId) {
    PollStatus();
    handled = TRUE;
    return 0;
  }
  handled = FALSE;
  return 0;
}

LRESULT SyncOverlayWindow::OnEraseBackground(UINT msg_id, WPARAM wparam,
                                             LPARAM lparam, BOOL& handled) {
  handled = TRUE;  // fully painted in WM_PAINT; avoids a flash
  return 1;
}

LRESULT SyncOverlayWindow::OnPaint(UINT msg_id, WPARAM wparam, LPARAM lparam,
                                   BOOL& handled) {
  PAINTSTRUCT paint = {};
  HDC dc = ::BeginPaint(m_hWnd, &paint);
  if (dc == nullptr) {
    handled = FALSE;
    return 0;
  }
  CRect client_rect;
  GetClientRect(&client_rect);

  if (HBRUSH brush = ::CreateSolidBrush(kBackgroundColor); brush != nullptr) {
    ::FillRect(dc, &client_rect, brush);
    ::DeleteObject(brush);
  }

  HGDIOBJ old_font = ::SelectObject(dc, font_);
  ::SetBkMode(dc, TRANSPARENT);
  ::SetTextColor(dc, kTextColor);
  RECT text_rect = client_rect;
  ::DrawTextW(dc, message_.c_str(), -1, &text_rect,
              DT_CENTER | DT_VCENTER | DT_WORDBREAK | DT_NOPREFIX);
  ::SelectObject(dc, old_font);

  ::EndPaint(m_hWnd, &paint);
  handled = TRUE;
  return 0;
}

LRESULT SyncOverlayWindow::OnDisplayChange(UINT msg_id, WPARAM wparam,
                                           LPARAM lparam, BOOL& handled) {
  // UpdateLayout() re-targets and refreshes DPI/font itself (see there), so
  // there is nothing further to do here while hidden -- the next show will
  // pick up the current monitor layout regardless.
  if (active_) {
    UpdateLayout();
  }
  handled = FALSE;
  return 0;
}

}  // namespace win32
}  // namespace renderer
}  // namespace mozc
