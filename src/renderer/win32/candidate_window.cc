// Copyright 2010-2021, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "renderer/win32/candidate_window.h"

#include <atlbase.h>
#include <atltypes.h>
#include <atlwin.h>
#include <wil/resource.h>
#include <windows.h>

#include <gdiplus.h>  // NOLINT

#include <algorithm>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "base/coordinates.h"
#include "base/specimen_kind.h"
#include "base/win32/wide_char.h"
#include "client/client_interface.h"
#include "composer_ids/ids_preview.h"
#include "gui/character_composer/composer_kage.h"
#include "protocol/candidate_window.pb.h"
#include "protocol/renderer_command.pb.h"
#include "renderer/candidate_kage.h"
#include "renderer/candidate_window_util.h"
#include "renderer/renderer_style_handler.h"
#include "renderer/table_layout.h"
#include "renderer/win32/resource.h"
#include "renderer/win32/text_renderer.h"
#include "renderer/win32/win32_dpi_util.h"

namespace mozc {
namespace renderer {
namespace win32 {
namespace {

// layout size constants in pixel unit in the default DPI.
constexpr int kIndicatorWidthInDefaultDPI = 4;
// Gaps around the footer logo, matching the macOS and Linux candidate windows
// (renderer/mac/CandidateView.mm, renderer/qt/qt_window_manager.cc).
constexpr int kFooterLogoLeftMarginInDefaultDPI = 4;
constexpr int kFooterLogoRightMarginInDefaultDPI = 8;

// usage type for each column.
enum COLUMN_TYPE {
  COLUMN_SHORTCUT = 0,  // show shortcut key
  COLUMN_GAP1,          // padding region
  COLUMN_CANDIDATE,     // show candidate string
  COLUMN_GAP2,          // padding region
  COLUMN_DESCRIPTION,   // show description message
  NUMBER_OF_COLUMNS,    // number of columns. (this item should be last)
};

COLORREF ToColorRef(const RendererStyle::RGBAColor& color,
                    COLORREF fallback) {
  if (!color.has_r() || !color.has_g() || !color.has_b()) {
    return fallback;
  }
  return RGB(static_cast<BYTE>(std::clamp(color.r(), 0.0, 255.0)),
             static_cast<BYTE>(std::clamp(color.g(), 0.0, 255.0)),
             static_cast<BYTE>(std::clamp(color.b(), 0.0, 255.0)));
}

COLORREF ToOpaqueColorRef(const RendererStyle::RGBAColor& color,
                          COLORREF background, COLORREF fallback) {
  if (!color.has_r() || !color.has_g() || !color.has_b()) {
    return fallback;
  }
  const double alpha = std::clamp(color.a(), 0.0, 1.0);
  const auto blend = [alpha](BYTE foreground, BYTE background) {
    return static_cast<BYTE>(foreground * alpha + background * (1.0 - alpha));
  };
  return RGB(blend(static_cast<BYTE>(std::clamp(color.r(), 0.0, 255.0)),
                   GetRValue(background)),
             blend(static_cast<BYTE>(std::clamp(color.g(), 0.0, 255.0)),
                   GetGValue(background)),
             blend(static_cast<BYTE>(std::clamp(color.b(), 0.0, 255.0)),
                   GetBValue(background)));
}

// ------------------------------------------------------------------------
// Utility functions
// ------------------------------------------------------------------------
CRect ToCRect(const Rect& rect) {
  return CRect(rect.Left(), rect.Top(), rect.Right(), rect.Bottom());
}

// Returns the smallest index of the given candidate list which satisfies
// candidates.candidate(i) == |candidate_index|.
// This function returns the size of the given candidate list when there
// aren't any candidates satisfying the above condition.
int GetCandidateArrayIndexByCandidateIndex(
    const commands::CandidateWindow& candidate_window, int candidate_index) {
  for (size_t i = 0; i < candidate_window.candidate_size(); ++i) {
    const commands::CandidateWindow::Candidate& candidate =
        candidate_window.candidate(i);

    if (candidate.index() == candidate_index) {
      return i;
    }
  }

  return candidate_window.candidate_size();
}

// Returns a text which includes the selected index number and
// the number of the candidates. For example, "13/123" means
// the selected index is "13" (in 1-origin) and the number of
// candidates is "123"
// Returns an empty string if index string should not be displayed.
std::string GetIndexGuideString(
    const commands::CandidateWindow& candidate_window) {
  if (!candidate_window.has_footer() ||
      !candidate_window.footer().index_visible()) {
    return "";
  }

  const int focused_index = candidate_window.focused_index();
  const int total_items = candidate_window.size();

  std::stringstream footer_string;
  footer_string << focused_index + 1 << "/" << total_items
                << " ";  // for padding.

  return footer_string.str();
}

// Returns the smallest index of the given candidate list which satisfies
// |candidates.focused_index| == |candidates.candidate(i).index()|.
// This function returns the size of the given candidate list when there
// aren't any candidates satisfying the above condition.
int GetFocusedArrayIndex(const commands::CandidateWindow& candidate_window) {
  const int invalid_index = candidate_window.candidate_size();

  if (!candidate_window.has_focused_index()) {
    return invalid_index;
  }

  const int focused_index = candidate_window.focused_index();

  return GetCandidateArrayIndexByCandidateIndex(candidate_window,
                                                focused_index);
}

// Retrieves the display string from the specified candidate for the specified
// column and returns it.
std::wstring GetDisplayStringByColumn(
    const commands::CandidateWindow::Candidate& candidate,
    COLUMN_TYPE column_type) {
  std::wstring display_string;

  switch (column_type) {
    case COLUMN_SHORTCUT:
      if (candidate.has_annotation()) {
        const commands::Annotation& annotation = candidate.annotation();
        if (annotation.has_shortcut()) {
          display_string = mozc::win32::Utf8ToWide(annotation.shortcut());
        }
      }
      break;
    case COLUMN_CANDIDATE:
      if (candidate.has_value()) {
        display_string = mozc::win32::Utf8ToWide(candidate.value());
      }
      if (candidate.has_annotation()) {
        const commands::Annotation& annotation = candidate.annotation();
        if (annotation.has_prefix()) {
          const std::wstring annotation_prefix =
              mozc::win32::Utf8ToWide(annotation.prefix());
          display_string = annotation_prefix + display_string;
        }
        if (annotation.has_suffix()) {
          const std::wstring annotation_suffix =
              mozc::win32::Utf8ToWide(annotation.suffix());
          display_string += annotation_suffix;
        }
      }
      break;
    case COLUMN_DESCRIPTION:
      if (candidate.has_annotation()) {
        const commands::Annotation& annotation = candidate.annotation();
        if (annotation.has_description()) {
          display_string = mozc::win32::Utf8ToWide(annotation.description());
        }
      }
      break;
    default:
      LOG(ERROR) << "Unknown column type: " << column_type;
      break;
  }

  return display_string;
}

// Loads a DIB from a Win32 resource in the specified module and returns its
// handle.  This function will fail if you try to load a top-down bitmap in
// Windows XP.
// Returns nullptr if failed to load the image.
// Caller must delete the object if this function returns non-null value.
HBITMAP LoadBitmapFromResource(HMODULE module, int resource_id) {
  // We can use LR_CREATEDIBSECTION to load a 32-bpp bitmap.
  // You cannot load a a top-down DIB with LoadImage in Windows XP.
  // http://b/2076264
  return reinterpret_cast<HBITMAP>(
      ::LoadImage(module, MAKEINTRESOURCE(resource_id), IMAGE_BITMAP, 0, 0,
                  LR_CREATEDIBSECTION));
}

void FillSolidRect(HDC dc, const RECT* rect, COLORREF color) {
  COLORREF old_color = ::SetBkColor(dc, color);
  if (old_color != CLR_INVALID) {
    ::ExtTextOut(dc, 0, 0, ETO_OPAQUE, rect, nullptr, 0, nullptr);
    ::SetBkColor(dc, old_color);
  }
}

void EnsureGdiplus() {
  static std::once_flag once;
  std::call_once(once, [] {
    static ULONG_PTR token = 0;
    Gdiplus::GdiplusStartupInput input;
    Gdiplus::GdiplusStartup(&token, &input, nullptr);
  });
}

// True when the value column paints a GlyphWiki/KAGE outline or IDS preview
// instead of the candidate text (Ctrl+2 SVG row, or amber tofu with strokes).
bool CandidateUsesDrawnValue(
    const commands::CandidateWindow::Candidate& candidate,
    bool glyph_drawable) {
  const specimen_kind::Kind kind =
      SpecimenKindForCandidate(candidate, glyph_drawable);
  if (kind == specimen_kind::Kind::kSvg) {
    return HasKageOutlineForKey(candidate.value()) ||
           composer_ids::ParseIdsPreview(candidate.value()).has_value();
  }
  if (kind == specimen_kind::Kind::kMissingFont) {
    return HasKageOutlineForKey(candidate.value());
  }
  return false;
}

bool PaintKageOutlines(HDC dc, const Rect& cell, absl::string_view key,
                       COLORREF ink) {
  const auto* outlines = CachedKageOutlines(key);
  if (outlines == nullptr || !outlines->has_value() || (*outlines)->empty()) {
    return false;
  }
  EnsureGdiplus();
  constexpr double kViewBox = 1000.0;
  Gdiplus::Graphics graphics(dc);
  graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  // Contours are defined for the non-zero fill rule (composer_kage.h).
  Gdiplus::GraphicsPath path(Gdiplus::FillModeWinding);
  for (const gui::Contour& contour : **outlines) {
    if (contour.size() < 2) {
      continue;
    }
    path.StartFigure();
    for (size_t i = 1; i < contour.size(); ++i) {
      path.AddLine(static_cast<Gdiplus::REAL>(contour[i - 1].first),
                   static_cast<Gdiplus::REAL>(contour[i - 1].second),
                   static_cast<Gdiplus::REAL>(contour[i].first),
                   static_cast<Gdiplus::REAL>(contour[i].second));
    }
    path.CloseFigure();
  }
  const double side =
      std::min(static_cast<double>(cell.size.width),
               static_cast<double>(cell.size.height));
  if (side <= 0) {
    return false;
  }
  Gdiplus::Matrix transform;
  transform.Translate(
      static_cast<Gdiplus::REAL>(cell.origin.x),
      static_cast<Gdiplus::REAL>(cell.origin.y + (cell.size.height - side) / 2.0));
  transform.Scale(static_cast<Gdiplus::REAL>(side / kViewBox),
                  static_cast<Gdiplus::REAL>(side / kViewBox));
  graphics.SetTransform(&transform);
  Gdiplus::SolidBrush brush(Gdiplus::Color(255, GetRValue(ink), GetGValue(ink),
                                           GetBValue(ink)));
  graphics.FillPath(&brush, &path);
  return true;
}

void PaintIdsPreview(HDC dc, const Rect& square,
                     const composer_ids::IdsPreview& preview, COLORREF ink) {
  EnsureGdiplus();
  Gdiplus::Graphics graphics(dc);
  graphics.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);
  graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  const double side = square.size.width;
  Gdiplus::FontFamily family(L"Yu Mincho");
  Gdiplus::Font font(&family, static_cast<Gdiplus::REAL>(side * 0.88),
                     Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
  Gdiplus::SolidBrush brush(Gdiplus::Color(255, GetRValue(ink), GetGValue(ink),
                                           GetBValue(ink)));
  for (size_t i = 0; i < preview.parts.size(); ++i) {
    const composer_ids::PreviewCell& cell = preview.cells[i];
    const std::wstring glyph = mozc::win32::Utf8ToWide(preview.parts[i]);
    if (glyph.empty()) {
      continue;
    }
    Gdiplus::GraphicsState state = graphics.Save();
    graphics.TranslateTransform(
        static_cast<Gdiplus::REAL>(square.origin.x + cell.x * side),
        static_cast<Gdiplus::REAL>(square.origin.y + cell.y * side));
    graphics.ScaleTransform(static_cast<Gdiplus::REAL>(cell.w),
                            static_cast<Gdiplus::REAL>(cell.h));
    Gdiplus::RectF layout(0, 0, static_cast<Gdiplus::REAL>(side),
                          static_cast<Gdiplus::REAL>(side));
    Gdiplus::StringFormat format;
    format.SetAlignment(Gdiplus::StringAlignmentCenter);
    format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    graphics.DrawString(glyph.c_str(), static_cast<INT>(glyph.size()), &font,
                        layout, &format, &brush);
    graphics.Restore(state);
  }
}

}  // namespace

// ------------------------------------------------------------------------
// CandidateWindow
// ------------------------------------------------------------------------

CandidateWindow::CandidateWindow()
    : candidate_window_(std::make_unique<commands::CandidateWindow>()),
      footer_logo_display_size_(0, 0),
      send_command_interface_(nullptr),
      table_layout_(std::make_unique<TableLayout>()),
      style_(),
      dpi_(::GetDpiForSystem()),
      text_renderer_(TextRenderer::Create(dpi_)),
      indicator_width_(0),
      metrics_changed_(false),
      mouse_moving_(true) {
  UpdateDpiDependentResources();
}

CandidateWindow::~CandidateWindow() = default;

void CandidateWindow::UpdateDpiDependentResources() {
  style_ = GetScaledRendererStyle(dpi_);
  const double scale_factor = GetDPIScalingFactor(dpi_);
  double image_scale_factor = 1.0;
  if (scale_factor < 1.125) {
    footer_logo_.reset(LoadBitmapFromResource(::GetModuleHandle(nullptr),
                                              IDB_FOOTER_LOGO_COLOR_100));
    image_scale_factor = 1.0;
  } else if (scale_factor < 1.375) {
    footer_logo_.reset(LoadBitmapFromResource(::GetModuleHandle(nullptr),
                                              IDB_FOOTER_LOGO_COLOR_125));
    image_scale_factor = 1.25;
  } else if (scale_factor < 1.75) {
    footer_logo_.reset(LoadBitmapFromResource(::GetModuleHandle(nullptr),
                                              IDB_FOOTER_LOGO_COLOR_150));
    image_scale_factor = 1.5;
  } else {
    footer_logo_.reset(LoadBitmapFromResource(::GetModuleHandle(nullptr),
                                              IDB_FOOTER_LOGO_COLOR_200));
    image_scale_factor = 2.0;
  }

  footer_logo_display_size_ = Size(0, 0);
  if (footer_logo_.is_valid()) {
    BITMAP bm = {};
    if (::GetObject(footer_logo_.get(), sizeof(bm), &bm)) {
      footer_logo_display_size_ =
          Size(bm.bmWidth * (scale_factor / image_scale_factor),
               bm.bmHeight * (scale_factor / image_scale_factor));
    }
  }

  indicator_width_ = kIndicatorWidthInDefaultDPI * scale_factor;
}

LRESULT CandidateWindow::OnCreate(LPCREATESTRUCT create_struct) {
  EnableOrDisableWindowForWorkaround();
  return 0;
}

void CandidateWindow::UpdateDpi(uint32_t dpi) {
  if (dpi == dpi_) {
    return;
  }
  dpi_ = dpi;
  UpdateDpiDependentResources();
  text_renderer_->OnDpiChanged(dpi_);
}

void CandidateWindow::UpdateWindowRegion() {
  if (!::IsWindow(m_hWnd) || !table_layout_->IsLayoutFrozen()) {
    return;
  }
  const Size size = table_layout_->GetTotalSize();
  const int radius = std::max(
      0, static_cast<int>(style_.corner_radius() * GetDPIScalingFactor(dpi_)));
  HRGN region = ::CreateRoundRectRgn(0, 0, size.width + 1, size.height + 1,
                                     radius * 2, radius * 2);
  if (region != nullptr) {
    ::SetWindowRgn(m_hWnd, region, TRUE);
  }
}

void CandidateWindow::EnableOrDisableWindowForWorkaround() {
  // Disable the window if SPI_GETACTIVEWINDOWTRACKING is enabled.
  // See b/2317702 for details.
  // TODO(yukawa): Support mouse operations before we add a GUI feature which
  //   requires UI interaction by mouse and/or touch. (b/2954874)
  BOOL is_tracking_enabled = FALSE;
  if (::SystemParametersInfo(SPI_GETACTIVEWINDOWTRACKING, 0,
                             &is_tracking_enabled, 0)) {
    EnableWindow(!is_tracking_enabled);
  }
}

void CandidateWindow::OnDestroy() {
  // PostQuitMessage may stop the message loop even though other
  // windows are not closed. WindowManager should close these windows
  // before process termination.
  ::PostQuitMessage(0);
}

BOOL CandidateWindow::OnEraseBkgnd(HDC dc) {
  // We do not have to erase background
  // because all pixels in client area will be drawn in the DoPaint method.
  return TRUE;
}

void CandidateWindow::OnGetMinMaxInfo(MINMAXINFO* min_max_info) {
  // Do not restrict the window size in case the candidate window must be
  // very small size.
  min_max_info->ptMinTrackSize.x = 1;
  min_max_info->ptMinTrackSize.y = 1;
  SetMsgHandled(TRUE);
}

void CandidateWindow::HandleMouseEvent(UINT nFlags, const CPoint& point,
                                       bool close_candidatewindow) {
  if (send_command_interface_ == nullptr) {
    LOG(ERROR) << "send_command_interface_ is nullptr";
    return;
  }

  (void)GetFocusedArrayIndex(*candidate_window_);

  for (size_t i = 0; i < candidate_window_->candidate_size(); ++i) {
    const commands::CandidateWindow::Candidate& candidate =
        candidate_window_->candidate(i);

    const CRect rect = ToCRect(table_layout_->GetRowRect(i));
    if (rect.PtInRect(point)) {
      commands::SessionCommand command;
      if (close_candidatewindow) {
        command.set_type(commands::SessionCommand::SELECT_CANDIDATE);
      } else {
        command.set_type(commands::SessionCommand::HIGHLIGHT_CANDIDATE);
      }
      command.set_id(candidate.id());
      commands::Output output;
      send_command_interface_->SendCommand(command, &output);
      return;
    }
  }
}

void CandidateWindow::OnLButtonDown(UINT nFlags, CPoint point) {
  HandleMouseEvent(nFlags, point, false);
}

void CandidateWindow::OnLButtonUp(UINT nFlags, CPoint point) {
  HandleMouseEvent(nFlags, point, true);
}

void CandidateWindow::OnMouseMove(UINT nFlags, CPoint point) {
  // Window manager sometimes generates WM_MOUSEMOVE message when the contents
  // under the mouse cursor has been changed (e.g. the window is moved) so that
  // the mouse handler can change its cursor image based on the contents to
  // which the cursor is newly pointing.  In order to filter these pseudo
  // WM_MOUSEMOVE out, |mouse_moving_| is checked here.
  // See http://blogs.msdn.com/b/oldnewthing/archive/2003/10/01/55108.aspx for
  // details about such an artificial WM_MOUSEMOVE.  See also b/3104996.
  if (!mouse_moving_) {
    return;
  }
  if ((nFlags & MK_LBUTTON) != MK_LBUTTON) {
    return;
  }

  HandleMouseEvent(nFlags, point, false);
}

void CandidateWindow::OnPaint(HDC dc) {
  CRect client_rect;
  this->GetClientRect(&client_rect);

  wil::unique_hdc_paint paint_dc;
  if (dc == nullptr) {
    paint_dc = wil::BeginPaint(this->m_hWnd);
  }
  HDC target_dc = paint_dc.is_valid() ? paint_dc.get() : dc;

  // Render to offline bitmap first to avoid tearing.
  wil::unique_hdc memdc(::CreateCompatibleDC(target_dc));
  wil::unique_hbitmap bitmap(::CreateCompatibleBitmap(
      target_dc, client_rect.Width(), client_rect.Height()));
  wil::unique_select_object old_bitmap =
      wil::SelectObject(memdc.get(), bitmap.get());
  DoPaint(memdc.get());
  ::BitBlt(target_dc, client_rect.left, client_rect.top, client_rect.Width(),
           client_rect.Height(), memdc.get(), 0, 0, SRCCOPY);
}

void CandidateWindow::OnPrintClient(HDC dc, UINT uFlags) { OnPaint(dc); }

void CandidateWindow::DoPaint(HDC dc) {
  switch (candidate_window_->category()) {
    case commands::CONVERSION:
    case commands::PREDICTION:
    case commands::TRANSLITERATION:
    case commands::SUGGESTION:
    case commands::USAGE:
      break;
    default:
      LOG(INFO) << "Unknown candidates category: "
                << candidate_window_->category();
      return;
  }

  if (!table_layout_->IsLayoutFrozen()) {
    LOG(WARNING) << "Table layout is not frozen.";
    return;
  }

  ::SetBkMode(dc, TRANSPARENT);

  DrawBackground(dc);
  DrawShortcutBackground(dc);
  DrawSelectedRect(dc);
  DrawSpecimenValuePlates(dc);
  DrawSpecimenDrawings(dc);
  DrawCells(dc);
  DrawInformationIcon(dc);
  DrawVScrollBar(dc);
  DrawFooter(dc);
  DrawFrame(dc);
}

void CandidateWindow::OnSettingChange(UINT uFlags, LPCTSTR /*lpszSection*/) {
  // Since TextRenderer uses dialog font to render,
  // we monitor font-related parameters to know when the font style is changed.
  switch (uFlags) {
    case 0x1049:  // = SPI_SETCLEARTYPE
    case SPI_SETFONTSMOOTHING:
    case SPI_SETFONTSMOOTHINGCONTRAST:
    case SPI_SETFONTSMOOTHINGORIENTATION:
    case SPI_SETFONTSMOOTHINGTYPE:
    case SPI_SETNONCLIENTMETRICS:
      metrics_changed_ = true;
      break;
    case SPI_SETACTIVEWINDOWTRACKING:
      EnableOrDisableWindowForWorkaround();
      [[fallthrough]];
    default:
      // We ignore other changes.
      break;
  }
}

void CandidateWindow::UpdateLayout(
    const commands::CandidateWindow& candidates) {
  *candidate_window_ = candidates;

  // If we detect any change of font parameters, update text renderer
  if (metrics_changed_) {
    text_renderer_->OnThemeChanged();
    metrics_changed_ = false;
  }

  switch (candidate_window_->category()) {
    case commands::CONVERSION:
    case commands::PREDICTION:
    case commands::TRANSLITERATION:
    case commands::SUGGESTION:
    case commands::USAGE:
      break;
    default:
      LOG(INFO) << "Unknown candidates category: "
                << candidate_window_->category();
      return;
  }

  table_layout_->Initialize(candidate_window_->candidate_size(),
                            NUMBER_OF_COLUMNS);

  table_layout_->SetWindowBorder(style_.window_border());

  // Add a vertical scroll bar if candidate list consists of more than
  // one page.
  if (candidate_window_->candidate_size() < candidate_window_->size()) {
    table_layout_->SetVScrollBar(indicator_width_);
  }

  if (candidate_window_->has_footer()) {
    Size footer_size(0, 0);

    // Calculate the size to display a label string.
    if (candidate_window_->footer().has_label()) {
      const std::wstring footer_label =
          mozc::win32::Utf8ToWide(candidate_window_->footer().label());
      const Size label_string_size = text_renderer_->MeasureString(
          TextRenderer::FONTSET_FOOTER_LABEL, L" " + footer_label + L" ");
      footer_size.width += label_string_size.width;
      footer_size.height =
          std::max(footer_size.height, label_string_size.height);
    } else if (candidate_window_->footer().has_sub_label()) {
      // Currently the sub label will not be shown unless (main) label is
      // absent.
      // TODO(yukawa): Refactor the layout system for the footer.
      const std::wstring footer_sub_label =
          mozc::win32::Utf8ToWide(candidate_window_->footer().sub_label());
      const Size label_string_size =
          text_renderer_->MeasureString(TextRenderer::FONTSET_FOOTER_SUBLABEL,
                                        L" " + footer_sub_label + L" ");
      footer_size.width += label_string_size.width;
      footer_size.height =
          std::max(footer_size.height, label_string_size.height);
    }

    // Calculate the size to display a index string.
    if (candidate_window_->footer().index_visible()) {
      const std::wstring index_guide_string =
          mozc::win32::Utf8ToWide(GetIndexGuideString(*candidate_window_));
      const Size index_guide_size = text_renderer_->MeasureString(
          TextRenderer::FONTSET_FOOTER_INDEX, index_guide_string);
      footer_size.width += index_guide_size.width;
      footer_size.height =
          std::max(footer_size.height, index_guide_size.height);
    }

    // Calculate the size to display a Footer logo.
    if (footer_logo_.is_valid()) {
      if (candidate_window_->footer().logo_visible()) {
        const double scale_factor = GetDPIScalingFactor(dpi_);
        footer_size.width +=
            static_cast<int>(kFooterLogoLeftMarginInDefaultDPI * scale_factor) +
            footer_logo_display_size_.width +
            static_cast<int>(kFooterLogoRightMarginInDefaultDPI * scale_factor);
        footer_size.height =
            std::max(footer_size.height, footer_logo_display_size_.height);
      } else if (footer_size.height > 0) {
        // Ensure the footer height is greater than the Footer logo height
        // even if the Footer logo is absent.  This hack prevents the footer
        // from changing its height too frequently.
        footer_size.height =
            std::max(footer_size.height, footer_logo_display_size_.height);
      }
    }

    // Ensure minimum columns width if candidate list consists of more than
    // one page.
    if (candidate_window_->candidate_size() < candidate_window_->size()) {
      // We use FONTSET_CANDIDATE for calculating the minimum width.
      const std::wstring minimum_width_as_wstring = mozc::win32::Utf8ToWide(
          style_.column_minimum_width_string());
      const Size minimum_size = text_renderer_->MeasureString(
          TextRenderer::FONTSET_CANDIDATE, minimum_width_as_wstring.c_str());
      table_layout_->EnsureColumnsWidth(COLUMN_CANDIDATE, COLUMN_DESCRIPTION,
                                        minimum_size.width);
    }

    // Add separator height
    footer_size.height += style_.footer_border_colors_size() > 0 ? 1 : 0;

    table_layout_->EnsureFooterSize(footer_size);
  }

  table_layout_->SetRowRectPadding(style_.row_rect_padding());

  // put a padding in COLUMN_GAP1.
  // the width is determined to be equal to the width of " ".
  const Size gap1_size =
      text_renderer_->MeasureString(TextRenderer::FONTSET_CANDIDATE, L" ");
  table_layout_->EnsureCellSize(COLUMN_GAP1, gap1_size);

  bool description_found = false;

  // calculate table size.
  for (size_t i = 0; i < candidate_window_->candidate_size(); ++i) {
    const commands::CandidateWindow::Candidate& candidate =
        candidate_window_->candidate(i);
    const std::wstring shortcut =
        GetDisplayStringByColumn(candidate, COLUMN_SHORTCUT);
    const std::wstring description =
        GetDisplayStringByColumn(candidate, COLUMN_DESCRIPTION);
    std::wstring candidate_string =
        GetDisplayStringByColumn(candidate, COLUMN_CANDIDATE);
    const bool drawable = text_renderer_->FontCanDraw(
        TextRenderer::FONTSET_CANDIDATE, candidate_string);
    if (CandidateUsesDrawnValue(candidate, drawable)) {
      // KAGE / IDS preview is drawn instead of the string: one character wide.
      candidate_string = L"\u3000";
    }

    if (!shortcut.empty()) {
      std::wstring text;
      text.push_back(L' ');  // put a space for padding
      text.append(shortcut);
      text.push_back(L' ');  // put a space for padding
      const Size rendering_size =
          text_renderer_->MeasureString(TextRenderer::FONTSET_SHORTCUT, text);
      table_layout_->EnsureCellSize(COLUMN_SHORTCUT, rendering_size);
    }

    if (!candidate_string.empty()) {
      std::wstring text;
      text.append(candidate_string);

      const Size rendering_size =
          text_renderer_->MeasureString(TextRenderer::FONTSET_CANDIDATE, text);
      table_layout_->EnsureCellSize(COLUMN_CANDIDATE, rendering_size);
    }

    if (!description.empty()) {
      std::wstring text;
      text.append(description);
      text.push_back(L' ');  // put a space for padding
      const Size rendering_size = text_renderer_->MeasureString(
          TextRenderer::FONTSET_DESCRIPTION, text);
      table_layout_->EnsureCellSize(COLUMN_DESCRIPTION, rendering_size);

      description_found = true;
    }
  }

  // Put a padding in COLUMN_GAP2.
  // We use wide padding if there is any description column.
  const wchar_t* gap2_string = (description_found ? L"   " : L" ");
  const Size gap2_size = text_renderer_->MeasureString(
      TextRenderer::FONTSET_CANDIDATE, gap2_string);
  table_layout_->EnsureCellSize(COLUMN_GAP2, gap2_size);

  table_layout_->FreezeLayout();
  UpdateWindowRegion();
}

void CandidateWindow::SetSendCommandInterface(
    client::SendCommandInterface* send_command_interface) {
  send_command_interface_ = send_command_interface;
}

Size CandidateWindow::GetLayoutSize() const {
  DCHECK(table_layout_->IsLayoutFrozen()) << "Table layout is not frozen.";

  return table_layout_->GetTotalSize();
}

Rect CandidateWindow::GetSelectionRectInScreenCord() const {
  const int focused_array_index = GetFocusedArrayIndex(*candidate_window_);

  if (0 <= focused_array_index &&
      focused_array_index < candidate_window_->candidate_size()) {
    (void)candidate_window_->candidate(focused_array_index);

    CRect rect = ToCRect(table_layout_->GetRowRect(focused_array_index));
    ClientToScreen(&rect);
    return Rect(rect.left, rect.top, rect.Width(), rect.Height());
  }

  return Rect();
}

Rect CandidateWindow::GetCandidateColumnInClientCord() const {
  DCHECK(table_layout_->IsLayoutFrozen()) << "Table layout is not frozen.";

  return table_layout_->GetCellRect(0, COLUMN_CANDIDATE);
}

Rect CandidateWindow::GetFirstRowInClientCord() const {
  DCHECK(table_layout_->IsLayoutFrozen()) << "Table layout is not frozen.";
  DCHECK_GT(table_layout_->number_of_rows(), 0)
      << "number of rows should be positive";
  return table_layout_->GetRowRect(0);
}

void CandidateWindow::DrawSpecimenValuePlates(HDC dc) {
  DCHECK(table_layout_->IsLayoutFrozen()) << "Table layout is not frozen.";
  for (size_t i = 0; i < candidate_window_->candidate_size(); ++i) {
    const commands::CandidateWindow::Candidate& candidate =
        candidate_window_->candidate(i);
    const std::wstring value =
        GetDisplayStringByColumn(candidate, COLUMN_CANDIDATE);
    const bool drawable =
        text_renderer_->FontCanDraw(TextRenderer::FONTSET_CANDIDATE, value);
    const specimen_kind::Kind kind =
        SpecimenKindForCandidate(candidate, drawable);
    uint8_t r = 0, g = 0, b = 0;
    if (!specimen_kind::PlateRgb(kind, &r, &g, &b)) {
      continue;
    }
    const Rect row_rect = table_layout_->GetRowRect(i);
    Rect cell_rect = table_layout_->GetCellRect(i, COLUMN_CANDIDATE);
    cell_rect.origin.y = row_rect.origin.y;
    cell_rect.size.height = row_rect.size.height;
    const CRect crect = ToCRect(cell_rect);
    FillSolidRect(dc, &crect, RGB(r, g, b));
  }
}

void CandidateWindow::DrawSpecimenDrawings(HDC dc) {
  DCHECK(table_layout_->IsLayoutFrozen()) << "Table layout is not frozen.";
  constexpr COLORREF kPlateInk = RGB(255, 255, 255);
  for (size_t i = 0; i < candidate_window_->candidate_size(); ++i) {
    const commands::CandidateWindow::Candidate& candidate =
        candidate_window_->candidate(i);
    const std::wstring value =
        GetDisplayStringByColumn(candidate, COLUMN_CANDIDATE);
    const bool drawable =
        text_renderer_->FontCanDraw(TextRenderer::FONTSET_CANDIDATE, value);
    if (!CandidateUsesDrawnValue(candidate, drawable)) {
      continue;
    }
    const specimen_kind::Kind kind =
        SpecimenKindForCandidate(candidate, drawable);
    const Rect row_rect = table_layout_->GetRowRect(i);
    const Rect cell_rect = table_layout_->GetCellRect(i, COLUMN_CANDIDATE);
    const int side = row_rect.size.height - 2;
    Rect square(cell_rect.origin.x + style_.candidate_style().left_padding(),
                row_rect.origin.y + 1, side, side);
    if (PaintKageOutlines(dc, square, candidate.value(), kPlateInk)) {
      continue;
    }
    if (kind == specimen_kind::Kind::kSvg) {
      const auto preview =
          composer_ids::ParseIdsPreview(candidate.value());
      if (preview.has_value()) {
        PaintIdsPreview(dc, square, *preview, kPlateInk);
      }
    }
  }
}

void CandidateWindow::DrawCells(HDC dc) {
  COLUMN_TYPE kColumnTypes[] = {COLUMN_SHORTCUT, COLUMN_CANDIDATE,
                                COLUMN_DESCRIPTION};
  TextRenderer::FONT_TYPE kFontTypes[] = {TextRenderer::FONTSET_SHORTCUT,
                                          TextRenderer::FONTSET_CANDIDATE,
                                          TextRenderer::FONTSET_DESCRIPTION};

  DCHECK_EQ(std::size(kColumnTypes), std::size(kFontTypes));
  for (size_t type_index = 0; type_index < std::size(kColumnTypes);
       ++type_index) {
    const COLUMN_TYPE column_type = kColumnTypes[type_index];
    const TextRenderer::FONT_TYPE font_type = kFontTypes[type_index];

    std::vector<TextRenderingInfo> display_list;
    std::vector<TextRenderingInfo> specimen_list;
    for (size_t i = 0; i < candidate_window_->candidate_size(); ++i) {
      const commands::CandidateWindow::Candidate& candidate =
          candidate_window_->candidate(i);
      const std::wstring display_string =
          GetDisplayStringByColumn(candidate, column_type);
      const Rect text_rect = table_layout_->GetCellRect(i, column_type);
      if (column_type == COLUMN_CANDIDATE) {
        const bool drawable = text_renderer_->FontCanDraw(
            TextRenderer::FONTSET_CANDIDATE, display_string);
        if (CandidateUsesDrawnValue(candidate, drawable)) {
          // Outline / IDS preview already painted in DrawSpecimenDrawings.
          continue;
        }
        const specimen_kind::Kind kind =
            SpecimenKindForCandidate(candidate, drawable);
        uint8_t r = 0, g = 0, b = 0;
        if (specimen_kind::PlateRgb(kind, &r, &g, &b)) {
          specimen_list.push_back(
              TextRenderingInfo(display_string, text_rect));
          continue;
        }
      }
      display_list.push_back(TextRenderingInfo(display_string, text_rect));
    }
    if (!display_list.empty()) {
      text_renderer_->RenderTextList(dc, display_list, font_type);
    }
    if (!specimen_list.empty()) {
      text_renderer_->RenderTextListWithColor(dc, specimen_list, font_type,
                                              RGB(255, 255, 255));
    }
  }
}

void CandidateWindow::DrawVScrollBar(HDC dc) {
  const Rect& vscroll_rect = table_layout_->GetVScrollBarRect();

  if (!vscroll_rect.IsRectEmpty() && candidate_window_->candidate_size() > 0) {
    const int begin_index = candidate_window_->candidate(0).index();
    const int candidates_in_page = candidate_window_->candidate_size();
    const int candidates_total = candidate_window_->size();
    const int end_index =
        candidate_window_->candidate(candidates_in_page - 1).index();

    const CRect background_crect = ToCRect(vscroll_rect);
    FillSolidRect(dc, &background_crect,
                  ToColorRef(style_.scrollbar_background_color(),
                             RGB(0xe0, 0xe0, 0xe0)));

    const mozc::Rect& indicator_rect = table_layout_->GetVScrollIndicatorRect(
        begin_index, end_index, candidates_total);

    const CRect indicator_crect = ToCRect(indicator_rect);
    FillSolidRect(dc, &indicator_crect,
                  ToColorRef(style_.scrollbar_indicator_color(),
                             RGB(0x75, 0x90, 0xb8)));
  }
}

void CandidateWindow::DrawShortcutBackground(HDC dc) {
  if (table_layout_->number_of_columns() > 0) {
    Rect shortcut_colmun_rect = table_layout_->GetColumnRect(0);
    if (!shortcut_colmun_rect.IsRectEmpty()) {
      // Due to the mismatch of the implementation of the TableLayout class
      // and the design requiement, we have to *fix* the width and origin
      // of the rectangle.
      // If you remove this *fix*, an empty region appears between the
      // left window border and the colored region of the shortcut column.
      const Rect row_rect = table_layout_->GetRowRect(0);
      const int width = shortcut_colmun_rect.Right() - row_rect.Left();
      shortcut_colmun_rect.origin.x = row_rect.Left();
      shortcut_colmun_rect.size.width = width;
      const CRect shortcut_colmun_crect = ToCRect(shortcut_colmun_rect);
      FillSolidRect(dc, &shortcut_colmun_crect,
                    ToOpaqueColorRef(style_.shortcut_style().background_color(),
                                     ToColorRef(style_.window_background_color(),
                                                RGB(0xff, 0xff, 0xff)),
                                     RGB(0xf3, 0xf4, 0xff)));
    }
  }
}

void CandidateWindow::DrawFooter(HDC dc) {
  const Rect& footer_rect = table_layout_->GetFooterRect();
  if (!candidate_window_->has_footer() || footer_rect.IsRectEmpty()) {
    return;
  }

  const COLORREF background_color =
      ToColorRef(style_.window_background_color(), RGB(0xff, 0xff, 0xff));
  const COLORREF footer_separator_color =
      style_.footer_border_colors_size() > 0
          ? ToOpaqueColorRef(style_.footer_border_colors(0), background_color,
                             RGB(0x96, 0x96, 0x96))
          : ToOpaqueColorRef(style_.border_color(), background_color,
                             RGB(0x96, 0x96, 0x96));
  const COLORREF footer_top_color =
      ToColorRef(style_.footer_top_color(), background_color);
  const COLORREF footer_bottom_color =
      ToColorRef(style_.footer_bottom_color(), background_color);
  const int separator_height =
      style_.footer_border_colors_size() > 0 ? 1 : 0;

  // DC pen is available in Windows 2000 and later.
  {
    wil::unique_select_object prev_pen =
        wil::SelectObject(dc, static_cast<HPEN>(::GetStockObject(DC_PEN)));
    for (size_t i = 0, y = footer_rect.Top(); i < separator_height;
         y++, i++) {
      ::SetDCPenColor(dc, footer_separator_color);
      ::MoveToEx(dc, footer_rect.Left(), y, nullptr);
      ::LineTo(dc, footer_rect.Right(), y);
    }
  }

  const Rect footer_content_rect(
      footer_rect.Left(), footer_rect.Top() + separator_height,
      footer_rect.Width(), footer_rect.Height() - separator_height);

  // Draw gradient rect in the footer area
  {
    TRIVERTEX vertices[] = {
        {footer_content_rect.Left(), footer_content_rect.Top(),
         static_cast<COLOR16>(GetRValue(footer_top_color) << 8),
         static_cast<COLOR16>(GetGValue(footer_top_color) << 8),
         static_cast<COLOR16>(GetBValue(footer_top_color) << 8), 0xff00},
        {footer_content_rect.Right(), footer_content_rect.Bottom(),
         static_cast<COLOR16>(GetRValue(footer_bottom_color) << 8),
         static_cast<COLOR16>(GetGValue(footer_bottom_color) << 8),
         static_cast<COLOR16>(GetBValue(footer_bottom_color) << 8), 0xff00}};
    GRADIENT_RECT indices[] = {{0, 1}};
    ::GradientFill(dc, &vertices[0], std::size(vertices), &indices[0],
                   std::size(indices), GRADIENT_FILL_RECT_V);
  }

  int left_used = 0;

  if (candidate_window_->footer().logo_visible() && footer_logo_.is_valid()) {
    const double scale_factor = GetDPIScalingFactor(dpi_);
    const int left_margin =
        static_cast<int>(kFooterLogoLeftMarginInDefaultDPI * scale_factor);
    const int right_margin =
        static_cast<int>(kFooterLogoRightMarginInDefaultDPI * scale_factor);
    const int top_offset =
        (footer_content_rect.Height() - footer_logo_display_size_.height) / 2;
    wil::unique_hdc src_dc(::CreateCompatibleDC(dc));
    wil::unique_select_object old_bitmap =
        wil::SelectObject(src_dc.get(), footer_logo_.get());

    BITMAP bm = {};
    ::GetObject(footer_logo_.get(), sizeof(bm), &bm);
    const CSize src_size(bm.bmWidth, bm.bmHeight);

    // NOTE: AC_SRC_ALPHA requires PBGRA (pre-multiplied alpha) DIB.
    const BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    ::AlphaBlend(
        dc, footer_content_rect.Left() + left_margin,
        footer_content_rect.Top() + top_offset,
        footer_logo_display_size_.width, footer_logo_display_size_.height,
        src_dc.get(), 0, 0, src_size.cx, src_size.cy, bf);

    left_used = footer_content_rect.Left() + left_margin +
                footer_logo_display_size_.width + right_margin;
  }

  int right_used = 0;
  if (candidate_window_->footer().index_visible()) {
    const std::wstring index_guide_string =
        mozc::win32::Utf8ToWide(GetIndexGuideString(*candidate_window_));
    const Size index_guide_size = text_renderer_->MeasureString(
        TextRenderer::FONTSET_FOOTER_INDEX, index_guide_string);
    const Rect index_rect(footer_content_rect.Right() - index_guide_size.width,
                          footer_content_rect.Top(), index_guide_size.width,
                          footer_content_rect.Height());
    text_renderer_->RenderText(dc, index_guide_string, index_rect,
                               TextRenderer::FONTSET_FOOTER_INDEX);
    right_used = index_guide_size.width;
  }

  if (candidate_window_->footer().has_label()) {
    const Rect label_rect(left_used, footer_content_rect.Top(),
                          footer_content_rect.Width() - left_used - right_used,
                          footer_content_rect.Height());
    const std::wstring footer_label =
        mozc::win32::Utf8ToWide(candidate_window_->footer().label());
    text_renderer_->RenderText(dc, L" " + footer_label + L" ", label_rect,
                               TextRenderer::FONTSET_FOOTER_LABEL);
  } else if (candidate_window_->footer().has_sub_label()) {
    const std::wstring footer_sub_label =
        mozc::win32::Utf8ToWide(candidate_window_->footer().sub_label());
    const Rect label_rect(left_used, footer_content_rect.Top(),
                          footer_content_rect.Width() - left_used - right_used,
                          footer_content_rect.Height());
    const std::wstring text = L" " + footer_sub_label + L" ";
    text_renderer_->RenderText(dc, text, label_rect,
                               TextRenderer::FONTSET_FOOTER_SUBLABEL);
  }
}

void CandidateWindow::DrawSelectedRect(HDC dc) {
  DCHECK(table_layout_->IsLayoutFrozen()) << "Table layout is not frozen.";

  const int focused_array_index = GetFocusedArrayIndex(*candidate_window_);

  if (0 <= focused_array_index &&
      focused_array_index < candidate_window_->candidate_size()) {
    (void)candidate_window_->candidate(focused_array_index);

    const CRect selected_rect =
        ToCRect(table_layout_->GetRowRect(focused_array_index));
    FillSolidRect(dc, &selected_rect,
                  ToColorRef(style_.focused_background_color(),
                             RGB(0xd1, 0xea, 0xff)));

    ::SetDCBrushColor(
        dc, ToOpaqueColorRef(style_.focused_border_color(),
                             ToColorRef(style_.window_background_color(),
                                        RGB(0xff, 0xff, 0xff)),
                             RGB(0x7f, 0xac, 0xdd)));
    ::FrameRect(dc, &selected_rect,
                static_cast<HBRUSH>(::GetStockObject(DC_BRUSH)));
  }
}

void CandidateWindow::DrawInformationIcon(HDC dc) {
  DCHECK(table_layout_->IsLayoutFrozen()) << "Table layout is not frozen.";
  const double scale_factor = GetDPIScalingFactor(dpi_);
  for (size_t i = 0; i < candidate_window_->candidate_size(); ++i) {
    if (candidate_window_->candidate(i).has_information_id()) {
      CRect rect = ToCRect(table_layout_->GetRowRect(i));
      rect.left = rect.right - (6.0 * scale_factor);
      rect.right = rect.right - (2.0 * scale_factor);
      rect.top += (2.0 * scale_factor);
      rect.bottom -= (2.0 * scale_factor);
      const COLORREF indicator_color =
          ToColorRef(style_.scrollbar_indicator_color(), RGB(0x75, 0x90, 0xb8));
      FillSolidRect(dc, &rect, indicator_color);
      ::SetDCBrushColor(dc, indicator_color);
      ::FrameRect(dc, &rect, static_cast<HBRUSH>(::GetStockObject(DC_BRUSH)));
    }
  }
}

void CandidateWindow::DrawBackground(HDC dc) {
  const Rect client_rect(Point(0, 0), table_layout_->GetTotalSize());
  const CRect client_crect = ToCRect(client_rect);
  FillSolidRect(dc, &client_crect,
                ToColorRef(style_.window_background_color(),
                           RGB(0xff, 0xff, 0xff)));
}

void CandidateWindow::DrawFrame(HDC dc) {
  const Rect client_rect(Point(0, 0), table_layout_->GetTotalSize());
  const CRect client_crect = ToCRect(client_rect);

  // DC brush is available in Windows 2000 and later.
  ::SetDCBrushColor(
      dc, ToOpaqueColorRef(style_.border_color(),
                           ToColorRef(style_.window_background_color(),
                                      RGB(0xff, 0xff, 0xff)),
                           RGB(0x96, 0x96, 0x96)));
  ::FrameRect(dc, &client_crect,
              static_cast<HBRUSH>(::GetStockObject(DC_BRUSH)));
}

void CandidateWindow::set_mouse_moving(bool moving) { mouse_moving_ = moving; }
}  // namespace win32
}  // namespace renderer
}  // namespace mozc
