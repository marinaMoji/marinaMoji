// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// Windows backend for base/font_coverage.h: DirectWrite is asked whether any
// installed font family (its regular face) has the character. Works on full
// code points, so characters outside the basic plane (Extension B and later,
// which are surrogate pairs in UTF-16) are judged correctly, and does not
// depend on which font the UI happens to use.
//
// Not built or run on the machine this was written on; verify on Windows.

#include <dwrite.h>
#include <wil/com.h>

#include "base/font_coverage.h"

namespace mozc {
namespace internal {
namespace {

// Shared factory and system collection, created once. Both are thread-safe
// and intentionally never released (process lifetime).
IDWriteFontCollection *SystemFonts() {
  static IDWriteFontCollection *collection = [] {
    IDWriteFactory *factory = nullptr;
    if (FAILED(::DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown **>(&factory))) ||
        factory == nullptr) {
      return static_cast<IDWriteFontCollection *>(nullptr);
    }
    IDWriteFontCollection *fonts = nullptr;
    if (FAILED(factory->GetSystemFontCollection(&fonts, FALSE))) {
      return static_cast<IDWriteFontCollection *>(nullptr);
    }
    return fonts;
  }();
  return collection;
}

}  // namespace

bool CanRenderCodePointUncached(char32_t code_point) {
  IDWriteFontCollection *fonts = SystemFonts();
  if (fonts == nullptr) return true;  // Never hide on a guess.

  const UINT32 families = fonts->GetFontFamilyCount();
  for (UINT32 i = 0; i < families; ++i) {
    wil::com_ptr_nothrow<IDWriteFontFamily> family;
    if (FAILED(fonts->GetFontFamily(i, family.put()))) continue;
    wil::com_ptr_nothrow<IDWriteFont> font;
    if (FAILED(family->GetFirstMatchingFont(
            DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            DWRITE_FONT_STYLE_NORMAL, font.put()))) {
      continue;
    }
    BOOL exists = FALSE;
    if (SUCCEEDED(font->HasCharacter(static_cast<UINT32>(code_point),
                                     &exists)) &&
        exists) {
      return true;
    }
  }
  return false;
}

}  // namespace internal
}  // namespace mozc
