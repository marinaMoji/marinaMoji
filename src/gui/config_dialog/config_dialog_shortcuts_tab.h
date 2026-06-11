#ifndef MOZC_GUI_CONFIG_DIALOG_CONFIG_DIALOG_SHORTCUTS_TAB_H_
#define MOZC_GUI_CONFIG_DIALOG_CONFIG_DIALOG_SHORTCUTS_TAB_H_

#include <QCheckBox>
#include <QLabel>
#include <QObject>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QWidget>

#include <vector>

#include "protocol/config.pb.h"

namespace mozc {
namespace gui {

class ConfigDialogShortcutsTab {
 public:
  explicit ConfigDialogShortcutsTab(QWidget* parent);

  void AddToTabWidget(QTabWidget* tab_widget);
  void LoadFromConfig(const config::Config& config);
  bool ValidateBeforeApply(QString* error_message) const;
  void ApplyToConfig(config::Config* config) const;
  void ConnectApplyButton(const QObject* receiver, const char* slot);
  void ConnectEditKaeritenButton(const QObject* receiver, const char* slot);

 private:
  std::vector<config::MarinaNumberRowBinding> CollectBindingsFromTable() const;
  bool BindingsMatchDefaults(
      const std::vector<config::MarinaNumberRowBinding>& bindings) const;

  QWidget* tab_;
  QCheckBox* disable_left_shift_direct_toggle_;
  QLabel* left_shift_help_;
  QLabel* number_row_help_;
  QLabel* mac_number_row_note_;
  QTableWidget* number_row_table_;
  QLabel* kaeriten_help_;
  QPushButton* edit_kaeriten_button_;
};

}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_CONFIG_DIALOG_CONFIG_DIALOG_SHORTCUTS_TAB_H_
