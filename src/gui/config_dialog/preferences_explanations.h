// Copyright 2026, marinaMoji contributors
// SPDX-License-Identifier: BSD-3-Clause
//
// Optional contextual explanations for Preferences (Phase: sidebar utilities).
// Content is condensed from the website Guide and bundled offline.

#ifndef MOZC_GUI_CONFIG_DIALOG_PREFERENCES_EXPLANATIONS_H_
#define MOZC_GUI_CONFIG_DIALOG_PREFERENCES_EXPLANATIONS_H_

#include <QFrame>
#include <QString>
#include <QToolButton>
#include <QWidget>
#include <vector>

class QLabel;
class QVBoxLayout;

namespace mozc {
namespace gui {

struct PreferencesExplanation {
  const char* id;       // stable key, e.g. "writing.auto_conversion"
  QString title;
  QString body;
  QString example;      // optional typographic specimen; may be empty
  QString guide_anchor; // website Guide accordion id, for documentation sync
};

// Look up a bundled explanation. Returns nullptr when unknown.
const PreferencesExplanation* FindPreferencesExplanation(const char* id);

// All known explanations (for tests / tooling).
const std::vector<PreferencesExplanation>& AllPreferencesExplanations();

// Persist Show-explanations UI preference under the user profile directory.
bool LoadShowPreferencesExplanations();
void SaveShowPreferencesExplanations(bool enabled);

// Small circled-i control; painted, not emoji.
class PreferencesInfoButton : public QToolButton {
  Q_OBJECT

 public:
  explicit PreferencesInfoButton(const char* explanation_id,
                                 QWidget* parent = nullptr);
  const char* explanation_id() const { return explanation_id_; }

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  const char* explanation_id_;
};

// Editorial popover: heading, body, optional example.
class PreferencesExplanationPopover : public QFrame {
  Q_OBJECT

 public:
  explicit PreferencesExplanationPopover(QWidget* parent = nullptr);
  void ShowFor(const PreferencesExplanation& explanation,
               const QRect& global_anchor);

 public slots:
  void Dismiss();

 protected:
  void keyPressEvent(QKeyEvent* event) override;
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  QLabel* title_ = nullptr;
  QLabel* body_ = nullptr;
  QLabel* example_ = nullptr;
  QVBoxLayout* layout_ = nullptr;
};

// Owns the active popover and wires info buttons for a Preferences dialog.
class PreferencesExplanationController : public QObject {
  Q_OBJECT

 public:
  explicit PreferencesExplanationController(QWidget* dialog);
  ~PreferencesExplanationController() override;

  bool enabled() const { return enabled_; }
  void SetEnabled(bool enabled);
  void ToggleEnabled();

  // Reserve an 18px slot beside a label/control row. The button is shown only
  // when explanations are enabled; the slot always keeps layout stable.
  QWidget* MakeInfoSlot(const char* explanation_id, QWidget* parent);

  void RegisterInfoButton(PreferencesInfoButton* button);
  void ClosePopover();
  void OnPreferencesPageChanged();

 signals:
  void enabledChanged(bool enabled);

 private slots:
  void OnInfoClicked();

 private:
  QWidget* dialog_ = nullptr;
  bool enabled_ = false;
  PreferencesExplanationPopover* popover_ = nullptr;
  std::vector<PreferencesInfoButton*> buttons_;
};

}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_CONFIG_DIALOG_PREFERENCES_EXPLANATIONS_H_
