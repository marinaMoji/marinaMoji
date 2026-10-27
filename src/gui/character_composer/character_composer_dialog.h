// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause
//
// marinaMoji's character composer: pick an IDS layout, fill the boxes (nesting
// as deep as needed) and see the encoded Unicode character that structure
// describes. Insert sends that character to the application when one exists;
// when the structure is unrecorded, Insert produces an SVG drawing instead
// (for office plugins / clipboard).

#ifndef MOZC_GUI_CHARACTER_COMPOSER_CHARACTER_COMPOSER_DIALOG_H_
#define MOZC_GUI_CHARACTER_COMPOSER_CHARACTER_COMPOSER_DIALOG_H_

#include <QDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QString>
#include <QWidget>
#include <memory>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "composer_ids/ids_table.h"
#include "composer_ids/ids_tree.h"
#include "kage/kage_engine.h"

class QButtonGroup;
class QComboBox;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QToolButton;

namespace mozc {
namespace gui {

class LayoutEditor;

// A one-component text field. Tells the editor when it gets the focus, which
// is how a field becomes the selected region.
class SlotEdit : public QLineEdit {
  Q_OBJECT

 public:
  SlotEdit(composer_ids::SlotPath path, QWidget *parent);
  const composer_ids::SlotPath &path() const { return path_; }
  void SetSelected(bool selected);

 signals:
  void gotFocus(const std::vector<int> &path);
  // Enter/Return in a box: dialog inserts when a result is ready (IME Enter
  // that confirms a candidate is consumed before it reaches here).
  void submitRequested();

 protected:
  void focusInEvent(QFocusEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

 private:
  composer_ids::SlotPath path_;
};

// One layout drawn as a square: its components sit in the operator's shape,
// nested layouts draw the same thing again inside their cell. Clicking its
// padding (not a field) selects it.
class LayoutView : public QWidget {
  Q_OBJECT

 public:
  LayoutView(LayoutEditor *editor, composer_ids::SlotPath path, std::string op,
             const std::vector<composer_ids::ComposerSlot> &parts,
             QWidget *parent);
  const composer_ids::SlotPath &path() const { return path_; }
  void SetSelected(bool selected);

 protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;

 private:
  void PlaceChildren();

  LayoutEditor *editor_;
  composer_ids::SlotPath path_;
  std::string op_;
  bool selected_ = false;
  std::vector<QWidget *> children_;  // SlotEdit or LayoutView, one per part
  QToolButton *remove_button_ = nullptr;
};

// Owns the composer state (outer operator, slot tree, selection) and the
// widget tree that shows it. The widgets are rebuilt when the structure
// changes and left alone while the user types.
class LayoutEditor : public QWidget {
  Q_OBJECT

 public:
  explicit LayoutEditor(QWidget *parent = nullptr);

  // The table consulted for positional radical forms (手 -> 扌 on the left of
  // ⿰). Not owned; may be null.
  void set_table(const composer_ids::IdsTable *table) { table_ = table; }
  const composer_ids::IdsTable *table() const { return table_; }

  const std::string &op() const { return op_; }
  const std::vector<composer_ids::ComposerSlot> &parts() const { return parts_; }
  const composer_ids::SlotPath &selection() const { return selection_; }

  void SetLayout(const std::string &op,
                 std::vector<composer_ids::ComposerSlot> parts);
  // A palette click: acts on the selected region (see ApplyPaletteOperator).
  void ApplyOperator(const std::string &op);
  // Replaces the selected field's character by the next form of its radical.
  // Returns false when nothing is selected or the character has no other form.
  bool CycleSelectedForm(const composer_ids::IdsTable &table);
  // Selects the region at |path| ({} is the outer square).
  void Select(const composer_ids::SlotPath &path);
  // Called by the view widgets.
  void SetLeafInput(const composer_ids::SlotPath &path, const QString &text);
  // × on a nested layout: back to a single field, or false (and a message) if
  // that would throw away more than one filled field.
  bool Flatten(const composer_ids::SlotPath &path);
  void Clear();
  // The operator of the region the palette would change (the selected layout,
  // or the outer one when a field or nothing is selected).
  std::string SelectedLayoutOp() const;

 signals:
  void changed();
  void selectionChanged();
  void flattenRefused();
  void submitRequested();

 private:
  void Rebuild();
  void ApplySelectionToWidgets();

  std::string op_ = "⿰";
  std::vector<composer_ids::ComposerSlot> parts_;
  composer_ids::SlotPath selection_;
  LayoutView *root_view_ = nullptr;
  const composer_ids::IdsTable *table_ = nullptr;
};

class CharacterComposerDialog : public QDialog {
  Q_OBJECT

 public:
  // |table| may be null (data files missing): the dialog still builds IDS
  // strings, only the character lookup is unavailable.
  CharacterComposerDialog(std::unique_ptr<composer_ids::IdsTable> table,
                          const QString &data_directory, QWidget *parent = nullptr);

  // The character the user chose to insert, UTF-8; empty if none (or SVG-only).
  const std::string &result() const { return result_; }
  // SVG for an unencoded structure; empty if Insert chose a Unicode character
  // or the dialog was cancelled.
  const std::string &result_svg() const { return result_svg_; }

  // Called with the SVG just before the dialog closes on Insert of an
  // unencoded structure, so the files other programs watch exist before focus
  // returns to them.
  void set_svg_inserted_callback(std::function<void(const std::string &)> cb) {
    on_svg_inserted_ = std::move(cb);
  }

 private slots:
  void OnLayoutChanged();
  void OnPaletteClicked(int index);
  void OnInsert();
  void OnCopy();
  void OnBreakApart();
  void OnFindContaining();
  void OnContainingPicked(QListWidgetItem *item);
  void OnCycleForm();
  void OnSubmitRequested();

 protected:
  void keyPressEvent(QKeyEvent *event) override;

 private:
  void BuildUi(const QString &data_directory);
  void UpdateResult();
  // Insert if ready, after committing any IME preedit.
  void TryInsert();
  void UpdatePaletteHighlight();

  std::unique_ptr<composer_ids::IdsTable> table_;
  std::unique_ptr<kage::Buhin> kage_glyphs_;  // GlyphWiki strokes; may be null
  LayoutEditor *editor_ = nullptr;
  QButtonGroup *palette_ = nullptr;
  QLabel *ids_label_ = nullptr;
  QLabel *char_label_ = nullptr;
  QLabel *codepoint_label_ = nullptr;
  QLabel *status_label_ = nullptr;
  QLineEdit *break_apart_edit_ = nullptr;
  QComboBox *decomposition_combo_ = nullptr;
  QLineEdit *containing_edit_ = nullptr;
  QListWidget *containing_list_ = nullptr;
  QPushButton *insert_button_ = nullptr;
  QPushButton *copy_button_ = nullptr;
  QPushButton *cycle_button_ = nullptr;

  QString note_;  // a one-off message that replaces the status line
  QString data_directory_;
  std::string found_;       // the encoded character for the current layout, or ""
  std::string result_;      // Unicode Insert chose, or ""
  std::function<void(const std::string &)> on_svg_inserted_;
  std::string result_svg_;  // SVG Insert chose for an unencoded layout, or ""
  std::string pending_svg_; // live SVG preview for the current filled layout
};

}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_CHARACTER_COMPOSER_CHARACTER_COMPOSER_DIALOG_H_
