#include "gui/config_dialog/config_dialog_shortcuts_tab.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>

#include "session/marina_number_row_bindings_util.h"

namespace mozc {
namespace gui {
namespace {

using ::mozc::config::MarinaNumberRowAction;
using ::mozc::config::MarinaNumberRowBinding;
using ::mozc::config::MarinaPhysicalSlot;
using ::mozc::config::MarinaShortcutModifier;

constexpr int kColumnAction = 0;
constexpr int kColumnModifier = 1;
constexpr int kColumnSlot = 2;

MarinaNumberRowAction ActionForRow(int row) {
  static const MarinaNumberRowAction kActions[] = {
      MarinaNumberRowAction::MARINA_NR_ODORIJI_DEFAULT,
      MarinaNumberRowAction::MARINA_NR_ODORIJI_PALETTE,
      MarinaNumberRowAction::MARINA_NR_TRADITIONAL_KANJI,
      MarinaNumberRowAction::MARINA_NR_MANYOSHU_HIRAGANA,
      MarinaNumberRowAction::MARINA_NR_HIRAGANA_DIRECT,
      MarinaNumberRowAction::MARINA_NR_WORD_REGISTER,
  };
  if (row < 0 || row >= 6) {
    return MarinaNumberRowAction::MARINA_NR_ODORIJI_DEFAULT;
  }
  return kActions[row];
}

QString SlotComboLabel(MarinaPhysicalSlot slot) {
  const char* shifted = session::PhysicalSlotShiftedLabel(slot);
  QString base;
  switch (slot) {
    case MarinaPhysicalSlot::MARINA_SLOT_1:
      base = "1";
      break;
    case MarinaPhysicalSlot::MARINA_SLOT_2:
      base = "2";
      break;
    case MarinaPhysicalSlot::MARINA_SLOT_3:
      base = "3";
      break;
    case MarinaPhysicalSlot::MARINA_SLOT_4:
      base = "4";
      break;
    case MarinaPhysicalSlot::MARINA_SLOT_5:
      base = "5";
      break;
    case MarinaPhysicalSlot::MARINA_SLOT_6:
      base = "6";
      break;
    case MarinaPhysicalSlot::MARINA_SLOT_7:
      base = "7";
      break;
    case MarinaPhysicalSlot::MARINA_SLOT_8:
      base = "8";
      break;
    case MarinaPhysicalSlot::MARINA_SLOT_9:
      base = "9";
      break;
    case MarinaPhysicalSlot::MARINA_SLOT_0:
      base = "0";
      break;
    case MarinaPhysicalSlot::MARINA_SLOT_GRAVE:
      base = "`";
      break;
    default:
      base = "?";
      break;
  }
  if (shifted != nullptr && shifted[0] != '\0') {
    return QString("%1 (%2)").arg(base, QString::fromUtf8(shifted));
  }
  return base;
}

}  // namespace

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

  number_row_help_ = new QLabel(
      QObject::tr(
          "Assign marina actions to physical number-row keys (the top row on "
          "your keyboard). Shortcuts follow the key position, not the letter "
          "printed on the key, so they work on Dvorak, AZERTY, and other "
          "layouts."),
      tab_);
  number_row_help_->setWordWrap(true);
  layout->addWidget(number_row_help_);

#if defined(__APPLE__)
  mac_number_row_note_ = new QLabel(
      QObject::tr(
          "Custom number-row bindings are saved here and apply on Linux. "
          "macOS still uses the default bindings until a future update."),
      tab_);
  mac_number_row_note_->setWordWrap(true);
  layout->addWidget(mac_number_row_note_);
#else
  mac_number_row_note_ = nullptr;
#endif

  number_row_table_ = new QTableWidget(6, 3, tab_);
  number_row_table_->setHorizontalHeaderLabels(
      {QObject::tr("Action"), QObject::tr("Modifier"), QObject::tr("Key")});
  number_row_table_->verticalHeader()->setVisible(false);
  number_row_table_->setSelectionMode(QAbstractItemView::NoSelection);
  number_row_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  for (int row = 0; row < 6; ++row) {
    const MarinaNumberRowAction action = ActionForRow(row);
    auto* action_item = new QTableWidgetItem(
        QObject::tr(session::MarinaActionDisplayName(action)));
    number_row_table_->setItem(row, kColumnAction, action_item);

    auto* modifier_combo = new QComboBox(number_row_table_);
    modifier_combo->addItem(QObject::tr("Ctrl"), MarinaShortcutModifier::MARINA_MOD_CTRL);
    modifier_combo->addItem(QObject::tr("Ctrl+Shift"),
                            MarinaShortcutModifier::MARINA_MOD_CTRL_SHIFT);
    number_row_table_->setCellWidget(row, kColumnModifier, modifier_combo);

    auto* slot_combo = new QComboBox(number_row_table_);
    static const MarinaPhysicalSlot kSlots[] = {
        MarinaPhysicalSlot::MARINA_SLOT_1, MarinaPhysicalSlot::MARINA_SLOT_2,
        MarinaPhysicalSlot::MARINA_SLOT_3, MarinaPhysicalSlot::MARINA_SLOT_4,
        MarinaPhysicalSlot::MARINA_SLOT_5, MarinaPhysicalSlot::MARINA_SLOT_6,
        MarinaPhysicalSlot::MARINA_SLOT_7, MarinaPhysicalSlot::MARINA_SLOT_8,
        MarinaPhysicalSlot::MARINA_SLOT_9, MarinaPhysicalSlot::MARINA_SLOT_0,
        MarinaPhysicalSlot::MARINA_SLOT_GRAVE,
    };
    for (const MarinaPhysicalSlot slot : kSlots) {
      slot_combo->addItem(SlotComboLabel(slot), slot);
    }
    number_row_table_->setCellWidget(row, kColumnSlot, slot_combo);
  }
  number_row_table_->resizeColumnsToContents();
  layout->addWidget(number_row_table_);

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

  const std::vector<MarinaNumberRowBinding> bindings =
      session::GetEffectiveMarinaNumberRowBindings(config);
  for (int row = 0; row < 6; ++row) {
    const MarinaNumberRowAction action = ActionForRow(row);
    MarinaNumberRowBinding binding;
    for (const auto& candidate : bindings) {
      if (candidate.action() == action) {
        binding = candidate;
        break;
      }
    }

    auto* modifier_combo = qobject_cast<QComboBox*>(
        number_row_table_->cellWidget(row, kColumnModifier));
    auto* slot_combo = qobject_cast<QComboBox*>(
        number_row_table_->cellWidget(row, kColumnSlot));
    if (modifier_combo == nullptr || slot_combo == nullptr) {
      continue;
    }

    const MarinaShortcutModifier modifier =
        binding.has_modifier() ? binding.modifier()
                               : MarinaShortcutModifier::MARINA_MOD_CTRL_SHIFT;
    const int mod_index = modifier_combo->findData(modifier);
    if (mod_index >= 0) {
      modifier_combo->setCurrentIndex(mod_index);
    }

    if (binding.has_slot()) {
      const int slot_index = slot_combo->findData(binding.slot());
      if (slot_index >= 0) {
        slot_combo->setCurrentIndex(slot_index);
      }
    }
  }
}

std::vector<MarinaNumberRowBinding>
ConfigDialogShortcutsTab::CollectBindingsFromTable() const {
  std::vector<MarinaNumberRowBinding> bindings;
  bindings.reserve(6);
  for (int row = 0; row < 6; ++row) {
    MarinaNumberRowBinding binding;
    binding.set_action(ActionForRow(row));

    const auto* modifier_combo = qobject_cast<const QComboBox*>(
        number_row_table_->cellWidget(row, kColumnModifier));
    const auto* slot_combo = qobject_cast<const QComboBox*>(
        number_row_table_->cellWidget(row, kColumnSlot));
    if (modifier_combo != nullptr) {
      binding.set_modifier(static_cast<MarinaShortcutModifier>(
          modifier_combo->currentData().toInt()));
    }
    if (slot_combo != nullptr) {
      binding.set_slot(
          static_cast<MarinaPhysicalSlot>(slot_combo->currentData().toInt()));
    }
    bindings.push_back(binding);
  }
  return bindings;
}

bool ConfigDialogShortcutsTab::BindingsMatchDefaults(
    const std::vector<MarinaNumberRowBinding>& bindings) const {
  const auto defaults = session::GetDefaultMarinaNumberRowBindings();
  if (bindings.size() != defaults.size()) {
    return false;
  }
  for (size_t i = 0; i < bindings.size(); ++i) {
    if (bindings[i].action() != defaults[i].action() ||
        bindings[i].modifier() != defaults[i].modifier() ||
        bindings[i].slot() != defaults[i].slot()) {
      return false;
    }
  }
  return true;
}

bool ConfigDialogShortcutsTab::ValidateBeforeApply(
    QString* error_message) const {
  const auto bindings = CollectBindingsFromTable();
  std::string error;
  if (!session::ValidateMarinaNumberRowBindings(bindings, &error)) {
    if (error_message != nullptr) {
      *error_message = QString::fromStdString(error);
    }
    return false;
  }
  return true;
}

void ConfigDialogShortcutsTab::ApplyToConfig(config::Config* config) const {
  config->set_disable_left_shift_direct_toggle(
      disable_left_shift_direct_toggle_->isChecked());

  const auto bindings = CollectBindingsFromTable();
  config->clear_marina_number_row_bindings();
  if (!BindingsMatchDefaults(bindings)) {
    for (const auto& binding : bindings) {
      *config->add_marina_number_row_bindings() = binding;
    }
  }
}

void ConfigDialogShortcutsTab::ConnectApplyButton(const QObject* receiver,
                                                  const char* slot) {
  QObject::connect(disable_left_shift_direct_toggle_, SIGNAL(clicked()), receiver,
                   slot);
  for (int row = 0; row < 6; ++row) {
    if (auto* modifier_combo = qobject_cast<QComboBox*>(
            number_row_table_->cellWidget(row, kColumnModifier))) {
      QObject::connect(modifier_combo, SIGNAL(currentIndexChanged(int)), receiver,
                       slot);
    }
    if (auto* slot_combo = qobject_cast<QComboBox*>(
            number_row_table_->cellWidget(row, kColumnSlot))) {
      QObject::connect(slot_combo, SIGNAL(currentIndexChanged(int)), receiver,
                       slot);
    }
  }
}

void ConfigDialogShortcutsTab::ConnectEditKaeritenButton(
    const QObject* receiver, const char* slot) {
  QObject::connect(edit_kaeriten_button_, SIGNAL(clicked()), receiver, slot);
}

}  // namespace gui
}  // namespace mozc
