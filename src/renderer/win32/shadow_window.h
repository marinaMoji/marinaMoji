// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0
//
// marinaMoji: a click-through window that draws nothing but a soft drop
// shadow, sized to sit immediately behind another window.
//
// The toolbar is WS_EX_LAYERED so its rounded corners can be composited with
// true per-pixel alpha. That rules out the usual way of getting a shadow:
// CS_DROPSHADOW is ignored for layered windows, and DWM does not decorate a
// WS_POPUP with no caption. The alternative -- inflating the toolbar's own
// bitmap and drawing the shadow into its margin -- would shift the origin of
// every button rect, and with it hit testing, dragging, screen-edge clamping
// and the MSAA rects in toolbar_accessible.cc. Keeping the shadow in its own
// window leaves all of that untouched: the toolbar's client area still starts
// at its visible top-left.
//
// WS_EX_TRANSPARENT makes the window invisible to hit testing, so clicks fall
// through to whatever is underneath exactly as they did before it existed.

#ifndef MOZC_RENDERER_WIN32_SHADOW_WINDOW_H_
#define MOZC_RENDERER_WIN32_SHADOW_WINDOW_H_

#include <atlbase.h>
#include <atltypes.h>
#include <atlwin.h>
#include <windows.h>

#include "base/const.h"

namespace mozc {
namespace renderer {
namespace win32 {

typedef ATL::CWinTraits<WS_POPUP,
                        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW |
                            WS_EX_TOPMOST | WS_EX_NOACTIVATE>
    ShadowWindowTraits;

class ShadowWindow : public ATL::CWindowImpl<ShadowWindow, ATL::CWindow,
                                             ShadowWindowTraits> {
 public:
  DECLARE_WND_CLASS_EX(kShadowWindowClassName, 0, COLOR_WINDOW);

  BEGIN_MSG_MAP(ShadowWindow)
  END_MSG_MAP()

  // How far the shadow reaches past the owner's edge, in logical pixels,
  // before it has faded to nothing.
  static constexpr int kBlurLogical = 12;
  // Downward shift, so the shadow reads as cast by a light source above.
  static constexpr int kOffsetYLogical = 3;

  ShadowWindow() = default;
  ShadowWindow(const ShadowWindow&) = delete;
  ShadowWindow& operator=(const ShadowWindow&) = delete;
  ~ShadowWindow() = default;

  void Initialize();
  void Destroy();

  // Positions the shadow for an owner occupying |content_rect| (screen
  // coordinates) whose corners have |corner_radius| device pixels of
  // rounding, and restacks it immediately behind |owner|. Shows it if it was
  // hidden. |scale| is the owner's DPI scaling factor, applied to the
  // logical constants above.
  //
  // Safe to call on every one of the owner's redraws: the bitmap is rebuilt
  // only when the size, radius or scale actually changed, so a hover repaint
  // costs one SetWindowPos.
  void Update(HWND owner, const RECT& content_rect, double corner_radius,
              double scale);

  void Hide();

 private:
  // Rebuilds the layered bitmap. |width| and |height| are the shadow
  // window's own size, already inflated past the owner.
  void Redraw(int width, int height, double corner_radius, double blur,
              double offset_y);

  // Bitmap parameters the current contents were drawn with, so Update() can
  // tell a move from a genuine change.
  int drawn_width_ = 0;
  int drawn_height_ = 0;
  double drawn_corner_radius_ = -1.0;
  double drawn_blur_ = -1.0;
  double drawn_offset_y_ = -1.0;
};

}  // namespace win32
}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_WIN32_SHADOW_WINDOW_H_
