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

// Visual tokens for the Preferences window (Phase 1 typographic refinement).
//
// Companion specs:
//   visual_style/docs/settings/preferences-design.md
//   visual_style/docs/settings/preferences-refinement.md
// Prussian blue is the harbour-splash sample already used by the setup wizard
// (#1E3A5F). Surfaces stay white in light mode; ivory paper is not reused.

#ifndef MOZC_GUI_CONFIG_DIALOG_CONFIG_DIALOG_THEME_H_
#define MOZC_GUI_CONFIG_DIALOG_CONFIG_DIALOG_THEME_H_

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QLabel>
#include <QPalette>
#include <QSizePolicy>
#include <QString>
#include <QStyleHints>
#include <QtGlobal>

namespace mozc {
namespace gui {
namespace config_dialog_theme {

// On macOS a point is one pixel. Qt treats a point as 1/72 inch of a 96 dpi
// screen on Windows and Linux, which draws the same number a third larger.
inline qreal PlatformPointSize(int mac_point_size) {
#ifdef __APPLE__
  return mac_point_size;
#else
  return mac_point_size * 0.75;
#endif  // __APPLE__
}

// Shared spacing scale (logical px at the macOS reference). Prefer these over
// ad-hoc margins so pages share one vertical rhythm.
struct Spacing {
  static constexpr int unit = 4;
  static constexpr int content_inset_l = 32;
  static constexpr int content_inset_r = 28;
  static constexpr int content_inset_t = 22;
  static constexpr int content_inset_b = 8;
  static constexpr int page_right_gutter = 12;  // clears scrollbar
  static constexpr int page_bottom_pad = 32;    // last control → window edge
  static constexpr int title_to_rule = 10;
  static constexpr int rule_to_section = 18;    // title → first section ≈ 28
  static constexpr int section_to_control = 16; // Level 2 → first control
  static constexpr int subhead_to_control = 14; // Level 3 → first control
  static constexpr int heading_rule_gap = 14;   // Level 2 text → rule
  static constexpr int row = 10;
  static constexpr int related = 8;
  static constexpr int label_to_help = 6;
  static constexpr int before_section = 28;     // last control → Level 2
  static constexpr int before_subhead = 24;     // last control → Level 3
  static constexpr int sidebar_width = 220;
  static constexpr int sidebar_pad_l = 18;
  static constexpr int sidebar_pad_r = 16;
  static constexpr int sidebar_pad_t = 24;
  static constexpr int sidebar_pad_b = 28;      // utilities → bottom
  static constexpr int logo_to_prefs = 18;      // brand → Preferences
  static constexpr int prefs_to_nav = 26;       // Preferences → Writing
  static constexpr int logo_right_clearance = 22;
  static constexpr int nav_row = 38;
  static constexpr int utility_row = 22;
  static constexpr int utility_gap = 6;         // between utility labels
  static constexpr int utility_rule_gap = 18;   // separator → first utility
  static constexpr int info_gap = 6;            // label text → ⓘ
};

struct Type {
  static constexpr int page_title = 34;  // Level 1 — serif chapter title
  static constexpr int section = 19;     // Level 2 — primary section
  static constexpr int subhead = 16;     // Level 3 — subsection
  static constexpr int label = 15;       // Level 4 — setting label
  static constexpr int secondary = 13;
  static constexpr int brand_sub = 12;
  static constexpr int nav = 14;
  static constexpr int utility = 13;
};

inline QFont DisplayFont(int mac_point_size) {
  QFont font;
  font.setFamilies({QStringLiteral("Hiragino Mincho ProN"),
                    QStringLiteral("Yu Mincho"),
                    QStringLiteral("Noto Serif JP"),
                    QStringLiteral("Noto Serif CJK JP"),
                    QStringLiteral("Georgia"),
                    QStringLiteral("serif")});
  font.setPointSizeF(PlatformPointSize(mac_point_size));
  font.setWeight(QFont::Medium);
  return font;
}

inline QFont PageTitleFont() {
  QFont font = DisplayFont(Type::page_title);
  // Slight tracking reads as typeset chapter titles without looking spaced-out.
  font.setLetterSpacing(QFont::PercentageSpacing, 102.0);
  return font;
}

inline QFont UiFont(int mac_point_size, QFont::Weight weight = QFont::Normal) {
  QFont font;
  font.setFamilies({QStringLiteral("Hiragino Sans"),
                    QStringLiteral("Hiragino Kaku Gothic ProN"),
                    QStringLiteral("Yu Gothic UI"),
                    QStringLiteral("Yu Gothic"),
                    QStringLiteral("Noto Sans CJK JP"),
                    QStringLiteral("Noto Sans JP"),
                    QStringLiteral("Segoe UI"),
                    QStringLiteral("sans-serif")});
  font.setPointSizeF(PlatformPointSize(mac_point_size));
  font.setWeight(weight);
  return font;
}

inline QFont SectionFont() {
  // Level 2: medium (~500), larger than labels.
  return UiFont(Type::section, QFont::Medium);
}

inline QFont SubheadFont() {
  // Level 3: semibold (~600) so subsections read as headings, not labels.
  return UiFont(Type::subhead, QFont::DemiBold);
}

inline QFont LabelFont() {
  return UiFont(Type::label, QFont::Normal);
}

// Ascender/descender room for heading QLabels (avoids vertical clipping).
inline void EnsureHeadingMetrics(QLabel* label) {
  if (label == nullptr) {
    return;
  }
  const QFontMetrics metrics(label->font());
  // height() already includes leading; a small extra guards CJK/serif faces.
  label->setMinimumHeight(metrics.height() + 2);
  label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
}

inline bool IsDark() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
  return QGuiApplication::palette().color(QPalette::Window).lightness() < 128;
#endif
}

struct Palette {
  QColor window;
  QColor sidebar;
  QColor raised;
  QColor text;
  QColor heading;
  QColor secondary;
  QColor disabled;
  QColor rule;
  QColor title_rule;
  QColor border_control;
  QColor nav_selected;
  QColor nav_hover;
  QColor accent;
  QColor accent_subtle;
  QColor on_accent;
  QColor primary;
  QColor primary_hover;
  QColor primary_pressed;
  QColor focus;
  QColor selection_bg;
  QColor selection_text;
  QColor warning;
  QColor check_border;
  QColor utility;  // muted sidebar utility labels
};

inline Palette Colors(bool dark) {
  if (dark) {
    return Palette{
        QColor(0x18, 0x26, 0x3B),  // window
        QColor(0x11, 0x1D, 0x30),  // sidebar
        QColor(0x22, 0x35, 0x50),  // raised
        QColor(0xF4, 0xF7, 0xFB),  // text
        QColor(0xF4, 0xF7, 0xFB),  // heading
        QColor(0xA9, 0xB9, 0xCE),  // secondary
        QColor(0x6E, 0x7F, 0x94),  // disabled
        QColor(0x34, 0x47, 0x61),  // rule
        QColor(0x4A, 0x60, 0x7C),  // title_rule (slightly stronger)
        QColor(0x4A, 0x60, 0x7C),  // border_control
        QColor(0x1A, 0x2C, 0x44),  // nav_selected (quiet wash)
        QColor(0x16, 0x24, 0x38),  // nav_hover
        QColor(0x94, 0xB8, 0xE5),  // accent
        QColor(0x2A, 0x40, 0x5C),  // accent_subtle
        QColor(0x11, 0x1D, 0x30),  // on_accent
        QColor(0x94, 0xB8, 0xE5),  // primary
        QColor(0xC5, 0xD7, 0xEE),  // primary_hover
        QColor(0x7A, 0xA3, 0xD4),  // primary_pressed
        QColor(0xC5, 0xD7, 0xEE),  // focus
        QColor(0x30, 0x4B, 0x6C),  // selection_bg
        QColor(0xF4, 0xF7, 0xFB),  // selection_text
        QColor(0xE0, 0xA4, 0x56),  // warning
        QColor(0x5A, 0x70, 0x8C),  // check_border
        QColor(0x7A, 0x8C, 0xA0),  // utility
    };
  }
  return Palette{
      QColor(0xFF, 0xFF, 0xFF),  // window
      QColor(0xF3, 0xF6, 0xFA),  // sidebar
      QColor(0xFF, 0xFF, 0xFF),  // raised
      QColor(0x27, 0x31, 0x3D),  // text
      QColor(0x1E, 0x3A, 0x5F),  // heading
      QColor(0x5A, 0x6A, 0x7C),  // secondary
      QColor(0x9A, 0xA3, 0xAD),  // disabled
      QColor(0xD7, 0xDE, 0xE7),  // rule (hairline blue-grey)
      QColor(0xC5, 0xD0, 0xDC),  // title_rule
      QColor(0xC5, 0xD0, 0xDC),  // border_control
      QColor(0x00, 0x00, 0x00, 0x00),  // nav_selected (transparent; bar carries selection)
      QColor(0xEA, 0xF0, 0xF6),  // nav_hover
      QColor(0x1E, 0x3A, 0x5F),  // accent
      QColor(0xE5, 0xED, 0xF7),  // accent_subtle
      QColor(0xFF, 0xFF, 0xFF),  // on_accent
      QColor(0x1E, 0x3A, 0x5F),  // primary
      QColor(0x18, 0x32, 0x4F),  // primary_hover
      QColor(0x10, 0x24, 0x3C),  // primary_pressed
      QColor(0x5B, 0x8F, 0xC7),  // focus
      QColor(0xE5, 0xED, 0xF7),  // selection_bg
      QColor(0x1E, 0x3A, 0x5F),  // selection_text
      QColor(0xB4, 0x53, 0x09),  // warning
      QColor(0xA8, 0xB4, 0xC2),  // check_border
      QColor(0x64, 0x7B, 0x91),  // utility (#647B91)
  };
}

inline QString Hex(const QColor& color) {
  if (color.alpha() < 255) {
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(color.alpha());
  }
  return color.name(QColor::HexRgb);
}

inline QString Stylesheet(bool dark) {
  const Palette palette = Colors(dark);
  QString qss = QStringLiteral(
      "QDialog#ConfigDialog { background-color: @@WINDOW@@; }"
      "QWidget#preferencesSidebar { background-color: @@SIDEBAR@@; }"
      "QWidget#preferencesContent,"
      "QWidget#preferencesBody { background-color: @@WINDOW@@; }"

      "QLabel {"
      "  color: @@TEXT@@; background: transparent;"
      "  padding-top: 1px; padding-bottom: 1px; }"
      "QLabel:disabled { color: @@DISABLED@@; }"
      "QLabel#preferencesBrand { color: @@HEADING@@; background: transparent; }"
      "QLabel#preferencesBrandSub {"
      "  color: @@SECONDARY@@; background: transparent;"
      "  letter-spacing: 0.04em; }"
      "QLabel#preferencesPageTitle {"
      "  color: @@HEADING@@; background: transparent;"
      "  padding: 0; margin: 0; }"
      "QLabel#preferencesHelp,"
      "QLabel#incognitoModeMessage,"
      "QLabel#usageStatsMessage,"
      "QLabel#preferencesSecondary {"
      "  color: @@SECONDARY@@; }"
      "QLabel#preferencesWarning { color: @@WARNING@@; }"
      "QLabel#preferencesSubhead {"
      "  color: @@HEADING@@; margin: 0; padding: 0; }"
      "QFrame[class=\"setting-group-header\"] {"
      "  margin: 0; padding: 0; }"
      "QFrame[class=\"setting-group-header\"] QLabel {"
      "  color: @@HEADING@@; margin: 0; padding: 0; }"

      "QFrame[class=\"setting-group-line\"],"
      "QFrame#preferencesSidebarRule,"
      "QFrame#preferencesTitleRule {"
      "  border: none; }"
      "QFrame[class=\"setting-group-line\"] {"
      "  color: @@RULE@@; background-color: @@RULE@@; max-height: 1px; }"
      "QFrame#preferencesTitleRule {"
      "  color: @@TITLE_RULE@@; background-color: @@TITLE_RULE@@;"
      "  max-height: 1px; }"
      "QFrame#preferencesSidebarRule {"
      "  color: @@RULE@@; background-color: @@RULE@@; max-width: 1px; }"

      /* Sidebar nav: table-of-contents — left hairline, no outlined pill. */
      "QListWidget#preferencesNav {"
      "  background: transparent; border: none; outline: none; }"
      "QListWidget#preferencesNav::item {"
      "  color: @@TEXT@@; background: transparent;"
      "  padding: 9px 10px 9px 12px;"
      "  border: none; border-left: 2px solid transparent;"
      "  border-radius: 0px; margin: 1px 0; }"
      "QListWidget#preferencesNav::item:hover {"
      "  background: @@NAV_HOVER@@; color: @@HEADING@@; }"
      "QListWidget#preferencesNav::item:selected {"
      "  background: @@NAV_SELECTED@@; color: @@ACCENT@@;"
      "  font-weight: 600; border-left: 2px solid @@ACCENT@@; }"
      "QListWidget#preferencesNav::item:selected:hover {"
      "  background: @@NAV_HOVER@@; }"
      "QListWidget#preferencesNav::item:focus {"
      "  background: @@NAV_HOVER@@;"
      "  border-left: 2px solid @@FOCUS@@; }"
      "QListWidget#preferencesNav::item:selected:focus {"
      "  border-left: 2px solid @@ACCENT@@;"
      "  background: @@ACCENT_SUBTLE@@; }"

      "QFrame#preferencesUtilityRule {"
      "  color: @@RULE@@; background-color: @@RULE@@; max-height: 1px;"
      "  border: none; }"
      /* Quiet utilities: same left text axis as nav (2px + 12px), no pill. */
      "QPushButton#preferencesUtilityButton {"
      "  color: @@UTILITY@@; background: transparent;"
      "  border: none; border-left: 2px solid transparent; border-radius: 0;"
      "  text-align: left; padding: 2px 8px 2px 12px; font-weight: 400; }"
      "QPushButton#preferencesUtilityButton:hover {"
      "  color: @@ACCENT@@; background: transparent; }"
      "QPushButton#preferencesUtilityButton:focus {"
      "  color: @@ACCENT@@; background: transparent;"
      "  border-left: 2px solid @@FOCUS@@; }"
      "QPushButton#preferencesUtilityButton[active=\"true\"] {"
      "  color: @@ACCENT@@; font-weight: 400; background: transparent; }"
      "QPushButton#preferencesUtilityButton[active=\"true\"]:hover {"
      "  color: @@ACCENT@@; background: transparent; }"
      "QToolButton#preferencesInfoButton {"
      "  background: transparent; border: none; padding: 0; margin: 0; }"
      "QFrame#preferencesExplanationPopover {"
      "  background: @@RAISED@@; border: 1px solid @@BORDER_CONTROL@@;"
      "  border-radius: 4px; }"
      "QLabel#preferencesExplanationTitle { color: @@HEADING@@; }"
      "QLabel#preferencesExplanationBody { color: @@TEXT@@; }"
      "QLabel#preferencesExplanationExample {"
      "  color: @@SECONDARY@@; padding-top: 4px; }"

      "QComboBox {"
      "  color: @@TEXT@@; background: @@RAISED@@;"
      "  border: 1px solid @@BORDER_CONTROL@@; border-radius: 3px;"
      "  padding: 6px 28px 6px 11px; min-height: 22px; }"
      "QComboBox:hover { border-color: @@ACCENT@@; }"
      "QComboBox:focus { border: 1px solid @@FOCUS@@; }"
      "QComboBox:disabled {"
      "  color: @@DISABLED@@; border-color: @@RULE@@; }"
      "QComboBox::drop-down {"
      "  border: none; background: transparent; width: 22px; }"
      "QComboBox::down-arrow {"
      "  image: url(:/combo_chevron.png); width: 9px; height: 9px; }"
      "QComboBox QAbstractItemView {"
      "  color: @@TEXT@@; background: @@RAISED@@; outline: 0;"
      "  border: 1px solid @@BORDER_CONTROL@@;"
      "  selection-background-color: @@SELECTION_BG@@;"
      "  selection-color: @@SELECTION_TEXT@@;"
      "  padding: 4px; }"

      "QSpinBox {"
      "  color: @@TEXT@@; background: @@RAISED@@;"
      "  border: 1px solid @@BORDER_CONTROL@@; border-radius: 3px;"
      "  padding: 5px 8px; min-height: 20px; }"
      "QSpinBox:hover { border-color: @@ACCENT@@; }"
      "QSpinBox:focus { border: 1px solid @@FOCUS@@; }"
      "QSpinBox:disabled { color: @@DISABLED@@; border-color: @@RULE@@; }"

      "QCheckBox {"
      "  color: @@TEXT@@; spacing: 9px;"
      "  margin-left: 0; padding-left: 0;"
      "  min-height: 1.45em; padding-top: 2px; padding-bottom: 2px; }"
      "QCheckBox:disabled { color: @@DISABLED@@; }"
      "QCheckBox::indicator {"
      "  width: 15px; height: 15px;"
      "  border: 1px solid @@CHECK_BORDER@@; border-radius: 2px;"
      "  background: @@RAISED@@; }"
      "QCheckBox::indicator:hover { border-color: @@ACCENT@@; }"
      "QCheckBox::indicator:checked {"
      "  background: @@ACCENT@@; border-color: @@ACCENT@@;"
      "  image: url(:/checkbox_check.png); }"
      "QCheckBox::indicator:disabled {"
      "  background: @@WINDOW@@; border-color: @@RULE@@; }"
      "QCheckBox::indicator:checked:disabled {"
      "  background: @@DISABLED@@; border-color: @@DISABLED@@;"
      "  image: url(:/checkbox_check.png); }"

      "QPushButton {"
      "  color: @@TEXT@@; background: @@RAISED@@;"
      "  border: 1px solid @@BORDER_CONTROL@@; border-radius: 3px;"
      "  min-height: 1.35em; padding: 6px 13px; }"
      "QPushButton:hover { border-color: @@ACCENT@@; }"
      "QPushButton:focus { border: 1px solid @@FOCUS@@; }"
      "QPushButton:pressed { background: @@ACCENT_SUBTLE@@; }"
      "QPushButton:disabled { color: @@DISABLED@@; border-color: @@RULE@@; }"
      "QPushButton[role=\"primary\"] {"
      "  color: @@ON_ACCENT@@; background: @@PRIMARY@@;"
      "  border: 1px solid @@PRIMARY@@; padding: 6px 16px; }"
      "QPushButton[role=\"primary\"]:hover {"
      "  background: @@PRIMARY_HOVER@@; border-color: @@PRIMARY_HOVER@@; }"
      "QPushButton[role=\"primary\"]:pressed {"
      "  background: @@PRIMARY_PRESSED@@; border-color: @@PRIMARY_PRESSED@@; }"
      "QPushButton[role=\"primary\"]:focus { border: 2px solid @@FOCUS@@; }"
      "QPushButton[role=\"primary\"]:disabled {"
      "  color: @@WINDOW@@; background: @@DISABLED@@; border-color: @@DISABLED@@; }"
      "QPushButton[role=\"secondary\"] {"
      "  color: @@ACCENT@@; background: @@WINDOW@@;"
      "  border: 1px solid @@BORDER_CONTROL@@; padding: 6px 14px; }"
      "QPushButton[role=\"secondary\"]:hover {"
      "  border-color: @@ACCENT@@; background: @@ACCENT_SUBTLE@@; }"
      "QPushButton[role=\"secondary\"]:focus { border: 1px solid @@FOCUS@@; }"
      "QPushButton[role=\"secondary\"]:disabled {"
      "  color: @@DISABLED@@; border-color: @@RULE@@; }"
      "QPushButton[role=\"quiet\"] {"
      "  color: @@SECONDARY@@; background: transparent;"
      "  border: 1px solid transparent; padding: 6px 10px; }"
      "QPushButton[role=\"quiet\"]:hover { color: @@HEADING@@; }"
      "QPushButton[role=\"quiet\"]:focus {"
      "  color: @@HEADING@@; border: 1px solid @@RULE@@; }"
      "QPushButton[role=\"quiet\"]:disabled { color: @@DISABLED@@; }"
      "QPushButton[role=\"disclosure\"] {"
      "  color: @@ACCENT@@; background: transparent;"
      "  border: none; padding: 2px 0; text-align: left; }"
      "QPushButton[role=\"disclosure\"]:hover { color: @@HEADING@@; }"
      "QPushButton[role=\"disclosure\"]:focus {"
      "  border: 1px solid @@FOCUS@@; border-radius: 2px; padding: 2px 4px; }"

      "QLineEdit {"
      "  color: @@TEXT@@; background: @@RAISED@@;"
      "  border: 1px solid @@BORDER_CONTROL@@; border-radius: 3px;"
      "  padding: 6px 9px; selection-background-color: @@SELECTION_BG@@;"
      "  selection-color: @@SELECTION_TEXT@@; }"
      "QLineEdit:hover { border-color: @@ACCENT@@; }"
      "QLineEdit:focus { border: 1px solid @@FOCUS@@; }"

      "QHeaderView::section {"
      "  color: @@HEADING@@; background: @@WINDOW@@;"
      "  border: none; border-bottom: 1px solid @@RULE@@;"
      "  padding: 7px 8px; font-weight: 600; }"
      "QTableWidget {"
      "  color: @@TEXT@@; background: @@WINDOW@@; gridline-color: @@RULE@@;"
      "  border: 1px solid @@RULE@@; border-radius: 3px;"
      "  selection-background-color: @@SELECTION_BG@@;"
      "  selection-color: @@SELECTION_TEXT@@; }"
      "QTableWidget::item { padding: 5px 8px; }"
      "QTableWidget::item:focus { border: 1px solid @@FOCUS@@; }"

      "QScrollArea#preferencesScroll { background: @@WINDOW@@; border: none; }"
      "QScrollArea#preferencesScroll > QWidget > QWidget {"
      "  background: transparent; }"
      "QScrollBar:vertical {"
      "  background: @@WINDOW@@; width: 11px; margin: 0; border: none; }"
      "QScrollBar::handle:vertical {"
      "  background: @@BORDER_CONTROL@@; min-height: 28px; border-radius: 4px;"
      "  margin: 2px; }"
      "QScrollBar::handle:vertical:hover { background: @@SECONDARY@@; }"
      "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
      "  height: 0; background: none; border: none; }"
      "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
      "  background: none; }"
      "QScrollBar:horizontal {"
      "  background: @@WINDOW@@; height: 11px; margin: 0; border: none; }"
      "QScrollBar::handle:horizontal {"
      "  background: @@BORDER_CONTROL@@; min-width: 28px; border-radius: 4px;"
      "  margin: 2px; }"
      "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
      "  width: 0; background: none; border: none; }"
      "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {"
      "  background: none; }"

      "QTabWidget::pane { border: none; background: transparent; }"
      "QGroupBox {"
      "  color: @@HEADING@@; border: none; margin-top: 0.6em;"
      "  padding-top: 0.9em; }"
      "QGroupBox::title { subcontrol-origin: margin; left: 0; padding: 0; }");

  const struct {
    const char* token;
    QColor color;
  } tokens[] = {
      {"@@WINDOW@@", palette.window},
      {"@@SIDEBAR@@", palette.sidebar},
      {"@@RAISED@@", palette.raised},
      {"@@TEXT@@", palette.text},
      {"@@HEADING@@", palette.heading},
      {"@@SECONDARY@@", palette.secondary},
      {"@@DISABLED@@", palette.disabled},
      {"@@RULE@@", palette.rule},
      {"@@TITLE_RULE@@", palette.title_rule},
      {"@@BORDER_CONTROL@@", palette.border_control},
      {"@@NAV_SELECTED@@", palette.nav_selected},
      {"@@NAV_HOVER@@", palette.nav_hover},
      {"@@ACCENT@@", palette.accent},
      {"@@ACCENT_SUBTLE@@", palette.accent_subtle},
      {"@@ON_ACCENT@@", palette.on_accent},
      {"@@PRIMARY@@", palette.primary},
      {"@@PRIMARY_HOVER@@", palette.primary_hover},
      {"@@PRIMARY_PRESSED@@", palette.primary_pressed},
      {"@@FOCUS@@", palette.focus},
      {"@@SELECTION_BG@@", palette.selection_bg},
      {"@@SELECTION_TEXT@@", palette.selection_text},
      {"@@WARNING@@", palette.warning},
      {"@@CHECK_BORDER@@", palette.check_border},
      {"@@UTILITY@@", palette.utility},
  };
  for (const auto& token : tokens) {
    qss.replace(QLatin1String(token.token), Hex(token.color));
  }
  return qss;
}

}  // namespace config_dialog_theme
}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_CONFIG_DIALOG_CONFIG_DIALOG_THEME_H_
