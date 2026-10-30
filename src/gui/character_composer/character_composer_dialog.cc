// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "gui/character_composer/character_composer_dialog.h"

#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QFocusEvent>
#include <QFont>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QGuiApplication>
#include <QIcon>
#include <QInputMethod>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QFontMetricsF>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QToolButton>
#include <QVBoxLayout>
#include <QFrame>
#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "composer_ids/ids_operator.h"
#include "composer_ids/ids_table.h"
#include "composer_ids/ids_tree.h"
#include "gui/character_composer/composer_kage.h"
#include "gui/character_composer/composer_svg.h"
#include "gui/character_composer/layout_geometry.h"

#include <QMimeData>

namespace mozc {
namespace gui {
namespace {

using composer_ids::ComposerSlot;
using composer_ids::SlotPath;

constexpr char kContext[] = "mozc::gui::CharacterComposerDialog";

const QColor kSelectionColor(240, 140, 0);

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

// Draws the operator's shape: dashed separators and frames, as in the square.
void DrawGeometry(QPainter *p, const OperatorGeometry &g, const QRectF &box) {
  for (const FLine &line : g.dividers) {
    p->drawLine(QPointF(box.x() + line.x1 * box.width(), box.y() + line.y1 * box.height()),
                QPointF(box.x() + line.x2 * box.width(), box.y() + line.y2 * box.height()));
  }
  for (const FRect &frame : g.frames) {
    p->drawRect(Scaled(frame, box));
  }
}

// The drawing Insert would produce, painted from the same glyph layout as the
// SVG (the SVG itself needs Qt's SVG module, which is not built here).
QPixmap DrawingPreview(const std::vector<PlacedGlyph> &glyphs, const QPalette &palette,
                       qreal device_pixel_ratio) {
  constexpr int kSize = 96;
  QPixmap pixmap(static_cast<int>(kSize * device_pixel_ratio),
                 static_cast<int>(kSize * device_pixel_ratio));
  pixmap.setDevicePixelRatio(device_pixel_ratio);
  pixmap.fill(Qt::transparent);
  QPainter p(&pixmap);
  p.setRenderHint(QPainter::Antialiasing);
  p.setRenderHint(QPainter::TextAntialiasing);
  p.setPen(palette.color(QPalette::Text));
  const double scale = kSize / 1000.0;  // glyphs are laid out in a 1000-unit square
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

// The same, for a glyph drawn from KAGE strokes: filled outlines, no fonts.
QPixmap OutlinePreview(const std::vector<Contour> &outlines, const QPalette &palette,
                       qreal device_pixel_ratio) {
  constexpr int kSize = 96;
  QPixmap pixmap(static_cast<int>(kSize * device_pixel_ratio),
                 static_cast<int>(kSize * device_pixel_ratio));
  pixmap.setDevicePixelRatio(device_pixel_ratio);
  pixmap.fill(Qt::transparent);
  QPainterPath path;
  path.setFillRule(Qt::WindingFill);
  const double scale = kSize / 1000.0;  // outlines are in a 1000-unit square
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

QIcon OperatorIcon(const std::string &op, const QPalette &palette) {
  constexpr int kSize = 32;
  QPixmap pixmap(kSize * 2, kSize * 2);
  pixmap.setDevicePixelRatio(2.0);
  pixmap.fill(Qt::transparent);
  QPainter p(&pixmap);
  p.setRenderHint(QPainter::Antialiasing);
  const QRectF box(3, 3, kSize - 6, kSize - 6);
  p.setPen(QPen(palette.color(QPalette::ButtonText), 1.5));
  p.setBrush(Qt::NoBrush);
  p.drawRect(box);
  p.setPen(QPen(palette.color(QPalette::ButtonText), 1.2, Qt::DashLine));
  DrawGeometry(&p, GeometryFor(op), box);
  p.end();
  return QIcon(pixmap);
}

}  // namespace

// --- SlotEdit ---------------------------------------------------------------

SlotEdit::SlotEdit(SlotPath path, QWidget *parent)
    : QLineEdit(parent), path_(std::move(path)) {
  setAlignment(Qt::AlignCenter);
  setPlaceholderText(QStringLiteral("字"));
  QFont f = font();
  f.setPointSize(std::max(f.pointSize(), 10) + 6);
  setFont(f);
}

void SlotEdit::SetSelected(bool selected) {
  setStyleSheet(selected ? QStringLiteral("QLineEdit { border: 2px solid #f08c00; }")
                         : QString());
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
        button->setStyleSheet(
            selected ? QStringLiteral(
                           "QToolButton { border: 1px solid #f08c00; "
                           "border-radius: 3px; padding: 1px 4px; "
                           "font-weight: bold; }")
                     : QStringLiteral(
                           "QToolButton { border: 1px solid transparent; "
                           "border-radius: 3px; padding: 1px 4px; "
                           "color: palette(mid); }"));
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
  // Tiny enclosure strips have no room; the giant ⿰ / ⿱ boxes do.
  forms_row_->setVisible(has_forms_ && height() >= 72);
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
  p.setPen(QPen(palette().color(QPalette::Mid), 1, Qt::SolidLine));
  p.drawRect(box);
  p.setPen(QPen(palette().color(QPalette::Mid), 1, Qt::DashLine));
  DrawGeometry(&p, GeometryFor(op_), box);
  if (selected_) {
    p.setPen(QPen(kSelectionColor, 2));
    p.drawRect(box.adjusted(1, 1, -1, -1));
  }
}

void LayoutView::mousePressEvent(QMouseEvent *event) {
  // Only the padding and gaps reach here; fields take their own clicks.
  editor_->Select(path_);
  event->accept();
}

// --- LayoutEditor -----------------------------------------------------------

LayoutEditor::LayoutEditor(QWidget *parent) : QWidget(parent) {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  setFixedSize(340, 340);
  parts_ = composer_ids::ResizePartsForOperator(op_, {});
  Rebuild();
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
    QWidget *parent)
    : QDialog(parent), table_(std::move(table)), data_directory_(data_directory) {
  // GlyphWiki's strokes, to draw a structure with no Unicode character as one
  // glyph. Missing is fine: the drawing falls back to the components as text.
  if (auto resources = KageResources::LoadFromDirectory(data_directory_.toStdString())) {
    kage_ = std::make_unique<KageResources>(*std::move(resources));
  }
  BuildUi(data_directory);
  OnLayoutChanged();
}

void CharacterComposerDialog::BuildUi(const QString &data_directory) {
  setWindowTitle(Tr("Character composer"));
  // A separate process cannot be made a child of the document window, so
  // without this the document hides the dialog as soon as it is clicked.
  setWindowFlags(Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint |
                 Qt::WindowTitleHint | Qt::WindowStaysOnTopHint);

  // Palette of the twelve layouts.
  palette_ = new QButtonGroup(this);
  palette_->setExclusive(false);
  auto *palette_grid = new QGridLayout;
  palette_grid->setSpacing(4);
  int index = 0;
  for (const auto &info : composer_ids::IdsOperators()) {
    auto *button = new QToolButton(this);
    button->setCheckable(true);
    button->setIcon(OperatorIcon(std::string(info.op), this->palette()));
    button->setIconSize(QSize(32, 32));
    button->setFixedSize(46, 46);
    button->setToolTip(QStringLiteral("%1  %2  %3")
                           .arg(FromUtf8(std::string(info.op)),
                                FromUtf8(std::string(info.abbr)),
                                Tr(std::string(info.name).c_str())));
    button->setFocusPolicy(Qt::NoFocus);  // keep the focus in the field
    palette_->addButton(button, index);
    palette_grid->addWidget(button, index / 6, index % 6);
    ++index;
  }
  connect(palette_, &QButtonGroup::idClicked, this,
          &CharacterComposerDialog::OnPaletteClicked);

  editor_ = new LayoutEditor(this);
  editor_->set_table(table_.get());
  connect(editor_, &LayoutEditor::changed, this, &CharacterComposerDialog::OnLayoutChanged);
  connect(editor_, &LayoutEditor::selectionChanged, this,
          &CharacterComposerDialog::UpdatePaletteHighlight);
  connect(editor_, &LayoutEditor::flattenRefused, this, [this]() {
    note_ = Tr("Clear all but one of the filled boxes before removing this layout.");
    UpdateResult();
  });
  connect(editor_, &LayoutEditor::submitRequested, this,
          &CharacterComposerDialog::OnSubmitRequested);

  auto *left = new QVBoxLayout;
  left->addLayout(palette_grid);
  left->addWidget(editor_, 0, Qt::AlignHCenter);

  // IDS entry: shows the draft sequence and accepts typing (LR言某 → ⿰言某).
  ids_edit_ = new QLineEdit(this);
  ids_edit_->setObjectName(QStringLiteral("ids_edit"));
  ids_edit_->setPlaceholderText(Tr("e.g. LR日月 or ⿰日月"));
  QFont ids_font = ids_edit_->font();
  ids_font.setPointSize(ids_font.pointSize() + 4);
  ids_edit_->setFont(ids_font);
  ids_info_button_ = new QToolButton(this);
  ids_info_button_->setText(QStringLiteral("ℹ"));
  ids_info_button_->setCheckable(true);
  ids_info_button_->setAutoRaise(true);
  ids_info_button_->setFocusPolicy(Qt::NoFocus);
  ids_info_button_->setToolTip(Tr("Layout codes for the IDS box"));
  auto *ids_row = new QHBoxLayout;
  ids_row->addWidget(ids_edit_, 1);
  ids_row->addWidget(ids_info_button_, 0, Qt::AlignTop);
  left->addLayout(ids_row);

  ids_info_panel_ = new QLabel(this);
  ids_info_panel_->setObjectName(QStringLiteral("ids_info_panel"));
  ids_info_panel_->setWordWrap(true);
  ids_info_panel_->setTextFormat(Qt::RichText);
  ids_info_panel_->setFrameShape(QFrame::StyledPanel);
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
  left->addWidget(ids_info_panel_);
  connect(ids_edit_, &QLineEdit::textEdited, this,
          &CharacterComposerDialog::OnIdsEdited);
  // After leaving a half-typed IDS, put the square's draft back in the field.
  connect(ids_edit_, &QLineEdit::editingFinished, this,
          [this]() { SyncIdsEditFromEditor(/*force=*/true); });
  connect(ids_info_button_, &QToolButton::toggled, this,
          &CharacterComposerDialog::OnIdsInfoToggled);
  left->addStretch(1);

  // Result column.
  char_label_ = new QLabel(this);
  char_label_->setAlignment(Qt::AlignCenter);
  char_label_->setMinimumSize(120, 100);
  char_label_->setFrameShape(QFrame::StyledPanel);
  char_label_->setWordWrap(true);
  char_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);

  codepoint_label_ = new QLabel(this);
  codepoint_label_->setAlignment(Qt::AlignCenter);

  status_label_ = new QLabel(this);
  status_label_->setWordWrap(true);
  status_label_->setMinimumHeight(40);

  auto *break_group = new QGroupBox(Tr("Break apart a character"), this);
  break_apart_edit_ = new QLineEdit(break_group);
  break_apart_edit_->setObjectName(QStringLiteral("break_apart_edit"));
  break_apart_edit_->setPlaceholderText(Tr("Type a character"));
  auto *break_button = new QPushButton(Tr("Break apart"), break_group);
  break_button->setAutoDefault(false);
  break_button->setEnabled(table_ != nullptr);
  decomposition_combo_ = new QComboBox(break_group);
  decomposition_combo_->setVisible(false);
  auto *break_layout = new QGridLayout(break_group);
  break_layout->addWidget(break_apart_edit_, 0, 0);
  break_layout->addWidget(break_button, 0, 1);
  break_layout->addWidget(decomposition_combo_, 1, 0, 1, 2);
  connect(break_button, &QPushButton::clicked, this, &CharacterComposerDialog::OnBreakApart);
  connect(break_apart_edit_, &QLineEdit::returnPressed, this,
          &CharacterComposerDialog::OnBreakApart);
  connect(decomposition_combo_, QOverload<int>::of(&QComboBox::activated), this,
          [this](int i) {
            const std::string ids = ToUtf8(decomposition_combo_->itemText(i));
            if (auto parsed = composer_ids::ParseIdsToComposerTree(ids)) {
              editor_->SetLayout(parsed->op, std::move(parsed->parts));
            }
          });

  auto *containing_group = new QGroupBox(Tr("Find characters containing…"), this);
  containing_edit_ = new QLineEdit(containing_group);
  containing_edit_->setObjectName(QStringLiteral("containing_edit"));
  containing_edit_->setPlaceholderText(Tr("Type one or more components"));
  auto *containing_button = new QPushButton(Tr("Find"), containing_group);
  containing_button->setAutoDefault(false);
  containing_button->setEnabled(table_ != nullptr);
  containing_list_ = new QListWidget(containing_group);
  containing_list_->setViewMode(QListView::IconMode);
  containing_list_->setResizeMode(QListView::Adjust);
  containing_list_->setMovement(QListView::Static);
  containing_list_->setUniformItemSizes(true);
  containing_list_->setGridSize(QSize(44, 44));
  containing_list_->setFixedHeight(140);
  containing_list_->setVisible(false);
  QFont containing_font = containing_list_->font();
  containing_font.setPointSize(containing_font.pointSize() + 8);
  containing_list_->setFont(containing_font);
  auto *containing_layout = new QGridLayout(containing_group);
  containing_layout->addWidget(containing_edit_, 0, 0);
  containing_layout->addWidget(containing_button, 0, 1);
  containing_layout->addWidget(containing_list_, 1, 0, 1, 2);
  connect(containing_button, &QPushButton::clicked, this,
          &CharacterComposerDialog::OnFindContaining);
  connect(containing_edit_, &QLineEdit::returnPressed, this,
          &CharacterComposerDialog::OnFindContaining);
  connect(containing_list_, &QListWidget::itemClicked, this,
          &CharacterComposerDialog::OnContainingPicked);
  connect(containing_list_, &QListWidget::itemActivated, this,
          &CharacterComposerDialog::OnContainingPicked);

  insert_button_ = new QPushButton(Tr("Insert"), this);
  insert_button_->setDefault(true);
  copy_button_ = new QPushButton(Tr("Copy"), this);
  copy_button_->setAutoDefault(false);
  auto *clear_button = new QPushButton(Tr("Clear"), this);
  clear_button->setAutoDefault(false);
  auto *close_button = new QPushButton(Tr("Close"), this);
  close_button->setAutoDefault(false);
  connect(insert_button_, &QPushButton::clicked, this, &CharacterComposerDialog::OnInsert);
  connect(copy_button_, &QPushButton::clicked, this, &CharacterComposerDialog::OnCopy);
  connect(clear_button, &QPushButton::clicked, editor_, &LayoutEditor::Clear);
  connect(close_button, &QPushButton::clicked, this, &QDialog::reject);

  auto *buttons = new QHBoxLayout;
  buttons->addWidget(clear_button);
  buttons->addStretch(1);
  buttons->addWidget(copy_button_);
  buttons->addWidget(insert_button_);
  buttons->addWidget(close_button);

  auto *right = new QVBoxLayout;
  right->addWidget(char_label_, 0, Qt::AlignHCenter);
  right->addWidget(codepoint_label_);
  right->addWidget(status_label_);
  right->addStretch(1);
  right->addWidget(break_group);
  right->addWidget(containing_group);
  right->addLayout(buttons);

  auto *root = new QHBoxLayout(this);
  root->addLayout(left);
  root->addSpacing(12);
  root->addLayout(right, 1);

  UpdatePaletteHighlight();
  (void)data_directory;
}

void CharacterComposerDialog::OnLayoutChanged() {
  note_.clear();
  UpdateResult();
  UpdatePaletteHighlight();
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
  // Leave the field alone while the user is typing into it.
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
    // Keep the same distance from the end so a code typed at the caret
    // becomes the operator under the caret (LR| → ⿰|).
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

  QString status;
  bool insertable = false;
  bool incomplete = false;
  if (table_ == nullptr) {
    status = Tr("Character data was not found in %1, so the character cannot be "
                "looked up. You can still build the IDS above.")
                 .arg(data_directory_);
  }
  if (const auto ids = composer_ids::ComposeIds(editor_->op(), editor_->parts())) {
    pending_svg_ = RenderComposerSvg(editor_->op(), editor_->parts(), 1000,
                                     kage_.get());
    insertable = !pending_svg_.empty();
    if (table_ != nullptr) {
      if (const auto found = table_->FindEncodedCharacter(*ids)) {
        found_ = *found;
      } else {
        status = Tr("No encoded character has this structure. Insert places a "
                    "drawing on the clipboard for Word (Kaeriten → Insert "
                    "composed character) or other office plugins.");
      }
    } else if (insertable) {
      status = Tr("Insert places a drawing on the clipboard (lookup data missing).");
    }
  } else {
    incomplete = true;
    // The fill-in prompt is shown in the preview square, not under it.
    status.clear();
  }
  if (!note_.isEmpty()) status = note_;

  if (!found_.empty()) {
    QFont char_font = font();
    char_font.setPointSize(48);
    char_label_->setFont(char_font);
    const QString text = FromUtf8(found_);
    char_label_->setText(text);
    const QList<uint> cps = text.toUcs4();
    codepoint_label_->setText(
        cps.isEmpty() ? QString()
                      : QStringLiteral("U+%1").arg(cps.first(), 4, 16, QLatin1Char('0')).toUpper());
  } else if (insertable) {
    char_label_->setFont(font());
    std::optional<std::vector<Contour>> outlines;
    if (kage_ != nullptr) {
      outlines = ComposerOutlines(editor_->op(), editor_->parts(), *kage_);
    }
    char_label_->setPixmap(
        outlines ? OutlinePreview(*outlines, palette(), devicePixelRatioF())
                 : DrawingPreview(ComposerGlyphs(editor_->op(), editor_->parts()),
                                  palette(), devicePixelRatioF()));
    codepoint_label_->clear();
  } else if (incomplete) {
    // Prompt sits where the character / drawing normally appears.
    QFont prompt = font();
    prompt.setPointSize(std::max(11, font().pointSize()));
    char_label_->setFont(prompt);
    char_label_->setText(Tr("Fill in every box to look up the character."));
    codepoint_label_->clear();
  } else {
    char_label_->clear();
    codepoint_label_->clear();
  }
  status_label_->setText(status);
  insert_button_->setEnabled(insertable);
  copy_button_->setEnabled(!found_.empty() || insertable);
}

void CharacterComposerDialog::OnSubmitRequested() { TryInsert(); }

void CharacterComposerDialog::TryInsert() {
  // Text still being composed in an IME is not in the box yet, so the layout
  // (and the Insert button) would be one character behind.
  if (QInputMethod *input_method = QGuiApplication::inputMethod()) {
    input_method->commit();
  }
  if (insert_button_ != nullptr && insert_button_->isEnabled()) {
    OnInsert();
  }
}

void CharacterComposerDialog::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    // Enter in the search fields runs the search, not Insert.
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
  // Office.js cannot read local files; the Word add-in reads this clipboard.
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
  // The list is only a pick-list; a component like 一 matches thousands.
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
