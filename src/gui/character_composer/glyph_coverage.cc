// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "gui/character_composer/glyph_coverage.h"

#include <QFontDatabase>
#include <QGlyphRun>
#include <QList>
#include <QRegularExpression>
#include <QTextLayout>
#include <QTextLine>
#include <algorithm>

namespace mozc {
namespace gui {
namespace {

// Variation selectors modify the glyph before them; they have none of their
// own.
bool IsVariationSelector(char32_t cp) {
  return (cp >= 0xFE00 && cp <= 0xFE0F) || (cp >= 0xE0100 && cp <= 0xE01EF);
}

QStringList FamiliesOf(const QFont &font) {
  QStringList families = font.families();
  if (families.isEmpty()) families.append(font.family());
  return families;
}

std::vector<QRawFont> LoadRawFonts(const QStringList &families,
                                   QStringList *loaded) {
  const QStringList installed = QFontDatabase::families();
  std::vector<QRawFont> fonts;
  for (const QString &family : families) {
    if (!installed.contains(family, Qt::CaseInsensitive)) continue;
    QFont probe(family);
    probe.setPixelSize(16);
    const QRawFont raw = QRawFont::fromFont(probe);
    if (!raw.isValid()) continue;
    fonts.push_back(raw);
    if (loaded != nullptr) loaded->append(family);
  }
  return fonts;
}

}  // namespace

QStringList WideCoverageFamilies() {
  static const QStringList families = [] {
    // ExtB-capable fonts first. HanaMinB is not tagged as Japanese in
    // fontconfig, so QFontDatabase::Japanese will not list it — Qt's default
    // merging therefore skips it even when gedit/fontconfig would use it.
    static const char *const kPreferred[] = {
        "HanaMinB",          "HanaMinA",         "Jigmo3",
        "Jigmo2",            "Jigmo",            "BabelStone Han",
        "Plangothic P2",     "Plangothic P1",    "Noto Serif CJK JP",
        "Noto Serif CJK SC", "Noto Serif CJK TC", "Noto Sans CJK JP",
        "Source Han Serif",  "Source Han Sans",  "SimSun-ExtB",
        "MingLiU-ExtB",      "MingLiU_HKSCS-ExtB", "PingFang HK",
        "PingFang TC",       "PingFang SC",      "Songti SC",
        "Songti TC",         "TW-Sung-Ext-B",    "TW-Sung",
    };
    const QStringList installed = QFontDatabase::families();
    QStringList out;
    for (const char *name : kPreferred) {
      const QString family = QString::fromLatin1(name);
      if (installed.contains(family, Qt::CaseInsensitive) &&
          !out.contains(family)) {
        out.append(family);
      }
    }
    static const QRegularExpression kHanLike(
        QStringLiteral(
            "Hana|Jigmo|CJK|\\bHan\\b|Sung|Ming|Song|Plangothic|Heiti"),
        QRegularExpression::CaseInsensitiveOption);
    for (const QString &family : installed) {
      if (!out.contains(family) && kHanLike.match(family).hasMatch()) {
        out.append(family);
      }
    }
    return out;
  }();
  return families;
}

QFont WithWideCoverage(QFont font) {
  QStringList families = FamiliesOf(font);
  for (const QString &family : WideCoverageFamilies()) {
    if (!families.contains(family, Qt::CaseInsensitive)) {
      families.append(family);
    }
  }
  font.setFamilies(families);
  return font;
}

GlyphCoverage::GlyphCoverage(const QFont &font)
    : font_(WithWideCoverage(font)) {
  own_fonts_ = LoadRawFonts(FamiliesOf(font_), nullptr);
  usable_ = Compute(QStringLiteral("木")).drawable;
}

GlyphCoverage::Result GlyphCoverage::Check(const QString &text) {
  if (!usable_) return Result();
  const auto it = cache_.constFind(text);
  if (it != cache_.constEnd()) return *it;
  const Result result = Compute(text);
  cache_.insert(text, result);
  return result;
}

QFont GlyphCoverage::FontFor(QFont base, const QString &text) {
  base = WithWideCoverage(base);
  const Result result = Check(text);
  if (result.extra_family.isEmpty()) return base;
  QStringList families = FamiliesOf(base);
  if (!families.contains(result.extra_family, Qt::CaseInsensitive)) {
    families.append(result.extra_family);
  }
  base.setFamilies(families);
  return base;
}

bool GlyphCoverage::QtDraws(const QFont &font, const QString &text) {
  QTextLayout layout(text, font);
  layout.beginLayout();
  QTextLine line = layout.createLine();
  if (line.isValid()) line.setLineWidth(1.0e6);
  layout.endLayout();
  if (!line.isValid()) return false;
  // Font merging puts each glyph in the first font, of |font|'s families and
  // then the system's fallbacks, that has it. Glyph 0 (.notdef) is what is
  // left when none does. On Linux, "system fallbacks" often miss ExtB fonts
  // that are not in |font|'s family list — hence WithWideCoverage.
  bool any = false;
  for (const QGlyphRun &run : layout.glyphRuns()) {
    for (const quint32 glyph : run.glyphIndexes()) {
      if (glyph == 0) return false;
      any = true;
    }
  }
  return any;
}

GlyphCoverage::Result GlyphCoverage::Compute(const QString &text) {
  std::vector<char32_t> cps;
  QString base;
  for (const char32_t cp : text.toUcs4()) {
    if (IsVariationSelector(cp)) continue;
    cps.push_back(cp);
    base.append(QString::fromUcs4(&cp, 1));
  }
  Result result;
  if (cps.empty()) return result;

  // 1. The drawing font's own families (Qt merges them glyph by glyph).
  const bool own = std::all_of(cps.begin(), cps.end(), [this](char32_t cp) {
    return std::any_of(own_fonts_.begin(), own_fonts_.end(),
                       [cp](const QRawFont &raw) {
                         return raw.supportsCharacter(static_cast<uint>(cp));
                       });
  });
  if (own) return result;

  // 2. Another installed Han font, named so that Qt uses it.
  LoadExtraFonts();
  for (size_t i = 0; i < extra_fonts_.size(); ++i) {
    const QRawFont &raw = extra_fonts_[i];
    if (std::all_of(cps.begin(), cps.end(), [&raw](char32_t cp) {
          return raw.supportsCharacter(static_cast<uint>(cp));
        })) {
      result.extra_family = extra_families_[i];
      return result;
    }
  }

  // 3. Whatever Qt and the system fallback find.
  result.drawable = QtDraws(font_, base);
  return result;
}

void GlyphCoverage::LoadExtraFonts() {
  if (extras_loaded_) return;
  extras_loaded_ = true;
  const QStringList own = FamiliesOf(font_);
  QStringList candidates;
  // Writing-system lists miss some ExtB fonts (e.g. HanaMinB); those are
  // already in |font_| via WideCoverageFamilies. Still scan JP/ZH/KO for
  // anything else installed.
  for (const QFontDatabase::WritingSystem system :
       {QFontDatabase::Japanese, QFontDatabase::TraditionalChinese,
        QFontDatabase::SimplifiedChinese, QFontDatabase::Korean}) {
    for (const QString &family : QFontDatabase::families(system)) {
      if (own.contains(family, Qt::CaseInsensitive) ||
          candidates.contains(family, Qt::CaseInsensitive)) {
        continue;
      }
      candidates.append(family);
    }
  }
  extra_fonts_ = LoadRawFonts(candidates, &extra_families_);
}

}  // namespace gui
}  // namespace mozc
