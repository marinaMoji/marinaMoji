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

// Colours sampled from the harbour splash (ivory paper, Prussian ink).

#ifndef MOZC_GUI_SETUP_WIZARD_SETUP_WIZARD_THEME_H_
#define MOZC_GUI_SETUP_WIZARD_SETUP_WIZARD_THEME_H_

#include <QColor>
#include <QFont>
#include <QString>

namespace mozc {
namespace gui {
namespace setup_theme {

inline const QColor kIvory(0xF4, 0xEB, 0xDD);
inline const QColor kInk(0x1C, 0x2A, 0x3A);
inline const QColor kMuted(0x5C, 0x6B, 0x7A);
inline const QColor kPrussian(0x1E, 0x3A, 0x5F);
inline const QColor kPrussianPressed(0x10, 0x24, 0x3C);
inline const QColor kLine(0xD3, 0xDC, 0xE4);
inline const QColor kDisabled(0x9A, 0xA3, 0xAD);

inline constexpr int kWindowWidth = 760;
inline constexpr int kWindowHeight = 600;
inline constexpr int kCornerRadius = 10;
inline constexpr int kSectionGap = 16;
inline constexpr int kChoiceIndent = 30;

// The point sizes below were chosen on macOS, where a point is one pixel. Qt
// treats a point as 1/72 inch of a 96 dpi screen on Windows and Linux, which
// draws the same numbers a third larger and pushed the pages past the window.
// Scale them so the text is the same pixel size everywhere.
inline qreal PlatformPointSize(int mac_point_size) {
#ifdef __APPLE__
  return mac_point_size;
#else
  return mac_point_size * 0.75;
#endif  // __APPLE__
}

// Website headings (site/user/themes/marinaquark/css/custom.css):
// Hiragino Mincho ProN, then Yu Mincho, Noto Serif JP, Georgia.
// Body text on the site is the system sans-serif. These faces ship with
// macOS, so the wizard does not bundle or download a font.
inline QFont DisplayFont(int point_size) {
  QFont font;
  font.setFamilies({QStringLiteral("Hiragino Mincho ProN"),
                    QStringLiteral("Yu Mincho"),
                    QStringLiteral("Noto Serif JP"),
                    QStringLiteral("Noto Serif CJK JP"),
                    QStringLiteral("Georgia"),
                    QStringLiteral("serif")});
  font.setPointSizeF(PlatformPointSize(point_size));
  font.setWeight(QFont::Medium);
  font.setLetterSpacing(QFont::AbsoluteSpacing, -0.3);
  return font;
}

inline QFont FontAt(int point_size, QFont::Weight weight = QFont::Normal) {
  // An empty family list is the platform UI font: San Francisco on macOS,
  // which is what the website's "-apple-system" stack resolves to. Qt does
  // not understand that CSS name.
  QFont font;
  font.setPointSizeF(PlatformPointSize(point_size));
  font.setWeight(weight);
  return font;
}

inline QFont WelcomeTitleFont() { return DisplayFont(20); }
inline QFont PageTitleFont() { return DisplayFont(27); }
inline QFont ReadyTitleFont() { return DisplayFont(29); }

// Matches slogan_en.svg: Cormorant Garamond Light Italic, open tracking
// (~0.056em). Didot is the usual macOS stand-in when Cormorant is absent.
inline QFont SloganFont() {
  QFont font;
  font.setFamilies({QStringLiteral("Cormorant Garamond"),
                    QStringLiteral("Didot"),
                    QStringLiteral("Bodoni 72"),
                    QStringLiteral("Georgia"),
                    QStringLiteral("serif")});
  // AbsoluteSpacing follows the SVG's ~0.056em tracking (0.056 × 13 ≈ 0.7).
  font.setPointSizeF(PlatformPointSize(13));
  font.setWeight(QFont::Light);
  font.setItalic(true);
  font.setLetterSpacing(QFont::AbsoluteSpacing, 0.7);
  return font;
}
inline QFont SectionFont() { return FontAt(15, QFont::Medium); }
inline QFont BodyFont() { return FontAt(14); }
inline QFont SecondaryFont() { return FontAt(13); }
inline QFont ProgressFont() { return FontAt(12); }
inline QFont JapaneseFont(int point_size) { return DisplayFont(point_size); }

inline QString Stylesheet() {
  return QStringLiteral(
      "QLabel { color: #1C2A3A; background: transparent; }"
      "QLabel#muted { color: #5C6B7A; }"
      "QRadioButton, QCheckBox {"
      "  color: #1C2A3A; background: transparent; spacing: 10px; min-height: 26px; }"
      "QRadioButton:disabled, QCheckBox:disabled { color: #9AA3AD; }"
      "QComboBox {"
      "  color: #1C2A3A; background: #F7F1E8;"
      "  border: 1px solid #C9D3DE; border-radius: 4px;"
      "  padding: 6px 28px 6px 12px; min-height: 24px; }"
      "QComboBox:hover { border-color: #8EA0B3; }"
      "QComboBox:focus { border: 1px solid #1E3A5F; }"
      "QComboBox:disabled { color: #9AA3AD; }"
      "QComboBox::drop-down {"
      "  border: none; background: transparent; width: 24px; }"
      "QComboBox::down-arrow { image: url(:/combo_chevron.png); width: 12px; height: 12px; }"
      "QComboBox QAbstractItemView {"
      "  color: #1C2A3A; background: #F7F1E8; outline: 0;"
      "  selection-background-color: #1E3A5F; selection-color: #F4EBDD; }"
      "QPushButton#primary {"
      "  color: #F4EBDD; background: #1E3A5F; border: 2px solid #1E3A5F;"
      "  border-radius: 4px; padding: 7px 18px; min-height: 20px; }"
      "QPushButton#primary:hover { background: #18324F; border-color: #18324F; }"
      "QPushButton#primary:pressed { background: #10243C; border-color: #10243C; }"
      "QPushButton#primary:focus { border: 2px solid #F4EBDD; }"
      "QPushButton#primary:disabled { background: #9AA3AD; border-color: #9AA3AD; }"
      "QPushButton#quiet {"
      "  color: #5C6B7A; background: transparent; border: 2px solid transparent;"
      "  border-radius: 4px; padding: 7px 12px; min-height: 20px; }"
      "QPushButton#quiet:hover { color: #1C2A3A; }"
      "QPushButton#quiet:focus { color: #1C2A3A; border: 2px solid #C9D3DE; }"
      "QPushButton#closeButton {"
      "  color: #5C6B7A; background: transparent; border: none;"
      "  border-radius: 4px; padding: 0; }"
      "QPushButton#closeButton:hover { color: #1C2A3A; }"
      "QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; }"
      "QFrame#line { background: #D3DCE4; border: none; max-height: 1px; }");
}

}  // namespace setup_theme
}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_SETUP_WIZARD_SETUP_WIZARD_THEME_H_
