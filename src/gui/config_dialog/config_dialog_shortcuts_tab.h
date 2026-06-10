#ifndef MOZC_GUI_CONFIG_DIALOG_CONFIG_DIALOG_SHORTCUTS_TAB_H_
#define MOZC_GUI_CONFIG_DIALOG_CONFIG_DIALOG_SHORTCUTS_TAB_H_

#include <QCheckBox>
#include <QLabel>
#include <QObject>
#include <QTabWidget>
#include <QWidget>

#include "protocol/config.pb.h"

namespace mozc {
namespace gui {

class ConfigDialogShortcutsTab {
 public:
  explicit ConfigDialogShortcutsTab(QWidget* parent);

  void AddToTabWidget(QTabWidget* tab_widget);
  void LoadFromConfig(const config::Config& config);
  void ApplyToConfig(config::Config* config) const;
  void ConnectApplyButton(const QObject* receiver, const char* slot);

 private:
  QWidget* tab_;
  QCheckBox* disable_left_shift_direct_toggle_;
  QLabel* left_shift_help_;
};

}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_CONFIG_DIALOG_CONFIG_DIALOG_SHORTCUTS_TAB_H_
