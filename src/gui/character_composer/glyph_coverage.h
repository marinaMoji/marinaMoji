// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// Which characters the composer can actually show, so that results and
// variants the user could only see as empty boxes can be left out.

#ifndef MOZC_GUI_CHARACTER_COMPOSER_GLYPH_COVERAGE_H_
#define MOZC_GUI_CHARACTER_COMPOSER_GLYPH_COVERAGE_H_

#include <QFont>
#include <QHash>
#include <QRawFont>
#include <QString>
#include <QStringList>
#include <vector>

namespace mozc {
namespace gui {

// A character counts as drawable when, in this order:
//  1. one of |font|'s own families has a glyph for it;
//  2. another installed font made for Han text (Japanese first, then
//     Traditional and Simplified Chinese, then Korean) has one. Qt's fallback
//     may not reach that font, so Check() names it and FontFor() adds it;
//  3. Qt itself finds a glyph when it lays the character out in |font|,
//     which takes in the operating system's own font fallback (fontconfig,
//     Core Text, DirectWrite).
// Answers are cached per string. Fonts installed after the first check are
// only seen after a restart.
class GlyphCoverage {
 public:
  struct Result {
    bool drawable = true;
    // Set in case 2: the family to append to the drawing font.
    QString extra_family;
  };

  explicit GlyphCoverage(const QFont &font);

  // False when nothing draws even a common character (a machine with no CJK
  // fonts, or a headless test run): nothing can be told apart then, and
  // Check() calls everything drawable.
  bool usable() const { return usable_; }

  Result Check(const QString &text);
  bool Drawable(const QString &text) { return Check(text).drawable; }

  // |base| with the extra family Check(text) names, if any, appended.
  QFont FontFor(QFont base, const QString &text);

  // Case 3 alone: whether Qt lays |text| out in |font| without a missing
  // glyph. Exposed for tests.
  static bool QtDraws(const QFont &font, const QString &text);

 private:
  Result Compute(const QString &text);
  void LoadExtraFonts();

  QFont font_;
  std::vector<QRawFont> own_fonts_;
  bool extras_loaded_ = false;
  QStringList extra_families_;
  std::vector<QRawFont> extra_fonts_;  // parallel to |extra_families_|
  bool usable_ = false;
  QHash<QString, Result> cache_;
};

}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_CHARACTER_COMPOSER_GLYPH_COVERAGE_H_
