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

#include <QAbstractScrollArea>
#include <QDialog>
#include <QFrame>
#include <QHash>
#include <QKeyEvent>
#include <QLineEdit>
#include <QRect>
#include <QSet>
#include <QSize>
#include <QToolButton>
#include <QString>
#include <QWidget>
#include <memory>
#include <functional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "composer_ids/char_info.h"
#include "composer_ids/ids_table.h"
#include "composer_ids/ids_tree.h"
#include "gui/character_composer/composer_kage.h"

class QAction;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QEvent;
class QGridLayout;
class QHBoxLayout;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QStackedWidget;
class QTimer;
class QVBoxLayout;

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

// One leaf box: the character field, and under it a row of every other form of
// that radical (when the table knows more than one). Click a form to put it in
// the field without the automatic position swap.
class LeafSlot : public QWidget {
  Q_OBJECT

 public:
  LeafSlot(LayoutEditor *editor, composer_ids::SlotPath path, std::string op,
           int part_index, const QString &initial, QWidget *parent);
  SlotEdit *edit() const { return edit_; }
  void RefreshForms();

 protected:
  void resizeEvent(QResizeEvent *event) override;

 private:
  void PickForm(const QString &form);
  void UpdateFormsVisibility();

  LayoutEditor *editor_;
  composer_ids::SlotPath path_;
  std::string op_;
  int part_index_ = 0;
  SlotEdit *edit_ = nullptr;
  QWidget *forms_row_ = nullptr;
  QHBoxLayout *forms_layout_ = nullptr;
  bool has_forms_ = false;
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
  std::vector<QWidget *> children_;  // LeafSlot or LayoutView, one per part
  QToolButton *remove_button_ = nullptr;
};

// Owns the composer state (outer operator, slot tree, selection) and the
// widget tree that shows it. The widgets are rebuilt when the structure
// changes and left alone while the user types.
class LayoutEditor : public QWidget {
  Q_OBJECT

 public:
  explicit LayoutEditor(QWidget *parent = nullptr);

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

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

// The scrolling catalogue inside the Components window: borderless components
// in a grid under one heading per stroke count (1 STROKE, 2 STROKES …), drawn
// directly rather than as hundreds of buttons. The columns follow the width;
// the glyphs keep their size.
class ComponentCatalogue : public QAbstractScrollArea {
  Q_OBJECT

 public:
  struct Section {
    int strokes = 0;
    std::vector<QString> components;
  };

  explicit ComponentCatalogue(QWidget *parent);

  // Replaces the sections. With |keep_position| the view stays at the section
  // that was at the top (or the next one if it is gone); otherwise it goes to
  // the start.
  void SetSections(std::vector<Section> sections, bool keep_position);
  // |selected| components are marked; |dimmed| ones are drawn faint and cannot
  // be clicked.
  void SetState(const QSet<QString> &selected, const QSet<QString> &dimmed);

  // For tests: what the catalogue shows, and the same toggle a click makes.
  bool Contains(const QString &component) const;
  bool IsSelected(const QString &component) const { return selected_.contains(component); }
  bool IsDimmed(const QString &component) const;
  bool Activate(const QString &component);

  QSize sizeHint() const override { return QSize(640, 520); }
  QSize minimumSizeHint() const override { return QSize(300, 200); }

 signals:
  void componentClicked(const QString &component);

 protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void focusInEvent(QFocusEvent *event) override;
  void focusOutEvent(QFocusEvent *event) override;

 private:
  struct Cell {
    QString text;
    int strokes = 0;
    QRect rect;  // in content coordinates (before scrolling)
  };
  struct Heading {
    int strokes = 0;
    QRect rect;
  };

  void Relayout();
  int CellAt(const QPoint &viewport_pos) const;
  void EnsureVisible(int cell);
  void MoveCurrent(int cell);
  bool Dimmed(int cell) const;
  QString AccessibleText(int cell) const;

  std::vector<Section> sections_;
  std::vector<Cell> cells_;
  std::vector<Heading> headings_;
  std::vector<std::vector<int>> rows_;  // cell indices of each grid row
  int content_height_ = 0;
  int hover_ = -1;
  int current_ = -1;  // the keyboard cursor
  int pressed_ = -1;
  bool show_focus_ring_ = false;  // keyboard focus only, not after a click
  QFont glyph_font_;
  QSet<QString> selected_;
  QSet<QString> dimmed_;
};

// A small circled "i" (painted, like the info buttons in Preferences) that
// opens a ComposerInfoPopover.
class ComposerInfoButton : public QToolButton {
  Q_OBJECT

 public:
  ComposerInfoButton(const QString &accessible_name, QWidget *parent);

 protected:
  void paintEvent(QPaintEvent *event) override;
};

// The explanation an info button opens: a title, a body (rich text) and an
// optional example line. Closes on Escape or a click elsewhere.
class ComposerInfoPopover : public QFrame {
  Q_OBJECT

 public:
  explicit ComposerInfoPopover(QWidget *parent);
  void ShowFor(const QString &title, const QString &body, const QString &example,
               const QRect &global_anchor);
  QString title() const;

 protected:
  void keyPressEvent(QKeyEvent *event) override;

 private:
  QLabel *title_ = nullptr;
  QLabel *body_ = nullptr;
  QLabel *example_ = nullptr;
};

// The Components tab of the middle column: common components grouped by
// stroke count. A click toggles the component in the search field (through
// the dialog), so the results on the right update while the user picks;
// components that would leave no result can be greyed out.
class ComponentsPanel : public QWidget {
  Q_OBJECT

 public:
  explicit ComponentsPanel(QWidget *parent);

  // Where the first strokes of the common components come from (for the
  // "First stroke" buttons); may be null, then those components are not
  // matched by a first stroke. Not owned.
  void set_table(const composer_ids::IdsTable *table) { table_ = table; }
  ComponentCatalogue *catalogue() const { return catalogue_; }

  // Highlights the components currently in the search field, and disables
  // those in |unavailable| (ignored unless "Dim unavailable" is ticked).
  void SetState(const std::vector<std::string> &selected,
                const std::vector<std::string> &unavailable);
  // Every component the window offers, as shown (variants included or not).
  std::vector<std::string> ShownComponents() const;
  bool grey_out_unavailable() const;
  // For screenshots: "Difficult components" and the first-stroke buttons as
  // a bit mask (bit 0 = 一 … bit 4 = 乛).
  void SetFilters(bool difficult, int first_stroke_mask);

 signals:
  void componentToggled(const QString &component);
  // "Radical variants", "Difficult components", a first stroke or "Dim
  // unavailable" changed.
  void optionsChanged();

 protected:
  void changeEvent(QEvent *event) override;

 private:
  void ApplyTheme();
  void Rebuild(bool keep_position);
  // Whether |ch| passes "Difficult components" and the first-stroke buttons.
  bool Wanted(const std::string &ch) const;

  const composer_ids::IdsTable *table_ = nullptr;
  ComponentCatalogue *catalogue_ = nullptr;
  // In the View menu.
  QAction *variants_action_ = nullptr;
  QAction *grey_out_action_ = nullptr;
  QAction *hard_action_ = nullptr;
  bool applying_theme_ = false;
  std::vector<QToolButton *> first_stroke_buttons_;  // 一 丨 丿 丶 乛
  std::vector<std::string> selected_;
  std::vector<std::string> unavailable_;
};

class CharacterComposerDialog : public QDialog {
  Q_OBJECT

 public:
  // |table| may be null (data files missing): the dialog still builds IDS
  // strings, only the character lookup is unavailable.
  CharacterComposerDialog(std::unique_ptr<composer_ids::IdsTable> table,
                          const QString &data_directory, QWidget *parent = nullptr,
                          std::unique_ptr<composer_ids::CharInfoTable> char_info =
                              nullptr);

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

  // Demo / screenshot helpers (env-driven capture from RunCharacterComposer).
  // Apply a full IDS expression to the composition without going through AX.
  void ApplyIdsExpression(const std::string &ids);
  // Run "Find by components" with |components| and optionally activate result
  // |result_index| (0-based). Negative index leaves composition active.
  void RunComponentSearch(const std::string &components, int result_index);
  // Resizes the composer to |size|, opens the Components tab with the given
  // filters (see ComponentsPanel::SetFilters) and |strokes| in the Strokes
  // field; SaveComponentsPicture saves a picture of the composer to |path|.
  void OpenComponentsForDemo(const QSize &size, bool difficult, const QString &strokes,
                             int first_stroke_mask);
  bool SaveComponentsPicture(const QString &path);

 private slots:
  void OnLayoutChanged();
  void OnPaletteClicked(int index);
  void OnInsert();
  void OnCopy();
  void OnBreakApart();
  void OnFindContaining();
  void OnContainingPicked(QListWidgetItem *item);
  void OnSubmitRequested();
  void OnIdsEdited(const QString &text);
  void ShowInfo(QWidget *anchor);
  void OnReturnToComposition();
  void OnClearSearchSelection();
  void OnShowComponentsTab();
  void OnComponentToggled(const QString &component);

 protected:
  void keyPressEvent(QKeyEvent *event) override;
  void changeEvent(QEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

 private:
  enum class ActiveSource {
    kComposition,
    kSearchResult,
  };

  void BuildUi(const QString &data_directory);
  void ApplyTheme();
  // Recompute composition lookup (found_ / pending_svg_) from LayoutEditor.
  void UpdateCompositionState();
  // Opens the hand-adjustment window for the current composition.
  void OnAdjustLayout();
  // The user's boxes while they still fit the composition; null otherwise.
  const LayoutAdjustments *ActiveAdjustments() const;
  // Refresh specimen, Character Information, and Copy/Insert from active source.
  void RefreshActiveView();
  void ActivateComposition();
  void ActivateSearchResult(const std::string &ch);
  void UpdateComponentRoles();
  void UpdateCharInfoTable(const composer_ids::CharInfo *info);
  void SetInfoRow(QLabel *label, QWidget *value, const QString &text);
  void SetVariantsChips(const composer_ids::CharInfo *info);
  // Insert if ready, after committing any IME preedit.
  void TryInsert();
  void UpdatePaletteHighlight();
  void RefreshPaletteIcons();
  // Writes DraftIds into the IDS entry box. Skips while the user is typing
  // there, unless |force|.
  void SyncIdsEditFromEditor(bool force = false);
  // The query typed in the field, with the Strokes field folded in.
  composer_ids::LookupQuery CurrentQuery() const;
  void UpdateComponentsPanel();
  // 0: the Character page of the middle column, 1: the Components page.
  void ShowMiddlePage(int page);

  std::unique_ptr<composer_ids::IdsTable> table_;
  std::unique_ptr<composer_ids::CharInfoTable> char_info_;  // Unihan display; may be null
  std::unique_ptr<KageResources> kage_;  // GlyphWiki strokes; may be null
  LayoutEditor *editor_ = nullptr;
  QButtonGroup *palette_ = nullptr;
  QWidget *construct_panel_ = nullptr;
  QWidget *examine_panel_ = nullptr;
  QWidget *discover_panel_ = nullptr;
  QLabel *component_roles_label_ = nullptr;
  QLineEdit *ids_edit_ = nullptr;
  ComposerInfoButton *ids_info_button_ = nullptr;
  ComposerInfoButton *decompose_info_button_ = nullptr;
  ComposerInfoButton *find_info_button_ = nullptr;
  ComposerInfoPopover *info_popover_ = nullptr;
  QStackedWidget *middle_stack_ = nullptr;
  QPushButton *character_tab_ = nullptr;
  QPushButton *components_tab_ = nullptr;
  QLabel *active_state_label_ = nullptr;
  QLabel *char_label_ = nullptr;
  QWidget *specimen_group_ = nullptr;
  QPushButton *return_to_composition_ = nullptr;
  QLabel *status_heading_ = nullptr;
  QLabel *status_detail_ = nullptr;
  QLabel *on_label_ = nullptr;
  QLabel *on_value_ = nullptr;
  QLabel *kun_label_ = nullptr;
  QLabel *kun_value_ = nullptr;
  QLabel *meaning_label_ = nullptr;
  QLabel *meaning_value_ = nullptr;
  QLabel *variants_label_ = nullptr;
  QWidget *variants_value_ = nullptr;  // flow of specimen-coloured chips
  QLabel *strokes_label_ = nullptr;
  QLabel *strokes_value_ = nullptr;
  QLabel *radical_label_ = nullptr;
  QLabel *radical_value_ = nullptr;
  QLabel *other_label_ = nullptr;
  QLabel *other_value_ = nullptr;
  QWidget *info_table_ = nullptr;
  QFrame *info_rule_ = nullptr;
  QLineEdit *break_apart_edit_ = nullptr;
  QComboBox *decomposition_combo_ = nullptr;
  QLineEdit *containing_edit_ = nullptr;
  QListWidget *results_list_ = nullptr;
  QLabel *results_status_ = nullptr;
  QLineEdit *stroke_edit_ = nullptr;  // "12", "<12", ">8", "9-12"; empty: any
  QTimer *stroke_timer_ = nullptr;    // searches after a pause in typing
  ComponentsPanel *components_panel_ = nullptr;
  QTimer *panel_timer_ = nullptr;
  // Components that find nothing for the query in |unavailable_key_|.
  std::string unavailable_key_;
  std::vector<std::string> unavailable_cache_;
  QPushButton *insert_button_ = nullptr;
  QPushButton *copy_button_ = nullptr;
  QPushButton *adjust_button_ = nullptr;
  // Components moved or resized by hand, for the IDS in |adjustments_ids_|
  // (dropped as soon as the composition changes).
  LayoutAdjustments adjustments_;
  std::string adjustments_ids_;
  QPushButton *clear_button_ = nullptr;
  QPushButton *close_button_ = nullptr;
  QPushButton *break_button_ = nullptr;
  QPushButton *find_button_ = nullptr;
  QPushButton *clear_results_button_ = nullptr;

  QString note_;  // a one-off message that replaces the status line
  QString data_directory_;
  bool applying_ids_edit_ = false;  // true while OnIdsEdited updates the square
  bool applying_theme_ = false;     // guards ApplyTheme vs changeEvent re-entry
  ActiveSource active_source_ = ActiveSource::kComposition;
  std::string search_active_char_;  // UTF-8 when a search result is active
  std::string found_;       // the encoded character for the current layout, or ""
  std::string result_;      // Unicode Insert chose, or ""
  std::function<void(const std::string &)> on_svg_inserted_;
  std::string result_svg_;  // SVG Insert chose for an unencoded layout, or ""
  std::string pending_svg_; // live SVG preview for the current filled layout
  // GlyphWiki names already found not to exist online (this session), so a
  // structure GlyphWiki does not register is not requested on every redraw.
  std::set<std::string> glyphwiki_unregistered_;
  bool glyphwiki_curl_warned_ = false;
};

}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_CHARACTER_COMPOSER_CHARACTER_COMPOSER_DIALOG_H_
