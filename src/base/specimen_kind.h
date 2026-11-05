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
};

// Amber plate: Unicode exists but no installed font can draw it.
inline constexpr uint8_t kMissingFontR = 0x7A;
inline constexpr uint8_t kMissingFontG = 0x64;
inline constexpr uint8_t kMissingFontB = 0x18;

// Prussian blue plate: unencoded SVG drawing.
inline constexpr uint8_t kSvgR = 0x00;
inline constexpr uint8_t kSvgG = 0x31;
inline constexpr uint8_t kSvgB = 0x53;

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

// Plate RGB for |kind|. Returns false for plain (no plate).
inline bool PlateRgb(Kind kind, uint8_t *r, uint8_t *g, uint8_t *b) {
  switch (kind) {
    case Kind::kMissingFont:
      *r = kMissingFontR;
      *g = kMissingFontG;
      *b = kMissingFontB;
      return true;
    case Kind::kSvg:
      *r = kSvgR;
      *g = kSvgG;
      *b = kSvgB;
      return true;
    case Kind::kPlain:
    default:
      return false;
  }
}

}  // namespace specimen_kind
}  // namespace mozc

#endif  // MOZC_BASE_SPECIMEN_KIND_H_
