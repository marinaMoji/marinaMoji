#ifndef MOZC_GUI_CONFIG_DIALOG_CONFIG_DIALOG_SYNC_TAB_H_
#define MOZC_GUI_CONFIG_DIALOG_CONFIG_DIALOG_SYNC_TAB_H_

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QWidget>

#include "client/client.h"
#include "protocol/commands.pb.h"

namespace mozc {
namespace gui {

class ConfigDialogSyncTab {
 public:
  explicit ConfigDialogSyncTab(client::Client* client, QWidget* parent);

  void AddToTabWidget(QTabWidget* tab_widget);
  void LoadFromServer();
  bool SaveToServer();

 private:
  void UpdateUiState();
  bool ShouldOfferShowSyncKey() const;
  void OnBrowseFile();
  void OnGenerateKey();
  void OnEnterOrShowKey();
  void OnShowKey();
  void OnEnterKey();
  void OnSyncNow();

  client::Client* client_;
  QWidget* tab_;
  QCheckBox* enabled_;
  QLineEdit* file_path_;
  QPushButton* browse_;
  QPushButton* generate_key_;
  QPushButton* enter_key_;
  QPushButton* sync_now_;
  QCheckBox* sync_dictionary_;
  QCheckBox* sync_history_;
  QComboBox* direction_;
  QComboBox* auto_sync_;
  QSpinBox* interval_;
  QSpinBox* cooldown_;
  QLabel* status_;

  commands::UserSyncConfig config_;
};

}  // namespace gui
}  // namespace mozc

#endif  // MOZC_GUI_CONFIG_DIALOG_CONFIG_DIALOG_SYNC_TAB_H_
