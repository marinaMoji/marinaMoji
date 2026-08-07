// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0
//
// marinaMoji: MSAA (IAccessible) exposure for the Windows floating toolbar.
//
// The toolbar composites its six buttons into a single layered bitmap, so
// unlike the Symbols Palette it has no child HWNDs and is invisible to
// screen readers by default -- there is nothing for Narrator to enumerate.
// This provides the accessible tree the buttons would otherwise have had:
// the window as a TOOLBAR containing one PUSHBUTTON per ButtonId, each with
// the same localized name its tooltip shows, its own screen rect, and a
// default action that runs the click handler.

#ifndef MOZC_RENDERER_WIN32_TOOLBAR_ACCESSIBLE_H_
#define MOZC_RENDERER_WIN32_TOOLBAR_ACCESSIBLE_H_

#include <windows.h>

namespace mozc {
namespace renderer {
namespace win32 {

class ToolbarWindow;

// Returns a new IAccessible (as void* so <oleacc.h> stays out of
// toolbar_window.h) with one reference held by the caller, or nullptr on
// failure. |window| must outlive the returned object; ToolbarWindow releases
// it in its WM_DESTROY handler.
void* CreateToolbarAccessible(ToolbarWindow* window, HWND window_handle);

}  // namespace win32
}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_WIN32_TOOLBAR_ACCESSIBLE_H_
