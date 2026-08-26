// Copyright 2010-2021, Google Inc.
// All rights reserved.

#include "renderer/win32/toolbar_window.h"

#include <commctrl.h>
#include <oleacc.h>
#include <wil/resource.h>
#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "base/file_util.h"
#include "base/system_util.h"
#include "base/win32/wide_char.h"
#include "dictionary/docket_store.h"
#include "protocol/commands.pb.h"
#include "protocol/renderer_command.pb.h"
#include "renderer/win32/marina_localized_string.h"
#include "renderer/win32/toolbar_accessible.h"
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
// decoding on Windows). The nearest tier is picked for the current DPI and
// then resampled to the exact pixel size the layout needs, so fractional
// scale factors (125% / 175%, the two most common Windows laptop settings)
// get correctly sized icons rather than the nearest tier's raw bitmap.
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
// Rounded highlight drawn behind the button under the cursor / being pressed.
constexpr double kButtonHighlightRadiusLogical = 6.0;
constexpr int kButtonHighlightInsetLogical = 2;

// Menu command ids. Kept out of the 1..N range ShowModeMenu() uses for its
// mode entries so the two menus can never be confused.
constexpr int kContextMenuDictionaryTool = 101;
constexpr int kContextMenuSettings = 102;
constexpr int kContextMenuShortcuts = 103;
constexpr int kContextMenuHideToolbar = 104;

int PickIconSizeTier(int target) {
  int best = kIconSizeTiers[0];
  int best_delta = std::abs(kIconSizeTiers[0] - target);
  for (int size : kIconSizeTiers) {
    const int delta = std::abs(size - target);
    // On a tie prefer the *larger* tier: downsampling a 48px asset to 42px
    // reads far better than upsampling a 36px one, and every fractional
    // Windows scale factor lands exactly on such a tie (125% -> 30 is
    // equidistant from 24 and 36; 175% -> 42 from 36 and 48).
    if (delta < best_delta || (delta == best_delta && size > best)) {
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

// Named LoadToolbarIcon rather than LoadIcon: <winuser.h> #defines LoadIcon
// to LoadIconW, which would silently rewrite both this definition and every
// call below and collide with the real API the moment anyone needs it.
wil::unique_hbitmap LoadToolbarIcon(const std::string& name, int size,
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

// Bilinearly samples channel-wise from premultiplied-BGRA |src| at the
// continuous source position (u, v), writing 4 bytes to |out|. Interpolating
// premultiplied components directly is correct -- premultiplied colour is
// linear in coverage, which is exactly what makes the blend below valid too.
void SampleBilinear(const uint8_t* src, int src_w, int src_h, double u,
                    double v, uint8_t* out) {
  const double cu = std::clamp(u, 0.0, static_cast<double>(src_w - 1));
  const double cv = std::clamp(v, 0.0, static_cast<double>(src_h - 1));
  const int x0 = static_cast<int>(cu);
  const int y0 = static_cast<int>(cv);
  const int x1 = std::min(x0 + 1, src_w - 1);
  const int y1 = std::min(y0 + 1, src_h - 1);
  const double fx = cu - x0;
  const double fy = cv - y0;
  for (int c = 0; c < 4; ++c) {
    const double p00 = src[(y0 * src_w + x0) * 4 + c];
    const double p10 = src[(y0 * src_w + x1) * 4 + c];
    const double p01 = src[(y1 * src_w + x0) * 4 + c];
    const double p11 = src[(y1 * src_w + x1) * 4 + c];
    const double top = p00 + (p10 - p00) * fx;
    const double bottom = p01 + (p11 - p01) * fx;
    out[c] = static_cast<uint8_t>(
        std::clamp(top + (bottom - top) * fy + 0.5, 0.0, 255.0));
  }
}

// Alpha-blends premultiplied-BGRA |src| (size src_w x src_h) onto
// premultiplied-BGRA |dst| (size dst_w x dst_h, stride dst_w * 4), resampled
// to |out_w| x |out_h| with its top-left corner at (dst_x, dst_y). |opacity|
// in [0, 1] scales the source's contribution.
void BlendIcon(uint8_t* dst, int dst_w, int dst_h, const uint8_t* src,
               int src_w, int src_h, int dst_x, int dst_y, int out_w,
               int out_h, double opacity) {
  if (src == nullptr || dst == nullptr || src_w <= 0 || src_h <= 0 ||
      out_w <= 0 || out_h <= 0) {
    return;
  }
  const bool resample = (out_w != src_w || out_h != src_h);
  uint8_t sample[4] = {};
  for (int y = 0; y < out_h; ++y) {
    const int dy = dst_y + y;
    if (dy < 0 || dy >= dst_h) {
      continue;
    }
    for (int x = 0; x < out_w; ++x) {
      const int dx = dst_x + x;
      if (dx < 0 || dx >= dst_w) {
        continue;
      }
      const uint8_t* s;
      if (resample) {
        SampleBilinear(src, src_w, src_h,
                       (x + 0.5) * src_w / out_w - 0.5,
                       (y + 0.5) * src_h / out_h - 0.5, sample);
        s = sample;
      } else {
        s = src + (y * src_w + x) * 4;
      }
      uint8_t* d = dst + (dy * dst_w + dx) * 4;
      const double sb = s[0] * opacity;
      const double sg = s[1] * opacity;
      const double sr = s[2] * opacity;
      const double sa = s[3] * opacity;
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

// Composites a solid rounded rect of |rect| (in |dst| coordinates) over the
// premultiplied-BGRA buffer |dst| at |alpha| (0-255).
void FillRoundedRect(uint8_t* dst, int dst_w, int dst_h, const CRect& rect,
                     double radius, uint8_t r, uint8_t g, uint8_t b,
                     uint8_t alpha) {
  const double w = rect.Width();
  const double h = rect.Height();
  if (w <= 0 || h <= 0 || alpha == 0) {
    return;
  }
  const double clamped_radius = std::min(radius, std::min(w, h) / 2.0);
  for (int y = rect.top; y < rect.bottom; ++y) {
    if (y < 0 || y >= dst_h) {
      continue;
    }
    for (int x = rect.left; x < rect.right; ++x) {
      if (x < 0 || x >= dst_w) {
        continue;
      }
      const double coverage =
          RoundedRectCoverage(x - rect.left + 0.5, y - rect.top + 0.5, w, h,
                              clamped_radius);
      const double sa = alpha * coverage;
      if (sa <= 0.0) {
        continue;
      }
      const double scale = sa / 255.0;
      uint8_t* d = dst + (y * dst_w + x) * 4;
      const double inv = (255.0 - sa) / 255.0;
      d[0] = static_cast<uint8_t>(std::clamp(b * scale + d[0] * inv, 0.0, 255.0));
      d[1] = static_cast<uint8_t>(std::clamp(g * scale + d[1] * inv, 0.0, 255.0));
      d[2] = static_cast<uint8_t>(std::clamp(r * scale + d[2] * inv, 0.0, 255.0));
      d[3] = static_cast<uint8_t>(std::clamp(sa + d[3] * inv, 0.0, 255.0));
    }
  }
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
      shortcuts_window_visible_(false),
      icon_draw_size_(kIconSize),
      logo_draw_size_(0, 0),
      window_origin_(0, 0),
      has_layout_(false),
      pressed_button_(-1),
      hovered_button_(-1),
      tracking_mouse_leave_(false),
      menu_open_(false),
      hide_deferred_by_menu_(false),
      tooltip_window_(nullptr),
      tooltip_layout_rect_(0, 0, 0, 0),
      accessible_(nullptr) {}

ToolbarWindow::~ToolbarWindow() {}

void ToolbarWindow::Initialize() {
  Create(nullptr);
  // Gives the window an accessible name for screen readers and window
  // enumerators; the toolbar draws no text of its own.
  SetWindowTextW(MarinaLocalizedString(L"MM.Toolbar"));
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
  // size/place the window on first show); refined by SyncDpiForCurrentMonitor
  // on WM_DPICHANGED, WM_DISPLAYCHANGE and after every drag.
  dpi_ = GetDpiForPoint(0, 0);
  return 0;
}

void ToolbarWindow::OnDestroy() {
  CancelPendingHide();
  if (accessible_ != nullptr) {
    static_cast<IAccessible*>(accessible_)->Release();
    accessible_ = nullptr;
  }
  if (tooltip_window_ != nullptr) {
    ::DestroyWindow(tooltip_window_);
    tooltip_window_ = nullptr;
  }
}

void ToolbarWindow::Hide() {
  // TrackPopupMenuEx runs a modal loop that can dispatch a focus-loss hide
  // before the selected command reaches the TIP. Keep the toolbar up until
  // that loop finishes, but remember that a hide was owed -- once the menu
  // closes there is no guarantee another RendererCommand will arrive (the
  // IME may no longer be focused anywhere), so dropping it outright can
  // strand the toolbar on screen.
  if (menu_open_) {
    hide_deferred_by_menu_ = true;
    return;
  }
  CancelPendingHide();
  hide_deferred_by_menu_ = false;
  hovered_button_ = -1;
  pressed_button_ = -1;
  ShowWindow(SW_HIDE);
}

void ToolbarWindow::SchedulePendingHide() {
  if (hide_pending_) {
    return;  // already scheduled; let the existing timer run its course
  }
  hide_pending_ = true;
  SetTimer(kHideDelayTimerId, kHideDelayMsec, nullptr);
}

void ToolbarWindow::CancelPendingHide() {
  if (!hide_pending_) {
    return;
  }
  hide_pending_ = false;
  KillTimer(kHideDelayTimerId);
}

void ToolbarWindow::OnTimerTick(UINT_PTR timer_id) {
  if (timer_id != kHideDelayTimerId) {
    return;
  }
  hide_pending_ = false;
  KillTimer(kHideDelayTimerId);
  Hide();
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
  bool shortcuts_window_visible = false;
  if (command.has_application_info()) {
    show_toolbar = (command.application_info().ui_visibilities() &
                    ApplicationInfo::ShowToolbar) ==
                   ApplicationInfo::ShowToolbar;
    symbols_palette_visible =
        command.application_info().has_symbols_palette_info();
    shortcuts_window_visible =
        command.application_info().has_shortcuts_info();
  }
  // marinaMoji TEMPORARY (2026-08-08): diagnosing "odoriji palette opens then
  // disappears" and "shortcuts button opens nothing" on real hardware. Every
  // RendererCommand that reaches the toolbar is reported with the three
  // visibility bits, so DebugView shows whether a palette is being closed by a
  // follow-up command that simply doesn't carry its info field (the suspected
  // race -- see the SHOW_SYMBOLS_PALETTE comment in renderer_server.cc) or
  // whether the show request never arrives at all. Remove with the other
  // marinaMoji TEMPORARY logging once these are fixed.
  {
    const std::string line = absl::StrCat(
        "[marinaMoji/toolbar] OnUpdate: show_toolbar=", show_toolbar,
        " symbols_palette=", symbols_palette_visible,
        " shortcuts=", shortcuts_window_visible,
        " has_output=", command.has_output(),
        " (was symbols=", symbols_palette_visible_,
        " shortcuts=", shortcuts_window_visible_, ")\n");
    ::OutputDebugStringA(line.c_str());
  }
  symbols_palette_visible_ = symbols_palette_visible;
  shortcuts_window_visible_ = shortcuts_window_visible;
  if (!show_toolbar || !command.has_output()) {
    SchedulePendingHide();
    return;
  }
  CancelPendingHide();

  const commands::Output& output = command.output();
  commands::CompositionMode mode = commands::DIRECT;
  bool activated = false;
  // Keep the toolbar in sync with the taskbar/indicator. The TIP populates
  // indicator_info from TipInputModeManager's effective state, which is the
  // authoritative state used by the language bar. Renderer output can lag or
  // omit status during focus and mode transitions, so use it only as a
  // fallback for clients that do not provide indicator status.
  const commands::Status* status = nullptr;
  if (command.has_application_info() &&
      command.application_info().has_indicator_info() &&
      command.application_info().indicator_info().has_status()) {
    status = &command.application_info().indicator_info().status();
  } else if (output.has_status()) {
    status = &output.status();
  }
  if (status != nullptr) {
    activated = status->activated();
    mode = activated ? status->mode() : commands::DIRECT;
  }
  // marinaMoji: the lock flag comes from the session, so it only ever exists
  // on |output.status()|. Never read it from |status| above: that may be the
  // indicator_info status, which the TIP builds field by field and which
  // therefore reports |left_shift_direct_lock| as its default false. The
  // indicator is visible exactly when the mode has just changed -- i.e. on
  // the second tap of the Left Shift double tap, the one that engages the
  // lock -- so reading it from there showed no lock until some later update
  // arrived without indicator_info, making the gesture look like it needed a
  // third tap. Sticky like |use_traditional_kanji_| below, so an update that
  // carries no status at all does not clear the icon.
  const bool lock = output.has_status()
                        ? output.status().left_shift_direct_lock()
                        : left_shift_direct_lock_;
  // marinaMoji: |output.config()| is only populated on the specific Output
  // that toggles it (ToggleTraditionalKanji et al.); ordinary per-keystroke
  // updates carry no config at all. Treat |use_traditional_kanji_| as sticky
  // local state so the icon doesn't revert on the next unrelated UPDATE.
  const bool use_trad = output.has_config()
                            ? output.config().use_traditional_kanji()
                            : use_traditional_kanji_;
  // marinaMoji: |is_dark_theme_| is not re-polled here -- it is a registry
  // read, and this function runs on every RendererCommand, i.e. every
  // keystroke while the toolbar is visible. OnCreate() sets the initial
  // value and OnSettingChange() (WM_SETTINGCHANGE) keeps it current; a
  // per-update poll would just repeat that same read up to once per
  // keystroke for no benefit.
  const bool state_changed =
      !has_state_ || mode != current_mode_ || activated != activated_ ||
      lock != left_shift_direct_lock_ || use_trad != use_traditional_kanji_;

  current_mode_ = mode;
  activated_ = activated;
  left_shift_direct_lock_ = lock;
  use_traditional_kanji_ = use_trad;

  const bool first_show = !has_state_;
  has_state_ = true;

  // Icons must be loaded before ComputeWindowSize(): the window width depends
  // on the logo bitmap's drawn width (|logo_draw_size_|).
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
  icon_draw_size_ = std::max(1, static_cast<int>(std::lround(kIconSize * dpi_scale)));
  const int tier = PickIconSizeTier(icon_draw_size_);
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
      GetOrLoadCachedIcon(mode_name, tier, nullptr);

  const std::string trad_name = absl::StrCat(
      use_traditional_kanji_ ? "toolbar_kyu_" : "toolbar_shin_", theme);
  icon_cache_[static_cast<int>(ButtonId::kTraditionalKanji)] =
      GetOrLoadCachedIcon(trad_name, tier, nullptr);

  icon_cache_[static_cast<int>(ButtonId::kSymbols)] = GetOrLoadCachedIcon(
      absl::StrCat("toolbar_symbols_", theme), tier, nullptr);
  icon_cache_[static_cast<int>(ButtonId::kDictionary)] = GetOrLoadCachedIcon(
      absl::StrCat("toolbar_dict_", theme), tier, nullptr);
  icon_cache_[static_cast<int>(ButtonId::kSettings)] = GetOrLoadCachedIcon(
      absl::StrCat("toolbar_settings_", theme), tier, nullptr);
  icon_cache_[static_cast<int>(ButtonId::kShortcuts)] = GetOrLoadCachedIcon(
      absl::StrCat("toolbar_shortcuts_", theme), tier, nullptr);

  CSize logo_natural(0, 0);
  logo_cache_ = GetOrLoadCachedIcon(absl::StrCat("logo_long_", theme), tier,
                                    &logo_natural);
  if (logo_natural.cx > 0 && logo_natural.cy > 0) {
    // Fit the logo to the icon height, preserving its aspect ratio -- the
    // same rule mac's loadLogoSvg: applies.
    logo_draw_size_ = CSize(
        std::max(1, static_cast<int>(std::lround(
                        static_cast<double>(logo_natural.cx) * icon_draw_size_ /
                        logo_natural.cy))),
        icon_draw_size_);
  } else {
    logo_draw_size_ = CSize(0, 0);
  }
}

HBITMAP ToolbarWindow::GetOrLoadCachedIcon(const std::string& name, int size,
                                           CSize* out_size) {
  const std::string path = ToolbarIconPath(name, size);
  const auto it = icon_disk_cache_.find(path);
  if (it != icon_disk_cache_.end()) {
    if (out_size != nullptr) {
      *out_size = it->second.second;
    }
    return it->second.first.get();
  }
  CSize loaded_size(0, 0);
  wil::unique_hbitmap bitmap = LoadToolbarIcon(name, size, &loaded_size);
  HBITMAP raw = bitmap.get();
  icon_disk_cache_.emplace(
      path, std::make_pair(std::move(bitmap), loaded_size));
  if (out_size != nullptr) {
    *out_size = loaded_size;
  }
  return raw;
}

int ToolbarWindow::LogoWidth(double scale) const {
  // The logo PNG keeps its natural aspect ratio, so its drawn width is
  // usually narrower than the logical 120px the mac toolbar reserves;
  // reserving the logical width would leave a large gap between the logo and
  // the mode button. Use the drawn width once the icons are loaded (the
  // logical width is only a pre-load fallback).
  if (logo_draw_size_.cx > 0) {
    return logo_draw_size_.cx;
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
  if (const absl::StatusOr<dictionary::DocketData> docket_data =
          dictionary::ReadDocketDataUnlocked();
      docket_data.ok()) {
    docket_pending_count_ = static_cast<int>(docket_data->pending.size());
  }

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
  // corners (matches mac's solid, non-vibrancy chrome). Only the corner
  // squares need the per-pixel coverage test; everything between them is
  // fully covered, so the interior is filled with a plain memset-style loop.
  const uint8_t bg_b = is_dark_theme_ ? 40 : 255;
  const uint8_t bg_g = is_dark_theme_ ? 35 : 255;
  const uint8_t bg_r = is_dark_theme_ ? 32 : 255;
  const int corner_span =
      std::min<int>(static_cast<int>(std::ceil(corner_radius)), height);
  for (int y = 0; y < height; ++y) {
    const bool corner_row = (y < corner_span) || (y >= height - corner_span);
    for (int x = 0; x < width; ++x) {
      const bool corner_col = (x < corner_span) || (x >= width - corner_span);
      const double coverage =
          (corner_row && corner_col)
              ? RoundedRectCoverage(x + 0.5, y + 0.5, width, height,
                                    corner_radius)
              : 1.0;
      uint8_t* p = bits + (y * width + x) * 4;
      p[0] = static_cast<uint8_t>(bg_b * coverage);
      p[1] = static_cast<uint8_t>(bg_g * coverage);
      p[2] = static_cast<uint8_t>(bg_r * coverage);
      p[3] = static_cast<uint8_t>(std::lround(255.0 * coverage));
    }
  }

  int x_cursor = margin;
  int icon_w = 0;
  int icon_h = 0;
  uint8_t* logo_bits = GetDibBits(logo_cache_, &icon_w, &icon_h);
  if (logo_bits != nullptr && logo_draw_size_.cx > 0) {
    const int y = (height - logo_draw_size_.cy) / 2;
    BlendIcon(bits, width, height, logo_bits, icon_w, icon_h, x_cursor, y,
              logo_draw_size_.cx, logo_draw_size_.cy, 1.0);
  }
  x_cursor += logo_w + margin;

  const int highlight_inset =
      std::max(1, static_cast<int>(std::lround(kButtonHighlightInsetLogical * scale)));
  const double highlight_radius = kButtonHighlightRadiusLogical * scale;

  for (int i = 0; i < kButtonCount; ++i) {
    const CRect rect(x_cursor, 0, x_cursor + button_w, height);
    button_rects_[i] = rect;
    has_layout_ = true;

    const auto id = static_cast<ButtonId>(i);

    // Hover / pressed feedback. Without this the six icons look like
    // decoration rather than controls; mac gets the equivalent for free from
    // NSButton. |pressed_button_| only counts while the cursor is still on
    // the button it went down on, matching the click semantics below.
    const bool is_pressed = (pressed_button_ == i && hovered_button_ == i);
    const bool is_hovered = (hovered_button_ == i);
    // The Symbols and Shortcuts buttons toggle a window, so show them held
    // down while that window is up.
    const bool is_toggled_on =
        (id == ButtonId::kSymbols && symbols_palette_visible_) ||
        (id == ButtonId::kShortcuts && shortcuts_window_visible_);
    uint8_t highlight_alpha = 0;
    if (is_pressed || is_toggled_on) {
      highlight_alpha = is_dark_theme_ ? 51 : 41;  // 20% / 16%
    } else if (is_hovered) {
      highlight_alpha = is_dark_theme_ ? 31 : 20;  // 12% / 8%
    }
    if (highlight_alpha > 0) {
      CRect highlight = rect;
      highlight.DeflateRect(highlight_inset, highlight_inset);
      const uint8_t tint = is_dark_theme_ ? 255 : 0;
      FillRoundedRect(bits, width, height, highlight, highlight_radius, tint,
                      tint, tint, highlight_alpha);
    }

    uint8_t* icon_bits = GetDibBits(icon_cache_[i], &icon_w, &icon_h);
    if (icon_bits != nullptr) {
      const int icon_x = x_cursor + (button_w - icon_draw_size_) / 2;
      const int icon_y = (height - icon_draw_size_) / 2;
      BlendIcon(bits, width, height, icon_bits, icon_w, icon_h, icon_x, icon_y,
                icon_draw_size_, icon_draw_size_, 1.0);

      // Docket badge: a small solid dot in the icon's top-right corner
      // when there's anything awaiting review. Reuses FillRoundedRect
      // (already alpha-correct for this same layered-window composite,
      // see the hover highlight above) rather than drawing a digit count,
      // since GDI text drawn into this raw premultiplied-alpha buffer
      // would need its own alpha-channel bookkeeping to composite
      // correctly.
      if (id == ButtonId::kDictionary && docket_pending_count_ > 0) {
        const int dot_d = std::max(1, static_cast<int>(std::lround(6 * scale)));
        const CRect dot_rect(icon_x + icon_draw_size_ - dot_d, icon_y,
                             icon_x + icon_draw_size_, icon_y + dot_d);
        FillRoundedRect(bits, width, height, dot_rect, dot_d / 2.0, 220, 60,
                        40, 255);
      }
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

  UpdateTooltips();
}

int ToolbarWindow::HitTestButton(const CPoint& point) const {
  if (!has_layout_) {
    return -1;
  }
  for (size_t i = 0; i < button_rects_.size(); ++i) {
    if (button_rects_[i].PtInRect(point)) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

bool ToolbarWindow::GetButtonScreenRect(ButtonId button, CRect* out_rect) const {
  const size_t index = static_cast<size_t>(button);
  if (out_rect == nullptr || !has_layout_ || index >= button_rects_.size()) {
    return false;
  }
  CRect rect = button_rects_[index];
  rect.OffsetRect(window_origin_);
  *out_rect = rect;
  return true;
}

// static
const wchar_t* ToolbarWindow::GetButtonName(ButtonId button) {
  switch (button) {
    case ButtonId::kMode:
      return MarinaLocalizedString(L"MM.InputMode");
    case ButtonId::kTraditionalKanji:
      return MarinaLocalizedString(L"MM.TraditionalKanji");
    case ButtonId::kSymbols:
      return MarinaLocalizedString(L"MM.SymbolsPalette");
    case ButtonId::kDictionary:
      return MarinaLocalizedString(L"MM.Docket");
    case ButtonId::kSettings:
      return MarinaLocalizedString(L"MM.Settings");
    case ButtonId::kShortcuts:
      return MarinaLocalizedString(L"MM.KeyboardShortcuts");
    default:
      return L"";
  }
}

void ToolbarWindow::UpdateTooltips() {
  if (!has_layout_) {
    return;
  }
  if (tooltip_window_ == nullptr) {
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_BAR_CLASSES};
    ::InitCommonControlsEx(&icc);
    // TTF_TRANSPARENT keeps the tip from swallowing clicks meant for the
    // button underneath; the toolbar is WS_EX_NOACTIVATE so the tooltip must
    // not try to activate anything either.
    tooltip_window_ = ::CreateWindowExW(
        WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
        WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT, m_hWnd, nullptr, nullptr, nullptr);
    if (tooltip_window_ == nullptr) {
      return;
    }
    tooltip_layout_rect_ = button_rects_[0];
    for (int i = 0; i < kButtonCount; ++i) {
      TTTOOLINFOW tool = {};
      tool.cbSize = sizeof(tool);
      tool.uFlags = TTF_SUBCLASS | TTF_TRANSPARENT;
      tool.hwnd = m_hWnd;
      tool.uId = static_cast<UINT_PTR>(i);
      tool.rect = button_rects_[i];
      tool.lpszText =
          const_cast<wchar_t*>(GetButtonName(static_cast<ButtonId>(i)));
      ::SendMessageW(tooltip_window_, TTM_ADDTOOLW, 0,
                     reinterpret_cast<LPARAM>(&tool));
    }
    return;
  }

  // Redraw() runs on every hover transition, but the rects only move when the
  // layout does (a DPI change). Re-pushing them each time would be pointless
  // work in the middle of the tooltip's own hover timing.
  if (button_rects_[0] == tooltip_layout_rect_) {
    return;
  }
  tooltip_layout_rect_ = button_rects_[0];
  for (int i = 0; i < kButtonCount; ++i) {
    TTTOOLINFOW tool = {};
    tool.cbSize = sizeof(tool);
    tool.hwnd = m_hWnd;
    tool.uId = static_cast<UINT_PTR>(i);
    tool.rect = button_rects_[i];
    ::SendMessageW(tooltip_window_, TTM_NEWTOOLRECTW, 0,
                   reinterpret_cast<LPARAM>(&tool));
  }
}

void ToolbarWindow::OnLButtonDown(UINT flags, CPoint point) {
  const int button = HitTestButton(point);
  if (button == pressed_button_) {
    return;
  }
  pressed_button_ = button;
  hovered_button_ = button;
  if (button >= 0) {
    Redraw();
  }
}

void ToolbarWindow::OnLButtonUp(UINT flags, CPoint point) {
  const int button = HitTestButton(point);
  const int pressed = pressed_button_;
  pressed_button_ = -1;
  if (pressed >= 0) {
    Redraw();
  }
  if (button < 0 || button != pressed) {
    return;
  }
  ActivateButton(static_cast<ButtonId>(button));
}

void ToolbarWindow::ActivateButton(ButtonId button) {
  switch (button) {
    case ButtonId::kMode:
      ShowModeMenu();
      break;
    case ButtonId::kTraditionalKanji:
      SendToggleTraditionalKanji();
      break;
    case ButtonId::kDictionary:
      SendLaunchDocketDialog();
      break;
    case ButtonId::kSettings:
      SendLaunchConfigDialog();
      break;
    case ButtonId::kSymbols:
      SendToggleSymbolsPalette();
      break;
    case ButtonId::kShortcuts:
      SendToggleShortcutsWindow();
      break;
    default:
      break;
  }
}

void ToolbarWindow::OnRButtonUp(UINT flags, CPoint point) {
  ShowContextMenu(point);
}

void ToolbarWindow::OnNcRButtonUp(UINT hit_test, CPoint screen_point) {
  // WM_NCHITTEST reports the draggable background as HTCAPTION, so a
  // right-click there is delivered here instead of to OnRButtonUp.
  CPoint client_point = screen_point;
  ScreenToClient(&client_point);
  ShowContextMenu(client_point);
}

void ToolbarWindow::OnMouseMove(UINT flags, CPoint point) {
  if (!tracking_mouse_leave_) {
    TRACKMOUSEEVENT track = {};
    track.cbSize = sizeof(track);
    track.dwFlags = TME_LEAVE;
    track.hwndTrack = m_hWnd;
    if (::TrackMouseEvent(&track)) {
      tracking_mouse_leave_ = true;
    }
  }
  const int button = HitTestButton(point);
  if (button == hovered_button_) {
    return;
  }
  hovered_button_ = button;
  Redraw();
}

void ToolbarWindow::OnMouseLeave() {
  tracking_mouse_leave_ = false;
  if (hovered_button_ < 0 && pressed_button_ < 0) {
    return;
  }
  hovered_button_ = -1;
  pressed_button_ = -1;
  Redraw();
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
  // A right-click on this area arrives as WM_NCRBUTTONUP rather than
  // WM_RBUTTONUP because of it; both are handled.
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
  // A drag can land on a monitor with a different scale factor. WM_DPICHANGED
  // covers that for most windows, but this one is only ever positioned by
  // UpdateLayeredWindow, so re-check explicitly rather than relying on it.
  SyncDpiForCurrentMonitor();
}

void ToolbarWindow::OnSettingChange(UINT flags, LPCTSTR section) {
  const bool dark = IsDarkTheme();
  if (dark != is_dark_theme_) {
    is_dark_theme_ = dark;
    LoadIcons();
    Redraw();
  }
}

void ToolbarWindow::OnDisplayChange() { SyncDpiForCurrentMonitor(); }

void ToolbarWindow::OnDpiChanged(uint32_t dpi, const RECT* suggested_rect) {
  if (dpi == 0 || dpi == dpi_) {
    return;
  }
  dpi_ = dpi;
  // Honour the origin Windows suggests for the new scale factor so the
  // toolbar keeps its relative position on the monitor it moved to; the size
  // is recomputed from the new DPI rather than taken from the suggestion.
  if (suggested_rect != nullptr) {
    window_origin_ = CPoint(suggested_rect->left, suggested_rect->top);
  }
  LoadIcons();
  window_origin_ = ClampToVisibleArea(window_origin_, ComputeWindowSize());
  Redraw();
  SavePosition();
}

bool ToolbarWindow::SyncDpiForCurrentMonitor() {
  CRect window_rect;
  GetWindowRect(&window_rect);
  const uint32_t new_dpi = GetDpiForPoint(window_rect.left, window_rect.top);
  if (new_dpi == dpi_) {
    return false;
  }
  dpi_ = new_dpi;
  LoadIcons();
  Redraw();
  return true;
}

LRESULT ToolbarWindow::HandleGetObject(WPARAM wparam, LPARAM lparam,
                                       BOOL& handled) {
  if (static_cast<DWORD>(lparam) != static_cast<DWORD>(OBJID_CLIENT)) {
    handled = FALSE;
    return 0;
  }
  if (accessible_ == nullptr) {
    accessible_ = CreateToolbarAccessible(this, m_hWnd);
  }
  if (accessible_ == nullptr) {
    handled = FALSE;
    return 0;
  }
  handled = TRUE;
  return ::LresultFromObject(IID_IAccessible, wparam,
                             static_cast<IAccessible*>(accessible_));
}

int ToolbarWindow::TrackMenu(HMENU menu, const CPoint& screen_point) {
  // Deliberately owned by this WS_EX_NOACTIVATE window, and deliberately
  // *not* preceded by SetForegroundWindow. The renderer owns the toolbar, but
  // the callback is handled by the TIP attached to the application's focused
  // text context: promoting any window of ours to foreground makes that
  // context disappear before the selected command is delivered, which looks
  // like a successful menu click but has no effect. The documented cost of a
  // non-foreground owner (KB135788) is that an outside click may not dismiss
  // the menu; pressing Esc or choosing an item always does. Do not "fix" that
  // by foregrounding -- it trades a cosmetic annoyance for a silently broken
  // command path. |menu_open_| suppresses the focus-loss hide traffic that
  // TrackPopupMenuEx's modal loop can dispatch meanwhile.
  menu_open_ = true;
  const int selected = ::TrackPopupMenuEx(
      menu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_NONOTIFY |
                TPM_RIGHTBUTTON,
      screen_point.x, screen_point.y, m_hWnd, nullptr);
  ::PostMessage(m_hWnd, WM_NULL, 0, 0);
  menu_open_ = false;
  // The pointer is very unlikely to still be over the button that opened the
  // menu, and no WM_MOUSELEAVE arrives while the modal loop owns the mouse.
  if (hovered_button_ >= 0 || pressed_button_ >= 0) {
    hovered_button_ = -1;
    pressed_button_ = -1;
    if (IsWindowVisible()) {
      Redraw();
    }
  }
  if (hide_deferred_by_menu_) {
    Hide();
  }
  return selected;
}

void ToolbarWindow::ShowModeMenu() {
  wil::unique_hmenu menu(::CreatePopupMenu());
  if (!menu) {
    return;
  }
  struct ModeEntry {
    const wchar_t* label_key;
    commands::CompositionMode mode;
  };
  constexpr ModeEntry kEntries[] = {
      {L"MM.Hiragana", commands::HIRAGANA},
      // marinaMoji: the "Katakana" entry enters Manyōshū mode, which replaces
      // traditional full-width katakana.
      {L"MM.KatakanaManyoshu", commands::MANYOSHU},
      {L"MM.HalfWidthKatakana", commands::HALF_KATAKANA},
      {L"MM.FullWidthRoman", commands::FULL_ASCII},
      {L"MM.HalfWidthRoman", commands::HALF_ASCII},
      {L"MM.DirectInputMode", commands::DIRECT},
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
    ::AppendMenuW(menu.get(), flags, i + 1,
                  MarinaLocalizedString(kEntries[i].label_key));
  }

  CRect button_rect;
  if (!GetButtonScreenRect(ButtonId::kMode, &button_rect)) {
    return;
  }
  const CPoint screen_point(button_rect.left, button_rect.bottom);

  const int selected = TrackMenu(menu.get(), screen_point);
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

void ToolbarWindow::ShowContextMenu(const CPoint& client_point) {
  wil::unique_hmenu menu(::CreatePopupMenu());
  if (!menu) {
    return;
  }
  // The Dictionary Tool has no button of its own (mac reaches it from a
  // right-click on the dict button), and the only other way to turn the
  // toolbar off is the TSF language-bar menu, which is effectively hidden on
  // Windows 11 -- so both live here.
  ::AppendMenuW(menu.get(), MF_STRING, kContextMenuDictionaryTool,
                MarinaLocalizedString(L"MM.DictionaryTool"));
  ::AppendMenuW(menu.get(), MF_STRING, kContextMenuShortcuts,
                MarinaLocalizedString(L"MM.KeyboardShortcuts"));
  ::AppendMenuW(menu.get(), MF_STRING, kContextMenuSettings,
                MarinaLocalizedString(L"MM.Settings"));
  ::AppendMenuW(menu.get(), MF_SEPARATOR, 0, nullptr);
  ::AppendMenuW(menu.get(), MF_STRING, kContextMenuHideToolbar,
                MarinaLocalizedString(L"MM.HideToolbar"));

  CPoint screen_point = client_point;
  ClientToScreen(&screen_point);

  switch (TrackMenu(menu.get(), screen_point)) {
    case kContextMenuDictionaryTool:
      SendLaunchDictionaryTool();
      break;
    case kContextMenuShortcuts:
      SendToggleShortcutsWindow();
      break;
    case kContextMenuSettings:
      SendLaunchConfigDialog();
      break;
    case kContextMenuHideToolbar:
      SendHideToolbar();
      break;
    default:
      break;
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

void ToolbarWindow::SendLaunchDocketDialog() {
  if (send_command_interface_ == nullptr) {
    return;
  }
  commands::SessionCommand command;
  command.set_type(commands::SessionCommand::LAUNCH_DOCKET_DIALOG);
  commands::Output output;
  send_command_interface_->SendCommand(command, &output);
}

void ToolbarWindow::SendLaunchDictionaryTool() {
  if (send_command_interface_ == nullptr) {
    return;
  }
  commands::SessionCommand command;
  command.set_type(commands::SessionCommand::LAUNCH_DICTIONARY_TOOL);
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
  // marinaMoji TEMPORARY (2026-08-08): the OnUpdate line below reports only
  // what comes *back*, so a capture with no palette in it was ambiguous
  // between "never clicked" and "clicked, nothing happened". This is the
  // outbound half. Note the toggle is driven by |symbols_palette_visible_|,
  // which every OnUpdate overwrites from the TIP -- so if the flag gets
  // cleared behind our back, the user's second click sends SHOW again rather
  // than HIDE (or vice versa), which is worth seeing here too.
  {
    const std::string line = absl::StrCat(
        "[marinaMoji/toolbar] symbols button clicked -> sending ",
        show ? "SHOW" : "HIDE", "_SYMBOLS_PALETTE\n");
    ::OutputDebugStringA(line.c_str());
  }
  symbols_palette_visible_ = show;
  command.set_type(show ? commands::SessionCommand::SHOW_SYMBOLS_PALETTE
                        : commands::SessionCommand::HIDE_SYMBOLS_PALETTE);
  commands::Output output;
  send_command_interface_->SendCommand(command, &output);
}

void ToolbarWindow::SendToggleShortcutsWindow() {
  if (send_command_interface_ == nullptr) {
    return;
  }
  commands::SessionCommand command;
  const bool show = !shortcuts_window_visible_;
  shortcuts_window_visible_ = show;
  command.set_type(show ? commands::SessionCommand::SHOW_SHORTCUTS_WINDOW
                        : commands::SessionCommand::HIDE_SHORTCUTS_WINDOW);
  commands::Output output;
  send_command_interface_->SendCommand(command, &output);
}

void ToolbarWindow::SendHideToolbar() {
  if (send_command_interface_ == nullptr) {
    return;
  }
  // The preference is owned by the TIP (win32/base/toolbar_config.cc), which
  // caches it and gates the ShowToolbar bit on it -- writing toolbar.conf
  // from here would be invisible to that cache and race the renderer's own
  // position writes.
  commands::SessionCommand command;
  command.set_type(commands::SessionCommand::HIDE_TOOLBAR);
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
    *out_position = ClampToVisibleArea(CPoint(x, y), window_size);
  }
}

void ToolbarWindow::SavePosition() const {
  CRect window_rect;
  GetWindowRect(&window_rect);

  // toolbar.conf is shared with the TIP, which stores |toolbar_visible| in it
  // (win32/base/toolbar_config.cc) from a different process. Rewriting the
  // file with just x/y would drop that key, so preserve every line we don't
  // own and only replace our own two.
  std::vector<std::string> lines;
  const std::string path = ConfigFilePath();
  if (const auto contents = FileUtil::GetContents(path); contents.ok()) {
    const std::string& text = *contents;
    size_t pos = 0;
    while (pos < text.size()) {
      const size_t eol = text.find('\n', pos);
      std::string line = text.substr(
          pos, eol == std::string::npos ? std::string::npos : eol - pos);
      if (!line.empty() && line.back() == '\r') {
        line.pop_back();
      }
      if (!line.empty()) {
        lines.push_back(std::move(line));
      }
      if (eol == std::string::npos) {
        break;
      }
      pos = eol + 1;
    }
  }

  bool replaced_x = false;
  bool replaced_y = false;
  for (std::string& line : lines) {
    const size_t eq = line.find('=');
    if (eq == std::string::npos) {
      continue;
    }
    const std::string key = line.substr(0, eq);
    if (key == "x") {
      line = absl::StrCat("x=", window_rect.left);
      replaced_x = true;
    } else if (key == "y") {
      line = absl::StrCat("y=", window_rect.top);
      replaced_y = true;
    }
  }
  if (!replaced_x) {
    lines.push_back(absl::StrCat("x=", window_rect.left));
  }
  if (!replaced_y) {
    lines.push_back(absl::StrCat("y=", window_rect.top));
  }

  std::string content;
  for (const std::string& line : lines) {
    absl::StrAppend(&content, line, "\n");
  }
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

CPoint ToolbarWindow::ClampToVisibleArea(const CPoint& origin,
                                         const CSize& window_size) const {
  // A position saved while a since-removed monitor was attached would leave
  // the toolbar somewhere the user can neither see nor drag it back from, and
  // there is no other UI for resetting it. Treat it as usable only if a
  // decent grabbable strip of it lands inside some monitor's work area.
  const int min_visible_x = std::min<int>(window_size.cx, 48);
  const int min_visible_y = std::min<int>(window_size.cy, 16);
  const RECT window_rect = {origin.x, origin.y, origin.x + window_size.cx,
                            origin.y + window_size.cy};
  const HMONITOR monitor =
      ::MonitorFromRect(&window_rect, MONITOR_DEFAULTTONULL);
  if (monitor != nullptr) {
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (::GetMonitorInfoW(monitor, &info)) {
      const int visible_x =
          std::min<int>(window_rect.right, info.rcWork.right) -
          std::max<int>(window_rect.left, info.rcWork.left);
      const int visible_y =
          std::min<int>(window_rect.bottom, info.rcWork.bottom) -
          std::max<int>(window_rect.top, info.rcWork.top);
      if (visible_x >= min_visible_x && visible_y >= min_visible_y) {
        return origin;
      }
    }
  }
  return DefaultWindowOrigin(window_size);
}

}  // namespace win32
}  // namespace renderer
}  // namespace mozc
