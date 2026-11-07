// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// Specimen plate kinds and colours shared by the character composer and the
// candidate-window renderers (plain Unicode, missing-font Unicode, SVG).

#ifndef MOZC_BASE_SPECIMEN_KIND_H_
#define MOZC_BASE_SPECIMEN_KIND_H_

#include <cstdint>

namespace mozc {
namespace specimen_kind {

// Matches the composer QSS property values specimenKind="…".
enum class Kind : uint8_t {
  kPlain = 0,
  kMissingFont = 1,
  kSvg = 2,
  // Not a specimen: the Ctrl+2 "Compose…" row, which opens the character
  // composer. Drawn as ordinary text (no plate, no attribute on the wire).
  kComposeAction = 3,
};

// Amber plate: Unicode exists but no installed font can draw it.
inline constexpr uint8_t kMissingFontR = 0x7A;
inline constexpr uint8_t kMissingFontG = 0x64;
inline constexpr uint8_t kMissingFontB = 0x18;

// Prussian blue plate: unencoded SVG drawing.
inline constexpr uint8_t kSvgR = 0x00;
inline constexpr uint8_t kSvgG = 0x31;
inline constexpr uint8_t kSvgB = 0x53;

// Dark mode. The two plates above sink into a dark window (Prussian blue is
// 1.1:1 against the dark composer), so dark mode turns them round: a light
// plate, with the character drawn in the window's own dark navy.
inline constexpr uint8_t kSvgDarkR = 0xFF;  // white
inline constexpr uint8_t kSvgDarkG = 0xFF;
inline constexpr uint8_t kSvgDarkB = 0xFF;
inline constexpr uint8_t kMissingFontDarkR = 0xF3;  // light warning yellow
inline constexpr uint8_t kMissingFontDarkG = 0xD3;
inline constexpr uint8_t kMissingFontDarkB = 0x6B;
inline constexpr uint8_t kDarkInkR = 0x18;  // the dark window colour
inline constexpr uint8_t kDarkInkG = 0x26;
inline constexpr uint8_t kDarkInkB = 0x3B;

inline constexpr char kPlainName[] = "plain";
inline constexpr char kMissingFontName[] = "missingFont";
inline constexpr char kSvgName[] = "svg";

inline const char *Name(Kind kind) {
  switch (kind) {
    case Kind::kMissingFont:
      return kMissingFontName;
    case Kind::kSvg:
      return kSvgName;
    case Kind::kPlain:
    default:
      return kPlainName;
  }
}

inline Kind FromName(const char *name) {
  if (name == nullptr) return Kind::kPlain;
  if (name[0] == 'm') return Kind::kMissingFont;  // missingFont
  if (name[0] == 's') return Kind::kSvg;           // svg
  return Kind::kPlain;
}

// Plate RGB for |kind| in light or dark mode. Returns false for plain (no
// plate).
inline bool PlateRgb(Kind kind, bool dark, uint8_t *r, uint8_t *g,
                     uint8_t *b) {
  switch (kind) {
    case Kind::kMissingFont:
      *r = dark ? kMissingFontDarkR : kMissingFontR;
      *g = dark ? kMissingFontDarkG : kMissingFontG;
      *b = dark ? kMissingFontDarkB : kMissingFontB;
      return true;
    case Kind::kSvg:
      *r = dark ? kSvgDarkR : kSvgR;
      *g = dark ? kSvgDarkG : kSvgG;
      *b = dark ? kSvgDarkB : kSvgB;
      return true;
    case Kind::kPlain:
    default:
      return false;
  }
}

// Light mode: as above.
inline bool PlateRgb(Kind kind, uint8_t *r, uint8_t *g, uint8_t *b) {
  return PlateRgb(kind, /*dark=*/false, r, g, b);
}

// The colour of the character drawn on a plate: white on the light-mode
// plates, the dark window colour on the dark-mode ones.
inline void PlateInkRgb(bool dark, uint8_t *r, uint8_t *g, uint8_t *b) {
  *r = dark ? kDarkInkR : 0xFF;
  *g = dark ? kDarkInkG : 0xFF;
  *b = dark ? kDarkInkB : 0xFF;
}

}  // namespace specimen_kind
}  // namespace mozc

#endif  // MOZC_BASE_SPECIMEN_KIND_H_
