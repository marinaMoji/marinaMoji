// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "gui/character_composer/character_composer_dialog.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QEvent>
#include <QFocusEvent>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputMethod>
#include <QLabel>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QFontMetricsF>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QScreen>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "composer_ids/char_info.h"
#include "composer_ids/ids_operator.h"
#include "composer_ids/ids_table.h"
#include "composer_ids/ids_tree.h"
#include "base/specimen_kind.h"
#include "config/config_handler.h"
#include "gui/character_composer/composer_kage.h"
#include "gui/character_composer/composer_svg.h"
#include "gui/character_composer/glyph_coverage.h"
#include "gui/character_composer/glyphwiki_client.h"
#include "gui/character_composer/layout_geometry.h"
#include "gui/config_dialog/config_dialog_theme.h"
#include "protocol/config.pb.h"

namespace mozc {
namespace gui {

namespace composer_dialog_metrics {
// Slightly under the previous 216px editor so the default window can sit closer
// to a typical metadata table without starving nested editing.
constexpr int kEditorHeight = 188;
// The structure buttons: a small layout icon over a sample character.
constexpr int kStructureButtonWidth = 32;
constexpr int kStructureButtonHeight = 44;
constexpr int kStructureIcon = 20;
// The Construct column keeps this width; the other two share the rest.
constexpr int kConstructWidth = 236;
constexpr int kPreviewLogical = 152;
constexpr int kDefaultWidth = 960;
constexpr int kDefaultHeight = 530;
constexpr int kMinimumWidth = 860;
constexpr int kMinimumHeight = 480;
}  // namespace composer_dialog_metrics

namespace {

using composer_ids::ComposerSlot;
using composer_ids::SlotPath;
namespace theme = config_dialog_theme;
using composer_dialog_metrics::kEditorHeight;
using composer_dialog_metrics::kStructureButtonHeight;
using composer_dialog_metrics::kStructureButtonWidth;
using composer_dialog_metrics::kStructureIcon;
using composer_dialog_metrics::kPreviewLogical;

constexpr char kContext[] = "mozc::gui::CharacterComposerDialog";

QString Tr(const char *source) {
  return QCoreApplication::translate(kContext, source);
}

QString FromUtf8(const std::string &s) { return QString::fromUtf8(s.data(), s.size()); }

std::string ToUtf8(const QString &s) {
  const QByteArray bytes = s.toUtf8();
  return std::string(bytes.constData(), bytes.size());
}

// The coverage of the composer's character tiles, shared by the results,
// the specimen and the variants row. Left alive to the end: QRawFont must not
// outlive the application object. GlyphCoverage already appends ExtB fonts
// (HanaMinB, …) that Qt would otherwise skip on Linux.
GlyphCoverage &Coverage() {
  static GlyphCoverage *const coverage =
      new GlyphCoverage(QApplication::font());
  return *coverage;
}

constexpr char kFontsGuideUrl[] =
    "https://marinamoji.crcao.fr/guide#section_fonts";

QString CodepointHeading(const QString &text) {
  const QList<uint> cps = text.toUcs4();
  if (cps.isEmpty()) return QString();
  return QStringLiteral("U+%1")
      .arg(cps.first(), 4, 16, QLatin1Char('0'))
      .toUpper();
}

QString MissingFontDetail() {
  return Tr("This character exists in Unicode, but no installed font can draw "
            "it. Insert can still place a drawing.") +
         QStringLiteral(" <a href=\"%1\">%2</a>")
             .arg(QString::fromUtf8(kFontsGuideUrl), Tr("Suggested fonts"));
}

// Applies the Character specimen plate kind (plain / missingFont / svg) and
// the compose-prompt styling. Coloured plates need a real background paint.
void ApplySpecimenPresentation(QLabel *label, const QString &kind, bool prompt) {
  const QString prev_kind = label->property("specimenKind").toString();
  const bool prev_prompt = label->property("prompt").toBool();
  if (prev_kind == kind && prev_prompt == prompt) return;
  label->setProperty("specimenKind", kind);
  label->setProperty("prompt", prompt);
  // Do not touch Qt::WA_TranslucentBackground here: once a widget has had it
  // set, its stylesheet background (the amber / Prussian-blue plate) is never
  // painted again, even if the attribute is cleared. Plain specimens are
  // transparent through the stylesheet instead.
  label->setWordWrap(prompt);
  label->setAlignment(prompt ? Qt::AlignHCenter | Qt::AlignVCenter
                             : Qt::AlignHCenter | Qt::AlignBottom);
  label->style()->unpolish(label);
  label->style()->polish(label);
}

// What the Components… window offers: common components, by stroke count.
struct ComponentGroup {
  int strokes;
  const char *components;  // UTF-8, one character each
};
constexpr ComponentGroup kCommonComponentGroups[] = {
    {1, "一丨丿丶乙亅"},
    {2, "二亠人亻𠆢儿入八冂冖冫几凵刀刂力勹匕匚十卜卩厂厶又廴"},
    {3, "口囗土士夂夕大女子宀寸小尸山川工己巾干广弓彡彳忄扌氵犭艹辶阝"},
    {4, "心戈戸手支攵文斗斤方日曰月木欠止歹殳毋比毛氏水火灬爪父片牛犬王礻耂"},
    {5, "玄瓦甘生用田疋疒白皮皿目矛矢石示禾穴立衤"},
    {6, "竹⺮米糸缶羊羽老而耳聿肉臣自至舌舟艮色虍虫血行衣西"},
    {7, "見角言谷豆豕貝赤走足身車辛辰酉里"},
    {8, "金長門隹雨青非"},
    {9, "面革音頁風飛食首香骨鬼"},
    {10, "馬高髟"},
    {11, "魚鳥鹿麻黄黒"},
    {14, "鼻"},
    {16, "龍"},
};
// Components that are hard to type, from the order of zi.tools' table of difficult
// components (the common ones above are left out): stroke count, first stroke
// (1 橫, 2 豎, 3 撇, 4 點, 5 折, as in IdsTable::LeadingStroke), components.
struct HardGroup {
  int strokes;
  int lead;
  const char *components;
};
constexpr HardGroup kHardComponentGroups[] = {
    {1, 4, "乀"},
    {1, 5, "乛𠃍𠃌㇇㇈𠄎㇁乚𠃊㇄𠄌𠃑㇉𠃋𡿨㇂"},
    {2, 1, "丅𬺰丆𠂇丂匸𠀁𫠠"},
    {2, 2, "⺊丄𠃎〢リ"},
    {2, 3, "𠂉𰀪𠂆乂㐅⺈𠂊𠤎⺆"},
    {2, 4, "丷𬼀⺀讠"},
    {2, 5, "コユ𰆊丩𠂈龴㔾巜𢎘"},
    {3, 1, "亍𰀁𰀄亐卄𰀃廾尢𠆣𬺢夨㐄𠫓𠫔𡕒"},
    {3, 2, "〣𡭔𰀠𣥂⺌𠔼𠀃"},
    {3, 3, "乇𫶧𰃦𠂋亼亽𠓛饣夊亇𠂎𠚤𱍸"},
    {3, 4, "丬䒑𭕄"},
    {3, 5, "𫜹⺕彐卂卪屮𠃓亾乆彑𫝀纟巛"},
    {4, 1, "龶龷卝廿𠮛帀朩𡭕丏厷旡𢆰"},
    {4, 2, "攴龰𰀡𠔿⺝冄冃𫩏𦉪禸𦉫𠁣"},
    {4, 3, "壬𡈼龵𠂒丯爫爻仌尣仒⺼𧘇𠂔厃卬卆𱼀"},
    {4, 4, "冘㓁"},
    {4, 5, "夬肀弔𢀳𠃜丮𠬝爿㞢刅𠬞丱㣺厸"},
    {5, 1, "𠀎𡗗戋𰀉𫇦𣄼圥朮𤴓卌𪩲犮戊𡗜𬺻戉"},
    {5, 2, "歺丗龸罒冋曳𠕁龱𪠲𠀐𦉰囙𮍌"},
    {5, 3, "钅㐌㐱尒㕣氐卯夗刍𠂡夘𬼉"},
    {5, 4, "𢆉宂𰃮"},
    {5, 5, "㠯㞋𡰪氶𤴔癶𢀖𠬤氺弁"},
    {6, 1, "耒幵㓞覀𢦏朿襾巩𤰔㐁𠀠"},
    {6, 2, "尗吅𠕋"},
    {6, 3, "𥫗𦥑𠂤囟甶𠂢𧰨乑𠇍𠂭𠫤舛𪧷夅𫥞"},
    {6, 4, "𭤨㐫𣅀𰁜龹𦍌屰"},
    {6, 5, "帇𢑑艸劦叒䏍厽𢆶"},
    {7, 1, "孛𦣞𦔮耴𡉵𢦒尨𦣻丣巠𫠩"},
    {7, 2, "镸𣦼𧾷邑肙圼冏囧甹"},
    {7, 3, "𦈢𭻾攸䖝𦥔皃豸釆寽佥夆"},
    {7, 4, "㐬"},
    {7, 5, "𠬶𠃬戼夋矣𣧄"},
    {8, 1, "忝枼夌𦭝𠦝臤坴亟叀𣏟豖㚔𡘆戔疌"},
    {8, 2, "卥㡀𫩠咼㝵畀冐𦙃"},
    {8, 3, "𥝢臾𨸏阜飠𠈌忩㸚㸒㑒㦰臽匊匋"},
    {8, 4, "咅𣏋㐭斉㫄罙𢼄"},
    {8, 5, "𡬠叕沓㣇甾"},
    {9, 1, "畐壴垔荅㪅𠀷耎𲺋"},
    {9, 2, "𠧪曷昜禺昷耑咢咠𬙙"},
    {9, 3, "臿叟禹𣬉爰弇爯矦㲋"},
    {9, 4, "㒸叜"},
    {9, 5, "叚𢏚凾芔癸㚇彖𡿺𩠐"},
    {10, 1, "冓尃㱿𤰇𡘤"},
    {10, 2, "丵𧴪𦐇眔畟"},
    {10, 3, "𠂹皋虒䍃奚芻𰮤"},
    {10, 4, "竜𤇾𡨄隺冡"},
    {10, 5, "圅鬯𣁋"},
    {11, 1, "𠩺殸堇㒼埶𦰩桼䙴𢛳殹𮍏"},
    {11, 2, "虘虖啚𠁁"},
    {11, 3, "𬀷皐悤𣶒𡕩"},
    {11, 4, "啇羕"},
    {11, 5, "翏㕘"},
    {12, 1, "賁棥尞𦓔厤畱朁"},
    {12, 2, "菐黹戢𢽠"},
    {12, 3, "𡍮敫雋衆舄翕㫺𤔔𦥯"},
    {12, 4, "戠𰕎"},
    {12, 5, "叅"},
    {13, 1, "畺𣪠"},
    {13, 2, "豦喿睘睪𥇡豊"},
    {13, 3, "僉"},
    {13, 4, "亶𣎆稟廌"},
    {13, 5, "𡕰"},
    {14, 1, "皷𰯲𡏳臧"},
    {14, 2, "㬎"},
    {14, 3, "㥯夐"},
    {14, 4, "賔"},
    {14, 5, "㡭"},
    {15, 1, "賛奭𧶠韯"},
    {15, 2, "畾嶲瞏"},
    {15, 3, "臱䝿𢀩"},
    {15, 4, "廛𦎫𦎧𬴘"},
    {15, 5, "巤"},
    {16, 1, "𤴡"},
    {16, 2, "鬳"},
    {16, 4, "褱𩫏"},
    {17, 1, "雚霝"},
    {17, 3, "龠韱毚"},
    {17, 4, "襄"},
    {18, 2, "巂瞿"},
    {18, 3, "𤐫"},
    {19, 1, "夒"},
    {19, 5, "䜌"},
    {21, 2, "𪉷"},
};

// The common groups with the harder ones appended to the row of the same
// stroke count; the common ones come first.
struct MergedGroup {
  int strokes;
  std::string components;
};
const std::vector<MergedGroup> &ComponentGroups() {
  static const std::vector<MergedGroup> groups = [] {
    std::map<int, std::string> by_strokes;
    for (const ComponentGroup &g : kCommonComponentGroups) by_strokes[g.strokes] += g.components;
    for (const HardGroup &g : kHardComponentGroups) by_strokes[g.strokes] += g.components;
    std::vector<MergedGroup> merged;
    for (auto &entry : by_strokes) merged.push_back({entry.first, std::move(entry.second)});
    return merged;
  }();
  return groups;
}

// The first-stroke class of each hard component (zi.tools' own grouping).
const std::map<std::string, int> &HardLeadingStrokes() {
  static const std::map<std::string, int> leads = [] {
    std::map<std::string, int> out;
    for (const HardGroup &g : kHardComponentGroups) {
      const QString text = QString::fromUtf8(g.components);
      for (const uint cp : text.toUcs4()) {
        out[QString::fromUcs4(reinterpret_cast<const char32_t *>(&cp), 1).toUtf8().toStdString()] =
            g.lead;
      }
    }
    return out;
  }();
  return leads;
}

// Positional forms hidden when "Show radical variants" is unticked. (王, 月
// and 艹 stay: they are components in their own right too.)
constexpr char kVariantForms[] = "亻𠆢刂忄扌氵犭礻衤⺮耂灬";

std::vector<std::string> SplitChars(const char *utf8) {
  std::vector<std::string> out;
  const QString text = QString::fromUtf8(utf8);
  for (const uint cp : text.toUcs4()) {
    out.push_back(ToUtf8(QString::fromUcs4(reinterpret_cast<const char32_t *>(&cp), 1)));
  }
  return out;
}

bool Contains(const std::vector<std::string> &list, const std::string &s) {
  return std::find(list.begin(), list.end(), s) != list.end();
}

// The glyph of a leading stroke class (see IdsTable::LeadingStroke).
QString LeadingStrokeGlyph(int stroke_class) {
  static const char *const kGlyphs[] = {"", "一", "丨", "丿", "丶", "乛"};
  if (stroke_class < 1 || stroke_class > 5) return QString();
  return QString::fromUtf8(kGlyphs[stroke_class]);
}

// Marks a stroke-count heading in the results list.
constexpr int kHeadingRole = Qt::UserRole + 1;
constexpr int kSpecimenKindRole = Qt::UserRole + 2;

SlotPath Child(const SlotPath &path, int index) {
  SlotPath child = path;
  child.push_back(index);
  return child;
}

QRectF Scaled(const FRect &r, const QRectF &box) {
  return QRectF(box.x() + r.x * box.width(), box.y() + r.y * box.height(),
                r.w * box.width(), r.h * box.height());
}

void DrawGeometry(QPainter *p, const OperatorGeometry &g, const QRectF &box,
                  bool dividers_only = false) {
  for (const FLine &line : g.dividers) {
    p->drawLine(QPointF(box.x() + line.x1 * box.width(), box.y() + line.y1 * box.height()),
                QPointF(box.x() + line.x2 * box.width(), box.y() + line.y2 * box.height()));
  }
  if (dividers_only) return;
  for (const FRect &frame : g.frames) {
    p->drawRect(Scaled(frame, box));
  }
}

QPixmap DrawingPreview(const std::vector<PlacedGlyph> &glyphs, const QColor &ink,
                       qreal device_pixel_ratio) {
  const int kSize = kPreviewLogical;
  QPixmap pixmap(static_cast<int>(kSize * device_pixel_ratio),
                 static_cast<int>(kSize * device_pixel_ratio));
  pixmap.setDevicePixelRatio(device_pixel_ratio);
  pixmap.fill(Qt::transparent);
  QPainter p(&pixmap);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::TextAntialiasing);
  p.setPen(ink);
  const double scale = kSize / 1000.0;
  for (const PlacedGlyph &g : glyphs) {
    QFont f;
    f.setFamilies({QStringLiteral("Hiragino Mincho ProN"), QStringLiteral("Yu Mincho"),
                   QStringLiteral("Songti SC"), QStringLiteral("Noto Serif CJK JP")});
    f.setPixelSize(std::max(1, static_cast<int>(g.font_size * scale)));
    p.setFont(f);
    const QString text = QString::fromUtf8(g.text.data(), static_cast<int>(g.text.size()));
    const qreal width = QFontMetricsF(f).horizontalAdvance(text);
    p.save();
    p.translate(g.x * scale, g.y * scale);
    p.scale(g.scale_x, g.scale_y);
    p.drawText(QPointF(-width / 2, 0), text);
    p.restore();
  }
  p.end();
  return pixmap;
}

QPixmap OutlinePreview(const std::vector<Contour> &outlines, const QColor &ink,
                       qreal device_pixel_ratio) {
  const int kSize = kPreviewLogical;
  QPixmap pixmap(static_cast<int>(kSize * device_pixel_ratio),
                 static_cast<int>(kSize * device_pixel_ratio));
  pixmap.setDevicePixelRatio(device_pixel_ratio);
  pixmap.fill(Qt::transparent);
  QPainterPath path;
  path.setFillRule(Qt::WindingFill);
  const double scale = kSize / 1000.0;
  for (const Contour &contour : outlines) {
    if (contour.empty()) continue;
    path.moveTo(contour[0].first * scale, contour[0].second * scale);
    for (size_t i = 1; i < contour.size(); ++i) {
      path.lineTo(contour[i].first * scale, contour[i].second * scale);
    }
    path.closeSubpath();
  }
  QPainter p(&pixmap);
  p.setRenderHint(QPainter::Antialiasing);
  p.setPen(Qt::NoPen);
  p.setBrush(ink);
  p.drawPath(path);
  p.end();
  return pixmap;
}

QIcon OperatorIcon(const std::string &op, const QColor &ink) {
  const int kSize = kStructureIcon;
  QPixmap pixmap(kSize * 2, kSize * 2);
  pixmap.setDevicePixelRatio(2.0);
  pixmap.fill(Qt::transparent);
  QPainter p(&pixmap);
  p.setRenderHint(QPainter::Antialiasing);
  const QRectF box(2.5, 2.5, kSize - 5, kSize - 5);
  p.setPen(QPen(ink, 1.4));
  p.setBrush(Qt::NoBrush);
  p.drawRect(box);
  p.setPen(QPen(ink, 1.1, Qt::DashLine));
  DrawGeometry(&p, GeometryFor(op), box);
  p.end();
  return QIcon(pixmap);
}

// Compact primary heading: restrained semibold serif. Genuine OpenType small
// caps are not reliably available on the marinaMoji display stack (Hiragino /
// Yu Mincho / Noto Serif) for both Latin and CJK, and Qt's SmallCaps mode
// synthesizes by uppercasing — which the design brief forbids. Slight tracking
// keeps the headings typographic without dominating the specimen.
QFont PrimaryHeadingFont() {
  QFont font = theme::DisplayFont(17);
  font.setWeight(QFont::DemiBold);
  font.setLetterSpacing(QFont::PercentageSpacing, 102.0);
  return font;
}

QWidget *MakeSectionHeading(const QString &title, QWidget *parent,
                            QWidget *trailing = nullptr) {
  auto *row = new QWidget(parent);
  auto *layout = new QHBoxLayout(row);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);
  auto *label = new QLabel(title, row);
  label->setObjectName(QStringLiteral("sectionHeading"));
  label->setFont(PrimaryHeadingFont());
  theme::EnsureHeadingMetrics(label);
  layout->addWidget(label, 0, Qt::AlignVCenter);
  auto *rule = new QFrame(row);
  rule->setObjectName(QStringLiteral("sectionRule"));
  rule->setFrameShape(QFrame::HLine);
  rule->setFixedHeight(1);
  rule->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  layout->addWidget(rule, 1, Qt::AlignVCenter);
  if (trailing != nullptr) {
    layout->addWidget(trailing, 0, Qt::AlignVCenter);
  }
  return row;
}

// A small all-caps caption, lighter than a section heading (Construct column,
// IDS box).
QLabel *MakeCaption(const QString &title, QWidget *parent) {
  auto *label = new QLabel(title.toUpper(), parent);
  label->setObjectName(QStringLiteral("panelCaption"));
  QFont font = theme::UiFont(11, QFont::DemiBold);
  font.setLetterSpacing(QFont::PercentageSpacing, 110.0);
  label->setFont(font);
  return label;
}

// A common character with each layout, shown under its structure button.
QString StructureExample(const std::string &op) {
  static const std::map<std::string, const char *> kExamples = {
      {"⿰", "明"}, {"⿱", "字"}, {"⿲", "街"}, {"⿳", "京"},
      {"⿴", "国"}, {"⿵", "問"}, {"⿶", "凶"}, {"⿷", "区"},
      {"⿸", "広"}, {"⿹", "式"}, {"⿺", "道"}, {"⿻", "中"},
  };
  const auto it = kExamples.find(op);
  return it == kExamples.end() ? QString() : QString::fromUtf8(it->second);
}

// Split ja_kana by script: katakana → on'yomi, hiragana → kun'yomi.
void SplitJaKana(const QString &kana, QStringList *on, QStringList *kun) {
  for (const QString &token :
       kana.split(QRegularExpression(QStringLiteral("\\s+")),
                  Qt::SkipEmptyParts)) {
    if (token.isEmpty()) continue;
    const QChar::Script script = token.at(0).script();
    if (script == QChar::Script_Hiragana) {
      kun->append(token);
    } else {
      on->append(token);
    }
  }
}

QString JoinDot(const QStringList &parts) {
  // Thin spaces around the middle dot keep readings distinct without the
  // looser spacing of a full space on either side.
  return parts.join(QStringLiteral("\u2009·\u2009"));
}

// One chip in the Variants row (character or "old form: …" label).
struct VariantChip {
  QString text;
  QString kind;  // specimen_kind name: plain / missingFont / svg
  QString tip;
};

std::vector<VariantChip> FormatVariantChips(const composer_ids::CharInfo &info,
                                            QFont *font) {
  GlyphCoverage &coverage = Coverage();
  std::vector<VariantChip> chips;
  auto add_char = [&](const QString &ch, const QString &tip) {
    if (ch.isEmpty()) return;
    VariantChip chip;
    chip.text = ch;
    chip.tip = tip;
    if (coverage.Drawable(ch)) {
      chip.kind = QString::fromUtf8(specimen_kind::kPlainName);
      *font = coverage.FontFor(*font, ch);
    } else {
      chip.kind = QString::fromUtf8(specimen_kind::kMissingFontName);
      chip.tip =
          tip.isEmpty()
              ? Tr("No installed font can draw this character.")
              : tip + QStringLiteral("\n") +
                    Tr("No installed font can draw this character.");
    }
    chips.push_back(std::move(chip));
  };
  if (!info.variants.empty()) {
    const QString raw = FromUtf8(info.variants);
    for (const QString &token :
         raw.split(QRegularExpression(QStringLiteral("\\s+")),
                   Qt::SkipEmptyParts)) {
      add_char(token, QString());
    }
  }
  if (!info.old_form.empty()) {
    add_char(FromUtf8(info.old_form), Tr("Old form"));
  }
  if (!info.new_form.empty()) {
    add_char(FromUtf8(info.new_form), Tr("New form"));
  }
  return chips;
}

QString ComponentRolesFor(const std::string &op) {
  if (op == "⿰") return Tr("Left / Right");
  if (op == "⿱") return Tr("Top / Bottom");
  if (op == "⿲") return Tr("Left / Middle / Right");
  if (op == "⿳") return Tr("Top / Middle / Bottom");
  if (op == "⿴") return Tr("Enclosure / Interior");
  if (op == "⿵" || op == "⿶" || op == "⿷") return Tr("Surround / Interior");
  if (op == "⿸") return Tr("Upper-left / Lower-right");
  if (op == "⿹") return Tr("Upper-right / Lower-left");
  if (op == "⿺") return Tr("Lower-left / Upper-right");
  if (op == "⿻") return Tr("Overlapping components");
  return QString();
}

// Specimen plate colours, light or dark (base/specimen_kind.h).
QColor PlateColor(specimen_kind::Kind kind, bool dark) {
  uint8_t r = 0, g = 0, b = 0;
  specimen_kind::PlateRgb(kind, dark, &r, &g, &b);
  return QColor(r, g, b);
}

QString PlateCss(specimen_kind::Kind kind, bool dark) {
  return PlateColor(kind, dark).name();
}

// The colour of a character drawn on a plate.
QColor PlateInkColor(bool dark) {
  uint8_t r = 0, g = 0, b = 0;
  specimen_kind::PlateInkRgb(dark, &r, &g, &b);
  return QColor(r, g, b);
}

QString ComposerStylesheet(bool dark) {
  const theme::Palette palette = theme::Colors(dark);
  QString qss = QStringLiteral(
      "QDialog#CharacterComposerDialog {"
      "  background-color: @@WINDOW@@; }"
      "QWidget#examinePanel, QWidget#discoverPanel {"
      "  background-color: @@WINDOW@@; }"
      "QWidget#constructPanel { background-color: @@SIDEBAR@@; }"
      "QScrollArea#examineScroll, QScrollArea#examineScroll > QWidget > QWidget {"
      "  background: transparent; border: none; }"
      "QFrame#columnRule, QFrame#footerRule {"
      "  border: none; color: @@RULE@@; background-color: @@RULE@@; }"
      "QFrame#sectionRule {"
      "  border: none; color: @@TITLE_RULE@@; background-color: @@TITLE_RULE@@; }"

      "QLabel { color: @@TEXT@@; background: transparent; }"
      "QLabel#sectionHeading { color: @@HEADING@@; }"
      "QLabel#componentRoles { color: @@ACCENT@@; }"
      "QLabel#activeState {"
      "  color: @@SECONDARY@@; letter-spacing: 0.12em; }"
      "QLabel#statusHeading { color: @@SECONDARY@@; }"
      "QLabel#statusDetail { color: @@SECONDARY@@; }"
      "QLabel#infoKey { color: @@SECONDARY@@; }"
      "QLabel#infoValue { color: @@TEXT@@; }"
      "QLabel#exploreSubhead { color: @@HEADING@@; }"
      "QLabel#resultsStatus { color: @@SECONDARY@@; }"
      "QLabel#panelCaption { color: @@SECONDARY@@; }"
      "QLabel#strokeHint { color: @@SECONDARY@@; }"
      "QWidget#panelTabs {"
      "  background: @@SIDEBAR@@; border: 1px solid @@RULE@@; border-radius: 4px; }"
      "QPushButton#panelTab {"
      "  color: @@SECONDARY@@; background: transparent;"
      "  border: 1px solid transparent; border-radius: 3px;"
      "  min-height: 1.2em; padding: 3px 10px; }"
      "QPushButton#panelTab:hover { color: @@HEADING@@; }"
      "QPushButton#panelTab:checked {"
      "  color: @@HEADING@@; background: @@WINDOW@@;"
      "  border: 1px solid @@BORDER_CONTROL@@; font-weight: 600; }"
      "QToolButton#viewMenuButton {"
      "  color: @@TEXT@@; background: @@RAISED@@;"
      "  border: 1px solid @@BORDER_CONTROL@@; border-radius: 3px;"
      "  padding: 3px 8px; }"
      "QToolButton#viewMenuButton:hover { border-color: @@ACCENT@@; }"
      "QToolButton#viewMenuButton::menu-indicator { image: none; width: 0; }"
      // No padding: the label is exactly kPreviewLogical square and the
      // preview pixmap / Unicode face are sized for that box. Padding used
      // to shrink the content rect so drawings spilled past the plate.
      "QLabel#specimenGlyph {"
      "  background: transparent; border: none; padding: 0; margin: 0; }"
      "QLabel#specimenGlyph[prompt=\"true\"] { color: @@SECONDARY@@; }"
      // Rounded plates mark drawings that are not ordinary system text.
      // Colours: base/specimen_kind.h (Prussian blue SVG, amber missingFont).
      "QLabel#specimenGlyph[specimenKind=\"svg\"] {"
      "  background: @@SVG_PLATE@@; border: none; border-radius: 12px;"
      "  padding: 0; color: @@PLATE_INK@@; }"
      "QLabel#specimenGlyph[specimenKind=\"missingFont\"] {"
      "  background: @@WARN_PLATE@@; border: none; border-radius: 12px;"
      "  padding: 0; color: @@PLATE_INK@@; }"
      "QLabel#variantChip {"
      "  background: transparent; border: none; border-radius: 6px;"
      "  padding: 2px 6px; margin: 0 4px 0 0; }"
      "QLabel#variantChip[specimenKind=\"plain\"] {"
      "  background: transparent; color: @@TEXT@@; padding: 2px 4px 2px 0; }"
      "QLabel#variantChip[specimenKind=\"svg\"] {"
      "  background: @@SVG_PLATE@@; color: @@PLATE_INK@@; }"
      "QLabel#variantChip[specimenKind=\"missingFont\"] {"
      "  background: @@WARN_PLATE@@; color: @@PLATE_INK@@; }"
      "QWidget#layoutEditor {"
      "  background-color: @@RAISED@@;"
      "  border: 1px solid @@RULE@@; border-radius: 2px; }"
      "QWidget#specimenGroup {"
      "  background: transparent; border: none; }"

      "QLineEdit {"
      "  color: @@TEXT@@; background: @@RAISED@@;"
      "  border: 1px solid @@BORDER_CONTROL@@; border-radius: 3px;"
      "  padding: 5px 8px; min-height: 22px;"
      "  selection-background-color: @@SELECTION_BG@@;"
      "  selection-color: @@SELECTION_TEXT@@; }"
      "QLineEdit:hover { border-color: @@ACCENT@@; }"
      "QLineEdit:focus { border: 1px solid @@FOCUS@@; }"
      "QSpinBox {"
      "  color: @@TEXT@@; background: @@RAISED@@;"
      "  border: 1px solid @@BORDER_CONTROL@@; border-radius: 3px;"
      "  padding: 4px 6px; min-height: 22px;"
      "  selection-background-color: @@SELECTION_BG@@;"
      "  selection-color: @@SELECTION_TEXT@@; }"
      "QSpinBox:hover { border-color: @@ACCENT@@; }"
      "QSpinBox:focus { border: 1px solid @@FOCUS@@; }"
      "QSpinBox:disabled { color: @@DISABLED@@; border-color: @@RULE@@; }"
      "QLineEdit#slotEdit {"
      "  padding: 1px; border-radius: 2px; border: 1px solid transparent;"
      "  background: transparent; }"
      "QLineEdit#slotEdit:focus { border: 1px solid @@FOCUS@@; }"
      "QLineEdit#slotEdit[selected=\"true\"] {"
      "  border: 1px solid @@ACCENT@@; }"

      "QToolButton#structureButton {"
      "  color: @@SECONDARY@@; background: transparent; border: 1px solid @@RULE@@;"
      "  border-radius: 3px; padding: 2px 0 1px 0; }"
      "QToolButton#structureButton:hover {"
      "  background: @@RAISED@@; border-color: @@ACCENT@@; }"
      "QToolButton#structureButton:checked {"
      "  color: @@ACCENT@@; background: @@ACCENT_SUBTLE@@;"
      "  border: 1px solid @@ACCENT@@; }"
      "QToolButton#structureButton:focus {"
      "  border: 1px solid @@FOCUS@@; }"
      "QToolButton#radical_form {"
      "  border: 1px solid transparent; border-radius: 3px;"
      "  padding: 1px 4px; color: @@SECONDARY@@; background: transparent; }"
      "QToolButton#radical_form[selectedForm=\"true\"] {"
      "  border: 1px solid @@ACCENT@@; color: @@ACCENT@@; font-weight: 600; }"

      "QComboBox {"
      "  color: @@TEXT@@; background: @@RAISED@@;"
      "  border: 1px solid @@BORDER_CONTROL@@; border-radius: 3px;"
      "  padding: 5px 28px 5px 9px; min-height: 22px; }"
      "QComboBox:hover { border-color: @@ACCENT@@; }"
      "QComboBox:focus { border: 1px solid @@FOCUS@@; }"
      "QComboBox QAbstractItemView {"
      "  color: @@TEXT@@; background: @@RAISED@@; outline: 0;"
      "  border: 1px solid @@BORDER_CONTROL@@;"
      "  selection-background-color: @@SELECTION_BG@@;"
      "  selection-color: @@SELECTION_TEXT@@; }"

      "QListWidget#resultsList {"
      "  color: @@TEXT@@; background: transparent;"
      "  border: none; outline: none; padding: 0; }"
      "QListWidget#resultsList::item {"
      "  border: 1px solid transparent; border-radius: 2px;"
      "  padding: 2px; }"
      "QListWidget#resultsList::item:selected {"
      "  background: @@ACCENT_SUBTLE@@; color: @@TEXT@@;"
      "  border: 1px solid @@ACCENT@@; }"
      "QListWidget#resultsList::item:focus {"
      "  border: 1px solid @@FOCUS@@; }"
      // Stroke-count headings (Qt::NoItemFlags, so :disabled).
      "QListWidget#resultsList::item:disabled {"
      "  color: @@SECONDARY@@; background: @@ACCENT_SUBTLE@@; }"
      "QWidget#componentsPanel { background-color: @@WINDOW@@; }"
      "QWidget#componentsPanel QScrollArea, QWidget#componentsPanel QScrollArea > QWidget > QWidget {"
      "  background: transparent; border: none; }"
      "QCheckBox { color: @@TEXT@@; background: transparent; }"
      "QCheckBox:disabled { color: @@DISABLED@@; }"
      "QLabel#strokeHeading {"
      "  color: @@SECONDARY@@; background: @@ACCENT_SUBTLE@@; border-radius: 2px; }"
      "QToolButton#componentTile {"
      "  color: @@TEXT@@; background: transparent; border: 1px solid @@RULE@@;"
      "  border-radius: 3px; padding: 0; }"
      "QToolButton#componentTile:hover {"
      "  background: @@RAISED@@; border-color: @@ACCENT@@; }"
      "QToolButton#componentTile:checked {"
      "  background: @@ACCENT_SUBTLE@@; border: 1px solid @@ACCENT@@; color: @@ACCENT@@; }"
      "QToolButton#firstStrokeTile {"
      "  color: @@TEXT@@; background: transparent; border: 1px solid @@RULE@@;"
      "  border-radius: 3px; padding: 0; }"
      "QToolButton#firstStrokeTile:hover {"
      "  background: @@RAISED@@; border-color: @@ACCENT@@; }"
      "QToolButton#firstStrokeTile:checked {"
      "  background: #003153; border: 1px solid #003153; color: #ffffff; }"
      "QToolButton#componentTile:disabled {"
      "  color: @@DISABLED@@; border-color: transparent; }"
      "QPushButton#returnLink {"
      "  color: @@ACCENT@@; background: transparent; border: none;"
      "  padding: 2px 4px; text-align: center; min-height: 0; }"
      "QPushButton#returnLink:hover { color: @@HEADING@@; }"
      "QPushButton#returnLink:disabled {"
      "  color: transparent; border: none; background: transparent; }"

      "QPushButton {"
      "  color: @@TEXT@@; background: @@RAISED@@;"
      "  border: 1px solid @@BORDER_CONTROL@@; border-radius: 3px;"
      "  min-height: 1.35em; padding: 6px 13px; }"
      "QPushButton:hover { border-color: @@ACCENT@@; }"
      "QPushButton:focus { border: 1px solid @@FOCUS@@; }"
      "QPushButton:pressed { background: @@ACCENT_SUBTLE@@; }"
      "QPushButton:disabled { color: @@DISABLED@@; border-color: @@RULE@@; }"
      "QPushButton#primaryButton {"
      "  color: @@ON_ACCENT@@; background: @@PRIMARY@@;"
      "  border: 1px solid @@PRIMARY@@; padding: 6px 14px; }"
      "QPushButton#primaryButton:hover {"
      "  background: @@PRIMARY_HOVER@@; border-color: @@PRIMARY_HOVER@@; }"
      "QPushButton#primaryButton:pressed {"
      "  background: @@PRIMARY_PRESSED@@; border-color: @@PRIMARY_PRESSED@@; }"
      "QPushButton#primaryButton:disabled {"
      "  color: @@WINDOW@@; background: @@DISABLED@@; border-color: @@DISABLED@@; }"
      "QPushButton#outlineButton {"
      "  color: @@ACCENT@@; background: @@WINDOW@@;"
      "  border: 1px solid @@BORDER_CONTROL@@; padding: 5px 12px; }"
      "QPushButton#outlineButton:hover {"
      "  border-color: @@ACCENT@@; background: @@ACCENT_SUBTLE@@; }"
      "QPushButton#secondaryButton {"
      "  color: @@ACCENT@@; background: @@WINDOW@@;"
      "  border: 1px solid @@BORDER_CONTROL@@;"
      "  min-width: 72px; padding: 5px 12px; }"
      "QPushButton#secondaryButton:hover {"
      "  border-color: @@ACCENT@@; background: @@ACCENT_SUBTLE@@; }"
      "QPushButton#textButton {"
      "  color: @@SECONDARY@@; background: transparent;"
      "  border: 1px solid transparent; }"
      "QPushButton#textButton:hover { color: @@HEADING@@; }"

      "QScrollBar:vertical {"
      "  background: @@WINDOW@@; width: 11px; margin: 0; border: none; }"
      "QScrollBar::handle:vertical {"
      "  background: @@BORDER_CONTROL@@; min-height: 28px; border-radius: 4px;"
      "  margin: 2px; }"
      "QScrollBar::handle:vertical:hover { background: @@SECONDARY@@; }"
      "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
      "  height: 0; background: none; border: none; }"
      "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
      "  background: none; }");
  // Plate colours: base/specimen_kind.h (a different pair in dark mode).
  qss.replace(QStringLiteral("@@SVG_PLATE@@"),
              PlateCss(specimen_kind::Kind::kSvg, dark));
  qss.replace(QStringLiteral("@@WARN_PLATE@@"),
              PlateCss(specimen_kind::Kind::kMissingFont, dark));
  qss.replace(QStringLiteral("@@PLATE_INK@@"), PlateInkColor(dark).name());
  return theme::ApplyPaletteTokens(qss, palette);
}

}  // namespace

// --- SlotEdit ---------------------------------------------------------------

SlotEdit::SlotEdit(SlotPath path, QWidget *parent)
    : QLineEdit(parent), path_(std::move(path)) {
  setObjectName(QStringLiteral("slotEdit"));
  setAlignment(Qt::AlignCenter);
  setPlaceholderText(QStringLiteral("字"));
  QFont f = font();
  f.setPixelSize(36);
  setFont(f);
}

void SlotEdit::SetSelected(bool selected) {
  setProperty("selected", selected);
  if (style() != nullptr) {
    style()->unpolish(this);
    style()->polish(this);
  }
  update();
}

void SlotEdit::focusInEvent(QFocusEvent *event) {
  QLineEdit::focusInEvent(event);
  emit gotFocus(path_);
}

void SlotEdit::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    emit submitRequested();
    return;
  }
  QLineEdit::keyPressEvent(event);
}

// --- LeafSlot ---------------------------------------------------------------

LeafSlot::LeafSlot(LayoutEditor *editor, SlotPath path, std::string op,
                   int part_index, const QString &initial, QWidget *parent)
    : QWidget(parent),
      editor_(editor),
      path_(std::move(path)),
      op_(std::move(op)),
      part_index_(part_index) {
  auto *column = new QVBoxLayout(this);
  column->setContentsMargins(0, 0, 0, 0);
  column->setSpacing(2);

  edit_ = new SlotEdit(path_, this);
  edit_->setText(initial);
  edit_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  column->addWidget(edit_, /*stretch=*/1);

  forms_row_ = new QWidget(this);
  forms_row_->setObjectName(QStringLiteral("radical_forms"));
  forms_layout_ = new QHBoxLayout(forms_row_);
  forms_layout_->setContentsMargins(0, 0, 0, 0);
  forms_layout_->setSpacing(2);
  forms_layout_->addStretch(1);
  forms_layout_->addStretch(1);
  column->addWidget(forms_row_, /*stretch=*/0, Qt::AlignHCenter);

  connect(edit_, &QLineEdit::textEdited, this, [this](const QString &typed) {
    QString text = typed;
    // A base radical typed where it changes shape is shown in that shape (手
    // on the left of ⿰ becomes 扌). Picking from the list below is not
    // "typed", so the base form can still be put back.
    if (const composer_ids::IdsTable *table = editor_->table()) {
      if (const auto form =
              table->AutomaticFormFor(ToUtf8(typed), op_, part_index_)) {
        text = FromUtf8(*form);
        edit_->setText(text);
      }
    }
    editor_->SetLeafInput(path_, text);
    RefreshForms();
  });
  connect(edit_, &SlotEdit::gotFocus, this,
          [this](const std::vector<int> &path) { editor_->Select(path); });
  connect(edit_, &SlotEdit::submitRequested, editor_,
          &LayoutEditor::submitRequested);

  RefreshForms();
}

void LeafSlot::PickForm(const QString &form) {
  editor_->Select(path_);
  edit_->setText(form);
  editor_->SetLeafInput(path_, form);
  RefreshForms();
  edit_->setFocus();
}

void LeafSlot::RefreshForms() {
  while (QLayoutItem *item = forms_layout_->takeAt(0)) {
    if (QWidget *w = item->widget()) w->deleteLater();
    delete item;
  }
  forms_layout_->addStretch(1);
  has_forms_ = false;
  const composer_ids::IdsTable *table = editor_->table();
  if (table != nullptr) {
    const std::vector<std::string> forms = table->FormsOf(ToUtf8(edit_->text()));
    if (forms.size() >= 2) {
      has_forms_ = true;
      const QString current = edit_->text();
      for (const std::string &form : forms) {
        const QString label = FromUtf8(form);
        auto *button = new QToolButton(forms_row_);
        button->setObjectName(QStringLiteral("radical_form"));
        button->setText(label);
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::NoFocus);
        button->setToolTip(Tr("Use this form of the radical"));
        QFont f = button->font();
        f.setPointSize(std::max(f.pointSize(), 11));
        button->setFont(f);
        const bool selected = label == current;
        button->setProperty("selectedForm", selected);
        if (button->style() != nullptr) {
          button->style()->unpolish(button);
          button->style()->polish(button);
        }
        connect(button, &QToolButton::clicked, this,
                [this, label]() { PickForm(label); });
        forms_layout_->addWidget(button, 0, Qt::AlignBottom);
      }
    }
  }
  forms_layout_->addStretch(1);
  UpdateFormsVisibility();
}

void LeafSlot::UpdateFormsVisibility() {
  // Tiny enclosure strips have no room for radical chips.
  forms_row_->setVisible(has_forms_ && height() >= 64);
  if (edit_ == nullptr) return;
  const int chip = forms_row_->isVisible() ? 20 : 0;
  const int avail = std::max(18, height() - chip - 4);
  // Aim for ~40–55px glyphs in simple cells; nestings scale down.
  const int px = std::clamp((avail * 3) / 4, 24, 56);
  QFont f = edit_->font();
  if (f.pixelSize() != px) {
    f.setPixelSize(px);
    edit_->setFont(f);
  }
}

void LeafSlot::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  UpdateFormsVisibility();
}

// --- LayoutView -------------------------------------------------------------

LayoutView::LayoutView(LayoutEditor *editor, SlotPath path, std::string op,
                       const std::vector<ComposerSlot> &parts, QWidget *parent)
    : QWidget(parent), editor_(editor), path_(std::move(path)), op_(std::move(op)) {
  for (size_t i = 0; i < parts.size(); ++i) {
    const ComposerSlot &part = parts[i];
    const SlotPath child_path = Child(path_, static_cast<int>(i));
    if (part.kind == ComposerSlot::Kind::kLeaf) {
      children_.push_back(new LeafSlot(editor_, child_path, op_,
                                       static_cast<int>(i), FromUtf8(part.input),
                                       this));
    } else {
      children_.push_back(
          new LayoutView(editor_, child_path, part.op, part.parts, this));
    }
  }
  // Later parts are drawn over the first: an enclosure's inner part sits on
  // top of the frame, which fills the whole square.
  for (QWidget *child : children_) child->raise();

  if (!path_.empty()) {
    remove_button_ = new QToolButton(this);
    remove_button_->setText(QStringLiteral("×"));
    remove_button_->setAutoRaise(true);
    remove_button_->setFixedSize(16, 16);
    remove_button_->setToolTip(Tr("Remove this layout"));
    connect(remove_button_, &QToolButton::clicked, this,
            [this]() { editor_->Flatten(path_); });
  }
  setMinimumSize(24, 24);
}

void LayoutView::SetSelected(bool selected) {
  if (selected_ == selected) return;
  selected_ = selected;
  update();
}

void LayoutView::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  PlaceChildren();
}

void LayoutView::PlaceChildren() {
  const OperatorGeometry g = GeometryFor(op_);
  const QRectF box(0, 0, width(), height());
  for (size_t i = 0; i < children_.size() && static_cast<int>(i) < g.count; ++i) {
    QWidget *child = children_[i];
    const bool leaf = qobject_cast<LeafSlot *>(child) != nullptr;
    const QRectF cell = Scaled(leaf ? g.edit[i] : g.cell[i], box);
    const int inset = leaf ? 4 : 3;
    child->setGeometry(cell.toRect().adjusted(inset, inset, -inset, -inset));
  }
  if (remove_button_ != nullptr) {
    remove_button_->move(width() - remove_button_->width() - 2, 2);
    remove_button_->raise();
  }
}

void LayoutView::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  const QRectF box = QRectF(rect()).adjusted(1, 1, -1, -1);
  p.setBrush(Qt::NoBrush);
  const bool dark = config_dialog_theme::IsDark();
  const auto colors = config_dialog_theme::Colors(dark);
  const bool root = path_.empty();
  // LayoutEditor QSS owns the single outer edge. Nested views keep a light
  // solid frame; the root only strokes when selected. Root geometry draws
  // dividers only — full operator frames sit on the QSS border and read as a
  // double outline.
  if (!root || selected_) {
    if (selected_) {
      p.setPen(QPen(colors.accent, 1.5));
    } else {
      p.setPen(QPen(colors.rule, 1, Qt::SolidLine));
    }
    p.drawRect(box);
  }
  p.setPen(QPen(colors.rule, 1, Qt::DashLine));
  DrawGeometry(&p, GeometryFor(op_), box, /*dividers_only=*/root);
}

void LayoutView::mousePressEvent(QMouseEvent *event) {
  // Only the padding and gaps reach here; fields take their own clicks.
  editor_->Select(path_);
  event->accept();
}

// --- LayoutEditor -----------------------------------------------------------

LayoutEditor::LayoutEditor(QWidget *parent) : QWidget(parent) {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(2, 2, 2, 2);
  setObjectName(QStringLiteral("layoutEditor"));
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  setFixedHeight(composer_dialog_metrics::kEditorHeight);
  setMinimumWidth(200);
  parts_ = composer_ids::ResizePartsForOperator(op_, {});
  Rebuild();
}

QSize LayoutEditor::sizeHint() const {
  return QSize(260, composer_dialog_metrics::kEditorHeight);
}

QSize LayoutEditor::minimumSizeHint() const {
  return QSize(200, composer_dialog_metrics::kEditorHeight);
}

void LayoutEditor::Rebuild() {
  if (root_view_ != nullptr) {
    layout()->removeWidget(root_view_);
    root_view_->hide();
    // May be called from a signal of a widget inside the old tree.
    root_view_->deleteLater();
  }
  root_view_ = new LayoutView(this, {}, op_, parts_, this);
  layout()->addWidget(root_view_);
  root_view_->show();
  ApplySelectionToWidgets();
}

void LayoutEditor::ApplySelectionToWidgets() {
  if (root_view_ == nullptr) return;
  root_view_->SetSelected(selection_.empty());
  for (LayoutView *view : root_view_->findChildren<LayoutView *>()) {
    view->SetSelected(view->path() == selection_);
  }
  for (SlotEdit *edit : root_view_->findChildren<SlotEdit *>()) {
    edit->SetSelected(edit->path() == selection_);
  }
}

void LayoutEditor::SetLayout(const std::string &op, std::vector<ComposerSlot> parts) {
  op_ = op;
  parts_ = composer_ids::ResizePartsForOperator(op, std::move(parts));
  selection_.clear();
  Rebuild();
  emit selectionChanged();
  emit changed();
}

void LayoutEditor::Clear() { SetLayout(op_, {}); }

void LayoutEditor::ApplyOperator(const std::string &op) {
  composer_ids::Layout next =
      composer_ids::ApplyPaletteOperator(op_, parts_, selection_, op);
  op_ = std::move(next.op);
  parts_ = std::move(next.parts);
  Rebuild();
  // The selection path now names the new nested region, or the outer square.
  emit selectionChanged();
  emit changed();
}

void LayoutEditor::Select(const SlotPath &path) {
  if (path == selection_) return;
  selection_ = path;
  ApplySelectionToWidgets();
  emit selectionChanged();
}

void LayoutEditor::SetLeafInput(const SlotPath &path, const QString &text) {
  const ComposerSlot *slot = composer_ids::SlotAt(parts_, path);
  if (slot == nullptr || slot->kind != ComposerSlot::Kind::kLeaf) return;
  ComposerSlot updated = *slot;
  updated.input = ToUtf8(text);
  parts_ = composer_ids::ReplaceSlotAt(std::move(parts_), path, updated);
  emit changed();
}

bool LayoutEditor::Flatten(const SlotPath &path) {
  const ComposerSlot *slot = composer_ids::SlotAt(parts_, path);
  if (slot == nullptr || slot->kind != ComposerSlot::Kind::kNested) return false;
  const std::optional<ComposerSlot> flat = composer_ids::FlattenNestedSlot(*slot);
  if (!flat) {
    emit flattenRefused();
    return false;
  }
  parts_ = composer_ids::ReplaceSlotAt(std::move(parts_), path, *flat);
  selection_ = path;
  Rebuild();
  emit selectionChanged();
  emit changed();
  return true;
}

std::string LayoutEditor::SelectedLayoutOp() const {
  if (selection_.empty()) return op_;
  const ComposerSlot *slot = composer_ids::SlotAt(parts_, selection_);
  return slot != nullptr && slot->kind == ComposerSlot::Kind::kNested ? slot->op
                                                                      : std::string();
}

// --- ComponentCatalogue -----------------------------------------------------

namespace {

constexpr int kCatCell = 32;          // cell width and height
constexpr int kCatGap = 3;            // between cells, both ways
constexpr int kCatMarginX = 16;
constexpr int kCatTop = 8;
constexpr int kCatHeading = 26;       // height of a stroke-count heading row
constexpr int kCatSectionGap = 14;    // after the last row of a section
constexpr int kCatBottom = 16;
constexpr int kCatScrollbar = 13;     // always left free, so no reflow loop

QString StrokeHeading(int strokes) {
  const QString text = strokes == 1 ? Tr("%1 stroke") : Tr("%1 strokes");
  return text.arg(strokes).toUpper();
}

QColor Mix(const QColor &a, const QColor &b, qreal amount_of_a) {
  return QColor::fromRgbF(a.redF() * amount_of_a + b.redF() * (1 - amount_of_a),
                          a.greenF() * amount_of_a + b.greenF() * (1 - amount_of_a),
                          a.blueF() * amount_of_a + b.blueF() * (1 - amount_of_a));
}

}  // namespace

ComponentCatalogue::ComponentCatalogue(QWidget *parent) : QAbstractScrollArea(parent) {
  setObjectName(QStringLiteral("componentCatalogue"));
  setFrameShape(QFrame::NoFrame);
  setFocusPolicy(Qt::StrongFocus);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setAccessibleName(Tr("Components"));
  viewport()->setMouseTracking(true);
  verticalScrollBar()->setSingleStep(kCatCell + kCatGap);
  glyph_font_ = WithWideCoverage(theme::UiFont(18));  // 1.5x the 12 px controls
}

void ComponentCatalogue::SetSections(std::vector<Section> sections, bool keep_position) {
  // The section at the top of the view, to come back to.
  int anchor = -1;
  if (keep_position) {
    const int top = verticalScrollBar()->value();
    for (const Heading &h : headings_) {
      if (h.rect.top() - kCatTop <= top) anchor = h.strokes;
    }
  }
  const QString current_text =
      current_ >= 0 && current_ < static_cast<int>(cells_.size()) ? cells_[current_].text
                                                                  : QString();
  sections_ = std::move(sections);
  hover_ = pressed_ = -1;
  current_ = -1;
  Relayout();
  for (size_t i = 0; i < cells_.size(); ++i) {
    if (cells_[i].text == current_text) current_ = static_cast<int>(i);
  }
  int target = 0;
  if (anchor >= 0) {
    for (const Heading &h : headings_) {
      if (h.strokes >= anchor) {
        target = std::max(0, h.rect.top() - kCatTop);
        break;
      }
    }
  }
  verticalScrollBar()->setValue(target);
  viewport()->update();
}

void ComponentCatalogue::SetState(const QSet<QString> &selected, const QSet<QString> &dimmed) {
  selected_ = selected;
  dimmed_ = dimmed;
  viewport()->update();
}

void ComponentCatalogue::Relayout() {
  cells_.clear();
  headings_.clear();
  rows_.clear();
  const int usable = std::max(kCatCell, width() - 2 * kCatMarginX - kCatScrollbar);
  const int columns = std::max(1, (usable + kCatGap) / (kCatCell + kCatGap));
  int y = kCatTop;
  for (const Section &section : sections_) {
    if (section.components.empty()) continue;
    headings_.push_back({section.strokes, QRect(kCatMarginX, y, usable, kCatHeading)});
    y += kCatHeading;
    int column = 0;
    for (const QString &text : section.components) {
      if (column == 0) rows_.emplace_back();
      Cell cell;
      cell.text = text;
      cell.strokes = section.strokes;
      cell.rect = QRect(kCatMarginX + column * (kCatCell + kCatGap), y, kCatCell, kCatCell);
      rows_.back().push_back(static_cast<int>(cells_.size()));
      cells_.push_back(std::move(cell));
      if (++column == columns) {
        column = 0;
        y += kCatCell + kCatGap;
      }
    }
    if (column != 0) y += kCatCell + kCatGap;
    y += kCatSectionGap - kCatGap;
  }
  content_height_ = y + kCatBottom;
  QScrollBar *bar = verticalScrollBar();
  bar->setPageStep(viewport()->height());
  bar->setRange(0, std::max(0, content_height_ - viewport()->height()));
  if (current_ >= static_cast<int>(cells_.size())) current_ = -1;
  viewport()->update();
}

void ComponentCatalogue::resizeEvent(QResizeEvent *event) {
  QAbstractScrollArea::resizeEvent(event);
  hover_ = -1;
  Relayout();
}

bool ComponentCatalogue::Dimmed(int cell) const {
  return cell >= 0 && dimmed_.contains(cells_[cell].text) && !selected_.contains(cells_[cell].text);
}

bool ComponentCatalogue::Contains(const QString &component) const {
  for (const Cell &cell : cells_) {
    if (cell.text == component) return true;
  }
  return false;
}

bool ComponentCatalogue::IsDimmed(const QString &component) const {
  for (size_t i = 0; i < cells_.size(); ++i) {
    if (cells_[i].text == component) return Dimmed(static_cast<int>(i));
  }
  return false;
}

bool ComponentCatalogue::Activate(const QString &component) {
  for (size_t i = 0; i < cells_.size(); ++i) {
    if (cells_[i].text != component) continue;
    if (Dimmed(static_cast<int>(i))) return false;
    emit componentClicked(component);
    return true;
  }
  return false;
}

int ComponentCatalogue::CellAt(const QPoint &viewport_pos) const {
  const QPoint content = viewport_pos + QPoint(0, verticalScrollBar()->value());
  for (size_t i = 0; i < cells_.size(); ++i) {
    if (cells_[i].rect.contains(content)) return static_cast<int>(i);
  }
  return -1;
}

QString ComponentCatalogue::AccessibleText(int cell) const {
  if (cell < 0 || cell >= static_cast<int>(cells_.size())) return QString();
  QString text = QStringLiteral("%1, %2").arg(cells_[cell].text, Tr("Strokes: %1").arg(cells_[cell].strokes));
  if (selected_.contains(cells_[cell].text)) text += QStringLiteral(", ") + Tr("selected");
  if (Dimmed(cell)) text += QStringLiteral(", ") + Tr("finds nothing");
  return text;
}

void ComponentCatalogue::EnsureVisible(int cell) {
  if (cell < 0 || cell >= static_cast<int>(cells_.size())) return;
  const QRect rect = cells_[cell].rect;
  QScrollBar *bar = verticalScrollBar();
  if (rect.top() - kCatHeading - kCatTop < bar->value()) {
    bar->setValue(std::max(0, rect.top() - kCatHeading - kCatTop));
  } else if (rect.bottom() + kCatBottom > bar->value() + viewport()->height()) {
    bar->setValue(rect.bottom() + kCatBottom - viewport()->height());
  }
}

void ComponentCatalogue::MoveCurrent(int cell) {
  if (cell < 0 || cell >= static_cast<int>(cells_.size())) return;
  current_ = cell;
  show_focus_ring_ = true;
  EnsureVisible(cell);
  setAccessibleDescription(AccessibleText(cell));
  viewport()->update();
}

void ComponentCatalogue::paintEvent(QPaintEvent *) {
  QPainter p(viewport());
  const theme::Palette c = theme::Colors(theme::IsDark());
  p.fillRect(viewport()->rect(), c.window);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::TextAntialiasing);
  const int offset = verticalScrollBar()->value();
  p.translate(0, -offset);
  const QRect visible(0, offset, viewport()->width(), viewport()->height());

  QFont heading_font = theme::UiFont(13, QFont::DemiBold);
  heading_font.setLetterSpacing(QFont::PercentageSpacing, 106.0);
  const QFontMetrics heading_metrics(heading_font);
  const QColor heading_color = Mix(c.accent, c.window, 0.78);
  for (const Heading &h : headings_) {
    if (!h.rect.intersects(visible)) continue;
    const QString text = StrokeHeading(h.strokes);
    p.setFont(heading_font);
    p.setPen(heading_color);
    const QRect text_rect(h.rect.left() + 2, h.rect.top(), heading_metrics.horizontalAdvance(text) + 6,
                          h.rect.height());
    p.drawText(text_rect, Qt::AlignLeft | Qt::AlignVCenter, text);
    const int y = h.rect.center().y();
    p.setPen(QPen(c.rule, 1));
    p.drawLine(text_rect.right() + 8, y, h.rect.right(), y);
  }

  p.setFont(glyph_font_);
  for (size_t i = 0; i < cells_.size(); ++i) {
    const Cell &cell = cells_[i];
    if (!cell.rect.intersects(visible)) continue;
    const int index = static_cast<int>(i);
    const bool selected = selected_.contains(cell.text);
    const bool dimmed = Dimmed(index);
    const bool hovered = index == hover_ && !dimmed;
    const QRectF box = QRectF(cell.rect).adjusted(0.5, 0.5, -0.5, -0.5);
    if (selected) {
      p.setPen(QPen(c.accent, 1));
      p.setBrush(c.accent_subtle);
      p.drawRoundedRect(box, 3, 3);
    } else if (hovered) {
      QColor wash = c.accent_subtle;
      wash.setAlphaF(0.7);
      p.setPen(Qt::NoPen);
      p.setBrush(wash);
      p.drawRoundedRect(box, 3, 3);
    }
    p.setBrush(Qt::NoBrush);
    p.setOpacity(dimmed ? 0.3 : 1.0);
    p.setPen(selected ? c.accent : c.text);
    // A component only an unnamed installed font has: name that font.
    p.setFont(Coverage().FontFor(glyph_font_, cell.text));
    p.drawText(cell.rect, Qt::AlignCenter, cell.text);
    p.setOpacity(1.0);
    if (show_focus_ring_ && hasFocus() && index == current_) {
      p.setPen(QPen(c.focus, 2));
      p.drawRoundedRect(QRectF(cell.rect).adjusted(1, 1, -1, -1), 4, 4);
    }
  }
}

void ComponentCatalogue::mouseMoveEvent(QMouseEvent *event) {
  const int cell = CellAt(event->position().toPoint());
  if (cell != hover_) {
    hover_ = cell;
    viewport()->setToolTip(cell < 0 ? QString()
                                    : QStringLiteral("%1  %2").arg(
                                          cells_[cell].text,
                                          Tr("Strokes: %1").arg(cells_[cell].strokes)));
    viewport()->setCursor(cell >= 0 && !Dimmed(cell) ? Qt::PointingHandCursor : Qt::ArrowCursor);
    viewport()->update();
  }
}

void ComponentCatalogue::leaveEvent(QEvent *event) {
  QAbstractScrollArea::leaveEvent(event);
  hover_ = -1;
  viewport()->update();
}

void ComponentCatalogue::mousePressEvent(QMouseEvent *event) {
  QAbstractScrollArea::mousePressEvent(event);
  if (event->button() != Qt::LeftButton) return;
  pressed_ = CellAt(event->position().toPoint());
  if (pressed_ >= 0) {
    current_ = pressed_;
    show_focus_ring_ = false;
    viewport()->update();
  }
}

void ComponentCatalogue::mouseReleaseEvent(QMouseEvent *event) {
  QAbstractScrollArea::mouseReleaseEvent(event);
  if (event->button() != Qt::LeftButton) return;
  const int cell = CellAt(event->position().toPoint());
  const int pressed = pressed_;
  pressed_ = -1;
  if (cell >= 0 && cell == pressed && !Dimmed(cell)) {
    emit componentClicked(cells_[cell].text);
  }
}

void ComponentCatalogue::keyPressEvent(QKeyEvent *event) {
  if (cells_.empty()) {
    QAbstractScrollArea::keyPressEvent(event);
    return;
  }
  const int count = static_cast<int>(cells_.size());
  const int cur = current_ < 0 ? 0 : current_;
  switch (event->key()) {
    case Qt::Key_Left:
      MoveCurrent(std::max(0, cur - 1));
      return;
    case Qt::Key_Right:
      MoveCurrent(std::min(count - 1, cur + 1));
      return;
    case Qt::Key_Home:
      MoveCurrent(0);
      return;
    case Qt::Key_End:
      MoveCurrent(count - 1);
      return;
    case Qt::Key_Up:
    case Qt::Key_Down: {
      const int step = event->key() == Qt::Key_Up ? -1 : 1;
      for (size_t r = 0; r < rows_.size(); ++r) {
        if (std::find(rows_[r].begin(), rows_[r].end(), cur) == rows_[r].end()) continue;
        const int target_row = static_cast<int>(r) + step;
        if (target_row < 0 || target_row >= static_cast<int>(rows_.size())) return;
        const int x = cells_[cur].rect.center().x();
        int best = rows_[target_row].front();
        for (int candidate : rows_[target_row]) {
          if (std::abs(cells_[candidate].rect.center().x() - x) <
              std::abs(cells_[best].rect.center().x() - x)) {
            best = candidate;
          }
        }
        MoveCurrent(best);
        return;
      }
      return;
    }
    case Qt::Key_Space:
    case Qt::Key_Return:
    case Qt::Key_Enter:
      if (current_ >= 0 && !Dimmed(current_)) emit componentClicked(cells_[current_].text);
      return;
    default:
      QAbstractScrollArea::keyPressEvent(event);
  }
}

void ComponentCatalogue::focusInEvent(QFocusEvent *event) {
  QAbstractScrollArea::focusInEvent(event);
  if (current_ < 0 && !cells_.empty()) current_ = 0;
  show_focus_ring_ = event->reason() != Qt::MouseFocusReason;
  if (current_ >= 0) setAccessibleDescription(AccessibleText(current_));
  viewport()->update();
}

void ComponentCatalogue::focusOutEvent(QFocusEvent *event) {
  QAbstractScrollArea::focusOutEvent(event);
  viewport()->update();
}

// --- ComposerInfoButton / ComposerInfoPopover ------------------------------

ComposerInfoButton::ComposerInfoButton(const QString &accessible_name, QWidget *parent)
    : QToolButton(parent) {
  setObjectName(QStringLiteral("composerInfoButton"));
  setCursor(Qt::PointingHandCursor);
  setFocusPolicy(Qt::StrongFocus);
  setAutoRaise(true);
  setFixedSize(18, 18);
  setToolTip(accessible_name);
  setAccessibleName(accessible_name);
}

void ComposerInfoButton::paintEvent(QPaintEvent *) {
  // Drawn like PreferencesInfoButton so the two dialogs match.
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  const theme::Palette palette = theme::Colors(theme::IsDark());
  const QColor ink = hasFocus() ? palette.focus : palette.accent;
  const QRectF circle = QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5);
  painter.setPen(QPen(ink, 1.2));
  painter.setBrush(Qt::NoBrush);
  painter.drawEllipse(circle);
  QFont font = painter.font();
  font.setFamilies({QStringLiteral("Georgia"), QStringLiteral("Times New Roman"),
                    QStringLiteral("serif")});
  font.setPointSizeF(theme::PlatformPointSize(10));
  font.setWeight(QFont::DemiBold);
  painter.setFont(font);
  painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("i"));
}

ComposerInfoPopover::ComposerInfoPopover(QWidget *parent)
    : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint) {
  setObjectName(QStringLiteral("composerInfoPopover"));
  setMinimumWidth(300);
  setMaximumWidth(360);
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(16, 14, 16, 14);
  layout->setSpacing(8);
  title_ = new QLabel(this);
  title_->setObjectName(QStringLiteral("infoPopoverTitle"));
  title_->setWordWrap(true);
  title_->setFont(theme::UiFont(14, QFont::Medium));
  body_ = new QLabel(this);
  body_->setObjectName(QStringLiteral("infoPopoverBody"));
  body_->setWordWrap(true);
  body_->setTextFormat(Qt::RichText);
  body_->setFont(theme::UiFont(12));
  body_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  example_ = new QLabel(this);
  example_->setObjectName(QStringLiteral("infoPopoverExample"));
  example_->setWordWrap(true);
  example_->setFont(theme::UiFont(12));
  example_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  layout->addWidget(title_);
  layout->addWidget(body_);
  layout->addWidget(example_);
}

QString ComposerInfoPopover::title() const { return title_->text(); }

void ComposerInfoPopover::ShowFor(const QString &title, const QString &body,
                                  const QString &example, const QRect &global_anchor) {
  const theme::Palette colors = theme::Colors(theme::IsDark());
  setStyleSheet(theme::ApplyPaletteTokens(
      QStringLiteral(
          "QFrame#composerInfoPopover {"
          "  background: @@RAISED@@; border: 1px solid @@BORDER_CONTROL@@;"
          "  border-radius: 6px; }"
          "QLabel { background: transparent; color: @@TEXT@@; }"
          "QLabel#infoPopoverTitle { color: @@HEADING@@; }"
          "QLabel#infoPopoverExample {"
          "  color: @@SECONDARY@@; background: @@SIDEBAR@@;"
          "  border-radius: 3px; padding: 6px 8px; }"),
      colors));
  title_->setText(title);
  body_->setText(body);
  example_->setText(example);
  example_->setVisible(!example.isEmpty());
  adjustSize();

  // To the right of the button, or to its left when the screen ends.
  QPoint pos(global_anchor.right() + 8, global_anchor.top() - 12);
  if (QScreen *screen = QGuiApplication::screenAt(global_anchor.center())) {
    const QRect avail = screen->availableGeometry();
    if (pos.x() + width() > avail.right() - 8) {
      pos.setX(global_anchor.left() - width() - 8);
    }
    pos.setX(std::max(pos.x(), avail.left() + 8));
    pos.setY(std::clamp(pos.y(), avail.top() + 8,
                        std::max(avail.top() + 8, avail.bottom() - height() - 8)));
  }
  move(pos);
  show();
  raise();
}

void ComposerInfoPopover::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    hide();
    return;
  }
  QFrame::keyPressEvent(event);
}

// --- ComponentsPanel ---------------------------------------------------------

ComponentsPanel::ComponentsPanel(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("componentsPanel"));

  // One row above the catalogue: the first-stroke filter, and a View menu
  // with the display options.
  QFont caption_font = theme::UiFont(11, QFont::DemiBold);
  caption_font.setLetterSpacing(QFont::PercentageSpacing, 110.0);
  auto *first_stroke_key = new QLabel(Tr("First stroke").toUpper(), this);
  first_stroke_key->setObjectName(QStringLiteral("componentsCaption"));
  first_stroke_key->setFont(caption_font);

  auto *row = new QHBoxLayout;
  row->setContentsMargins(0, 0, 0, 0);
  row->setSpacing(4);
  row->addWidget(first_stroke_key);
  row->addSpacing(6);
  static const char *const kFirstStrokeNames[] = {"一", "丨", "丿", "丶", "乛"};
  static const char *const kFirstStrokeTips[] = {"horizontal 一", "vertical 丨",
                                                 "left-falling 丿", "dot 丶", "turning 乛"};
  for (int i = 0; i < 5; ++i) {
    auto *button = new QToolButton(this);
    button->setObjectName(QStringLiteral("firstStrokeTile"));
    button->setText(QString::fromUtf8(kFirstStrokeNames[i]));
    button->setFont(theme::UiFont(14));
    button->setCheckable(true);
    button->setFixedSize(26, 26);
    const QString name = Tr("First stroke: %1")
                             .arg(QCoreApplication::translate(kContext, kFirstStrokeTips[i]));
    button->setToolTip(name);
    button->setAccessibleName(name);
    connect(button, &QToolButton::toggled, this, [this]() {
      Rebuild(/*keep_position=*/false);
      emit optionsChanged();
    });
    first_stroke_buttons_.push_back(button);
    row->addWidget(button);
  }
  row->addStretch(1);

  auto *view_menu = new QMenu(this);
  variants_action_ = view_menu->addAction(Tr("Radical variants"));
  variants_action_->setCheckable(true);
  variants_action_->setChecked(true);
  variants_action_->setToolTip(Tr("Show radical variants (亻 扌 氵 …)"));
  hard_action_ = view_menu->addAction(Tr("Difficult components"));
  hard_action_->setCheckable(true);
  hard_action_->setChecked(false);
  hard_action_->setToolTip(Tr("Show difficult components"));
  grey_out_action_ = view_menu->addAction(Tr("Dim unavailable"));
  grey_out_action_->setCheckable(true);
  grey_out_action_->setChecked(true);
  grey_out_action_->setToolTip(Tr("Grey out components that find nothing"));
  view_menu->setToolTipsVisible(true);
  connect(variants_action_, &QAction::toggled, this, [this]() {
    Rebuild(/*keep_position=*/true);
    emit optionsChanged();
  });
  connect(hard_action_, &QAction::toggled, this, [this]() {
    Rebuild(/*keep_position=*/true);
    emit optionsChanged();
  });
  connect(grey_out_action_, &QAction::toggled, this, &ComponentsPanel::optionsChanged);
  auto *view_button = new QToolButton(this);
  view_button->setObjectName(QStringLiteral("viewMenuButton"));
  view_button->setText(Tr("View") + QStringLiteral(" ▾"));
  view_button->setFont(theme::UiFont(12));
  view_button->setPopupMode(QToolButton::InstantPopup);
  view_button->setMenu(view_menu);
  row->addWidget(view_button);

  auto *rule = new QFrame(this);
  rule->setObjectName(QStringLiteral("componentsRule"));
  rule->setFixedHeight(1);

  catalogue_ = new ComponentCatalogue(this);
  connect(catalogue_, &ComponentCatalogue::componentClicked, this,
          &ComponentsPanel::componentToggled);

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(0, 2, 0, 0);
  root->setSpacing(8);
  root->addLayout(row);
  root->addWidget(rule);
  root->addWidget(catalogue_, 1);

  ApplyTheme();
  Rebuild(/*keep_position=*/false);
}

void ComponentsPanel::ApplyTheme() {
  if (applying_theme_) return;
  applying_theme_ = true;
  const bool dark = theme::IsDark();
  const theme::Palette colors = theme::Colors(dark);
  setStyleSheet(theme::ApplyPaletteTokens(
      ComposerStylesheet(dark) +
          QStringLiteral(
              "QWidget#componentsPanel { background-color: @@WINDOW@@; }"
              "QFrame#componentsRule {"
              "  border: none; background-color: @@RULE@@; }"
              "QLabel#componentsCaption { color: @@SECONDARY@@; background: transparent; }"
              "QMenu { color: @@TEXT@@; background: @@RAISED@@;"
              "  border: 1px solid @@BORDER_CONTROL@@; padding: 4px; }"
              "QMenu::item { padding: 5px 14px 5px 26px; border-radius: 2px; }"
              "QMenu::item:selected {"
              "  background: @@SELECTION_BG@@; color: @@SELECTION_TEXT@@; }"),
      colors));
  theme::ApplySurfacePalette(this, colors);
  if (catalogue_ != nullptr) catalogue_->viewport()->update();
  applying_theme_ = false;
}

void ComponentsPanel::changeEvent(QEvent *event) {
  QWidget::changeEvent(event);
  if (event->type() == QEvent::PaletteChange ||
      event->type() == QEvent::ApplicationPaletteChange ||
      event->type() == QEvent::StyleChange) {
    ApplyTheme();
  }
}

void ComponentsPanel::SetFilters(bool difficult, int first_stroke_mask) {
  hard_action_->setChecked(difficult);
  for (size_t i = 0; i < first_stroke_buttons_.size(); ++i) {
    first_stroke_buttons_[i]->setChecked((first_stroke_mask >> i) & 1);
  }
  Rebuild(/*keep_position=*/false);
}

bool ComponentsPanel::grey_out_unavailable() const {
  return grey_out_action_->isChecked();
}

bool ComponentsPanel::Wanted(const std::string &ch) const {
  // A component no installed font can draw would only show as a box.
  if (!Coverage().Drawable(FromUtf8(ch))) return false;
  const std::map<std::string, int> &hard = HardLeadingStrokes();
  const auto hard_it = hard.find(ch);
  const bool is_hard = hard_it != hard.end();
  if (is_hard && !hard_action_->isChecked()) return false;
  bool any_stroke = false;
  bool stroke_matches = false;
  for (size_t i = 0; i < first_stroke_buttons_.size(); ++i) {
    if (!first_stroke_buttons_[i]->isChecked()) continue;
    any_stroke = true;
    const int lead = is_hard ? hard_it->second
                             : (table_ != nullptr ? table_->LeadingStroke(ch) : 0);
    // The button order 橫 豎 撇 點 折 is the order of the stroke classes.
    if (lead == static_cast<int>(i) + 1) stroke_matches = true;
  }
  return !any_stroke || stroke_matches;
}

std::vector<std::string> ComponentsPanel::ShownComponents() const {
  const std::vector<std::string> variants = SplitChars(kVariantForms);
  std::vector<std::string> shown;
  for (const MergedGroup &group : ComponentGroups()) {
    for (std::string &ch : SplitChars(group.components.c_str())) {
      if (!variants_action_->isChecked() && Contains(variants, ch)) continue;
      if (!Wanted(ch)) continue;
      shown.push_back(std::move(ch));
    }
  }
  return shown;
}

void ComponentsPanel::Rebuild(bool keep_position) {
  const std::vector<std::string> variants = SplitChars(kVariantForms);
  std::vector<ComponentCatalogue::Section> sections;
  for (const MergedGroup &group : ComponentGroups()) {
    ComponentCatalogue::Section section;
    section.strokes = group.strokes;
    for (std::string &ch : SplitChars(group.components.c_str())) {
      if (!variants_action_->isChecked() && Contains(variants, ch)) continue;
      if (!Wanted(ch)) continue;
      section.components.push_back(FromUtf8(ch));
    }
    if (!section.components.empty()) sections.push_back(std::move(section));
  }
  catalogue_->SetSections(std::move(sections), keep_position);
  SetState(selected_, unavailable_);
}

void ComponentsPanel::SetState(const std::vector<std::string> &selected,
                               const std::vector<std::string> &unavailable) {
  selected_ = selected;
  unavailable_ = unavailable;
  QSet<QString> marked;
  QSet<QString> dimmed;
  for (const std::string &ch : selected_) marked.insert(FromUtf8(ch));
  if (grey_out_action_->isChecked()) {
    for (const std::string &ch : unavailable_) {
      const QString text = FromUtf8(ch);
      if (!marked.contains(text)) dimmed.insert(text);
    }
  }
  catalogue_->SetState(marked, dimmed);
}

// --- CharacterComposerDialog ------------------------------------------------

CharacterComposerDialog::CharacterComposerDialog(
    std::unique_ptr<composer_ids::IdsTable> table, const QString &data_directory,
    QWidget *parent, std::unique_ptr<composer_ids::CharInfoTable> char_info)
    : QDialog(parent),
      table_(std::move(table)),
      char_info_(std::move(char_info)),
      data_directory_(data_directory) {
  kage_ = KageResources::LoadFromDirectory(data_directory_.toStdString());
  BuildUi(data_directory);
  ApplyTheme();
  OnLayoutChanged();
}

void CharacterComposerDialog::changeEvent(QEvent *event) {
  QDialog::changeEvent(event);
  if (applying_theme_) return;
  if (event->type() == QEvent::PaletteChange ||
      event->type() == QEvent::ApplicationPaletteChange ||
      event->type() == QEvent::StyleChange) {
    ApplyTheme();
    RefreshPaletteIcons();
    UpdateCompositionState();
    RefreshActiveView();
  }
}

void CharacterComposerDialog::BuildUi(const QString &data_directory) {
  setObjectName(QStringLiteral("CharacterComposerDialog"));
  setWindowTitle(Tr("Character composer"));
  setWindowFlags(Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint |
                 Qt::WindowTitleHint | Qt::WindowStaysOnTopHint);
  setMinimumSize(composer_dialog_metrics::kMinimumWidth,
                 composer_dialog_metrics::kMinimumHeight);
  resize(composer_dialog_metrics::kDefaultWidth,
         composer_dialog_metrics::kDefaultHeight);

  palette_ = new QButtonGroup(this);
  palette_->setExclusive(false);
  auto *palette_grid = new QGridLayout;
  palette_grid->setHorizontalSpacing(4);
  palette_grid->setVerticalSpacing(4);
  palette_grid->setContentsMargins(0, 0, 0, 0);
  int index = 0;
  for (const auto &info : composer_ids::IdsOperators()) {
    auto *button = new QToolButton(this);
    button->setObjectName(QStringLiteral("structureButton"));
    button->setCheckable(true);
    button->setIconSize(QSize(kStructureIcon, kStructureIcon));
    button->setFixedSize(kStructureButtonWidth, kStructureButtonHeight);
    // A sample character under the layout icon, as an illustration.
    const QString example = StructureExample(std::string(info.op));
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    button->setText(example);
    button->setFont(theme::UiFont(10));
    button->setToolTip(QStringLiteral("%1  %2  %3  ·  %4")
                           .arg(FromUtf8(std::string(info.op)),
                                FromUtf8(std::string(info.abbr)),
                                Tr(std::string(info.name).c_str()), example));
    button->setFocusPolicy(Qt::StrongFocus);
    palette_->addButton(button, index);
    palette_grid->addWidget(button, index / 6, index % 6);
    ++index;
  }
  connect(palette_, &QButtonGroup::idClicked, this,
          &CharacterComposerDialog::OnPaletteClicked);

  editor_ = new LayoutEditor(this);
  editor_->set_table(table_.get());
  connect(editor_, &LayoutEditor::changed, this,
          &CharacterComposerDialog::OnLayoutChanged);
  connect(editor_, &LayoutEditor::selectionChanged, this, [this]() {
    UpdatePaletteHighlight();
    UpdateComponentRoles();
    ActivateComposition();
  });
  connect(editor_, &LayoutEditor::flattenRefused, this, [this]() {
    note_ = Tr("Clear all but one of the filled boxes before removing this layout.");
    UpdateCompositionState();
    RefreshActiveView();
  });
  connect(editor_, &LayoutEditor::submitRequested, this,
          &CharacterComposerDialog::OnSubmitRequested);

  component_roles_label_ = new QLabel(this);
  component_roles_label_->setObjectName(QStringLiteral("componentRoles"));
  component_roles_label_->setFont(theme::UiFont(12, QFont::DemiBold));

  ids_edit_ = new QLineEdit(this);
  ids_edit_->setObjectName(QStringLiteral("ids_edit"));
  ids_edit_->setPlaceholderText(Tr("e.g. LR日月 or ⿰日月"));
  ids_edit_->installEventFilter(this);
  info_popover_ = new ComposerInfoPopover(this);
  ids_info_button_ = new ComposerInfoButton(Tr("About the IDS box"), this);
  connect(ids_info_button_, &QToolButton::clicked, this,
          [this]() { ShowInfo(ids_info_button_); });
  auto *ids_caption = MakeCaption(Tr("IDS"), this);
  ids_caption->setBuddy(ids_edit_);
  auto *ids_header = new QHBoxLayout;
  ids_header->setSpacing(6);
  ids_header->addWidget(ids_caption);
  ids_header->addWidget(ids_info_button_);
  ids_header->addStretch(1);
  connect(ids_edit_, &QLineEdit::textEdited, this,
          &CharacterComposerDialog::OnIdsEdited);
  connect(ids_edit_, &QLineEdit::editingFinished, this,
          [this]() { SyncIdsEditFromEditor(/*force=*/true); });

  construct_panel_ = new QWidget(this);
  construct_panel_->setObjectName(QStringLiteral("constructPanel"));
  auto *construct = new QVBoxLayout(construct_panel_);
  construct->setContentsMargins(12, 12, 10, 8);
  construct->setSpacing(6);
  construct->addWidget(MakeSectionHeading(Tr("Structure"), construct_panel_));
  construct->addLayout(palette_grid);
  construct->addSpacing(6);
  construct->addWidget(MakeSectionHeading(Tr("Components"), construct_panel_,
                                          component_roles_label_));
  construct->addWidget(editor_, 0);
  construct->addSpacing(6);
  construct->addLayout(ids_header);
  construct->addWidget(ids_edit_);
  construct->addStretch(1);
  construct_panel_->setFixedWidth(composer_dialog_metrics::kConstructWidth);

  // --- Examine --------------------------------------------------------------
  active_state_label_ = new QLabel(Tr("COMPOSITION"), this);
  active_state_label_->setObjectName(QStringLiteral("activeState"));
  active_state_label_->setAlignment(Qt::AlignCenter);
  active_state_label_->setFont(theme::UiFont(11, QFont::DemiBold));

  char_label_ = new QLabel(this);
  char_label_->setObjectName(QStringLiteral("specimenGlyph"));
  // Sit the glyph on the bottom of its band so the Unicode caption reads as
  // belonging to the specimen rather than floating below a tall empty box.
  char_label_->setAlignment(Qt::AlignHCenter | Qt::AlignBottom);
  // Match DrawingPreview / OutlinePreview (kPreviewLogical). A shorter band
  // let 152px drawings and a ~108px Unicode face spill out of the plate.
  char_label_->setFixedSize(kPreviewLogical, kPreviewLogical);
  char_label_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  char_label_->setFrameShape(QFrame::NoFrame);
  char_label_->setAutoFillBackground(false);
  // So specimenKind plate backgrounds from the stylesheet actually paint. Not
  // Qt::WA_TranslucentBackground: that stops them painting at all.
  char_label_->setAttribute(Qt::WA_StyledBackground, true);
  char_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  char_label_->setScaledContents(false);

  return_to_composition_ = new QPushButton(Tr("← Return to composition"), this);
  return_to_composition_->setObjectName(QStringLiteral("returnLink"));
  return_to_composition_->setFlat(true);
  return_to_composition_->setCursor(Qt::PointingHandCursor);
  return_to_composition_->setAutoDefault(false);
  return_to_composition_->setFont(theme::UiFont(11));
  // Keep a stable-height slot so the metadata divider does not jump when the
  // return action appears for a search result.
  {
    const QFontMetrics return_metrics(return_to_composition_->font());
    return_to_composition_->setFixedHeight(return_metrics.height() + 2);
  }
  return_to_composition_->setEnabled(false);
  connect(return_to_composition_, &QPushButton::clicked, this,
          &CharacterComposerDialog::OnReturnToComposition);

  status_heading_ = new QLabel(this);
  status_heading_->setObjectName(QStringLiteral("statusHeading"));
  status_heading_->setAlignment(Qt::AlignCenter);
  status_heading_->setWordWrap(true);
  status_heading_->setFont(theme::UiFont(12, QFont::Medium));

  status_detail_ = new QLabel(this);
  status_detail_->setObjectName(QStringLiteral("statusDetail"));
  status_detail_->setAlignment(Qt::AlignCenter);
  status_detail_->setWordWrap(true);
  status_detail_->setFont(theme::UiFont(11));
  status_detail_->setTextFormat(Qt::RichText);
  status_detail_->setOpenExternalLinks(true);
  status_detail_->setTextInteractionFlags(Qt::TextBrowserInteraction);

  specimen_group_ = new QWidget(this);
  specimen_group_->setObjectName(QStringLiteral("specimenGroup"));
  auto *specimen_layout = new QVBoxLayout(specimen_group_);
  specimen_layout->setContentsMargins(0, 0, 0, 0);
  specimen_layout->setSpacing(0);
  specimen_layout->addWidget(active_state_label_);
  specimen_layout->addWidget(char_label_, 0, Qt::AlignHCenter);
  specimen_layout->addSpacing(1);
  specimen_layout->addWidget(status_heading_);
  specimen_layout->addSpacing(2);
  specimen_layout->addWidget(return_to_composition_, 0, Qt::AlignHCenter);
  specimen_layout->addWidget(status_detail_);

  auto make_key = [this](const QString &text) {
    auto *label = new QLabel(text, this);
    label->setObjectName(QStringLiteral("infoKey"));
    label->setFont(theme::UiFont(13));
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    return label;
  };
  auto make_val = [this]() {
    auto *label = new QLabel(this);
    label->setObjectName(QStringLiteral("infoValue"));
    label->setFont(theme::UiFont(13));
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    return label;
  };

  on_label_ = make_key(Tr("On"));
  on_value_ = make_val();
  kun_label_ = make_key(Tr("Kun"));
  kun_value_ = make_val();
  meaning_label_ = make_key(Tr("Meaning"));
  meaning_value_ = make_val();
  variants_label_ = make_key(Tr("Variants"));
  variants_value_ = new QWidget(this);
  variants_value_->setObjectName(QStringLiteral("variantsChips"));
  auto *variants_layout = new QHBoxLayout(variants_value_);
  variants_layout->setContentsMargins(0, 0, 0, 0);
  variants_layout->setSpacing(2);
  variants_layout->addStretch(1);
  strokes_label_ = make_key(Tr("Strokes"));
  strokes_value_ = make_val();
  radical_label_ = make_key(Tr("Radical"));
  radical_value_ = make_val();
  other_label_ = make_key(Tr("Mandarin"));
  other_value_ = make_val();

  info_table_ = new QWidget(this);
  info_table_->setObjectName(QStringLiteral("infoTable"));
  auto *grid = new QGridLayout(info_table_);
  grid->setContentsMargins(0, 0, 0, 0);
  grid->setHorizontalSpacing(8);
  grid->setVerticalSpacing(4);
  grid->setColumnStretch(1, 1);
  const struct {
    QLabel *key;
    QWidget *val;
  } rows[] = {
      {on_label_, on_value_},         {kun_label_, kun_value_},
      {meaning_label_, meaning_value_}, {variants_label_, variants_value_},
      {strokes_label_, strokes_value_}, {radical_label_, radical_value_},
      {other_label_, other_value_},
  };
  // One shared label-column width from the longest localised key.
  {
    const QFontMetrics metrics(theme::UiFont(13));
    int label_width = 0;
    for (const auto &row : rows) {
      label_width =
          std::max(label_width, metrics.horizontalAdvance(row.key->text()));
    }
    label_width += 2;
    for (const auto &row : rows) {
      row.key->setFixedWidth(label_width);
    }
  }
  for (int r = 0; r < 7; ++r) {
    grid->addWidget(rows[r].key, r, 0, Qt::AlignLeft | Qt::AlignTop);
    grid->addWidget(rows[r].val, r, 1);
  }

  info_rule_ = new QFrame(this);
  auto *info_rule = info_rule_;
  info_rule->setObjectName(QStringLiteral("sectionRule"));
  info_rule->setFrameShape(QFrame::HLine);
  info_rule->setFixedHeight(1);

  auto *info_inner = new QWidget(this);
  auto *info_inner_layout = new QVBoxLayout(info_inner);
  info_inner_layout->setContentsMargins(0, 2, 4, 0);
  info_inner_layout->setSpacing(0);
  info_inner_layout->addWidget(info_table_);
  info_inner_layout->addStretch(1);

  auto *info_scroll = new QScrollArea(this);
  info_scroll->setObjectName(QStringLiteral("examineScroll"));
  info_scroll->setWidgetResizable(true);
  info_scroll->setFrameShape(QFrame::NoFrame);
  info_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  info_scroll->setWidget(info_inner);

  auto *character_page = new QWidget(this);
  auto *examine = new QVBoxLayout(character_page);
  examine->setContentsMargins(0, 0, 0, 0);
  examine->setSpacing(3);
  // Full width, not AlignHCenter: an aligned group is laid out at its narrow
  // size hint, but the height of its word-wrapped status text was worked out
  // for the full column, so the text ran into the character above it.
  examine->addWidget(specimen_group_);
  examine->addWidget(info_rule);
  examine->addWidget(info_scroll, 1);

  // The components picker shares the middle column with the character, so
  // the results on the right stay in view while components are picked.
  components_panel_ = new ComponentsPanel(this);
  components_panel_->set_table(table_.get());
  connect(components_panel_, &ComponentsPanel::componentToggled, this,
          &CharacterComposerDialog::OnComponentToggled);
  connect(components_panel_, &ComponentsPanel::optionsChanged, this,
          &CharacterComposerDialog::UpdateComponentsPanel);

  middle_stack_ = new QStackedWidget(this);
  middle_stack_->addWidget(character_page);
  middle_stack_->addWidget(components_panel_);

  auto *tabs = new QWidget(this);
  tabs->setObjectName(QStringLiteral("panelTabs"));
  auto *tabs_layout = new QHBoxLayout(tabs);
  tabs_layout->setContentsMargins(2, 2, 2, 2);
  tabs_layout->setSpacing(2);
  auto *tab_group = new QButtonGroup(this);
  tab_group->setExclusive(true);
  character_tab_ = new QPushButton(Tr("Character"), tabs);
  components_tab_ = new QPushButton(Tr("Components"), tabs);
  for (QPushButton *tab : {character_tab_, components_tab_}) {
    tab->setObjectName(QStringLiteral("panelTab"));
    tab->setCheckable(true);
    tab->setAutoDefault(false);
    tab->setFont(theme::UiFont(12));
    tab_group->addButton(tab);
    tabs_layout->addWidget(tab, 1);
  }
  character_tab_->setChecked(true);
  components_tab_->setEnabled(table_ != nullptr);
  connect(character_tab_, &QPushButton::clicked, this, [this]() { ShowMiddlePage(0); });
  connect(components_tab_, &QPushButton::clicked, this,
          &CharacterComposerDialog::OnShowComponentsTab);

  examine_panel_ = new QWidget(this);
  examine_panel_->setObjectName(QStringLiteral("examinePanel"));
  auto *middle = new QVBoxLayout(examine_panel_);
  middle->setContentsMargins(12, 10, 12, 8);
  middle->setSpacing(8);
  middle->addWidget(tabs);
  middle->addWidget(middle_stack_, 1);

  // --- Discover -------------------------------------------------------------
  break_apart_edit_ = new QLineEdit(this);
  break_apart_edit_->setObjectName(QStringLiteral("break_apart_edit"));
  break_apart_edit_->setPlaceholderText(Tr("Character"));
  break_button_ = new QPushButton(Tr("Break apart"), this);
  break_button_->setObjectName(QStringLiteral("secondaryButton"));
  break_button_->setAutoDefault(false);
  break_button_->setEnabled(table_ != nullptr);
  break_button_->setFixedWidth(100);
  decomposition_combo_ = new QComboBox(this);
  decomposition_combo_->setVisible(false);
  auto *break_row = new QHBoxLayout;
  break_row->setSpacing(6);
  break_row->addWidget(break_apart_edit_, 1);
  break_row->addWidget(break_button_);
  connect(break_button_, &QPushButton::clicked, this,
          &CharacterComposerDialog::OnBreakApart);
  connect(break_apart_edit_, &QLineEdit::returnPressed, this,
          &CharacterComposerDialog::OnBreakApart);
  connect(decomposition_combo_, QOverload<int>::of(&QComboBox::activated), this,
          [this](int i) {
            const std::string ids = ToUtf8(decomposition_combo_->itemText(i));
            if (auto parsed = composer_ids::ParseIdsToComposerTree(ids)) {
              ActivateComposition();
              editor_->SetLayout(parsed->op, std::move(parsed->parts));
            }
          });

  containing_edit_ = new QLineEdit(this);
  containing_edit_->setObjectName(QStringLiteral("containing_edit"));
  containing_edit_->setPlaceholderText(Tr("Enter components"));
  find_button_ = new QPushButton(Tr("Find"), this);
  find_button_->setObjectName(QStringLiteral("secondaryButton"));
  find_button_->setAutoDefault(false);
  find_button_->setEnabled(table_ != nullptr);
  find_button_->setFixedWidth(100);
  auto *find_row = new QHBoxLayout;
  find_row->setSpacing(6);
  find_row->addWidget(containing_edit_, 1);
  find_row->addWidget(find_button_);
  connect(find_button_, &QPushButton::clicked, this,
          &CharacterComposerDialog::OnFindContaining);
  connect(containing_edit_, &QLineEdit::returnPressed, this,
          &CharacterComposerDialog::OnFindContaining);
  // Greying out components costs a search each; wait for a pause in typing.
  panel_timer_ = new QTimer(this);
  panel_timer_->setSingleShot(true);
  panel_timer_->setInterval(150);
  connect(panel_timer_, &QTimer::timeout, this,
          &CharacterComposerDialog::UpdateComponentsPanel);
  connect(containing_edit_, &QLineEdit::textChanged, panel_timer_,
          QOverload<>::of(&QTimer::start));

  // The stroke count of the whole character: 12, <12, >8, 9-12 (the same
  // syntax the search field accepts). Empty means any.
  stroke_edit_ = new QLineEdit(this);
  stroke_edit_->setObjectName(QStringLiteral("stroke_filter"));
  stroke_edit_->setAccessibleName(Tr("Strokes"));
  stroke_edit_->setPlaceholderText(Tr("any"));
  stroke_edit_->setFixedWidth(90);
  stroke_edit_->setEnabled(table_ != nullptr);
  stroke_timer_ = new QTimer(this);
  stroke_timer_->setSingleShot(true);
  stroke_timer_->setInterval(250);
  connect(stroke_timer_, &QTimer::timeout, this,
          &CharacterComposerDialog::OnFindContaining);
  connect(stroke_edit_, &QLineEdit::textChanged, stroke_timer_,
          QOverload<>::of(&QTimer::start));
  connect(stroke_edit_, &QLineEdit::returnPressed, stroke_timer_, &QTimer::stop);
  connect(stroke_edit_, &QLineEdit::returnPressed, this,
          &CharacterComposerDialog::OnFindContaining);
  auto *strokes_key = new QLabel(Tr("Strokes"), this);
  strokes_key->setObjectName(QStringLiteral("infoKey"));
  strokes_key->setFont(theme::UiFont(12));
  strokes_key->setBuddy(stroke_edit_);
  auto *strokes_hint = new QLabel(QStringLiteral("12 · <12 · >8 · 9-12"), this);
  strokes_hint->setObjectName(QStringLiteral("strokeHint"));
  strokes_hint->setFont(theme::UiFont(11));
  auto *stroke_row = new QHBoxLayout;
  stroke_row->setSpacing(6);
  stroke_row->addWidget(strokes_key);
  stroke_row->addWidget(stroke_edit_);
  stroke_row->addWidget(strokes_hint);
  stroke_row->addStretch(1);

  // The explanations live behind the info buttons, not in the panel.
  const auto make_subhead = [this](const QString &text, ComposerInfoButton *info) {
    auto *row = new QHBoxLayout;
    row->setSpacing(6);
    auto *label = new QLabel(text, this);
    label->setObjectName(QStringLiteral("exploreSubhead"));
    label->setFont(theme::UiFont(13, QFont::Medium));
    row->addWidget(label);
    row->addWidget(info);
    row->addStretch(1);
    return row;
  };
  decompose_info_button_ = new ComposerInfoButton(Tr("About Decompose a character"), this);
  connect(decompose_info_button_, &QToolButton::clicked, this,
          [this]() { ShowInfo(decompose_info_button_); });
  find_info_button_ = new ComposerInfoButton(Tr("About Find by components"), this);
  connect(find_info_button_, &QToolButton::clicked, this,
          [this]() { ShowInfo(find_info_button_); });

  results_status_ = new QLabel(this);
  results_status_->setObjectName(QStringLiteral("resultsStatus"));
  results_status_->setWordWrap(true);
  results_status_->setFont(theme::UiFont(10));

  results_list_ = new QListWidget(this);
  results_list_->setObjectName(QStringLiteral("resultsList"));
  results_list_->setViewMode(QListView::IconMode);
  results_list_->setResizeMode(QListView::Adjust);
  results_list_->setMovement(QListView::Static);
  results_list_->setUniformItemSizes(true);
  results_list_->setGridSize(QSize(42, 42));
  results_list_->setMinimumHeight(72);
  results_list_->setFrameShape(QFrame::NoFrame);
  results_list_->setAttribute(Qt::WA_MacShowFocusRect, false);
  QFont results_font = results_list_->font();
  results_font.setPointSize(results_font.pointSize() + 7);
  results_list_->setFont(WithWideCoverage(results_font));
  connect(results_list_, &QListWidget::itemClicked, this,
          &CharacterComposerDialog::OnContainingPicked);
  connect(results_list_, &QListWidget::itemActivated, this,
          &CharacterComposerDialog::OnContainingPicked);

  clear_results_button_ = new QPushButton(Tr("Clear results"), this);
  clear_results_button_->setObjectName(QStringLiteral("textButton"));
  clear_results_button_->setAutoDefault(false);
  clear_results_button_->setVisible(false);
  connect(clear_results_button_, &QPushButton::clicked, this,
          &CharacterComposerDialog::OnClearSearchSelection);

  auto *discover_controls = new QWidget(this);
  auto *controls_layout = new QVBoxLayout(discover_controls);
  controls_layout->setContentsMargins(0, 0, 0, 0);
  controls_layout->setSpacing(5);
  controls_layout->addWidget(
      MakeSectionHeading(Tr("Explore characters"), discover_controls));
  controls_layout->addLayout(make_subhead(Tr("Decompose a character"), decompose_info_button_));
  controls_layout->addLayout(break_row);
  controls_layout->addWidget(decomposition_combo_);
  controls_layout->addSpacing(2);
  controls_layout->addLayout(make_subhead(Tr("Find by components"), find_info_button_));
  controls_layout->addLayout(find_row);
  controls_layout->addLayout(stroke_row);
  controls_layout->addSpacing(4);
  controls_layout->addWidget(
      MakeSectionHeading(Tr("Results"), discover_controls, clear_results_button_));
  controls_layout->addWidget(results_status_);

  discover_panel_ = new QWidget(this);
  discover_panel_->setObjectName(QStringLiteral("discoverPanel"));
  auto *discover = new QVBoxLayout(discover_panel_);
  discover->setContentsMargins(12, 10, 12, 8);
  discover->setSpacing(6);
  discover->addWidget(discover_controls, 0);
  discover->addWidget(results_list_, 1);

  auto *columns = new QHBoxLayout;
  columns->setContentsMargins(0, 0, 0, 0);
  columns->setSpacing(0);
  columns->addWidget(construct_panel_, 0);
  auto *div1 = new QFrame(this);
  div1->setObjectName(QStringLiteral("columnRule"));
  div1->setFrameShape(QFrame::VLine);
  div1->setFixedWidth(1);
  columns->addWidget(div1);
  columns->addWidget(examine_panel_, 47);
  auto *div2 = new QFrame(this);
  div2->setObjectName(QStringLiteral("columnRule"));
  div2->setFrameShape(QFrame::VLine);
  div2->setFixedWidth(1);
  columns->addWidget(div2);
  columns->addWidget(discover_panel_, 53);

  clear_button_ = new QPushButton(Tr("Clear"), this);
  clear_button_->setObjectName(QStringLiteral("textButton"));
  clear_button_->setAutoDefault(false);
  copy_button_ = new QPushButton(Tr("Copy"), this);
  copy_button_->setObjectName(QStringLiteral("outlineButton"));
  copy_button_->setAutoDefault(false);
  insert_button_ = new QPushButton(Tr("Insert"), this);
  insert_button_->setObjectName(QStringLiteral("primaryButton"));
  insert_button_->setDefault(true);
  close_button_ = new QPushButton(Tr("Close"), this);
  close_button_->setObjectName(QStringLiteral("textButton"));
  close_button_->setAutoDefault(false);
  connect(insert_button_, &QPushButton::clicked, this,
          &CharacterComposerDialog::OnInsert);
  connect(copy_button_, &QPushButton::clicked, this,
          &CharacterComposerDialog::OnCopy);
  connect(clear_button_, &QPushButton::clicked, this, [this]() {
    ActivateComposition();
    editor_->Clear();
  });
  connect(close_button_, &QPushButton::clicked, this, &QDialog::reject);

  auto *footer = new QHBoxLayout;
  footer->setContentsMargins(14, 5, 14, 6);
  footer->setSpacing(8);
  footer->addWidget(clear_button_);
  footer->addStretch(1);
  footer->addWidget(copy_button_);
  footer->addWidget(insert_button_);
  footer->addWidget(close_button_);

  auto *footer_rule = new QFrame(this);
  footer_rule->setObjectName(QStringLiteral("footerRule"));
  footer_rule->setFrameShape(QFrame::HLine);
  footer_rule->setFixedHeight(1);

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);
  root->addLayout(columns, 1);
  root->addWidget(footer_rule);
  root->addLayout(footer);

  RefreshPaletteIcons();
  UpdatePaletteHighlight();
  UpdateComponentRoles();
  (void)data_directory;
}

void CharacterComposerDialog::ApplyTheme() {
  if (applying_theme_) return;
  applying_theme_ = true;
  const bool dark = theme::IsDark();
  const theme::Palette colors = theme::Colors(dark);
  setStyleSheet(ComposerStylesheet(dark));
  theme::ApplySurfacePalette(this, colors);
  if (construct_panel_ != nullptr) {
    theme::ApplySurfacePalette(construct_panel_, colors);
  }
  if (examine_panel_ != nullptr) {
    theme::ApplySurfacePalette(examine_panel_, colors);
  }
  if (discover_panel_ != nullptr) {
    theme::ApplySurfacePalette(discover_panel_, colors);
  }
  if (editor_ != nullptr) {
    // Background/border come from the stylesheet; avoid a second surface frame.
    editor_->update();
  }
  if (copy_button_ != nullptr && insert_button_ != nullptr) {
    copy_button_->setMinimumWidth(0);
    insert_button_->setMinimumWidth(0);
    copy_button_->setMaximumWidth(QWIDGETSIZE_MAX);
    insert_button_->setMaximumWidth(QWIDGETSIZE_MAX);
    style()->unpolish(copy_button_);
    style()->unpolish(insert_button_);
    style()->polish(copy_button_);
    style()->polish(insert_button_);
    const int action_width = std::max(copy_button_->sizeHint().width(),
                                      insert_button_->sizeHint().width());
    copy_button_->setFixedWidth(action_width);
    insert_button_->setFixedWidth(action_width);
  }
  applying_theme_ = false;
  // The drawings on the plates take their colour from the mode.
  if (char_label_ != nullptr && status_heading_ != nullptr &&
      info_rule_ != nullptr && editor_ != nullptr) {
    RefreshActiveView();
  }
}

void CharacterComposerDialog::RefreshPaletteIcons() {
  if (palette_ == nullptr) return;
  const theme::Palette colors = theme::Colors(theme::IsDark());
  const auto operators = composer_ids::IdsOperators();
  for (size_t i = 0; i < operators.size(); ++i) {
    if (QAbstractButton *button = palette_->button(static_cast<int>(i))) {
      button->setIcon(OperatorIcon(std::string(operators[i].op), colors.heading));
    }
  }
}

void CharacterComposerDialog::UpdateComponentRoles() {
  if (component_roles_label_ == nullptr || editor_ == nullptr) return;
  component_roles_label_->setText(ComponentRolesFor(editor_->SelectedLayoutOp()));
}

void CharacterComposerDialog::SetInfoRow(QLabel *label, QWidget *value,
                                          const QString &text) {
  const bool show = !text.isEmpty();
  label->setVisible(show);
  value->setVisible(show);
  if (show) {
    if (auto *as_label = qobject_cast<QLabel *>(value)) {
      as_label->setText(text);
    }
  }
}

void CharacterComposerDialog::SetVariantsChips(
    const composer_ids::CharInfo *info) {
  if (variants_value_ == nullptr || variants_label_ == nullptr) return;
  auto *layout = qobject_cast<QHBoxLayout *>(variants_value_->layout());
  if (layout == nullptr) return;
  while (QLayoutItem *child = layout->takeAt(0)) {
    if (QWidget *w = child->widget()) w->deleteLater();
    delete child;
  }
  if (info == nullptr) {
    variants_label_->setVisible(false);
    variants_value_->setVisible(false);
    variants_value_->setToolTip(QString());
    return;
  }
  QFont variants_font = WithWideCoverage(theme::UiFont(16));
  const std::vector<VariantChip> chips =
      FormatVariantChips(*info, &variants_font);
  if (chips.empty()) {
    variants_label_->setVisible(false);
    variants_value_->setVisible(false);
    variants_value_->setToolTip(QString());
    return;
  }
  variants_label_->setVisible(true);
  variants_value_->setVisible(true);
  int missing = 0;
  for (const VariantChip &chip : chips) {
    auto *label = new QLabel(chip.text, variants_value_);
    label->setObjectName(QStringLiteral("variantChip"));
    label->setFont(variants_font);
    label->setProperty(
        "specimenKind",
        chip.kind.isEmpty() ? QString::fromUtf8(specimen_kind::kPlainName)
                            : chip.kind);
    label->setAttribute(Qt::WA_StyledBackground, true);
    // Click to make this variant the chosen character; right-click to copy
    // it or the whole row (see eventFilter).
    label->setCursor(Qt::PointingHandCursor);
    label->installEventFilter(this);
    QString tip = chip.tip;
    tip += (tip.isEmpty() ? QString() : QStringLiteral("\n")) +
           Tr("Click to choose this character. Right-click to copy.");
    label->setToolTip(tip);
    if (chip.kind == QLatin1String(specimen_kind::kMissingFontName)) {
      ++missing;
    }
    label->style()->unpolish(label);
    label->style()->polish(label);
    layout->addWidget(label, 0, Qt::AlignVCenter);
  }
  layout->addStretch(1);
  variants_value_->setToolTip(
      missing == 0 ? QString()
      : missing == 1
          ? Tr("1 variant has no installed font that can draw it.")
          : Tr("%1 variants have no installed font that can draw them.")
                .arg(missing));
}

void CharacterComposerDialog::UpdateCharInfoTable(
    const composer_ids::CharInfo *info) {
  if (info == nullptr) {
    SetInfoRow(on_label_, on_value_, QString());
    SetInfoRow(kun_label_, kun_value_, QString());
    SetInfoRow(meaning_label_, meaning_value_, QString());
    SetVariantsChips(nullptr);
    SetInfoRow(strokes_label_, strokes_value_, QString());
    SetInfoRow(radical_label_, radical_value_, QString());
    SetInfoRow(other_label_, other_value_, QString());
    info_rule_->setVisible(false);
    return;
  }
  info_rule_->setVisible(true);

  QStringList on;
  QStringList kun;
  if (!info->ja_kana.empty()) {
    SplitJaKana(FromUtf8(info->ja_kana), &on, &kun);
  } else {
    if (!info->ja_on.empty()) {
      on += FromUtf8(info->ja_on)
                .split(QRegularExpression(QStringLiteral("\\s+")),
                       Qt::SkipEmptyParts);
    }
    if (!info->ja_kun.empty()) {
      kun += FromUtf8(info->ja_kun)
                 .split(QRegularExpression(QStringLiteral("\\s+")),
                        Qt::SkipEmptyParts);
    }
  }
  SetInfoRow(on_label_, on_value_, on.isEmpty() ? QString() : JoinDot(on));
  SetInfoRow(kun_label_, kun_value_, kun.isEmpty() ? QString() : JoinDot(kun));
  SetInfoRow(meaning_label_, meaning_value_,
             info->definition.empty() ? QString() : FromUtf8(info->definition));
  SetVariantsChips(info);
  SetInfoRow(strokes_label_, strokes_value_,
             info->strokes.empty() ? QString() : FromUtf8(info->strokes));
  SetInfoRow(radical_label_, radical_value_,
             info->rs.empty() ? QString() : FromUtf8(info->rs));
  SetInfoRow(other_label_, other_value_,
             info->pinyin.empty() ? QString() : FromUtf8(info->pinyin));
}

void CharacterComposerDialog::ActivateComposition() {
  if (active_source_ == ActiveSource::kComposition && search_active_char_.empty()) {
    active_state_label_->setText(Tr("COMPOSITION"));
    return_to_composition_->setText(QString());
    return_to_composition_->setEnabled(false);
    return;
  }
  active_source_ = ActiveSource::kComposition;
  search_active_char_.clear();
  if (results_list_ != nullptr) results_list_->clearSelection();
  RefreshActiveView();
}

void CharacterComposerDialog::ActivateSearchResult(const std::string &ch) {
  if (ch.empty()) return;
  active_source_ = ActiveSource::kSearchResult;
  search_active_char_ = ch;
  RefreshActiveView();
}

void CharacterComposerDialog::OnReturnToComposition() { ActivateComposition(); }

void CharacterComposerDialog::OnClearSearchSelection() {
  if (results_list_ != nullptr) {
    results_list_->clear();
    if (results_status_ != nullptr) results_status_->clear();
  }
  if (clear_results_button_ != nullptr) clear_results_button_->setVisible(false);
  ActivateComposition();
}

bool CharacterComposerDialog::eventFilter(QObject *watched, QEvent *event) {
  if (watched == ids_edit_ && event->type() == QEvent::FocusIn) {
    ActivateComposition();
  }
  // A chip in the Variants row.
  if (auto *chip = qobject_cast<QLabel *>(watched);
      chip != nullptr && chip->objectName() == QLatin1String("variantChip")) {
    if (event->type() == QEvent::MouseButtonRelease &&
        static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) {
      const QString text = chip->text();
      // Rebuilds the row (this chip is deleted later), so nothing here may
      // touch |chip| afterwards.
      ActivateSearchResult(ToUtf8(text));
      return true;
    }
    if (event->type() == QEvent::ContextMenu) {
      QString all;
      if (variants_value_ != nullptr) {
        for (const QLabel *other :
             variants_value_->findChildren<QLabel *>(
                 QStringLiteral("variantChip"), Qt::FindDirectChildrenOnly)) {
          all += other->text();
        }
      }
      QMenu menu(chip);
      QAction *copy_one = menu.addAction(Tr("Copy %1").arg(chip->text()));
      QAction *copy_all = menu.addAction(Tr("Copy all variants"));
      copy_all->setEnabled(!all.isEmpty());
      const QPoint where = static_cast<QContextMenuEvent *>(event)->globalPos();
      QAction *chosen = menu.exec(where);
      if (chosen == copy_one) {
        QApplication::clipboard()->setText(chip->text());
      } else if (chosen == copy_all) {
        QApplication::clipboard()->setText(all);
      }
      return true;
    }
  }
  return QDialog::eventFilter(watched, event);
}

void CharacterComposerDialog::OnLayoutChanged() {
  note_.clear();
  ActivateComposition();
  // A change to the composition shows the composed character.
  ShowMiddlePage(0);
  UpdateCompositionState();
  RefreshActiveView();
  UpdatePaletteHighlight();
  UpdateComponentRoles();
}

void CharacterComposerDialog::OnPaletteClicked(int index) {
  ActivateComposition();
  const auto operators = composer_ids::IdsOperators();
  if (index < 0 || static_cast<size_t>(index) >= operators.size()) return;
  editor_->ApplyOperator(std::string(operators[index].op));
}

void CharacterComposerDialog::UpdatePaletteHighlight() {
  const std::string current = editor_->SelectedLayoutOp();
  const auto operators = composer_ids::IdsOperators();
  for (size_t i = 0; i < operators.size(); ++i) {
    if (QAbstractButton *button = palette_->button(static_cast<int>(i))) {
      button->setChecked(std::string(operators[i].op) == current);
    }
  }
}

void CharacterComposerDialog::SyncIdsEditFromEditor(bool force) {
  if (ids_edit_ == nullptr || applying_ids_edit_) return;
  if (!force && ids_edit_->hasFocus()) return;
  const QString draft =
      FromUtf8(composer_ids::DraftIds(editor_->op(), editor_->parts()));
  if (ids_edit_->text() != draft) ids_edit_->setText(draft);
}

void CharacterComposerDialog::ShowInfo(QWidget *anchor) {
  if (info_popover_ == nullptr || anchor == nullptr) return;
  QString title;
  QString body;
  QString example;
  if (anchor == ids_info_button_) {
    title = Tr("IDS box");
    body = Tr("Type an Ideographic Description Sequence here. "
              "These codes become layout operators:");
    body += QStringLiteral("<table cellspacing=\"3\">");
    for (const auto &info : composer_ids::IdsOperators()) {
      body += QStringLiteral("<tr><td><b>%1</b></td><td>%2</td><td>%3</td></tr>")
                  .arg(FromUtf8(std::string(info.abbr)),
                       FromUtf8(std::string(info.op)),
                       Tr(std::string(info.name).c_str()));
    }
    body += QStringLiteral("</table>");
    body += QStringLiteral("<p>%1</p>")
                .arg(Tr("A question mark (?) stands for an empty box."));
    example = QStringLiteral("LR氵每  →  ⿰氵每  →  海");
  } else if (anchor == decompose_info_button_) {
    title = Tr("Decompose a character");
    body = Tr("Type or paste one character and press Break apart (or Enter). Its "
              "parts fill the layout boxes on the left, where you can change "
              "them. When a character has more than one decomposition, a list "
              "lets you pick one.");
    example = QStringLiteral("海  →  ⿰ 氵 每");
  } else if (anchor == find_info_button_) {
    title = Tr("Find by components");
    body = Tr("Type components in the box, or click them in the Components tab: "
              "the results follow as you pick. Strokes limits the stroke count of "
              "the whole character: 12 (exactly), &lt;12 (fewer), &gt;8 (more), "
              "9-12 (between). A count typed after the components works too.") +
           QStringLiteral("<p>%1</p>")
               .arg(Tr("Results are sorted by stroke count, then by first stroke "
                       "一 丨 丿 丶 乛. Click one to examine it."));
    example = Tr("木 and Strokes 8  →  林 松 枝 板 …");
  } else {
    return;
  }
  const QRect anchor_rect(anchor->mapToGlobal(QPoint(0, 0)), anchor->size());
  info_popover_->ShowFor(title, body, example, anchor_rect);
}

void CharacterComposerDialog::OnIdsEdited(const QString &text) {
  ActivateComposition();
  const std::string expanded =
      composer_ids::ExpandIdsAbbreviations(ToUtf8(text));
  const QString expanded_q = FromUtf8(expanded);
  if (expanded_q != text) {
    const int from_end = text.size() - ids_edit_->cursorPosition();
    ids_edit_->setText(expanded_q);
    ids_edit_->setCursorPosition(
        std::max(0, static_cast<int>(expanded_q.size()) - from_end));
  }

  auto parsed = composer_ids::ParseIdsToComposerTree(expanded);
  if (!parsed) return;
  applying_ids_edit_ = true;
  editor_->SetLayout(parsed->op, std::move(parsed->parts));
  applying_ids_edit_ = false;
}

void CharacterComposerDialog::UpdateCompositionState() {
  SyncIdsEditFromEditor();
  found_.clear();
  pending_svg_.clear();

  if (const auto ids = composer_ids::ComposeIds(editor_->op(), editor_->parts())) {
    pending_svg_ = RenderComposerSvg(editor_->op(), editor_->parts(), 1000,
                                     kage_.get());
    if (table_ != nullptr) {
      if (const auto found = table_->FindEncodedCharacter(*ids)) {
        found_ = *found;
      }
    }
  }
}

void CharacterComposerDialog::RefreshActiveView() {
  const bool search_active = active_source_ == ActiveSource::kSearchResult &&
                             !search_active_char_.empty();
  active_state_label_->setText(search_active ? Tr("SEARCH RESULT")
                                             : Tr("COMPOSITION"));
  return_to_composition_->setText(search_active ? Tr("← Return to composition")
                                                : QString());
  return_to_composition_->setEnabled(search_active);

  QString status_heading;
  QString status_detail;
  bool insertable = false;
  bool prompt_shown = false;
  QString specimen_kind = QStringLiteral("plain");
  const composer_ids::CharInfo *info = nullptr;
  const QColor plate_ink = PlateInkColor(theme::IsDark());

  auto show_unicode_text = [&](const QString &text) {
    // Pixel size leaves room inside the kPreviewLogical plate for CJK
    // ascent/descent (point-size 108 was ~full box height and overflowed).
    QFont face = theme::DisplayFont(108);
    face.setPixelSize(static_cast<int>(kPreviewLogical * 0.72));
    char_label_->setFont(Coverage().FontFor(face, text));
    char_label_->setPixmap(QPixmap());
    char_label_->setText(text);
    status_heading = CodepointHeading(text);
  };

  const bool prefer_glyphwiki =
      config::ConfigHandler::GetSharedConfig()->prefer_glyphwiki_drawings();

  // Prefer GlyphWiki's registered uXXXX glyph (local dump, then cache/network)
  // when the setting is on and |unicode_ch| names a character; otherwise the
  // IDS composition. Unencoded structures pass an empty |unicode_ch|.
  auto show_composition_drawing = [&](const QColor &ink,
                                      std::string_view unicode_ch = {}) {
    char_label_->setFont(font());
    char_label_->setText(QString());
    std::optional<std::vector<Contour>> outlines;
    if (prefer_glyphwiki && kage_ != nullptr && !unicode_ch.empty()) {
      outlines = NamedGlyphOutlines(unicode_ch, *kage_);
      if (!outlines) {
        const std::string name = GlyphWikiNameForCharacter(unicode_ch);
        kage::Buhin scratch(&kage_->glyphs);
        if (!name.empty() &&
            EnsureGlyphWikiGlyph(name, &scratch, /*allow_network=*/true)) {
          outlines = NamedGlyphOutlinesFromBuhin(name, scratch);
        }
      }
    }
    if (!outlines && kage_ != nullptr) {
      outlines = ComposerOutlines(editor_->op(), editor_->parts(), *kage_);
    }
    char_label_->setPixmap(
        outlines ? OutlinePreview(*outlines, ink, devicePixelRatioF())
                 : DrawingPreview(ComposerGlyphs(editor_->op(), editor_->parts()),
                                  ink, devicePixelRatioF()));
  };

  if (search_active) {
    const QString text = FromUtf8(search_active_char_);
    if (!Coverage().Drawable(text) && prefer_glyphwiki) {
      specimen_kind = QStringLiteral("missingFont");
      show_composition_drawing(plate_ink, search_active_char_);
      status_detail = MissingFontDetail();
    } else {
      show_unicode_text(text);
      if (!Coverage().Drawable(text)) {
        specimen_kind = QStringLiteral("missingFont");
        status_detail = MissingFontDetail();
      }
    }
    insertable = true;
    if (char_info_ != nullptr) info = char_info_->Find(search_active_char_);
  } else {
    if (table_ == nullptr) {
      status_detail =
          Tr("Character data was not found in %1, so the character cannot be "
             "looked up. You can still build the IDS above.")
              .arg(data_directory_);
    }
    if (!found_.empty()) {
      const QString text = FromUtf8(found_);
      status_heading = CodepointHeading(text);
      insertable = true;
      if (char_info_ != nullptr) info = char_info_->Find(found_);
      if (Coverage().Drawable(text)) {
        show_unicode_text(text);
      } else {
        // Encoded, but nothing installed can draw it: show GlyphWiki's glyph
        // when preferred, otherwise the IDS composition, on the amber plate.
        specimen_kind = QStringLiteral("missingFont");
        show_composition_drawing(plate_ink, found_);
        status_detail = MissingFontDetail();
      }
    } else if (!pending_svg_.empty()) {
      specimen_kind = QStringLiteral("svg");
      // Unencoded: always local IDS composition (never GlyphWiki fetch).
      show_composition_drawing(plate_ink);
      status_heading = Tr("Not encoded in Unicode");
      status_detail =
          Tr("Insert places a drawing on the clipboard for Word "
             "(Kaeriten → Insert composed character) or other office plugins.");
      insertable = true;
    } else {
      char_label_->setFont(theme::UiFont(13));
      char_label_->setPixmap(QPixmap());
      char_label_->setText(Tr("Complete the composition to preview the character."));
      prompt_shown = true;
      specimen_kind.clear();
    }
  }
  ApplySpecimenPresentation(char_label_, specimen_kind, prompt_shown);

  if (!note_.isEmpty()) status_detail = note_;

  UpdateCharInfoTable(info);
  status_heading_->setText(status_heading);
  status_detail_->setText(status_detail);
  insert_button_->setEnabled(insertable);
  copy_button_->setEnabled(insertable);
}

void CharacterComposerDialog::OnSubmitRequested() { TryInsert(); }

void CharacterComposerDialog::TryInsert() {
  if (QInputMethod *input_method = QGuiApplication::inputMethod()) {
    input_method->commit();
  }
  if (insert_button_ != nullptr && insert_button_->isEnabled()) {
    OnInsert();
  }
}

void CharacterComposerDialog::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    QWidget *focus = focusWidget();
    // Enter in the search fields runs the search, not Insert.
    const auto in_box = [focus](const QWidget *box) {
      return focus != nullptr && (focus == box || box->isAncestorOf(focus));
    };
    if (in_box(stroke_edit_)) {
      stroke_timer_->stop();
      OnFindContaining();
    } else if (focus != break_apart_edit_ && focus != containing_edit_) {
      TryInsert();
    }
    return;
  }
  QDialog::keyPressEvent(event);
}

void CharacterComposerDialog::OnInsert() {
  result_.clear();
  result_svg_.clear();
  if (active_source_ == ActiveSource::kSearchResult &&
      !search_active_char_.empty()) {
    result_ = search_active_char_;
    accept();
    return;
  }
  if (!found_.empty()) {
    result_ = found_;
    accept();
    return;
  }
  if (pending_svg_.empty()) return;
  result_svg_ = pending_svg_;
  auto *mime = new QMimeData;
  mime->setData(QStringLiteral("image/svg+xml"),
                QByteArray::fromStdString(result_svg_));
  mime->setText(QString::fromStdString(result_svg_));
  QApplication::clipboard()->setMimeData(mime);
  if (on_svg_inserted_) on_svg_inserted_(result_svg_);
  accept();
}

void CharacterComposerDialog::OnCopy() {
  if (active_source_ == ActiveSource::kSearchResult &&
      !search_active_char_.empty()) {
    QApplication::clipboard()->setText(FromUtf8(search_active_char_));
    note_ = Tr("Copied to the clipboard.");
    RefreshActiveView();
    return;
  }
  if (!found_.empty()) {
    QApplication::clipboard()->setText(FromUtf8(found_));
    note_ = Tr("Copied to the clipboard.");
    RefreshActiveView();
    return;
  }
  if (pending_svg_.empty()) return;
  auto *mime = new QMimeData;
  mime->setData(QStringLiteral("image/svg+xml"),
                QByteArray::fromStdString(pending_svg_));
  mime->setText(QString::fromStdString(pending_svg_));
  QApplication::clipboard()->setMimeData(mime);
  note_ = Tr("Drawing copied to the clipboard.");
  RefreshActiveView();
}

void CharacterComposerDialog::OnBreakApart() {
  decomposition_combo_->setVisible(false);
  decomposition_combo_->clear();
  if (table_ == nullptr) return;
  const std::string input = ToUtf8(break_apart_edit_->text());
  std::vector<std::string> options;
  for (std::string &ids : table_->FindDecompositions(input)) {
    if (composer_ids::ParseIdsToComposerTree(ids)) options.push_back(std::move(ids));
  }
  if (options.empty()) {
    note_ = Tr("No decomposition on record for this character.");
    results_status_->setText(note_);
    RefreshActiveView();
    return;
  }
  // Decompose intentionally replaces the composition with editable parts.
  ActivateComposition();
  auto parsed = composer_ids::ParseIdsToComposerTree(options.front());
  editor_->SetLayout(parsed->op, std::move(parsed->parts));
  if (parsed->unencoded_count > 0) {
    note_ = Tr("Some parts have no encoded character; fill those boxes in.");
  }
  if (options.size() > 1) {
    for (const std::string &ids : options) {
      decomposition_combo_->addItem(FromUtf8(ids));
    }
    decomposition_combo_->setVisible(true);
  }
}

composer_ids::LookupQuery CharacterComposerDialog::CurrentQuery() const {
  composer_ids::LookupQuery query =
      composer_ids::ParseLookupQuery(ToUtf8(containing_edit_->text()));
  // Only the stroke filter counts in the Strokes field; other text is ignored.
  const std::optional<composer_ids::StrokeRange> field =
      composer_ids::ParseLookupQuery(ToUtf8(stroke_edit_->text())).strokes;
  if (!field.has_value()) return query;
  composer_ids::StrokeRange range = *field;
  if (range.min > range.max) std::swap(range.min, range.max);  // "12-9"
  if (query.strokes.has_value()) {
    // A count typed in the search field as well: both must hold.
    range.min = std::max(range.min, query.strokes->min);
    range.max = std::min(range.max, query.strokes->max);
  }
  query.strokes = range;
  return query;
}

void CharacterComposerDialog::OnFindContaining() {
  results_list_->clear();
  results_status_->clear();
  if (clear_results_button_ != nullptr) clear_results_button_->setVisible(false);
  if (table_ == nullptr) return;
  const composer_ids::LookupQuery query = CurrentQuery();
  UpdateComponentsPanel();
  if (query.components.empty() && !query.strokes.has_value()) return;
  if (query.strokes.has_value() && !table_->has_stroke_counts()) {
    results_status_->setText(Tr("Stroke counts were not found in %1, so the "
                                "stroke filter cannot be used.")
                                 .arg(data_directory_));
    return;
  }
  std::vector<std::string> matches =
      table_->FindContaining(query.components, query.strokes);
  if (matches.empty()) {
    results_status_->setText(
        query.strokes.has_value()
            ? Tr("No character matches these components and stroke count.")
            : Tr("No character contains all of these components."));
    RefreshActiveView();
    return;
  }
  GlyphCoverage &coverage = Coverage();
  size_t missing_font = 0;
  for (const std::string &ch : matches) {
    if (!coverage.Drawable(FromUtf8(ch))) ++missing_font;
  }
  const QString missing_note =
      missing_font == 0 ? QString()
      : missing_font == 1
          ? QStringLiteral(" ") +
                Tr("1 has no installed font (amber).")
          : QStringLiteral(" ") +
                Tr("%1 have no installed font (amber).").arg(missing_font);
  table_->SortByStrokes(&matches);

  // The list is only a pick-list; a component like 一 matches thousands.
  // |matches| is sorted by stroke count, so when there are headings the limit
  // applies to each stroke count: a broad range still reaches its high end.
  constexpr size_t kMaxShown = 500;
  constexpr size_t kMinPerCount = 40;
  const bool headings = table_->has_stroke_counts();
  std::vector<std::optional<int>> counts;
  size_t groups = 0;
  for (const std::string &ch : matches) {
    counts.push_back(table_->StrokeCount(ch));
    if (counts.size() == 1 || counts.back() != counts[counts.size() - 2]) {
      ++groups;
    }
  }
  const size_t limit =
      headings ? std::max(kMinPerCount, kMaxShown / std::max<size_t>(groups, 1))
               : kMaxShown;
  const QFont heading_font = theme::UiFont(11, QFont::DemiBold);
  size_t shown = 0;
  size_t in_group = 0;
  for (size_t i = 0; i < matches.size(); ++i) {
    const std::optional<int> strokes = counts[i];
    const bool new_group = i == 0 || strokes != counts[i - 1];
    if (new_group) in_group = 0;
    if (headings && new_group) {
      // A stroke-count heading cell, like the numbers in Components….
      auto *heading = new QListWidgetItem(
          strokes ? QString::number(*strokes) : QStringLiteral("?"), results_list_);
      heading->setTextAlignment(Qt::AlignCenter);
      heading->setFlags(Qt::NoItemFlags);
      heading->setData(kHeadingRole, true);
      heading->setFont(heading_font);
      heading->setToolTip(strokes ? Tr("Strokes: %1").arg(*strokes)
                                  : Tr("Stroke count unknown"));
    }
    if (in_group++ >= limit) continue;
    ++shown;
    const QString text = FromUtf8(matches[i]);
    auto *item = new QListWidgetItem(text, results_list_);
    item->setTextAlignment(Qt::AlignCenter);
    const bool drawable = coverage.Drawable(text);
    // A character only an unnamed installed font has: name that font.
    if (drawable && !coverage.Check(text).extra_family.isEmpty()) {
      item->setFont(coverage.FontFor(results_list_->font(), text));
    }
    if (!drawable) {
      const bool dark_plates = theme::IsDark();
      item->setBackground(
          PlateColor(specimen_kind::Kind::kMissingFont, dark_plates));
      item->setForeground(PlateInkColor(dark_plates));
      item->setData(kSpecimenKindRole,
                    QString::fromUtf8(specimen_kind::kMissingFontName));
    } else {
      item->setData(kSpecimenKindRole,
                    QString::fromUtf8(specimen_kind::kPlainName));
    }
    const QString leading = LeadingStrokeGlyph(table_->LeadingStroke(matches[i]));
    QString tip;
    if (strokes && !leading.isEmpty()) {
      tip = Tr("Strokes: %1, starts with %2").arg(*strokes).arg(leading);
    } else if (strokes) {
      tip = Tr("Strokes: %1").arg(*strokes);
    }
    if (!drawable) {
      const QString missing =
          Tr("No installed font can draw this character.");
      tip = tip.isEmpty() ? missing : tip + QStringLiteral("\n") + missing;
    }
    if (!tip.isEmpty()) item->setToolTip(tip);
  }
  if (clear_results_button_ != nullptr) clear_results_button_->setVisible(true);
  if (shown == matches.size()) {
    results_status_->setText(
        Tr("%1 characters found. Click one to examine it.").arg(matches.size()) +
        missing_note);
  } else if (headings) {
    results_status_->setText(
        Tr("%1 characters found; showing the first %2 of each stroke count. "
           "Add components or narrow the strokes.")
            .arg(matches.size())
            .arg(limit) +
        missing_note);
  } else {
    results_status_->setText(
        Tr("%1 characters found; showing the first %2. Add components or "
           "narrow the strokes.")
            .arg(matches.size())
            .arg(kMaxShown) +
        missing_note);
  }
}

void CharacterComposerDialog::OnShowComponentsTab() { ShowMiddlePage(1); }

void CharacterComposerDialog::ShowMiddlePage(int page) {
  if (middle_stack_ == nullptr) return;
  if (page == 1 && table_ == nullptr) page = 0;
  middle_stack_->setCurrentIndex(page);
  character_tab_->setChecked(page == 0);
  components_tab_->setChecked(page == 1);
  if (page == 1) UpdateComponentsPanel();
}

void CharacterComposerDialog::OnComponentToggled(const QString &component) {
  // Rebuild the field from its parts, keeping a stroke count typed there.
  QString text = containing_edit_->text();
  if (text.contains(component)) {
    text.remove(component);
  } else {
    if (!text.isEmpty() && !text.endsWith(QLatin1Char(' '))) text += QLatin1Char(' ');
    text += component;
  }
  containing_edit_->setText(text.simplified());
  OnFindContaining();
}

void CharacterComposerDialog::UpdateComponentsPanel() {
  if (components_panel_ == nullptr) return;
  panel_timer_->stop();
  const composer_ids::LookupQuery query = CurrentQuery();
  const std::vector<std::string> selected =
      SplitChars(query.components.c_str());
  std::vector<std::string> unavailable;
  // Only worth working out while the window shows it.
  if (table_ != nullptr && components_panel_->isVisible() &&
      components_panel_->grey_out_unavailable() &&
      (!query.components.empty() || query.strokes.has_value()) &&
      (!query.strokes.has_value() || table_->has_stroke_counts())) {
    const std::vector<std::string> shown = components_panel_->ShownComponents();
    // The answer depends on these alone; options toggled in the window and
    // repeated calls for the same text reuse it.
    const std::string key =
        query.components + "|" +
        (query.strokes ? std::to_string(query.strokes->min) + "-" +
                             std::to_string(query.strokes->max)
                       : std::string()) +
        "|" + std::to_string(shown.size());
    if (key != unavailable_key_) {
      unavailable_cache_.clear();
      for (const std::string &ch : shown) {
        if (Contains(selected, ch)) continue;
        // Unavailable also when every match is hidden as undrawable.
        const std::vector<std::string> found =
            table_->FindContaining(query.components + ch, query.strokes);
        if (std::none_of(found.begin(), found.end(),
                         [](const std::string &match) {
                           return Coverage().Drawable(FromUtf8(match));
                         })) {
          unavailable_cache_.push_back(ch);
        }
      }
      unavailable_key_ = key;
    }
    unavailable = unavailable_cache_;
  }
  components_panel_->SetState(selected, unavailable);
}

void CharacterComposerDialog::OnContainingPicked(QListWidgetItem *item) {
  if (item == nullptr) return;
  if (item->data(kHeadingRole).toBool()) return;  // a stroke-count heading
  // Browse only: do not overwrite LayoutEditor / IDS.
  ActivateSearchResult(ToUtf8(item->text()));
  ShowMiddlePage(0);
}

void CharacterComposerDialog::ApplyIdsExpression(const std::string &ids) {
  auto parsed = composer_ids::ParseIdsToComposerTree(
      composer_ids::ExpandIdsAbbreviations(ids));
  if (!parsed) return;
  ActivateComposition();
  applying_ids_edit_ = true;
  editor_->SetLayout(parsed->op, std::move(parsed->parts));
  applying_ids_edit_ = false;
  SyncIdsEditFromEditor(/*force=*/true);
}

void CharacterComposerDialog::RunComponentSearch(const std::string &components,
                                                 int result_index) {
  if (containing_edit_ == nullptr) return;
  containing_edit_->setText(FromUtf8(components));
  OnFindContaining();
  if (result_index < 0 || results_list_ == nullptr) return;
  // |result_index| counts characters only, not the stroke-count headings.
  for (int row = 0; row < results_list_->count(); ++row) {
    QListWidgetItem *item = results_list_->item(row);
    if (item->data(kHeadingRole).toBool()) continue;
    if (result_index-- > 0) continue;
    results_list_->setCurrentItem(item);
    OnContainingPicked(item);
    return;
  }
}

void CharacterComposerDialog::OpenComponentsForDemo(const QSize &size, bool difficult,
                                                    const QString &strokes,
                                                    int first_stroke_mask) {
  if (components_panel_ == nullptr) return;
  if (size.isValid()) resize(size);
  stroke_edit_->setText(strokes);
  stroke_timer_->stop();
  ShowMiddlePage(1);
  components_panel_->SetFilters(difficult, first_stroke_mask);
  OnFindContaining();
}

bool CharacterComposerDialog::SaveComponentsPicture(const QString &path) {
  return grab().save(path, "PNG");
}

}  // namespace gui
}  // namespace mozc
