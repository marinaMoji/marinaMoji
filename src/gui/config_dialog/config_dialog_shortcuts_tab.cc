#include "gui/config_dialog/config_dialog_shortcuts_tab.h"

#include <QCheckBox>
#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>

namespace mozc {
namespace gui {

ConfigDialogShortcutsTab::ConfigDialogShortcutsTab(QWidget* parent)
    : tab_(new QWidget(parent)) {
  auto* layout = new QVBoxLayout(tab_);

  disable_left_shift_direct_toggle_ = new QCheckBox(
      QObject::tr("Disable Left Shift toggle (Japanese ↔ Direct input)"), tab_);
  layout->addWidget(disable_left_shift_direct_toggle_);

  left_shift_help_ = new QLabel(
      QObject::tr(
          "When enabled (default), press and release Left Shift alone to switch "
          "between hiragana and direct input only (Manyōshū, half-width, and "
          "other modes are unaffected). From direct input, Left Shift returns "
          "to the hiragana or full-katakana mode you were using before. Press "
          "and release Ctrl+Left Shift alone to lock the current mode and "
          "prevent accidental toggles; use the same chord again to unlock. "
          "Right Shift alone toggles hiragana and Manyōshū (katakana). Shift "
          "still works for capitals and shortcuts while locked."),
      tab_);
  left_shift_help_->setWordWrap(true);
  layout->addWidget(left_shift_help_);

  layout->addSpacing(16);

  kaeriten_help_ = new QLabel(
      QObject::tr(
          "In direct input, type semicolon then your keys to insert kaeriten "
          "marks (for example ;r → ㆑). Marks not in the default set (e.g. "
          "linking mark ㆐) can be added with New entry."),
      tab_);
  kaeriten_help_->setWordWrap(true);
  layout->addWidget(kaeriten_help_);

  layout->addSpacing(8);

  edit_kaeriten_button_ =
      new QPushButton(QObject::tr("Edit kaeriten shortcuts..."), tab_);
  layout->addWidget(edit_kaeriten_button_);

  layout->addSpacing(12);

  layout->addStretch();
}

void ConfigDialogShortcutsTab::AddToTabWidget(QTabWidget* tab_widget) {
  tab_widget->addTab(tab_, QObject::tr("Shortcuts"));
}

void ConfigDialogShortcutsTab::LoadFromConfig(const config::Config& config) {
  disable_left_shift_direct_toggle_->setChecked(
      config.disable_left_shift_direct_toggle());
}

void ConfigDialogShortcutsTab::ApplyToConfig(config::Config* config) const {
  config->set_disable_left_shift_direct_toggle(
      disable_left_shift_direct_toggle_->isChecked());
}

void ConfigDialogShortcutsTab::ConnectApplyButton(const QObject* receiver,
                                                  const char* slot) {
  QObject::connect(disable_left_shift_direct_toggle_, SIGNAL(clicked()), receiver,
                   slot);
}

void ConfigDialogShortcutsTab::ConnectEditKaeritenButton(
    const QObject* receiver, const char* slot) {
  QObject::connect(edit_kaeriten_button_, SIGNAL(clicked()), receiver, slot);
}

}  // namespace gui
}  // namespace mozc
