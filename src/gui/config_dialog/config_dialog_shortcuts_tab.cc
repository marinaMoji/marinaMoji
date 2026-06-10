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
          "between Japanese input and direct input. From direct input, Left Shift "
          "returns to the Japanese mode you were using before (hiragana, "
          "katakana, etc.). Press and release Ctrl+Left Shift alone to lock the "
          "current mode and prevent accidental toggles; use the same chord again "
          "to unlock. Shift still works for capitals and shortcuts while locked."),
      tab_);
  left_shift_help_->setWordWrap(true);
  layout->addWidget(left_shift_help_);

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

}  // namespace gui
}  // namespace mozc
