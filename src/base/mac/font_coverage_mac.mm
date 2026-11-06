// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// macOS backend for base/font_coverage.h. Same test as the candidate window:
// ask CoreText for the font that would draw the character, and treat the
// "LastResort" font (boxes with the Unicode block name) as "no font".

#include "base/font_coverage.h"

#import <CoreFoundation/CoreFoundation.h>
#import <CoreText/CoreText.h>

namespace mozc {
namespace internal {

bool CanRenderCodePointUncached(char32_t code_point) {
  const UTF32Char scalar = static_cast<UTF32Char>(code_point);
  CFStringRef text = CFStringCreateWithBytes(
      kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(&scalar),
      sizeof(scalar), kCFStringEncodingUTF32LE, false);
  if (text == nullptr) return true;

  CTFontRef base =
      CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, 16.0, nullptr);
  CTFontRef used = CTFontCreateForString(
      base, text, CFRangeMake(0, CFStringGetLength(text)));
  bool renderable = true;
  if (used != nullptr) {
    CFStringRef name = CTFontCopyPostScriptName(used);
    if (name != nullptr) {
      renderable = CFStringCompare(name, CFSTR("LastResort"), 0) !=
                   kCFCompareEqualTo;
      CFRelease(name);
    }
    CFRelease(used);
  }
  if (base != nullptr) CFRelease(base);
  CFRelease(text);
  return renderable;
}

}  // namespace internal
}  // namespace mozc
