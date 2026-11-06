// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// "Adjust layout": slide and resize the components of a composed character by
// hand. The drawing is the composer's own (GlyphWiki strokes in boxes); each
// component's box can be dragged, resized by its handles, nudged with the arrow
// keys or typed in numerically, and snapped to guides. The result is a set of
// boxes (LayoutAdjustments) the composer draws instead of its automatic ones.

#ifndef MOZC_GUI_CHARACTER_COMPOSER_LAYOUT_ADJUST_DIALOG_H_
#define MOZC_GUI_CHARACTER_COMPOSER_LAYOUT_ADJUST_DIALOG_H_

#include <QDialog>
#include <QPointF>
#include <QRectF>
#include <QWidget>
#include <optional>
#include <string>
#include <vector>

#include "composer_ids/ids_tree.h"
#include "gui/character_composer/composer_kage.h"

class QListWidget;
class QSpinBox;
class QCheckBox;

namespace mozc {
namespace gui {

// The square the components are drawn and dragged in. Units are KAGE's 0..200
// of the whole glyph ("root" units below); a nested component's own box is
// converted to and from its parent's space.
class AdjustCanvas : public QWidget {
  Q_OBJECT

 public:
  AdjustCanvas(std::string op, std::vector<composer_ids::ComposerSlot> parts,
               const KageResources *kage, LayoutAdjustments initial,
               QWidget *parent = nullptr);

  const LayoutAdjustments &adjustments() const { return adjust_; }
  const std::vector<PlacedBox> &boxes() const { return boxes_; }
  int selected() const { return selected_; }
  void Select(int index);
  void set_snapping(bool on) { snapping_ = on; }

  // The selected component's box in root units.
  QRectF SelectedRect() const;
  // Moves/resizes the selected component (root units), as a drag would.
  void SetSelectedRect(const QRectF &rect);
  void ResetSelected();
  void ResetAll();
  bool IsAdjusted(int index) const;

  QSize sizeHint() const override { return QSize(400, 400); }
  QSize minimumSizeHint() const override { return QSize(260, 260); }

 signals:
  void selectionChanged(int index);
  void geometryChanged();

 protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

 private:
  enum Edge { kLeft = 1, kTop = 2, kRight = 4, kBottom = 8, kMove = 16 };

  void Recompose();
  QRectF Square() const;                    // the drawing area in pixels
  QRectF RootToPixels(const QRectF &r) const;
  QPointF PixelsToRoot(const QPointF &p) const;
  QRectF RootRect(const PlacedBox &box, bool automatic = false) const;
  void SetRootRect(int index, QRectF rect);  // stores it as the adjustment
  int HandleAt(const QPointF &pixel) const;  // edge mask, 0 when none
  int BoxAt(const QPointF &pixel) const;
  // |rect| pulled onto nearby guides (lit while dragging) when snapping is on.
  QRectF Snapped(QRectF rect, int mask);

  std::string op_;
  std::vector<composer_ids::ComposerSlot> parts_;
  const KageResources *kage_;
  LayoutAdjustments adjust_;
  std::vector<PlacedBox> boxes_;
  std::optional<std::vector<Contour>> outlines_;
  int selected_ = -1;
  bool snapping_ = true;

  int drag_mask_ = 0;
  QPointF drag_start_;     // in root units
  QRectF drag_origin_;     // the box when the drag began, root units
  std::vector<double> guides_x_, guides_y_;  // lit while dragging
};

class LayoutAdjustDialog : public QDialog {
  Q_OBJECT

 public:
  LayoutAdjustDialog(const std::string &op,
                     std::vector<composer_ids::ComposerSlot> parts,
                     const KageResources *kage, LayoutAdjustments initial,
                     QWidget *parent = nullptr);

  const LayoutAdjustments &adjustments() const { return canvas_->adjustments(); }

 private:
  void FillList();
  void SyncFields();

  AdjustCanvas *canvas_ = nullptr;
  QListWidget *list_ = nullptr;
  QSpinBox *x_ = nullptr, *y_ = nullptr, *w_ = nullptr, *h_ = nullptr;
  QCheckBox *snap_ = nullptr;
  bool syncing_ = false;
};

}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_CHARACTER_COMPOSER_LAYOUT_ADJUST_DIALOG_H_
