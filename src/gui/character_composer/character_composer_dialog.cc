// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "gui/character_composer/character_composer_dialog.h"

#include <QAbstractButton>
#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
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
#include <QListView>
#include <QListWidget>
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
#include <QSizePolicy>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "composer_ids/char_info.h"
#include "composer_ids/ids_operator.h"
#include "composer_ids/ids_table.h"
#include "composer_ids/ids_tree.h"
#include "gui/character_composer/composer_kage.h"
#include "gui/character_composer/composer_svg.h"
#include "gui/character_composer/layout_geometry.h"
#include "gui/config_dialog/config_dialog_theme.h"

namespace mozc {
namespace gui {

namespace composer_dialog_metrics {
// Slightly under the previous 216px editor so the default window can sit closer
// to a typical metadata table without starving nested editing.
constexpr int kEditorHeight = 188;
constexpr int kStructureButton = 44;
constexpr int kStructureIcon = 28;
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
using composer_dialog_metrics::kStructureButton;
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

QPixmap DrawingPreview(const std::vector<PlacedGlyph> &glyphs, const QPalette &palette,
                       qreal device_pixel_ratio) {
  const int kSize = kPreviewLogical;
  QPixmap pixmap(static_cast<int>(kSize * device_pixel_ratio),
                 static_cast<int>(kSize * device_pixel_ratio));
  pixmap.setDevicePixelRatio(device_pixel_ratio);
  pixmap.fill(Qt::transparent);
  QPainter p(&pixmap);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::TextAntialiasing);
  p.setPen(palette.color(QPalette::Text));
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

QPixmap OutlinePreview(const std::vector<Contour> &outlines, const QPalette &palette,
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
  p.setBrush(palette.color(QPalette::Text));
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

QString FormatVariants(const composer_ids::CharInfo &info) {
  QStringList parts;
  if (!info.variants.empty()) {
    const QString raw = FromUtf8(info.variants);
    for (const QString &token :
         raw.split(QRegularExpression(QStringLiteral("\\s+")),
                   Qt::SkipEmptyParts)) {
      parts << token;
    }
  }
  if (!info.old_form.empty()) {
    parts << Tr("old form: %1").arg(FromUtf8(info.old_form));
  }
  if (!info.new_form.empty()) {
    parts << Tr("new form: %1").arg(FromUtf8(info.new_form));
  }
  return parts.isEmpty() ? QString() : parts.join(QStringLiteral("  "));
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

QString ComposerStylesheet(bool dark) {
  const theme::Palette palette = theme::Colors(dark);
  QString qss = QStringLiteral(
      "QDialog#CharacterComposerDialog {"
      "  background-color: @@WINDOW@@; }"
      "QWidget#constructPanel { background-color: @@SIDEBAR@@; }"
      "QWidget#examinePanel, QWidget#discoverPanel {"
      "  background-color: @@WINDOW@@; }"
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
      "QLabel#ids_info_panel {"
      "  color: @@TEXT@@; background: @@RAISED@@;"
      "  border: 1px solid @@BORDER_CONTROL@@; border-radius: 4px;"
      "  padding: 8px; }"
      "QLabel#specimenGlyph {"
      "  background: transparent; border: none; padding: 0; margin: 0; }"
      "QWidget#layoutEditor {"
      "  background-color: @@SIDEBAR@@;"
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
      "QLineEdit#slotEdit {"
      "  padding: 1px; border-radius: 2px; border: 1px solid transparent;"
      "  background: transparent; }"
      "QLineEdit#slotEdit:focus { border: 1px solid @@FOCUS@@; }"
      "QLineEdit#slotEdit[selected=\"true\"] {"
      "  border: 1px solid @@ACCENT@@; }"

      "QToolButton#structureButton {"
      "  background: transparent; border: 1px solid @@RULE@@;"
      "  border-radius: 3px; padding: 1px; }"
      "QToolButton#structureButton:hover {"
      "  background: @@RAISED@@; border-color: @@ACCENT@@; }"
      "QToolButton#structureButton:checked {"
      "  background: @@ACCENT_SUBTLE@@; border: 1px solid @@ACCENT@@; }"
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

// --- CharacterComposerDialog ------------------------------------------------

CharacterComposerDialog::CharacterComposerDialog(
    std::unique_ptr<composer_ids::IdsTable> table, const QString &data_directory,
    QWidget *parent, std::unique_ptr<composer_ids::CharInfoTable> char_info)
    : QDialog(parent),
      table_(std::move(table)),
      char_info_(std::move(char_info)),
      data_directory_(data_directory) {
  if (auto resources = KageResources::LoadFromDirectory(data_directory_.toStdString())) {
    kage_ = std::make_unique<KageResources>(*std::move(resources));
  }
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
  palette_grid->setHorizontalSpacing(8);
  palette_grid->setVerticalSpacing(5);
  palette_grid->setContentsMargins(0, 0, 0, 0);
  int index = 0;
  for (const auto &info : composer_ids::IdsOperators()) {
    auto *button = new QToolButton(this);
    button->setObjectName(QStringLiteral("structureButton"));
    button->setCheckable(true);
    button->setIconSize(QSize(kStructureIcon, kStructureIcon));
    button->setFixedSize(kStructureButton, kStructureButton);
    button->setToolTip(QStringLiteral("%1  %2  %3")
                           .arg(FromUtf8(std::string(info.op)),
                                FromUtf8(std::string(info.abbr)),
                                Tr(std::string(info.name).c_str())));
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
  component_roles_label_->setFont(theme::UiFont(13, QFont::DemiBold));

  ids_edit_ = new QLineEdit(this);
  ids_edit_->setObjectName(QStringLiteral("ids_edit"));
  ids_edit_->setPlaceholderText(Tr("e.g. LR日月 or ⿰日月"));
  ids_edit_->installEventFilter(this);
  ids_info_button_ = new QToolButton(this);
  ids_info_button_->setText(QStringLiteral("ℹ"));
  ids_info_button_->setCheckable(true);
  ids_info_button_->setAutoRaise(true);
  ids_info_button_->setFocusPolicy(Qt::NoFocus);
  ids_info_button_->setToolTip(Tr("Layout codes for the IDS box"));
  auto *ids_row = new QHBoxLayout;
  ids_row->setSpacing(6);
  ids_row->addWidget(ids_edit_, 1);
  ids_row->addWidget(ids_info_button_, 0, Qt::AlignTop);

  ids_info_panel_ = new QLabel(this);
  ids_info_panel_->setObjectName(QStringLiteral("ids_info_panel"));
  ids_info_panel_->setWordWrap(true);
  ids_info_panel_->setTextFormat(Qt::RichText);
  ids_info_panel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  ids_info_panel_->setVisible(false);
  {
    QString html = Tr("Type an Ideographic Description Sequence here. "
                      "These codes become layout operators:");
    html += QStringLiteral("<table cellspacing=\"4\">");
    for (const auto &info : composer_ids::IdsOperators()) {
      html += QStringLiteral("<tr><td><b>%1</b></td><td>%2</td><td>%3</td></tr>")
                  .arg(FromUtf8(std::string(info.abbr)),
                       FromUtf8(std::string(info.op)),
                       Tr(std::string(info.name).c_str()));
    }
    html += QStringLiteral("</table>");
    html += QStringLiteral("<p>%1</p>")
                .arg(Tr("A question mark (?) stands for an empty box."));
    ids_info_panel_->setText(html);
  }
  connect(ids_edit_, &QLineEdit::textEdited, this,
          &CharacterComposerDialog::OnIdsEdited);
  connect(ids_edit_, &QLineEdit::editingFinished, this,
          [this]() { SyncIdsEditFromEditor(/*force=*/true); });
  connect(ids_info_button_, &QToolButton::toggled, this,
          &CharacterComposerDialog::OnIdsInfoToggled);

  construct_panel_ = new QWidget(this);
  construct_panel_->setObjectName(QStringLiteral("constructPanel"));
  auto *construct = new QVBoxLayout(construct_panel_);
  construct->setContentsMargins(12, 10, 10, 8);
  construct->setSpacing(7);
  construct->addWidget(MakeSectionHeading(Tr("Structure"), construct_panel_));
  construct->addLayout(palette_grid);
  construct->addWidget(MakeSectionHeading(Tr("Components"), construct_panel_));
  construct->addWidget(component_roles_label_);
  construct->addWidget(editor_, 0);
  construct->addLayout(ids_row);
  construct->addWidget(ids_info_panel_);
  construct->addStretch(1);

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
  char_label_->setMinimumWidth(kPreviewLogical);
  char_label_->setFixedHeight(128);
  char_label_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  char_label_->setFrameShape(QFrame::NoFrame);
  char_label_->setAutoFillBackground(false);
  char_label_->setAttribute(Qt::WA_TranslucentBackground, true);
  char_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);

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
  variants_value_ = make_val();
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
    QLabel *val;
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

  auto *info_rule = new QFrame(this);
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

  examine_panel_ = new QWidget(this);
  examine_panel_->setObjectName(QStringLiteral("examinePanel"));
  auto *examine = new QVBoxLayout(examine_panel_);
  examine->setContentsMargins(12, 10, 12, 8);
  examine->setSpacing(3);
  examine->addWidget(specimen_group_, 0, Qt::AlignHCenter);
  examine->addWidget(info_rule);
  examine->addWidget(info_scroll, 1);

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

  auto *decompose_label = new QLabel(Tr("Decompose a character"), this);
  decompose_label->setObjectName(QStringLiteral("exploreSubhead"));
  decompose_label->setFont(theme::UiFont(13, QFont::Medium));
  auto *find_label = new QLabel(Tr("Find by components"), this);
  find_label->setObjectName(QStringLiteral("exploreSubhead"));
  find_label->setFont(theme::UiFont(13, QFont::Medium));

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
  results_list_->setFont(results_font);
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
  controls_layout->addWidget(decompose_label);
  controls_layout->addLayout(break_row);
  controls_layout->addWidget(decomposition_combo_);
  controls_layout->addSpacing(2);
  controls_layout->addWidget(find_label);
  controls_layout->addLayout(find_row);
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
  columns->addWidget(construct_panel_, 32);
  auto *div1 = new QFrame(this);
  div1->setObjectName(QStringLiteral("columnRule"));
  div1->setFrameShape(QFrame::VLine);
  div1->setFixedWidth(1);
  columns->addWidget(div1);
  columns->addWidget(examine_panel_, 33);
  auto *div2 = new QFrame(this);
  div2->setObjectName(QStringLiteral("columnRule"));
  div2->setFrameShape(QFrame::VLine);
  div2->setFixedWidth(1);
  columns->addWidget(div2);
  columns->addWidget(discover_panel_, 35);

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
    theme::ApplySurfacePalette(construct_panel_, colors, /*sidebar_surface=*/true);
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

void CharacterComposerDialog::SetInfoRow(QLabel *label, QLabel *value,
                                          const QString &text) {
  const bool show = !text.isEmpty();
  label->setVisible(show);
  value->setVisible(show);
  if (show) value->setText(text);
}

void CharacterComposerDialog::UpdateCharInfoTable(
    const composer_ids::CharInfo *info) {
  if (info == nullptr) {
    SetInfoRow(on_label_, on_value_, QString());
    SetInfoRow(kun_label_, kun_value_, QString());
    SetInfoRow(meaning_label_, meaning_value_, QString());
    SetInfoRow(variants_label_, variants_value_, QString());
    SetInfoRow(strokes_label_, strokes_value_, QString());
    SetInfoRow(radical_label_, radical_value_, QString());
    SetInfoRow(other_label_, other_value_, QString());
    return;
  }

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
  SetInfoRow(variants_label_, variants_value_, FormatVariants(*info));
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
    results_status_->clear();
  }
  if (clear_results_button_ != nullptr) clear_results_button_->setVisible(false);
  ActivateComposition();
}

bool CharacterComposerDialog::eventFilter(QObject *watched, QEvent *event) {
  if (watched == ids_edit_ && event->type() == QEvent::FocusIn) {
    ActivateComposition();
  }
  return QDialog::eventFilter(watched, event);
}

void CharacterComposerDialog::OnLayoutChanged() {
  note_.clear();
  ActivateComposition();
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

void CharacterComposerDialog::OnIdsInfoToggled(bool checked) {
  ids_info_panel_->setVisible(checked);
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
  const composer_ids::CharInfo *info = nullptr;

  if (search_active) {
    QFont char_font = theme::DisplayFont(108);
    char_label_->setFont(char_font);
    char_label_->setPixmap(QPixmap());
    const QString text = FromUtf8(search_active_char_);
    char_label_->setText(text);
    const QList<uint> cps = text.toUcs4();
    status_heading =
        cps.isEmpty()
            ? QString()
            : QStringLiteral("U+%1")
                  .arg(cps.first(), 4, 16, QLatin1Char('0'))
                  .toUpper();
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
      QFont char_font = theme::DisplayFont(108);
      char_label_->setFont(char_font);
      char_label_->setPixmap(QPixmap());
      const QString text = FromUtf8(found_);
      char_label_->setText(text);
      const QList<uint> cps = text.toUcs4();
      status_heading =
          cps.isEmpty()
              ? QString()
              : QStringLiteral("U+%1")
                    .arg(cps.first(), 4, 16, QLatin1Char('0'))
                    .toUpper();
      insertable = true;
      if (char_info_ != nullptr) info = char_info_->Find(found_);
    } else if (!pending_svg_.empty()) {
      char_label_->setFont(font());
      char_label_->setText(QString());
      std::optional<std::vector<Contour>> outlines;
      if (kage_ != nullptr) {
        outlines = ComposerOutlines(editor_->op(), editor_->parts(), *kage_);
      }
      char_label_->setPixmap(
          outlines ? OutlinePreview(*outlines, palette(), devicePixelRatioF())
                   : DrawingPreview(ComposerGlyphs(editor_->op(), editor_->parts()),
                                    palette(), devicePixelRatioF()));
      status_heading = Tr("Not encoded in Unicode");
      status_detail =
          Tr("Insert places a drawing on the clipboard for Word "
             "(Kaeriten → Insert composed character) or other office plugins.");
      insertable = true;
    } else {
      QFont prompt = theme::UiFont(13);
      char_label_->setFont(prompt);
      char_label_->setPixmap(QPixmap());
      char_label_->setText(Tr("Fill in every box to look up the character."));
      status_heading = Tr("Incomplete composition");
      if (status_detail.isEmpty()) {
        status_detail = Tr("Enter a character in each component box.");
      }
    }
  }

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
    if (focus != break_apart_edit_ && focus != containing_edit_) TryInsert();
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

void CharacterComposerDialog::OnFindContaining() {
  results_list_->clear();
  if (clear_results_button_ != nullptr) clear_results_button_->setVisible(false);
  if (table_ == nullptr) return;
  const std::vector<std::string> matches =
      table_->FindContaining(ToUtf8(containing_edit_->text()));
  if (matches.empty()) {
    results_status_->setText(
        Tr("No character contains all of these components."));
    RefreshActiveView();
    return;
  }
  constexpr size_t kMaxShown = 500;
  for (size_t i = 0; i < matches.size() && i < kMaxShown; ++i) {
    auto *item = new QListWidgetItem(FromUtf8(matches[i]), results_list_);
    item->setTextAlignment(Qt::AlignCenter);
  }
  if (clear_results_button_ != nullptr) clear_results_button_->setVisible(true);
  results_status_->setText(
      matches.size() > kMaxShown
          ? Tr("%1 characters found; showing the first %2. Add more "
               "components to narrow the search.")
                .arg(matches.size())
                .arg(kMaxShown)
          : Tr("%1 characters found. Click one to examine it.")
                .arg(matches.size()));
}

void CharacterComposerDialog::OnContainingPicked(QListWidgetItem *item) {
  if (item == nullptr) return;
  // Browse only: do not overwrite LayoutEditor / IDS.
  ActivateSearchResult(ToUtf8(item->text()));
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
  if (result_index >= results_list_->count()) return;
  QListWidgetItem *item = results_list_->item(result_index);
  results_list_->setCurrentItem(item);
  OnContainingPicked(item);
}

}  // namespace gui
}  // namespace mozc
