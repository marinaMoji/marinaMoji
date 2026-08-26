// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0

#include "renderer/win32/shadow_window.h"

#include <wil/resource.h>
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace mozc {
namespace renderer {
namespace win32 {
namespace {

// Opacity directly under the owner. The owner is opaque, so this is only
// ever seen where the shadow spills past it -- mostly at the rounded
// corners, where a lower value would make the corners look bitten out.
constexpr double kMaxAlpha = 96.0;

// Signed distance from (px, py) to a rounded rectangle centred on the
// origin: negative inside, zero on the edge, positive outside. Same
// construction as the coverage test in toolbar_window.cc, generalised to
// report a distance rather than a 0..1 coverage.
double RoundedRectDistance(double px, double py, double half_width,
                           double half_height, double radius) {
  const double qx = std::abs(px) - (half_width - radius);
  const double qy = std::abs(py) - (half_height - radius);
  const double outside_x = std::max(qx, 0.0);
  const double outside_y = std::max(qy, 0.0);
  return std::sqrt(outside_x * outside_x + outside_y * outside_y) +
         std::min(std::max(qx, qy), 0.0) - radius;
}

}  // namespace

void ShadowWindow::Initialize() {
  Create(nullptr);
  ShowWindow(SW_HIDE);
}

void ShadowWindow::Destroy() {
  if (IsWindow()) {
    DestroyWindow();
  }
}

void ShadowWindow::Hide() {
  if (IsWindow()) {
    ShowWindow(SW_HIDE);
  }
}

void ShadowWindow::Redraw(int width, int height, double corner_radius,
                          double blur, double offset_y) {
  BITMAPINFO bitmap_info = {};
  bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bitmap_info.bmiHeader.biWidth = width;
  bitmap_info.bmiHeader.biHeight = -height;  // top-down
  bitmap_info.bmiHeader.biPlanes = 1;
  bitmap_info.bmiHeader.biBitCount = 32;
  bitmap_info.bmiHeader.biCompression = BI_RGB;

  void* bits_ptr = nullptr;
  wil::unique_hbitmap dib(::CreateDIBSection(nullptr, &bitmap_info,
                                             DIB_RGB_COLORS, &bits_ptr, nullptr,
                                             0));
  if (!dib || bits_ptr == nullptr) {
    return;
  }
  uint8_t* bits = static_cast<uint8_t*>(bits_ptr);

  // The silhouette is the owner's rounded rect, centred in this window and
  // pushed down by offset_y.
  const double centre_x = width / 2.0;
  const double centre_y = height / 2.0 + offset_y;
  const double half_width = width / 2.0 - blur;
  const double half_height = height / 2.0 - blur;
  // A radius larger than the half-extent would make the distance field fold
  // in on itself.
  const double radius =
      std::min(corner_radius, std::min(half_width, half_height));

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const double distance =
          RoundedRectDistance(x + 0.5 - centre_x, y + 0.5 - centre_y,
                              half_width, half_height, radius);
      double alpha;
      if (distance <= 0.0) {
        alpha = kMaxAlpha;
      } else if (distance >= blur) {
        alpha = 0.0;
      } else {
        // Quadratic falloff. A true Gaussian blur of the silhouette would
        // need a separable convolution over the whole bitmap; for a solid
        // rounded rect this is visually equivalent and costs one sqrt per
        // pixel.
        const double t = 1.0 - distance / blur;
        alpha = kMaxAlpha * t * t;
      }
      // Black, and UpdateLayeredWindow with AC_SRC_ALPHA expects
      // premultiplied channels, so the colour components are zero whatever
      // the alpha.
      uint8_t* p = bits + (static_cast<size_t>(y) * width + x) * 4;
      p[0] = 0;
      p[1] = 0;
      p[2] = 0;
      p[3] = static_cast<uint8_t>(std::lround(alpha));
    }
  }

  ::GdiFlush();

  CSize size(width, height);
  CPoint src_origin(0, 0);
  BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
  wil::unique_hdc mem_dc(::CreateCompatibleDC(nullptr));
  wil::unique_select_object old_bitmap =
      wil::SelectObject(mem_dc.get(), dib.get());
  // Position is handled by SetWindowPos in Update(); passing nullptr for the
  // destination point leaves it alone.
  ::UpdateLayeredWindow(m_hWnd, nullptr, nullptr, &size, mem_dc.get(),
                        &src_origin, 0, &blend, ULW_ALPHA);
}

void ShadowWindow::Update(HWND owner, const RECT& content_rect,
                          double corner_radius, double scale) {
  if (!IsWindow()) {
    return;
  }

  const double blur = std::max(1.0, kBlurLogical * scale);
  const double offset_y = kOffsetYLogical * scale;
  const int margin = static_cast<int>(std::ceil(blur));
  const int width = (content_rect.right - content_rect.left) + margin * 2;
  const int height = (content_rect.bottom - content_rect.top) + margin * 2;
  if (width <= 0 || height <= 0) {
    Hide();
    return;
  }

  if (width != drawn_width_ || height != drawn_height_ ||
      corner_radius != drawn_corner_radius_ || blur != drawn_blur_ ||
      offset_y != drawn_offset_y_) {
    Redraw(width, height, corner_radius, blur, offset_y);
    drawn_width_ = width;
    drawn_height_ = height;
    drawn_corner_radius_ = corner_radius;
    drawn_blur_ = blur;
    drawn_offset_y_ = offset_y;
  }

  // Insert directly behind the owner. Passing the owner as hWndInsertAfter
  // also makes this window topmost, which it has to be to stay above the
  // application the owner floats over.
  ::SetWindowPos(m_hWnd, owner, content_rect.left - margin,
                 content_rect.top - margin, width, height,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

}  // namespace win32
}  // namespace renderer
}  // namespace mozc
