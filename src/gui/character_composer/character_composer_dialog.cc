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
constexpr int kEditorHeight = 216;
constexpr int kStructureButton = 44;
constexpr int kStructureIcon = 28;
constexpr int kPreviewLogical = 152;
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

void DrawGeometry(QPainter *p, const OperatorGeometry &g, const QRectF &box) {
  for (const FLine &line : g.dividers) {
    p->drawLine(QPointF(box.x() + line.x1 * box.width(), box.y() + line.y1 * box.height()),
                QPointF(box.x() + line.x2 * box.width(), box.y() + line.y2 * box.height()));
  }
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

QWidget *MakeSectionHeading(const QString &number, const QString &title,
                             QWidget *parent) {
  auto *row = new QWidget(parent);
  auto *layout = new QHBoxLayout(row);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);
  if (!number.isEmpty()) {
    auto *num = new QLabel(number, row);
    num->setObjectName(QStringLiteral("sectionNumber"));
    num->setFont(theme::DisplayFont(18));
    theme::EnsureHeadingMetrics(num);
    layout->addWidget(num, 0, Qt::AlignVCenter);
  }
  auto *label = new QLabel(title, row);
  label->setObjectName(QStringLiteral("sectionHeading"));
  label->setFont(theme::DisplayFont(22));
  theme::EnsureHeadingMetrics(label);
  layout->addWidget(label, 0, Qt::AlignVCenter);
  auto *rule = new QFrame(row);
  rule->setObjectName(QStringLiteral("sectionRule"));
  rule->setFrameShape(QFrame::HLine);
  rule->setFixedHeight(1);
  rule->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  layout->addWidget(rule, 1, Qt::AlignVCenter);
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
  return parts.join(QStringLiteral(" · "));
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

QLabel *MakeInfoValue(QWidget *parent) {
  auto *label = new QLabel(QStringLiteral("—"), parent);
  label->setObjectName(QStringLiteral("infoValue"));
  label->setWordWrap(true);
  label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  return label;
}

QString ComposerStylesheet(bool dark) {
  const theme::Palette palette = theme::Colors(dark);
  QString qss = QStringLiteral(
      "QDialog#CharacterComposerDialog {"
      "  background-color: @@WINDOW@@; }"
      "QWidget#constructPanel { background-color: @@SIDEBAR@@; }"
      "QWidget#examinePanel, QWidget#explorePanel {"
      "  background-color: @@WINDOW@@; }"
      "QScrollArea#exploreScroll, QScrollArea#exploreScroll > QWidget > QWidget {"
      "  background: transparent; border: none; }"
      "QFrame#columnRule, QFrame#footerRule {"
      "  border: none; color: @@RULE@@; background-color: @@RULE@@; }"
      "QFrame#sectionRule {"
      "  border: none; color: @@TITLE_RULE@@; background-color: @@TITLE_RULE@@; }"

      "QLabel { color: @@TEXT@@; background: transparent; }"
      "QLabel#sectionHeading { color: @@HEADING@@; }"
      "QLabel#sectionNumber { color: @@SECONDARY@@; }"
      "QLabel#componentRoles { color: @@ACCENT@@; }"
      "QLabel#specimenCaption {"
      "  color: @@SECONDARY@@; letter-spacing: 0.14em; }"
      "QLabel#statusHeading { color: @@HEADING@@; }"
      "QLabel#statusDetail { color: @@SECONDARY@@; }"
      "QLabel#infoKey { color: @@SECONDARY@@; }"
      "QLabel#infoValue { color: @@TEXT@@; }"
      "QLabel#exploreSubhead { color: @@HEADING@@; }"
      "QLabel#ids_info_panel {"
      "  color: @@TEXT@@; background: @@RAISED@@;"
      "  border: 1px solid @@BORDER_CONTROL@@; border-radius: 4px;"
      "  padding: 8px; }"
      "QLabel#specimenGlyph {"
      "  background: transparent; border: none; padding: 0; margin: 0; }"
      "QWidget#layoutEditor {"
      "  background-color: @@RAISED@@;"
      "  border: 1px solid @@RULE@@; border-radius: 3px; }"
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
      "  background: @@RAISED@@; border: 1px solid @@BORDER_CONTROL@@;"
      "  border-radius: 3px; padding: 1px; }"
      "QToolButton#structureButton:hover { border-color: @@ACCENT@@; }"
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

      "QListWidget#containingList {"
      "  color: @@TEXT@@; background: @@RAISED@@;"
      "  border: 1px solid @@BORDER_CONTROL@@; border-radius: 3px;"
      "  outline: none; }"
      "QListWidget#containingList::item:selected {"
      "  background: @@SELECTION_BG@@; color: @@SELECTION_TEXT@@; }"

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
      "  border: 1px solid @@PRIMARY@@; padding: 6px 16px; }"
      "QPushButton#primaryButton:hover {"
      "  background: @@PRIMARY_HOVER@@; border-color: @@PRIMARY_HOVER@@; }"
      "QPushButton#primaryButton:pressed {"
      "  background: @@PRIMARY_PRESSED@@; border-color: @@PRIMARY_PRESSED@@; }"
      "QPushButton#primaryButton:disabled {"
      "  color: @@WINDOW@@; background: @@DISABLED@@; border-color: @@DISABLED@@; }"
      "QPushButton#outlineButton, QPushButton#secondaryButton {"
      "  color: @@ACCENT@@; background: @@WINDOW@@;"
      "  border: 1px solid @@BORDER_CONTROL@@;"
      "  min-width: 88px; padding: 5px 12px; }"
      "QPushButton#outlineButton:hover, QPushButton#secondaryButton:hover {"
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
  if (selected_) {
    p.setPen(QPen(colors.accent, 1.5));
  } else {
    p.setPen(QPen(colors.rule, 1, Qt::SolidLine));
  }
  p.drawRect(box);
  p.setPen(QPen(colors.rule, 1, Qt::DashLine));
  DrawGeometry(&p, GeometryFor(op_), box);
}

void LayoutView::mousePressEvent(QMouseEvent *event) {
  // Only the padding and gaps reach here; fields take their own clicks.
  editor_->Select(path_);
  event->accept();
}

// --- LayoutEditor -----------------------------------------------------------

LayoutEditor::LayoutEditor(QWidget *parent) : QWidget(parent) {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(4, 4, 4, 4);
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
    UpdateResult();
  }
}

void CharacterComposerDialog::BuildUi(const QString &data_directory) {
  setObjectName(QStringLiteral("CharacterComposerDialog"));
  setWindowTitle(Tr("Character composer"));
  setWindowFlags(Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint |
                 Qt::WindowTitleHint | Qt::WindowStaysOnTopHint);
  setMinimumSize(900, 620);
  resize(980, 740);

  palette_ = new QButtonGroup(this);
  palette_->setExclusive(false);
  auto *palette_grid = new QGridLayout;
  palette_grid->setHorizontalSpacing(8);
  palette_grid->setVerticalSpacing(8);
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
  });
  connect(editor_, &LayoutEditor::flattenRefused, this, [this]() {
    note_ = Tr("Clear all but one of the filled boxes before removing this layout.");
    UpdateResult();
  });
  connect(editor_, &LayoutEditor::submitRequested, this,
          &CharacterComposerDialog::OnSubmitRequested);

  component_roles_label_ = new QLabel(this);
  component_roles_label_->setObjectName(QStringLiteral("componentRoles"));
  component_roles_label_->setFont(theme::UiFont(13, QFont::DemiBold));

  ids_edit_ = new QLineEdit(this);
  ids_edit_->setObjectName(QStringLiteral("ids_edit"));
  ids_edit_->setPlaceholderText(Tr("e.g. LR日月 or ⿰日月"));
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
  construct->setContentsMargins(14, 14, 12, 10);
  construct->setSpacing(10);
  construct->addWidget(
      MakeSectionHeading(Tr("01"), Tr("Structure"), construct_panel_));
  construct->addLayout(palette_grid);
  construct->addWidget(
      MakeSectionHeading(Tr("02"), Tr("Components"), construct_panel_));
  construct->addWidget(component_roles_label_);
  construct->addWidget(editor_, 0);
  construct->addLayout(ids_row);
  construct->addWidget(ids_info_panel_);
  construct->addStretch(1);

  char_label_ = new QLabel(this);
  char_label_->setObjectName(QStringLiteral("specimenGlyph"));
  char_label_->setAlignment(Qt::AlignCenter);
  char_label_->setMinimumSize(kPreviewLogical, kPreviewLogical);
  char_label_->setFixedHeight(kPreviewLogical + 12);
  char_label_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  char_label_->setFrameShape(QFrame::NoFrame);
  char_label_->setAutoFillBackground(false);
  char_label_->setAttribute(Qt::WA_TranslucentBackground, true);
  char_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);

  specimen_caption_ = new QLabel(Tr("COMPOSED CHARACTER"), this);
  specimen_caption_->setObjectName(QStringLiteral("specimenCaption"));
  specimen_caption_->setAlignment(Qt::AlignCenter);
  specimen_caption_->setFont(theme::UiFont(10, QFont::DemiBold));

  status_heading_ = new QLabel(this);
  status_heading_->setObjectName(QStringLiteral("statusHeading"));
  status_heading_->setAlignment(Qt::AlignCenter);
  status_heading_->setWordWrap(true);
  status_heading_->setFont(theme::UiFont(16, QFont::Medium));

  status_detail_ = new QLabel(this);
  status_detail_->setObjectName(QStringLiteral("statusDetail"));
  status_detail_->setAlignment(Qt::AlignCenter);
  status_detail_->setWordWrap(true);
  status_detail_->setFont(theme::UiFont(12));

  specimen_group_ = new QWidget(this);
  specimen_group_->setObjectName(QStringLiteral("specimenGroup"));
  auto *specimen_layout = new QVBoxLayout(specimen_group_);
  specimen_layout->setContentsMargins(0, 0, 0, 0);
  specimen_layout->setSpacing(8);
  specimen_layout->addWidget(char_label_, 0, Qt::AlignHCenter);
  specimen_layout->addWidget(specimen_caption_);
  specimen_layout->addWidget(status_heading_);
  specimen_layout->addWidget(status_detail_);

  examine_panel_ = new QWidget(this);
  examine_panel_->setObjectName(QStringLiteral("examinePanel"));
  auto *examine = new QVBoxLayout(examine_panel_);
  examine->setContentsMargins(16, 20, 16, 16);
  examine->setSpacing(0);
  examine->addStretch(1);
  examine->addWidget(specimen_group_, 0, Qt::AlignHCenter);
  examine->addStretch(1);

  japanese_readings_value_ = MakeInfoValue(this);
  other_readings_value_ = MakeInfoValue(this);
  meaning_value_ = MakeInfoValue(this);
  classification_value_ = MakeInfoValue(this);
  variants_value_ = MakeInfoValue(this);
  structure_value_ = MakeInfoValue(this);
  unicode_value_ = MakeInfoValue(this);

  auto add_info_row = [](QVBoxLayout *host, const QString &key, QLabel *value) {
    auto *key_label = new QLabel(key);
    key_label->setObjectName(QStringLiteral("infoKey"));
    key_label->setFont(theme::UiFont(13));
    value->setFont(theme::UiFont(14));
    host->addWidget(key_label);
    host->addWidget(value);
    host->addSpacing(6);
  };

  auto *info_block = new QWidget(this);
  auto *info_layout = new QVBoxLayout(info_block);
  info_layout->setContentsMargins(0, 0, 0, 0);
  info_layout->setSpacing(2);
  add_info_row(info_layout, Tr("Japanese readings"), japanese_readings_value_);
  add_info_row(info_layout, Tr("Other readings"), other_readings_value_);
  add_info_row(info_layout, Tr("Meaning"), meaning_value_);
  add_info_row(info_layout, Tr("Classification"), classification_value_);
  add_info_row(info_layout, Tr("Variants"), variants_value_);
  add_info_row(info_layout, Tr("Structure"), structure_value_);
  add_info_row(info_layout, Tr("Unicode"), unicode_value_);

  break_apart_edit_ = new QLineEdit(this);
  break_apart_edit_->setObjectName(QStringLiteral("break_apart_edit"));
  break_apart_edit_->setPlaceholderText(Tr("Type a character"));
  break_button_ = new QPushButton(Tr("Break apart"), this);
  break_button_->setObjectName(QStringLiteral("secondaryButton"));
  break_button_->setAutoDefault(false);
  break_button_->setEnabled(table_ != nullptr);
  break_button_->setFixedWidth(104);
  decomposition_combo_ = new QComboBox(this);
  decomposition_combo_->setVisible(false);
  auto *break_row = new QHBoxLayout;
  break_row->setSpacing(8);
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
              editor_->SetLayout(parsed->op, std::move(parsed->parts));
            }
          });

  containing_edit_ = new QLineEdit(this);
  containing_edit_->setObjectName(QStringLiteral("containing_edit"));
  containing_edit_->setPlaceholderText(Tr("Type one or more components"));
  find_button_ = new QPushButton(Tr("Find"), this);
  find_button_->setObjectName(QStringLiteral("secondaryButton"));
  find_button_->setAutoDefault(false);
  find_button_->setEnabled(table_ != nullptr);
  find_button_->setFixedWidth(104);
  containing_list_ = new QListWidget(this);
  containing_list_->setObjectName(QStringLiteral("containingList"));
  containing_list_->setViewMode(QListView::IconMode);
  containing_list_->setResizeMode(QListView::Adjust);
  containing_list_->setMovement(QListView::Static);
  containing_list_->setUniformItemSizes(true);
  containing_list_->setGridSize(QSize(44, 44));
  containing_list_->setMinimumHeight(100);
  containing_list_->setMaximumHeight(180);
  containing_list_->setVisible(false);
  QFont containing_font = containing_list_->font();
  containing_font.setPointSize(containing_font.pointSize() + 8);
  containing_list_->setFont(containing_font);
  auto *find_row = new QHBoxLayout;
  find_row->setSpacing(8);
  find_row->addWidget(containing_edit_, 1);
  find_row->addWidget(find_button_);
  connect(find_button_, &QPushButton::clicked, this,
          &CharacterComposerDialog::OnFindContaining);
  connect(containing_edit_, &QLineEdit::returnPressed, this,
          &CharacterComposerDialog::OnFindContaining);
  connect(containing_list_, &QListWidget::itemClicked, this,
          &CharacterComposerDialog::OnContainingPicked);
  connect(containing_list_, &QListWidget::itemActivated, this,
          &CharacterComposerDialog::OnContainingPicked);

  auto *decompose_label = new QLabel(Tr("Decompose a character"), this);
  decompose_label->setObjectName(QStringLiteral("exploreSubhead"));
  decompose_label->setFont(theme::UiFont(13, QFont::DemiBold));
  auto *find_label = new QLabel(Tr("Find by components"), this);
  find_label->setObjectName(QStringLiteral("exploreSubhead"));
  find_label->setFont(theme::UiFont(13, QFont::DemiBold));

  auto *explore_inner = new QWidget(this);
  auto *explore_layout = new QVBoxLayout(explore_inner);
  explore_layout->setContentsMargins(0, 0, 8, 0);
  explore_layout->setSpacing(8);
  explore_layout->addWidget(MakeSectionHeading(
      QString(), Tr("Character information"), explore_inner));
  explore_layout->addWidget(info_block);
  explore_layout->addSpacing(10);
  explore_layout->addWidget(
      MakeSectionHeading(QString(), Tr("Explore characters"), explore_inner));
  explore_layout->addWidget(decompose_label);
  explore_layout->addLayout(break_row);
  explore_layout->addWidget(decomposition_combo_);
  explore_layout->addSpacing(6);
  explore_layout->addWidget(find_label);
  explore_layout->addLayout(find_row);
  explore_layout->addWidget(containing_list_, 1);
  explore_layout->addStretch(0);

  auto *explore_scroll = new QScrollArea(this);
  explore_scroll->setObjectName(QStringLiteral("exploreScroll"));
  explore_scroll->setWidgetResizable(true);
  explore_scroll->setFrameShape(QFrame::NoFrame);
  explore_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  explore_scroll->setWidget(explore_inner);

  explore_panel_ = new QWidget(this);
  explore_panel_->setObjectName(QStringLiteral("explorePanel"));
  auto *explore_outer = new QVBoxLayout(explore_panel_);
  explore_outer->setContentsMargins(14, 16, 16, 12);
  explore_outer->addWidget(explore_scroll);

  auto *columns = new QHBoxLayout;
  columns->setContentsMargins(0, 0, 0, 0);
  columns->setSpacing(0);
  columns->addWidget(construct_panel_, 28);
  auto *div1 = new QFrame(this);
  div1->setObjectName(QStringLiteral("columnRule"));
  div1->setFrameShape(QFrame::VLine);
  div1->setFixedWidth(1);
  columns->addWidget(div1);
  columns->addWidget(examine_panel_, 38);
  auto *div2 = new QFrame(this);
  div2->setObjectName(QStringLiteral("columnRule"));
  div2->setFrameShape(QFrame::VLine);
  div2->setFixedWidth(1);
  columns->addWidget(div2);
  columns->addWidget(explore_panel_, 34);

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
  connect(clear_button_, &QPushButton::clicked, editor_, &LayoutEditor::Clear);
  connect(close_button_, &QPushButton::clicked, this, &QDialog::reject);

  auto *footer = new QHBoxLayout;
  footer->setContentsMargins(16, 8, 16, 10);
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
  if (explore_panel_ != nullptr) {
    theme::ApplySurfacePalette(explore_panel_, colors);
  }
  if (editor_ != nullptr) {
    theme::ApplySurfacePalette(editor_, colors);
    editor_->update();
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

void CharacterComposerDialog::UpdateCharInfoPanel(
    const composer_ids::CharInfo *info, const QString &ids_text,
    const QString &unicode_text) {
  const QString dash = QStringLiteral("—");
  if (info != nullptr) {
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
    QStringList japanese;
    if (!on.isEmpty()) japanese << JoinDot(on);
    if (!kun.isEmpty()) japanese << JoinDot(kun);
    japanese_readings_value_->setText(japanese.isEmpty() ? dash
                                                         : japanese.join(QLatin1Char('\n')));

    other_readings_value_->setText(
        info->pinyin.empty() ? dash : FromUtf8(info->pinyin));
    meaning_value_->setText(
        info->definition.empty() ? dash : FromUtf8(info->definition));

    QStringList classification;
    if (!info->strokes.empty()) {
      classification << Tr("%1 strokes").arg(FromUtf8(info->strokes));
    }
    if (!info->rs.empty()) {
      classification << Tr("Radical %1").arg(FromUtf8(info->rs));
    }
    classification_value_->setText(
        classification.isEmpty() ? dash : JoinDot(classification));

    const QString variants = FormatVariants(*info);
    variants_value_->setText(variants.isEmpty() ? dash : variants);
  } else {
    japanese_readings_value_->setText(dash);
    other_readings_value_->setText(dash);
    meaning_value_->setText(dash);
    classification_value_->setText(dash);
    variants_value_->setText(dash);
  }
  structure_value_->setText(ids_text.isEmpty() ? dash : ids_text);
  unicode_value_->setText(unicode_text.isEmpty() ? dash : unicode_text);
}

void CharacterComposerDialog::OnLayoutChanged() {
  note_.clear();
  UpdateResult();
  UpdatePaletteHighlight();
  UpdateComponentRoles();
}

void CharacterComposerDialog::OnPaletteClicked(int index) {
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

void CharacterComposerDialog::UpdateResult() {
  SyncIdsEditFromEditor();
  found_.clear();
  pending_svg_.clear();

  QString status_heading;
  QString status_detail;
  bool insertable = false;
  bool incomplete = false;
  QString ids_text;
  QString unicode_text;

  if (table_ == nullptr) {
    status_detail =
        Tr("Character data was not found in %1, so the character cannot be "
           "looked up. You can still build the IDS above.")
            .arg(data_directory_);
  }

  if (const auto ids = composer_ids::ComposeIds(editor_->op(), editor_->parts())) {
    ids_text = FromUtf8(*ids);
    pending_svg_ = RenderComposerSvg(editor_->op(), editor_->parts(), 1000,
                                     kage_.get());
    insertable = !pending_svg_.empty();
    if (table_ != nullptr) {
      if (const auto found = table_->FindEncodedCharacter(*ids)) {
        found_ = *found;
      } else {
        status_heading = Tr("Not encoded in Unicode");
        status_detail =
            Tr("Insert places a drawing on the clipboard for Word "
               "(Kaeriten → Insert composed character) or other office plugins.");
      }
    } else if (insertable) {
      status_heading = Tr("Drawing only");
      status_detail =
          Tr("Insert places a drawing on the clipboard (lookup data missing).");
    }
  } else {
    incomplete = true;
    ids_text = FromUtf8(composer_ids::DraftIds(editor_->op(), editor_->parts()));
  }

  if (!note_.isEmpty()) {
    status_detail = note_;
  }

  const composer_ids::CharInfo *info = nullptr;
  if (!found_.empty()) {
    QFont char_font = theme::DisplayFont(108);
    char_label_->setFont(char_font);
    char_label_->setPixmap(QPixmap());
    const QString text = FromUtf8(found_);
    char_label_->setText(text);
    const QList<uint> cps = text.toUcs4();
    unicode_text =
        cps.isEmpty()
            ? QString()
            : QStringLiteral("U+%1")
                  .arg(cps.first(), 4, 16, QLatin1Char('0'))
                  .toUpper();
    status_heading = unicode_text;
    if (status_detail.isEmpty()) {
      status_detail = Tr("Encoded character");
    }
    if (char_info_ != nullptr) {
      info = char_info_->Find(found_);
    }
  } else if (insertable) {
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
  } else if (incomplete) {
    QFont prompt = theme::UiFont(13);
    char_label_->setFont(prompt);
    char_label_->setPixmap(QPixmap());
    char_label_->setText(Tr("Fill in every box to look up the character."));
    status_heading = Tr("Incomplete composition");
    if (status_detail.isEmpty()) {
      status_detail = Tr("Enter a character in each component box.");
    }
  } else {
    char_label_->clear();
  }

  // CharInfo only for a genuine encoded match — never for incomplete /
  // unencoded compositions that merely resemble another character.
  UpdateCharInfoPanel(info, ids_text, unicode_text);

  status_heading_->setText(status_heading);
  status_detail_->setText(status_detail);
  insert_button_->setEnabled(insertable);
  copy_button_->setEnabled(!found_.empty() || insertable);
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
  if (!found_.empty()) {
    QApplication::clipboard()->setText(FromUtf8(found_));
    note_ = Tr("Copied to the clipboard.");
    UpdateResult();
    return;
  }
  if (pending_svg_.empty()) return;
  auto *mime = new QMimeData;
  mime->setData(QStringLiteral("image/svg+xml"),
                QByteArray::fromStdString(pending_svg_));
  mime->setText(QString::fromStdString(pending_svg_));
  QApplication::clipboard()->setMimeData(mime);
  note_ = Tr("Drawing copied to the clipboard.");
  UpdateResult();
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
    UpdateResult();
    return;
  }
  auto parsed = composer_ids::ParseIdsToComposerTree(options.front());
  editor_->SetLayout(parsed->op, std::move(parsed->parts));
  if (parsed->unencoded_count > 0) {
    note_ = Tr("Some parts have no encoded character; fill those boxes in.");
    UpdateResult();
  }
  if (options.size() > 1) {
    for (const std::string &ids : options) {
      decomposition_combo_->addItem(FromUtf8(ids));
    }
    decomposition_combo_->setVisible(true);
  }
}

void CharacterComposerDialog::OnFindContaining() {
  containing_list_->clear();
  containing_list_->setVisible(false);
  if (table_ == nullptr) return;
  const std::vector<std::string> matches =
      table_->FindContaining(ToUtf8(containing_edit_->text()));
  if (matches.empty()) {
    note_ = Tr("No character contains all of these components.");
    UpdateResult();
    return;
  }
  constexpr size_t kMaxShown = 500;
  for (size_t i = 0; i < matches.size() && i < kMaxShown; ++i) {
    auto *item = new QListWidgetItem(FromUtf8(matches[i]), containing_list_);
    item->setTextAlignment(Qt::AlignCenter);
  }
  containing_list_->setVisible(true);
  note_ = matches.size() > kMaxShown
              ? Tr("%1 characters found; showing the first %2. Add more "
                   "components to narrow the search.")
                    .arg(matches.size())
                    .arg(kMaxShown)
              : Tr("%1 characters found. Click one to load its parts.")
                    .arg(matches.size());
  UpdateResult();
}

void CharacterComposerDialog::OnContainingPicked(QListWidgetItem *item) {
  if (item == nullptr || table_ == nullptr) return;
  const std::string ch = ToUtf8(item->text());
  for (const std::string &ids : table_->FindDecompositions(ch)) {
    auto parsed = composer_ids::ParseIdsToComposerTree(ids);
    if (!parsed) continue;
    editor_->SetLayout(parsed->op, std::move(parsed->parts));
    if (parsed->unencoded_count > 0) {
      note_ = Tr("Some parts have no encoded character; fill those boxes in.");
      UpdateResult();
    }
    return;
  }
}

}  // namespace gui
}  // namespace mozc
