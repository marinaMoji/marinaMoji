// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "gui/character_composer/layout_adjust_dialog.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <utility>

namespace mozc {
namespace gui {
namespace {

using composer_ids::ComposerSlot;

QString Tr(const char *source) {
  return QCoreApplication::translate("mozc::gui::LayoutAdjustDialog", source);
}

constexpr double kMargin = 14;      // pixels around the square
constexpr double kMinSize = 12;     // smallest box, root units
constexpr double kSnapRadius = 4;   // root units
constexpr double kLimitLow = -20, kLimitHigh = 220;  // how far a box may stray
constexpr double kHandle = 5;       // handle half-size, pixels

QRectF Normalised(QRectF r) {
  if (r.width() < kMinSize) r.setWidth(kMinSize);
  if (r.height() < kMinSize) r.setHeight(kMinSize);
  return r;
}

}  // namespace

AdjustCanvas::AdjustCanvas(std::string op, std::vector<ComposerSlot> parts,
                           const KageResources *kage, LayoutAdjustments initial,
                           QWidget *parent)
    : QWidget(parent),
      op_(std::move(op)),
      parts_(std::move(parts)),
      kage_(kage),
      adjust_(std::move(initial)) {
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
  setAccessibleName(Tr("Layout drawing"));
  setAccessibleDescription(
      Tr("Drag a component to move it, drag its handles to resize it, or use "
         "the arrow keys. Shift makes arrow steps larger."));
  Recompose();
  if (!boxes_.empty()) selected_ = 0;
}

void AdjustCanvas::Recompose() {
  boxes_.clear();
  outlines_.reset();
  if (kage_ == nullptr) return;
  boxes_ = ComposerBoxes(op_, parts_, *kage_, &adjust_);
  outlines_ = ComposerOutlines(op_, parts_, *kage_, 1000, &adjust_);
  update();
}

QRectF AdjustCanvas::Square() const {
  const double side = std::min(width(), height()) - 2 * kMargin;
  return QRectF((width() - side) / 2, (height() - side) / 2, side, side);
}

QRectF AdjustCanvas::RootToPixels(const QRectF &r) const {
  const QRectF sq = Square();
  const double k = sq.width() / 200.0;
  return QRectF(sq.left() + r.left() * k, sq.top() + r.top() * k,
                r.width() * k, r.height() * k);
}

QPointF AdjustCanvas::PixelsToRoot(const QPointF &p) const {
  const QRectF sq = Square();
  const double k = 200.0 / sq.width();
  return QPointF((p.x() - sq.left()) * k, (p.y() - sq.top()) * k);
}

QRectF AdjustCanvas::RootRect(const PlacedBox &box, bool automatic) const {
  const BoxRect &b = automatic ? box.auto_box : box.box;
  const double sx = (box.parent.right - box.parent.left) / 200.0;
  const double sy = (box.parent.bottom - box.parent.top) / 200.0;
  return QRectF(QPointF(box.parent.left + b.left * sx, box.parent.top + b.top * sy),
                QPointF(box.parent.left + b.right * sx,
                        box.parent.top + b.bottom * sy));
}

QRectF AdjustCanvas::SelectedRect() const {
  if (selected_ < 0 || selected_ >= static_cast<int>(boxes_.size())) return {};
  return RootRect(boxes_[selected_]);
}

bool AdjustCanvas::IsAdjusted(int index) const {
  return index >= 0 && index < static_cast<int>(boxes_.size()) &&
         adjust_.count(boxes_[index].path) > 0;
}

void AdjustCanvas::SetRootRect(int index, QRectF rect) {
  if (index < 0 || index >= static_cast<int>(boxes_.size())) return;
  const PlacedBox &box = boxes_[index];
  rect = Normalised(rect.normalized());
  // Back into the parent's own 0..200 space.
  const double sx = (box.parent.right - box.parent.left) / 200.0;
  const double sy = (box.parent.bottom - box.parent.top) / 200.0;
  if (sx <= 0 || sy <= 0) return;
  BoxRect local;
  local.left = std::round((rect.left() - box.parent.left) / sx);
  local.top = std::round((rect.top() - box.parent.top) / sy);
  local.right = std::round((rect.right() - box.parent.left) / sx);
  local.bottom = std::round((rect.bottom() - box.parent.top) / sy);
  const composer_ids::SlotPath path = box.path;
  const BoxRect automatic = box.auto_box;
  if (local.left == std::round(automatic.left) &&
      local.top == std::round(automatic.top) &&
      local.right == std::round(automatic.right) &&
      local.bottom == std::round(automatic.bottom)) {
    adjust_.erase(path);
  } else {
    adjust_[path] = local;
  }
  Recompose();
  selected_ = std::min(index, static_cast<int>(boxes_.size()) - 1);
  emit geometryChanged();
}

void AdjustCanvas::SetSelectedRect(const QRectF &rect) {
  SetRootRect(selected_, rect);
}

void AdjustCanvas::ResetSelected() {
  if (selected_ < 0 || selected_ >= static_cast<int>(boxes_.size())) return;
  const int keep = selected_;
  adjust_.erase(boxes_[selected_].path);
  Recompose();
  selected_ = keep;
  emit geometryChanged();
}

void AdjustCanvas::ResetAll() {
  const int keep = selected_;
  adjust_.clear();
  Recompose();
  selected_ = keep;
  emit geometryChanged();
}

void AdjustCanvas::Select(int index) {
  if (index == selected_ || index < -1 ||
      index >= static_cast<int>(boxes_.size())) {
    return;
  }
  selected_ = index;
  update();
  emit selectionChanged(index);
}

int AdjustCanvas::HandleAt(const QPointF &pixel) const {
  if (selected_ < 0) return 0;
  const QRectF r = RootToPixels(SelectedRect());
  const double slack = kHandle + 2;
  const bool near_left = std::abs(pixel.x() - r.left()) <= slack;
  const bool near_right = std::abs(pixel.x() - r.right()) <= slack;
  const bool near_top = std::abs(pixel.y() - r.top()) <= slack;
  const bool near_bottom = std::abs(pixel.y() - r.bottom()) <= slack;
  const bool in_x = pixel.x() >= r.left() - slack && pixel.x() <= r.right() + slack;
  const bool in_y = pixel.y() >= r.top() - slack && pixel.y() <= r.bottom() + slack;
  int mask = 0;
  if (near_left && in_y) mask |= kLeft;
  if (near_right && in_y) mask |= kRight;
  if (near_top && in_x) mask |= kTop;
  if (near_bottom && in_x) mask |= kBottom;
  return mask;
}

int AdjustCanvas::BoxAt(const QPointF &pixel) const {
  // The smallest box under the pointer wins, so a part inside another is
  // still reachable.
  int best = -1;
  double best_area = 1e18;
  for (int i = 0; i < static_cast<int>(boxes_.size()); ++i) {
    const QRectF r = RootToPixels(RootRect(boxes_[i]));
    if (!r.contains(pixel)) continue;
    const double area = r.width() * r.height();
    if (area < best_area) {
      best = i;
      best_area = area;
    }
  }
  return best;
}

QRectF AdjustCanvas::Snapped(QRectF rect, int mask) {
  guides_x_.clear();
  guides_y_.clear();
  if (!snapping_) return rect;
  // What a box edge can stick to: the square's edges and middle, the other
  // components' edges and middles, and where the composer would put this one.
  std::vector<double> xs = {0, 100, 200}, ys = {0, 100, 200};
  for (int i = 0; i < static_cast<int>(boxes_.size()); ++i) {
    const QRectF other = RootRect(boxes_[i], /*automatic=*/i == selected_);
    xs.insert(xs.end(), {other.left(), other.center().x(), other.right()});
    ys.insert(ys.end(), {other.top(), other.center().y(), other.bottom()});
  }
  // The offset that brings the nearest of |edges| onto a candidate, or 0.
  const auto pull = [](const std::vector<double> &edges,
                       const std::vector<double> &candidates, double *hit) {
    double best = kSnapRadius + 1e-9, delta = 0;
    for (const double e : edges) {
      for (const double c : candidates) {
        if (std::abs(c - e) < std::abs(best) && std::abs(c - e) > 1e-9) {
          best = c - e;
          delta = c - e;
          *hit = c;
        }
      }
    }
    return delta;
  };
  double hit = 0;
  if (mask == kMove) {
    const double dx = pull({rect.left(), rect.center().x(), rect.right()}, xs, &hit);
    if (dx != 0) guides_x_.push_back(hit);
    rect.translate(dx, 0);
    const double dy = pull({rect.top(), rect.center().y(), rect.bottom()}, ys, &hit);
    if (dy != 0) guides_y_.push_back(hit);
    rect.translate(0, dy);
    return rect;
  }
  if (mask & kLeft) {
    const double d = pull({rect.left()}, xs, &hit);
    if (d != 0) { rect.setLeft(rect.left() + d); guides_x_.push_back(hit); }
  }
  if (mask & kRight) {
    const double d = pull({rect.right()}, xs, &hit);
    if (d != 0) { rect.setRight(rect.right() + d); guides_x_.push_back(hit); }
  }
  if (mask & kTop) {
    const double d = pull({rect.top()}, ys, &hit);
    if (d != 0) { rect.setTop(rect.top() + d); guides_y_.push_back(hit); }
  }
  if (mask & kBottom) {
    const double d = pull({rect.bottom()}, ys, &hit);
    if (d != 0) { rect.setBottom(rect.bottom() + d); guides_y_.push_back(hit); }
  }
  return rect;
}

void AdjustCanvas::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton) return;
  setFocus();
  const QPointF pos = event->position();
  int mask = HandleAt(pos);
  if (mask == 0) {
    const int hit = BoxAt(pos);
    if (hit < 0) return;
    Select(hit);
    mask = kMove;
  }
  drag_mask_ = mask;
  drag_start_ = PixelsToRoot(pos);
  drag_origin_ = SelectedRect();
}

void AdjustCanvas::mouseMoveEvent(QMouseEvent *event) {
  const QPointF pos = event->position();
  if (drag_mask_ == 0) {
    const int mask = HandleAt(pos);
    const bool horizontal = (mask & (kLeft | kRight)) != 0;
    const bool vertical = (mask & (kTop | kBottom)) != 0;
    if (horizontal && vertical) {
      const bool diagonal_down =
          (mask == (kLeft | kTop)) || (mask == (kRight | kBottom));
      setCursor(diagonal_down ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor);
    } else if (horizontal) {
      setCursor(Qt::SizeHorCursor);
    } else if (vertical) {
      setCursor(Qt::SizeVerCursor);
    } else {
      setCursor(BoxAt(pos) >= 0 ? Qt::OpenHandCursor : Qt::ArrowCursor);
    }
    return;
  }
  const QPointF delta = PixelsToRoot(pos) - drag_start_;
  QRectF rect = drag_origin_;
  if (drag_mask_ == kMove) {
    rect.translate(delta);
  } else {
    if (drag_mask_ & kLeft) rect.setLeft(std::min(rect.left() + delta.x(), rect.right() - kMinSize));
    if (drag_mask_ & kRight) rect.setRight(std::max(rect.right() + delta.x(), rect.left() + kMinSize));
    if (drag_mask_ & kTop) rect.setTop(std::min(rect.top() + delta.y(), rect.bottom() - kMinSize));
    if (drag_mask_ & kBottom) rect.setBottom(std::max(rect.bottom() + delta.y(), rect.top() + kMinSize));
  }
  rect = Snapped(rect, drag_mask_);
  const bool corner =
      (drag_mask_ & (kLeft | kRight)) && (drag_mask_ & (kTop | kBottom));
  if ((event->modifiers() & Qt::ShiftModifier) && corner &&
      drag_origin_.width() > 0 && drag_origin_.height() > 0) {
    // Keep the shape: the box follows whichever side was pulled further.
    const double k = std::max(rect.width() / drag_origin_.width(),
                              rect.height() / drag_origin_.height());
    const double w = drag_origin_.width() * k, h = drag_origin_.height() * k;
    if (drag_mask_ & kLeft) rect.setLeft(drag_origin_.right() - w);
    else rect.setRight(drag_origin_.left() + w);
    if (drag_mask_ & kTop) rect.setTop(drag_origin_.bottom() - h);
    else rect.setBottom(drag_origin_.top() + h);
  }
  // Stay near the square.
  if (rect.left() < kLimitLow) rect.moveLeft(kLimitLow);
  if (rect.top() < kLimitLow) rect.moveTop(kLimitLow);
  if (rect.right() > kLimitHigh) rect.moveRight(kLimitHigh);
  if (rect.bottom() > kLimitHigh) rect.moveBottom(kLimitHigh);
  SetRootRect(selected_, rect);
}

void AdjustCanvas::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton) return;
  drag_mask_ = 0;
  guides_x_.clear();
  guides_y_.clear();
  update();
}

void AdjustCanvas::keyPressEvent(QKeyEvent *event) {
  const double step = (event->modifiers() & Qt::ShiftModifier) ? 10 : 1;
  QRectF r = SelectedRect();
  switch (event->key()) {
    case Qt::Key_Left: r.translate(-step, 0); break;
    case Qt::Key_Right: r.translate(step, 0); break;
    case Qt::Key_Up: r.translate(0, -step); break;
    case Qt::Key_Down: r.translate(0, step); break;
    case Qt::Key_Plus:
    case Qt::Key_Equal:
      r.adjust(-step / 2, -step / 2, step / 2, step / 2);
      break;
    case Qt::Key_Minus:
      r.adjust(step / 2, step / 2, -step / 2, -step / 2);
      break;
    case Qt::Key_Tab:
    case Qt::Key_Backtab: {
      if (boxes_.empty()) return;
      const int n = static_cast<int>(boxes_.size());
      const int dir = event->key() == Qt::Key_Backtab ? n - 1 : 1;
      Select((std::max(selected_, 0) + dir) % n);
      return;
    }
    case Qt::Key_Delete:
    case Qt::Key_Backspace:
      ResetSelected();
      return;
    default:
      QWidget::keyPressEvent(event);
      return;
  }
  if (selected_ >= 0) SetSelectedRect(r);
}

void AdjustCanvas::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  const QPalette &pal = palette();
  const QColor ink = pal.color(QPalette::WindowText);
  const QColor accent = pal.color(QPalette::Highlight);
  const QColor faint(ink.red(), ink.green(), ink.blue(), 60);
  const QRectF sq = Square();

  // The square, with the middle lines.
  p.fillRect(sq, pal.color(QPalette::Base));
  p.setPen(QPen(faint, 1, Qt::DashLine));
  p.drawLine(QPointF(sq.center().x(), sq.top()), QPointF(sq.center().x(), sq.bottom()));
  p.drawLine(QPointF(sq.left(), sq.center().y()), QPointF(sq.right(), sq.center().y()));
  p.setPen(QPen(faint, 1));
  p.drawRect(sq);

  // The glyph.
  if (outlines_) {
    QPainterPath path;
    path.setFillRule(Qt::WindingFill);
    const double k = sq.width() / 1000.0;
    for (const Contour &contour : *outlines_) {
      if (contour.empty()) continue;
      path.moveTo(sq.left() + contour[0].first * k, sq.top() + contour[0].second * k);
      for (size_t i = 1; i < contour.size(); ++i) {
        path.lineTo(sq.left() + contour[i].first * k, sq.top() + contour[i].second * k);
      }
      path.closeSubpath();
    }
    p.setPen(Qt::NoPen);
    p.setBrush(ink);
    p.drawPath(path);
  } else {
    p.setPen(ink);
    p.drawText(sq, Qt::AlignCenter | Qt::TextWordWrap,
               Tr("This structure cannot be drawn from GlyphWiki strokes."));
  }

  // Snap guides while dragging.
  p.setPen(QPen(accent, 1, Qt::DashLine));
  for (const double x : guides_x_) {
    const double px = RootToPixels(QRectF(x, 0, 0, 0)).left();
    p.drawLine(QPointF(px, sq.top()), QPointF(px, sq.bottom()));
  }
  for (const double y : guides_y_) {
    const double py = RootToPixels(QRectF(0, y, 0, 0)).top();
    p.drawLine(QPointF(sq.left(), py), QPointF(sq.right(), py));
  }

  // The boxes.
  QFont label = font();
  label.setPointSizeF(label.pointSizeF() * 0.85);
  p.setFont(label);
  for (int i = 0; i < static_cast<int>(boxes_.size()); ++i) {
    const QRectF r = RootToPixels(RootRect(boxes_[i]));
    const bool selected = i == selected_;
    QColor edge = selected ? accent : ink;
    edge.setAlpha(selected ? 255 : 110);
    p.setBrush(selected ? QColor(accent.red(), accent.green(), accent.blue(), 28)
                        : QBrush(Qt::NoBrush));
    p.setPen(QPen(edge, selected ? 1.6 : 1, selected ? Qt::SolidLine : Qt::DashLine));
    p.drawRect(r);
    // A number tab at the corner.
    const QString tag =
        QString::number(i + 1) + (IsAdjusted(i) ? QStringLiteral("*") : QString());
    const QRectF tab(r.left(), r.top() - 14,
                     8 + QFontMetrics(label).horizontalAdvance(tag), 14);
    p.setPen(Qt::NoPen);
    p.setBrush(edge);
    p.drawRect(tab);
    p.setPen(selected ? pal.color(QPalette::HighlightedText) : pal.color(QPalette::Base));
    p.drawText(tab, Qt::AlignCenter, tag);
  }
  if (selected_ >= 0 && selected_ < static_cast<int>(boxes_.size())) {
    const QRectF r = RootToPixels(SelectedRect());
    p.setPen(QPen(accent, 1));
    p.setBrush(pal.color(QPalette::Base));
    const QPointF pts[] = {r.topLeft(),     {r.center().x(), r.top()},
                           r.topRight(),    {r.right(), r.center().y()},
                           r.bottomRight(), {r.center().x(), r.bottom()},
                           r.bottomLeft(),  {r.left(), r.center().y()}};
    for (const QPointF &pt : pts) {
      p.drawRect(QRectF(pt.x() - kHandle, pt.y() - kHandle, 2 * kHandle, 2 * kHandle));
    }
  }
  if (hasFocus()) {
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(accent, 1, Qt::DotLine));
    p.drawRect(rect().adjusted(1, 1, -1, -1));
  }
}

LayoutAdjustDialog::LayoutAdjustDialog(const std::string &op,
                                       std::vector<ComposerSlot> parts,
                                       const KageResources *kage,
                                       LayoutAdjustments initial, QWidget *parent)
    : QDialog(parent) {
  setWindowTitle(Tr("Adjust layout"));
  setModal(true);

  canvas_ = new AdjustCanvas(op, std::move(parts), kage, std::move(initial), this);

  list_ = new QListWidget(this);
  list_->setMaximumHeight(130);
  list_->setAccessibleName(Tr("Components"));

  const auto make_spin = [this](int low, int high) {
    auto *spin = new QSpinBox(this);
    spin->setRange(low, high);
    return spin;
  };
  x_ = make_spin(-20, 220);
  y_ = make_spin(-20, 220);
  w_ = make_spin(12, 240);
  h_ = make_spin(12, 240);
  x_->setAccessibleName(Tr("Left"));
  y_->setAccessibleName(Tr("Top"));
  w_->setAccessibleName(Tr("Width"));
  h_->setAccessibleName(Tr("Height"));

  snap_ = new QCheckBox(Tr("Snap to guides"), this);
  snap_->setChecked(true);

  auto *centre_x = new QPushButton(Tr("Centre across"), this);
  auto *centre_y = new QPushButton(Tr("Centre down"), this);
  auto *reset = new QPushButton(Tr("Reset component"), this);
  auto *reset_all = new QPushButton(Tr("Reset all"), this);
  for (auto *b : {centre_x, centre_y, reset, reset_all}) b->setAutoDefault(false);

  auto *fields = new QGridLayout;
  fields->addWidget(new QLabel(Tr("Left"), this), 0, 0);
  fields->addWidget(x_, 0, 1);
  fields->addWidget(new QLabel(Tr("Top"), this), 0, 2);
  fields->addWidget(y_, 0, 3);
  fields->addWidget(new QLabel(Tr("Width"), this), 1, 0);
  fields->addWidget(w_, 1, 1);
  fields->addWidget(new QLabel(Tr("Height"), this), 1, 2);
  fields->addWidget(h_, 1, 3);

  auto *hint = new QLabel(
      Tr("Drag a box to move it and its handles to resize it (Shift on a "
         "corner keeps the shape). Arrow keys nudge by 1, Shift+arrows by 10; "
         "+ and - grow and shrink; Tab picks the next component; Delete "
         "resets it."),
      this);
  hint->setWordWrap(true);
  hint->setMaximumWidth(260);

  auto *side = new QVBoxLayout;
  side->addWidget(new QLabel(Tr("Components"), this));
  side->addWidget(list_);
  side->addLayout(fields);
  side->addWidget(snap_);
  side->addWidget(centre_x);
  side->addWidget(centre_y);
  side->addWidget(reset);
  side->addWidget(reset_all);
  side->addWidget(hint);
  side->addStretch(1);

  auto *buttons =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  auto *body = new QHBoxLayout;
  body->addWidget(canvas_, 1);
  body->addLayout(side);
  auto *root = new QVBoxLayout(this);
  root->addLayout(body, 1);
  root->addWidget(buttons);

  FillList();
  SyncFields();

  connect(list_, &QListWidget::currentRowChanged, this, [this](int row) {
    if (!syncing_ && row >= 0) canvas_->Select(row);
  });
  connect(canvas_, &AdjustCanvas::selectionChanged, this, [this](int row) {
    syncing_ = true;
    list_->setCurrentRow(row);
    syncing_ = false;
    SyncFields();
  });
  connect(canvas_, &AdjustCanvas::geometryChanged, this, [this] {
    FillList();
    SyncFields();
  });
  const auto edited = [this](int) {
    if (syncing_) return;
    canvas_->SetSelectedRect(
        QRectF(x_->value(), y_->value(), w_->value(), h_->value()));
  };
  for (QSpinBox *spin : {x_, y_, w_, h_}) {
    connect(spin, qOverload<int>(&QSpinBox::valueChanged), this, edited);
  }
  connect(snap_, &QCheckBox::toggled, canvas_, &AdjustCanvas::set_snapping);
  connect(reset, &QPushButton::clicked, canvas_, &AdjustCanvas::ResetSelected);
  connect(reset_all, &QPushButton::clicked, canvas_, &AdjustCanvas::ResetAll);
  connect(centre_x, &QPushButton::clicked, this, [this] {
    QRectF r = canvas_->SelectedRect();
    r.moveLeft(100 - r.width() / 2);
    canvas_->SetSelectedRect(r);
  });
  connect(centre_y, &QPushButton::clicked, this, [this] {
    QRectF r = canvas_->SelectedRect();
    r.moveTop(100 - r.height() / 2);
    canvas_->SetSelectedRect(r);
  });
  canvas_->setFocus();
}

void LayoutAdjustDialog::FillList() {
  syncing_ = true;
  const int keep = canvas_->selected();
  list_->clear();
  const auto &boxes = canvas_->boxes();
  for (int i = 0; i < static_cast<int>(boxes.size()); ++i) {
    QString text = QStringLiteral("%1  %2").arg(i + 1).arg(
        boxes[i].text.empty() ? Tr("(nested layout)")
                              : QString::fromStdString(boxes[i].text));
    if (canvas_->IsAdjusted(i)) text += QStringLiteral("  *");
    list_->addItem(text);
  }
  list_->setCurrentRow(keep);
  syncing_ = false;
}

void LayoutAdjustDialog::SyncFields() {
  syncing_ = true;
  const QRectF r = canvas_->SelectedRect();
  const bool any = canvas_->selected() >= 0;
  for (QSpinBox *spin : {x_, y_, w_, h_}) spin->setEnabled(any);
  x_->setValue(static_cast<int>(std::lround(r.left())));
  y_->setValue(static_cast<int>(std::lround(r.top())));
  w_->setValue(static_cast<int>(std::lround(r.width())));
  h_->setValue(static_cast<int>(std::lround(r.height())));
  syncing_ = false;
}

}  // namespace gui
}  // namespace mozc
