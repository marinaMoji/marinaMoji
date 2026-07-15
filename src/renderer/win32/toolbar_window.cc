// Copyright 2010-2021, Google Inc.
// All rights reserved.

#include "renderer/win32/toolbar_window.h"

#include <wil/resource.h>
#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "base/file_util.h"
#include "base/system_util.h"
#include "base/win32/wide_char.h"
#include "protocol/commands.pb.h"
#include "protocol/renderer_command.pb.h"
#include "renderer/win32/win32_dpi_util.h"
#include "renderer/win32/win32_image_util.h"
#include "renderer/win32/win32_renderer_util.h"

namespace mozc {
namespace renderer {
namespace win32 {

namespace {

using ApplicationInfo = ::mozc::commands::RendererCommand::ApplicationInfo;

// Icon assets are pre-rendered at these fixed sizes by
// src/data/images/win/generate_toolbar_icons.py (no SVG/arbitrary-size
// decoding on Windows). Pick the nearest tier for the current DPI.
constexpr int kIconSizeTiers[] = {24, 36, 48};

constexpr int kButtonCount =
    static_cast<int>(ToolbarWindow::ButtonId::kNumButtons);

// Logical (96 DPI) layout constants, mirroring src/mac/mozc_toolbar.mm and
// docs/GTK_TOOLBAR.md so the three platforms look consistent.
constexpr int kIconSize = 24;
constexpr int kLogoWidthLogical = 120;
constexpr int kButtonWidthLogical = 36;
constexpr int kToolbarHeightLogical = 36;
constexpr int kMarginLogical = 20;  // distance from screen edge
constexpr double kCornerRadiusLogical = 10.0;
// Symbols/Shortcuts buttons open windows that don't exist on Windows yet
// (see docs/WINDOWS_PORT_PLAN.md Phase 4); render them visually disabled.
constexpr double kDisabledOpacity = 0.35;

int PickIconSizeTier(double dpi_scale) {
  const int target = static_cast<int>(std::lround(kIconSize * dpi_scale));
  int best = kIconSizeTiers[0];
  int best_delta = std::abs(kIconSizeTiers[0] - target);
  for (int size : kIconSizeTiers) {
    const int delta = std::abs(size - target);
    if (delta < best_delta) {
      best = size;
      best_delta = delta;
    }
  }
  return best;
}

std::string ToolbarIconPath(const std::string& name, int size) {
  return FileUtil::JoinPath(
      {SystemUtil::GetServerDirectory(), "toolbar_icons",
       absl::StrCat(name, "_", size, ".png")});
}

wil::unique_hbitmap LoadIcon(const std::string& name, int size,
                             CSize* out_size) {
  const std::string path = ToolbarIconPath(name, size);
  SIZE size_out = {};
  wil::unique_hbitmap bitmap(
      LoadPngFileToHBitmap(mozc::win32::Utf8ToWide(path), &size_out));
  if (!bitmap) {
    LOG(WARNING) << "Failed to load toolbar icon: " << path;
    if (out_size != nullptr) {
      *out_size = CSize(0, 0);
    }
    return bitmap;
  }
  if (out_size != nullptr) {
    *out_size = CSize(size_out.cx, size_out.cy);
  }
  return bitmap;
}

// Returns the raw top-down 32bpp premultiplied-BGRA pixel buffer of a DIB
// section created by CreateDIBSection (as LoadPngFileToHBitmap returns).
// Returns nullptr if |bitmap| is not such a DIB section.
uint8_t* GetDibBits(HBITMAP bitmap, int* width, int* height) {
  if (bitmap == nullptr) {
    return nullptr;
  }
  DIBSECTION dib = {};
  if (::GetObject(bitmap, sizeof(dib), &dib) == 0 || dib.dsBm.bmBits == nullptr) {
    return nullptr;
  }
  if (width != nullptr) {
    *width = dib.dsBm.bmWidth;
  }
  if (height != nullptr) {
    *height = dib.dsBm.bmHeight;
  }
  return static_cast<uint8_t*>(dib.dsBm.bmBits);
}

// Alpha-blends premultiplied-BGRA |src| (size src_w x src_h) onto
// premultiplied-BGRA |dst| (size dst_w x dst_h, stride dst_w * 4) with its
// top-left corner at (dst_x, dst_y). |opacity| in [0, 1] scales the source's
// contribution (see class comment in the header for why this is valid on
// premultiplied data).
void BlendIcon(uint8_t* dst, int dst_w, int dst_h, const uint8_t* src,
              int src_w, int src_h, int dst_x, int dst_y, double opacity) {
  if (src == nullptr || dst == nullptr) {
    return;
  }
  for (int y = 0; y < src_h; ++y) {
    const int dy = dst_y + y;
    if (dy < 0 || dy >= dst_h) {
      continue;
    }
    for (int x = 0; x < src_w; ++x) {
      const int dx = dst_x + x;
      if (dx < 0 || dx >= dst_w) {
        continue;
      }
      const uint8_t* s = src + (y * src_w + x) * 4;
      uint8_t* d = dst + (dy * dst_w + dx) * 4;
      const double src_alpha_scale = opacity;
      const double sb = s[0] * src_alpha_scale;
      const double sg = s[1] * src_alpha_scale;
      const double sr = s[2] * src_alpha_scale;
      const double sa = s[3] * src_alpha_scale;
      const double inv = (255.0 - sa) / 255.0;
      d[0] = static_cast<uint8_t>(std::clamp(sb + d[0] * inv, 0.0, 255.0));
      d[1] = static_cast<uint8_t>(std::clamp(sg + d[1] * inv, 0.0, 255.0));
      d[2] = static_cast<uint8_t>(std::clamp(sr + d[2] * inv, 0.0, 255.0));
      d[3] = static_cast<uint8_t>(std::clamp(sa + d[3] * inv, 0.0, 255.0));
    }
  }
}

// Coverage in [0, 1] of a rounded rect (size w x h, corner radius r) at pixel
// center (px, py), antialiased over a 1px band at the boundary.
double RoundedRectCoverage(double px, double py, double w, double h,
                           double r) {
  // Distance (in pixels) from (px, py) to the nearest edge of the rounded
  // rect, positive when inside, negative when outside.
  const double cx = std::clamp(px, r, w - r);
  const double cy = std::clamp(py, r, h - r);
  const bool in_corner_region =
      (px < r || px > w - r) && (py < r || py > h - r);
  double signed_dist;
  if (in_corner_region) {
    const double dx = px - cx;
    const double dy = py - cy;
    signed_dist = r - std::sqrt(dx * dx + dy * dy);
  } else {
    signed_dist = std::min({px, py, w - px, h - py});
  }
  return std::clamp(signed_dist + 0.5, 0.0, 1.0);
}

}  // namespace

ToolbarWindow::ToolbarWindow()
    : send_command_interface_(nullptr),
      dpi_(USER_DEFAULT_SCREEN_DPI),
      is_dark_theme_(false),
      has_state_(false),
      current_mode_(commands::DIRECT),
      activated_(false),
      left_shift_direct_lock_(false),
      use_traditional_kanji_(false),
      symbols_palette_visible_(false),
      window_origin_(0, 0),
      pressed_button_(-1),
      mode_menu_open_(false) {}

ToolbarWindow::~ToolbarWindow() {}

void ToolbarWindow::Initialize() {
  Create(nullptr);
  ShowWindow(SW_HIDE);
}

void ToolbarWindow::Destroy() {
  if (IsWindow()) {
    DestroyWindow();
  }
}

void ToolbarWindow::SetSendCommandInterface(
    client::SendCommandInterface* send_command_interface) {
  send_command_interface_ = send_command_interface;
}

LRESULT ToolbarWindow::OnCreate(LPCREATESTRUCT create_struct) {
  is_dark_theme_ = IsDarkTheme();
  // Best guess before the window has a real on-screen position (used to
  // size/place the window on first show); refined via OnDisplayChange /
  // whenever the window subsequently moves to another monitor.
  dpi_ = GetDpiForPoint(0, 0);
  return 0;
}

void ToolbarWindow::Hide() {
  // TrackPopupMenuEx runs a modal loop that can dispatch a focus-loss hide
  // before the selected mode command reaches the TIP. Keep the toolbar up
  // until that loop finishes (see |mode_menu_open_| in the header).
  if (mode_menu_open_) {
    return;
  }
  ShowWindow(SW_HIDE);
}

bool ToolbarWindow::IsDarkTheme() const {
  HKEY key = nullptr;
  if (::RegOpenKeyExW(
          HKEY_CURRENT_USER,
          L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes"
          L"\\Personalize",
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

void ToolbarWindow::OnUpdate(const commands::RendererCommand& command) {
  bool show_toolbar = false;
  bool symbols_palette_visible = false;
  if (command.has_application_info()) {
    show_toolbar = (command.application_info().ui_visibilities() &
                    ApplicationInfo::ShowToolbar) ==
                   ApplicationInfo::ShowToolbar;
    symbols_palette_visible =
        command.application_info().has_symbols_palette_info();
  }
  symbols_palette_visible_ = symbols_palette_visible;
  if (!show_toolbar || !command.has_output()) {
    Hide();
    return;
  }

  const commands::Output& output = command.output();
  commands::CompositionMode mode = commands::DIRECT;
  bool activated = false;
  bool lock = false;
  if (output.has_status()) {
    activated = output.status().activated();
    mode = activated ? output.status().mode() : commands::DIRECT;
    lock = output.status().left_shift_direct_lock();
  }
  // marinaMoji: |output.config()| is only populated on the specific Output
  // that toggles it (ToggleTraditionalKanji et al.); ordinary per-keystroke
  // updates carry no config at all. Treat |use_traditional_kanji_| as sticky
  // local state so the icon doesn't revert on the next unrelated UPDATE.
  const bool use_trad = output.has_config()
                            ? output.config().use_traditional_kanji()
                            : use_traditional_kanji_;
  const bool dark = IsDarkTheme();

  const bool state_changed =
      !has_state_ || mode != current_mode_ || activated != activated_ ||
      lock != left_shift_direct_lock_ || use_trad != use_traditional_kanji_ ||
      dark != is_dark_theme_;

  current_mode_ = mode;
  activated_ = activated;
  left_shift_direct_lock_ = lock;
  use_traditional_kanji_ = use_trad;
  is_dark_theme_ = dark;

  const bool first_show = !has_state_;
  has_state_ = true;

  // Icons must be loaded before ComputeWindowSize(): the window width depends
  // on the logo bitmap's actual width (|logo_size_|).
  if (state_changed || !IsWindowVisible()) {
    LoadIcons();
  }

  if (first_show) {
    LoadSavedPosition(&window_origin_, ComputeWindowSize());
  }

  if (state_changed || !IsWindowVisible()) {
    Redraw();
  }

  if (!IsWindowVisible()) {
    SetWindowPos(HWND_TOPMOST, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
  }
}

void ToolbarWindow::LoadIcons() {
  const double dpi_scale = GetDPIScalingFactor(dpi_);
  const int size = PickIconSizeTier(dpi_scale);
  const char* theme = is_dark_theme_ ? "dark" : "light";

  icon_cache_.clear();
  icon_cache_.resize(kButtonCount);

  std::string mode_name;
  switch (current_mode_) {
    case commands::DIRECT:
      mode_name = absl::StrCat("toolbar_roman_", theme,
                               left_shift_direct_lock_ ? "_lock" : "");
      break;
    case commands::HALF_ASCII:
      mode_name = absl::StrCat("toolbar_roma_half_", theme);
      break;
    case commands::FULL_ASCII:
      mode_name = absl::StrCat("toolbar_roma_full_", theme);
      break;
    case commands::HALF_KATAKANA:
      mode_name = absl::StrCat("toolbar_kata_half_", theme);
      break;
    case commands::FULL_KATAKANA:
    case commands::MANYOSHU:
      mode_name = absl::StrCat("toolbar_kata_", theme,
                               left_shift_direct_lock_ ? "_lock" : "");
      break;
    case commands::HIRAGANA:
    default:
      mode_name = absl::StrCat("toolbar_hira_", theme,
                               left_shift_direct_lock_ ? "_lock" : "");
      break;
  }
  icon_cache_[static_cast<int>(ButtonId::kMode)] =
      LoadIcon(mode_name, size, nullptr);

  const std::string trad_name = absl::StrCat(
      use_traditional_kanji_ ? "toolbar_kyu_" : "toolbar_shin_", theme);
  icon_cache_[static_cast<int>(ButtonId::kTraditionalKanji)] =
      LoadIcon(trad_name, size, nullptr);

  icon_cache_[static_cast<int>(ButtonId::kSymbols)] =
      LoadIcon(absl::StrCat("toolbar_symbols_", theme), size, nullptr);
  icon_cache_[static_cast<int>(ButtonId::kDictionary)] =
      LoadIcon(absl::StrCat("toolbar_dict_", theme), size, nullptr);
  icon_cache_[static_cast<int>(ButtonId::kSettings)] =
      LoadIcon(absl::StrCat("toolbar_settings_", theme), size, nullptr);
  icon_cache_[static_cast<int>(ButtonId::kShortcuts)] =
      LoadIcon(absl::StrCat("toolbar_shortcuts_", theme), size, nullptr);

  logo_cache_ = LoadIcon(absl::StrCat("logo_long_", theme), size, &logo_size_);
}

int ToolbarWindow::LogoWidth(double scale) const {
  // The logo PNG is pre-rendered with its natural aspect ratio at the icon
  // size tier, so its width is usually narrower than the logical 120px the
  // mac toolbar reserves; reserving the logical width leaves a large gap
  // between the logo and the mode button. Use the actual bitmap width once
  // the icons are loaded (the logical width is only a pre-load fallback).
  if (logo_size_.cx > 0) {
    return logo_size_.cx;
  }
  return static_cast<int>(std::lround(kLogoWidthLogical * scale));
}

CSize ToolbarWindow::ComputeWindowSize() const {
  const double scale = GetDPIScalingFactor(dpi_);
  const int margin = static_cast<int>(std::lround(4 * scale));
  const int button_w = static_cast<int>(std::lround(kButtonWidthLogical * scale));
  const int height = static_cast<int>(std::lround(kToolbarHeightLogical * scale));
  const int width =
      margin + LogoWidth(scale) + margin + button_w * kButtonCount + margin;
  return CSize(width, height);
}

void ToolbarWindow::Redraw() {
  const double scale = GetDPIScalingFactor(dpi_);
  const int margin = static_cast<int>(std::lround(4 * scale));
  const int button_w = static_cast<int>(std::lround(kButtonWidthLogical * scale));
  const double corner_radius = kCornerRadiusLogical * scale;
  const int logo_w = LogoWidth(scale);

  const CSize window_size = ComputeWindowSize();
  const int width = window_size.cx;
  const int height = window_size.cy;

  BITMAPINFO bitmap_info = {};
  bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bitmap_info.bmiHeader.biWidth = width;
  bitmap_info.bmiHeader.biHeight = -height;  // top-down
  bitmap_info.bmiHeader.biPlanes = 1;
  bitmap_info.bmiHeader.biBitCount = 32;
  bitmap_info.bmiHeader.biCompression = BI_RGB;

  void* bits_ptr = nullptr;
  wil::unique_hbitmap dib(::CreateDIBSection(nullptr, &bitmap_info,
                                             DIB_RGB_COLORS, &bits_ptr,
                                             nullptr, 0));
  if (!dib || bits_ptr == nullptr) {
    return;
  }
  uint8_t* bits = static_cast<uint8_t*>(bits_ptr);

  // Background fill: solid opaque chrome, antialiased only at the rounded
  // corners (matches mac's solid, non-vibrancy chrome).
  const uint8_t bg_b = is_dark_theme_ ? 40 : 255;
  const uint8_t bg_g = is_dark_theme_ ? 35 : 255;
  const uint8_t bg_r = is_dark_theme_ ? 32 : 255;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const double coverage = RoundedRectCoverage(
          x + 0.5, y + 0.5, width, height, corner_radius);
      uint8_t* p = bits + (y * width + x) * 4;
      const uint8_t alpha = static_cast<uint8_t>(std::lround(255.0 * coverage));
      p[0] = static_cast<uint8_t>(bg_b * coverage);
      p[1] = static_cast<uint8_t>(bg_g * coverage);
      p[2] = static_cast<uint8_t>(bg_r * coverage);
      p[3] = alpha;
    }
  }

  int x_cursor = margin;
  int icon_w = 0;
  int icon_h = 0;
  uint8_t* logo_bits = GetDibBits(logo_cache_.get(), &icon_w, &icon_h);
  if (logo_bits != nullptr) {
    const int y = (height - icon_h) / 2;
    BlendIcon(bits, width, height, logo_bits, icon_w, icon_h, x_cursor, y, 1.0);
  }
  x_cursor += logo_w + margin;

  button_rects_.clear();
  button_rects_.reserve(kButtonCount);
  for (int i = 0; i < kButtonCount; ++i) {
    const CRect rect(x_cursor, 0, x_cursor + button_w, height);
    button_rects_.push_back(rect);

    const auto id = static_cast<ButtonId>(i);
    // marinaMoji: Symbols toggles SymbolsPaletteWindow; Shortcuts still has
    // no Windows implementation (see docs/WINDOWS_PORT_PLAN.md Phase 4), so
    // only that button is rendered visually disabled.
    const double opacity = (id == ButtonId::kShortcuts) ? kDisabledOpacity : 1.0;
    uint8_t* icon_bits = GetDibBits(icon_cache_[i].get(), &icon_w, &icon_h);
    if (icon_bits != nullptr) {
      const int icon_x = x_cursor + (button_w - icon_w) / 2;
      const int icon_y = (height - icon_h) / 2;
      BlendIcon(bits, width, height, icon_bits, icon_w, icon_h, icon_x,
               icon_y, opacity);
    }
    x_cursor += button_w;
  }

  ::GdiFlush();

  CPoint top_left = window_origin_;
  CSize size(width, height);
  CPoint src_origin(0, 0);
  BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
  wil::unique_hdc mem_dc(::CreateCompatibleDC(nullptr));
  wil::unique_select_object old_bitmap =
      wil::SelectObject(mem_dc.get(), dib.get());
  ::UpdateLayeredWindow(m_hWnd, nullptr, &top_left, &size, mem_dc.get(),
                        &src_origin, 0, &blend, ULW_ALPHA);
}

int ToolbarWindow::HitTestButton(const CPoint& point) const {
  for (size_t i = 0; i < button_rects_.size(); ++i) {
    if (button_rects_[i].PtInRect(point)) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void ToolbarWindow::OnLButtonDown(UINT flags, CPoint point) {
  pressed_button_ = HitTestButton(point);
}

void ToolbarWindow::OnLButtonUp(UINT flags, CPoint point) {
  const int button = HitTestButton(point);
  const int pressed = pressed_button_;
  pressed_button_ = -1;
  if (button < 0 || button != pressed) {
    return;
  }
  switch (static_cast<ButtonId>(button)) {
    case ButtonId::kMode:
      ShowModeMenu();
      break;
    case ButtonId::kTraditionalKanji:
      SendToggleTraditionalKanji();
      break;
    case ButtonId::kDictionary:
      SendLaunchWordRegisterDialog();
      break;
    case ButtonId::kSettings:
      SendLaunchConfigDialog();
      break;
    case ButtonId::kSymbols:
      SendToggleSymbolsPalette();
      break;
    case ButtonId::kShortcuts:
      // Not implemented on Windows yet; button is rendered disabled.
      break;
    default:
      break;
  }
}

LRESULT ToolbarWindow::OnNcHitTest(CPoint point) {
  CPoint client_point = point;
  ScreenToClient(&client_point);
  if (HitTestButton(client_point) >= 0) {
    return HTCLIENT;
  }
  // Background drag: let DefWindowProc's caption-drag machinery move the
  // window (works fine with WS_EX_NOACTIVATE, unlike a manual mouse-capture
  // loop which would need to fight the non-activating popup for focus).
  return HTCAPTION;
}

void ToolbarWindow::OnExitSizeMove() {
  // The OS's own caption-drag machinery (triggered by returning HTCAPTION
  // from WM_NCHITTEST) moves the real window rect directly; resync our
  // cached origin (used by Redraw(), since GetWindowRect() on this
  // WS_EX_LAYERED window isn't otherwise consulted) and persist it.
  CRect window_rect;
  GetWindowRect(&window_rect);
  window_origin_ = window_rect.TopLeft();
  SavePosition();
}

void ToolbarWindow::OnSettingChange(UINT flags, LPCTSTR section) {
  const bool dark = IsDarkTheme();
  if (dark != is_dark_theme_) {
    is_dark_theme_ = dark;
    LoadIcons();
    Redraw();
  }
}

void ToolbarWindow::OnDisplayChange() {
  CRect window_rect;
  GetWindowRect(&window_rect);
  const uint32_t new_dpi =
      GetDpiForPoint(window_rect.left, window_rect.top);
  if (new_dpi != dpi_) {
    dpi_ = new_dpi;
    LoadIcons();
    Redraw();
  }
}

void ToolbarWindow::ShowModeMenu() {
  wil::unique_hmenu menu(::CreatePopupMenu());
  if (!menu) {
    return;
  }
  struct ModeEntry {
    const wchar_t* label;
    commands::CompositionMode mode;
  };
  constexpr ModeEntry kEntries[] = {
      {L"Hiragana", commands::HIRAGANA},
      // marinaMoji: the "Katakana" entry enters Manyōshū mode, which replaces
      // traditional full-width katakana.
      {L"Katakana", commands::MANYOSHU},
      {L"Half-width Katakana", commands::HALF_KATAKANA},
      {L"Full-width Roman", commands::FULL_ASCII},
      {L"Half-width Roman", commands::HALF_ASCII},
      {L"Direct input", commands::DIRECT},
  };
  for (size_t i = 0; i < std::size(kEntries); ++i) {
    UINT flags = MF_STRING;
    // FULL_KATAKANA can still be reported by older sessions; show it as the
    // Katakana (Manyōshū) entry being active.
    const bool checked =
        kEntries[i].mode == current_mode_ ||
        (kEntries[i].mode == commands::MANYOSHU &&
         current_mode_ == commands::FULL_KATAKANA);
    if (activated_ && checked) {
      flags |= MF_CHECKED;
    }
    ::AppendMenuW(menu.get(), flags, i + 1, kEntries[i].label);
  }

  CRect button_rect = button_rects_.empty()
                          ? CRect(0, 0, 0, 0)
                          : button_rects_[static_cast<int>(ButtonId::kMode)];
  CPoint screen_point(button_rect.left, button_rect.bottom);
  ClientToScreen(&screen_point);

  // Keep the application in the foreground while the popup is open.  The
  // renderer owns this non-activating toolbar, but the callback is handled by
  // the TIP attached to the application's focused text context.  Promoting
  // this window to foreground makes that context disappear before the selected
  // command is delivered, which looks like a successful menu click but has no
  // effect.  |mode_menu_open_| still suppresses focus-loss hide traffic that
  // can be dispatched by TrackPopupMenuEx's modal loop.
  mode_menu_open_ = true;
  const int selected = ::TrackPopupMenuEx(
      menu.get(), TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_NONOTIFY,
      screen_point.x, screen_point.y, m_hWnd, nullptr);
  ::PostMessage(m_hWnd, WM_NULL, 0, 0);
  mode_menu_open_ = false;
  if (selected <= 0 || selected > static_cast<int>(std::size(kEntries))) {
    return;
  }
  const commands::CompositionMode mode = kEntries[selected - 1].mode;
  if (mode == commands::DIRECT) {
    SendTurnOffIme();
  } else {
    SendSwitchCompositionMode(mode);
  }
}

void ToolbarWindow::SendSwitchCompositionMode(commands::CompositionMode mode) {
  if (send_command_interface_ == nullptr) {
    return;
  }
  commands::SessionCommand command;
  command.set_type(commands::SessionCommand::SWITCH_COMPOSITION_MODE);
  // The renderer->TIP channel only carries (type, id); |id| repurposed to
  // carry the target CompositionMode (see renderer_server.cc / tip_edit_
  // session.cc's OnRendererCallbackAsync).
  command.set_id(static_cast<int32_t>(mode));
  commands::Output output;
  send_command_interface_->SendCommand(command, &output);
}

void ToolbarWindow::SendTurnOffIme() {
  if (send_command_interface_ == nullptr) {
    return;
  }
  commands::SessionCommand command;
  command.set_type(commands::SessionCommand::TURN_OFF_IME);
  commands::Output output;
  send_command_interface_->SendCommand(command, &output);
}

void ToolbarWindow::SendToggleTraditionalKanji() {
  if (send_command_interface_ == nullptr) {
    return;
  }
  commands::SessionCommand command;
  command.set_type(commands::SessionCommand::TOGGLE_TRADITIONAL_KANJI);
  commands::Output output;
  send_command_interface_->SendCommand(command, &output);
}

void ToolbarWindow::SendLaunchWordRegisterDialog() {
  if (send_command_interface_ == nullptr) {
    return;
  }
  commands::SessionCommand command;
  command.set_type(commands::SessionCommand::LAUNCH_WORD_REGISTER_DIALOG);
  commands::Output output;
  send_command_interface_->SendCommand(command, &output);
}

void ToolbarWindow::SendLaunchConfigDialog() {
  if (send_command_interface_ == nullptr) {
    return;
  }
  commands::SessionCommand command;
  command.set_type(commands::SessionCommand::LAUNCH_CONFIG_DIALOG);
  commands::Output output;
  send_command_interface_->SendCommand(command, &output);
}

void ToolbarWindow::SendToggleSymbolsPalette() {
  if (send_command_interface_ == nullptr) {
    return;
  }
  commands::SessionCommand command;
  const bool show = !symbols_palette_visible_;
  symbols_palette_visible_ = show;
  command.set_type(show ? commands::SessionCommand::SHOW_SYMBOLS_PALETTE
                        : commands::SessionCommand::HIDE_SYMBOLS_PALETTE);
  commands::Output output;
  send_command_interface_->SendCommand(command, &output);
}

std::string ToolbarWindow::ConfigFilePath() const {
  return FileUtil::JoinPath(
      {SystemUtil::GetUserProfileDirectory(), "toolbar.conf"});
}

void ToolbarWindow::LoadSavedPosition(CPoint* out_position,
                                      const CSize& window_size) const {
  *out_position = DefaultWindowOrigin(window_size);
  const auto contents = FileUtil::GetContents(ConfigFilePath());
  if (!contents.ok()) {
    return;
  }
  int x = 0;
  int y = 0;
  bool has_x = false;
  bool has_y = false;
  size_t pos = 0;
  const std::string& text = *contents;
  while (pos < text.size()) {
    const size_t eol = text.find('\n', pos);
    const std::string line =
        text.substr(pos, eol == std::string::npos ? std::string::npos
                                                   : eol - pos);
    const size_t eq = line.find('=');
    if (eq != std::string::npos) {
      const std::string key = line.substr(0, eq);
      const std::string value = line.substr(eq + 1);
      if (key == "x") {
        x = std::atoi(value.c_str());
        has_x = true;
      } else if (key == "y") {
        y = std::atoi(value.c_str());
        has_y = true;
      }
    }
    if (eol == std::string::npos) {
      break;
    }
    pos = eol + 1;
  }
  if (has_x && has_y) {
    *out_position = CPoint(x, y);
  }
}

void ToolbarWindow::SavePosition() const {
  CRect window_rect;
  GetWindowRect(&window_rect);
  const std::string content =
      absl::StrCat("x=", window_rect.left, "\ny=", window_rect.top, "\n");
  const std::string path = ConfigFilePath();
  (void)FileUtil::CreateDirectory(FileUtil::Dirname(path));
  (void)FileUtil::SetContents(path, content);
}

CPoint ToolbarWindow::DefaultWindowOrigin(const CSize& window_size) const {
  RECT working_area = {0, 0, 1920, 1080};
  const POINT origin = {0, 0};
  GetWorkingAreaFromPoint(origin, &working_area);
  const double scale = GetDPIScalingFactor(dpi_);
  const int margin = static_cast<int>(std::lround(kMarginLogical * scale));
  const int x = working_area.right - window_size.cx - margin;
  const int y = working_area.bottom - window_size.cy - margin;
  return CPoint(x, y);
}

}  // namespace win32
}  // namespace renderer
}  // namespace mozc
