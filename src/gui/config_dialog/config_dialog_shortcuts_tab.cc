#include "gui/config_dialog/config_dialog_shortcuts_tab.h"

#include "gui/config_dialog/config_dialog_theme.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QFontMetrics>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QTabWidget>
#include <QVBoxLayout>

#include <string>

#include "session/marina_number_row_bindings_util.h"

#ifdef _WIN32
#include "base/process.h"
#include "win32/base/sticky_keys_util.h"
#endif  // _WIN32

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
      MarinaNumberRowAction::MARINA_NR_LIVE_CONVERSION,
      MarinaNumberRowAction::MARINA_NR_WORD_REGISTER,
  };
  if (row < 0 || row >= 7) {
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

QString ActionLabel(MarinaNumberRowAction action) {
  switch (action) {
    case MarinaNumberRowAction::MARINA_NR_ODORIJI_DEFAULT:
      return QObject::tr("Insert default odoriji");
    case MarinaNumberRowAction::MARINA_NR_ODORIJI_PALETTE:
      return QObject::tr("Odoriji palette");
    case MarinaNumberRowAction::MARINA_NR_TRADITIONAL_KANJI:
      return QObject::tr("Shin / kyū kanji");
    case MarinaNumberRowAction::MARINA_NR_MANYOSHU_HIRAGANA:
      return QObject::tr("Hiragana ↔ Manyōshū");
    case MarinaNumberRowAction::MARINA_NR_HIRAGANA_DIRECT:
      return QObject::tr("Japanese ↔ direct input");
    case MarinaNumberRowAction::MARINA_NR_LIVE_CONVERSION:
      return QObject::tr("Live conversion");
    case MarinaNumberRowAction::MARINA_NR_WORD_REGISTER:
      return QObject::tr("Dictionary entry");
    default:
      return QString();
  }
}

constexpr MarinaPhysicalSlot kAssignableSlots[] = {
    MarinaPhysicalSlot::MARINA_SLOT_1, MarinaPhysicalSlot::MARINA_SLOT_2,
    MarinaPhysicalSlot::MARINA_SLOT_3, MarinaPhysicalSlot::MARINA_SLOT_4,
    MarinaPhysicalSlot::MARINA_SLOT_5, MarinaPhysicalSlot::MARINA_SLOT_6,
    MarinaPhysicalSlot::MARINA_SLOT_7, MarinaPhysicalSlot::MARINA_SLOT_8,
    MarinaPhysicalSlot::MARINA_SLOT_9, MarinaPhysicalSlot::MARINA_SLOT_0,
    MarinaPhysicalSlot::MARINA_SLOT_GRAVE,
};

void PopulateSlotCombo(QComboBox* slot_combo, MarinaShortcutModifier modifier,
                       MarinaPhysicalSlot preferred_slot) {
  if (slot_combo == nullptr) {
    return;
  }
  const bool signals_blocked = slot_combo->blockSignals(true);
  slot_combo->clear();
  for (const MarinaPhysicalSlot slot : kAssignableSlots) {
    if (session::IsMarinaNumberRowChordBlocked(modifier, slot)) {
      continue;
    }
    slot_combo->addItem(SlotComboLabel(slot), slot);
  }
  int index = slot_combo->findData(preferred_slot);
  if (index < 0) {
    index = 0;
  }
  if (slot_combo->count() > 0) {
    slot_combo->setCurrentIndex(index);
  }
  slot_combo->blockSignals(signals_blocked);
}

QFrame* MakeSectionHeader(const QString& title, QWidget* parent) {
  auto* header = new QFrame(parent);
  header->setProperty("class", QStringLiteral("setting-group-header"));
  header->setFrameShape(QFrame::NoFrame);
  header->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
  auto* layout = new QHBoxLayout(header);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(config_dialog_theme::Spacing::heading_rule_gap);
  auto* label = new QLabel(title, header);
  label->setFont(config_dialog_theme::SectionFont());
  config_dialog_theme::EnsureHeadingMetrics(label);
  layout->addWidget(label, 0, Qt::AlignLeft | Qt::AlignVCenter);
  auto* line = new QFrame(header);
  line->setProperty("class", QStringLiteral("setting-group-line"));
  line->setFrameShape(QFrame::HLine);
  line->setFixedHeight(1);
  layout->addWidget(line, 1, Qt::AlignVCenter);
  return header;
}

QLabel* MakeSecondaryLabel(const QString& text, QWidget* parent) {
  auto* label = new QLabel(text, parent);
  label->setObjectName(QStringLiteral("preferencesSecondary"));
  label->setFont(config_dialog_theme::UiFont(
      config_dialog_theme::Type::secondary));
  label->setWordWrap(true);
  label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  return label;
}

QPushButton* MakeDisclosure(const QString& closed_text, QWidget* parent) {
  auto* button = new QPushButton(closed_text, parent);
  button->setProperty("role", QStringLiteral("disclosure"));
  button->setCheckable(true);
  button->setFlat(true);
  button->setCursor(Qt::PointingHandCursor);
  button->setFocusPolicy(Qt::StrongFocus);
  return button;
}

}  // namespace

ConfigDialogShortcutsTab::ConfigDialogShortcutsTab(QWidget* parent)
    : tab_(new QWidget(parent)) {
  auto* layout = new QVBoxLayout(tab_);
  layout->setContentsMargins(0, 4, 8, 12);
  layout->setSpacing(10);

  // --- Mode switching ---
  // Mode-switching help lives in the Preferences ⓘ popover
  // (keyboard.left_shift_toggle), attached after this page is embedded.
  layout->addWidget(MakeSectionHeader(QObject::tr("Mode switching"), tab_));

  disable_left_shift_direct_toggle_ = new QCheckBox(
      QObject::tr("Disable Left Shift toggle (Japanese ↔ Direct input)"), tab_);
  layout->addWidget(disable_left_shift_direct_toggle_);

#ifdef _WIN32
  sticky_keys_warning_ = MakeSecondaryLabel(
      QObject::tr(
          "Windows StickyKeys is currently on. It will interfere with the "
          "Left/Right Shift shortcuts above (a tap latches Shift for the "
          "next key). If you don't need it, turn it off in Ease of Access "
          "settings."),
      tab_);
  sticky_keys_warning_->setObjectName(QStringLiteral("preferencesWarning"));
  layout->addWidget(sticky_keys_warning_);

  open_sticky_keys_settings_button_ = new QPushButton(
      QObject::tr("Open Ease of Access keyboard settings..."), tab_);
  open_sticky_keys_settings_button_->setProperty(
      "role", QStringLiteral("secondary"));
  open_sticky_keys_settings_button_->setSizePolicy(QSizePolicy::Maximum,
                                                   QSizePolicy::Fixed);
  layout->addWidget(open_sticky_keys_settings_button_, 0, Qt::AlignLeft);
  QObject::connect(open_sticky_keys_settings_button_, &QPushButton::clicked,
                   [] {
                     Process::OpenBrowser("ms-settings:easeofaccess-keyboard");
                   });

  const bool sticky_keys_on = win32::StickyKeysUtil::IsCurrentlyOn();
  sticky_keys_warning_->setVisible(sticky_keys_on);
  open_sticky_keys_settings_button_->setVisible(sticky_keys_on);
#else   // _WIN32
  sticky_keys_warning_ = nullptr;
  open_sticky_keys_settings_button_ = nullptr;
#endif  // _WIN32

  // --- Custom shortcuts ---
  layout->addWidget(MakeSectionHeader(QObject::tr("Custom shortcuts"), tab_));

  number_row_help_ = MakeSecondaryLabel(
      QObject::tr(
          "Assign marina actions to physical number-row keys. Shortcuts follow "
          "key position, not the printed letter, so they work on Dvorak, "
          "AZERTY, and other layouts."),
      tab_);
  layout->addWidget(number_row_help_);

#if defined(__APPLE__)
  mac_number_row_note_ = MakeSecondaryLabel(
      QObject::tr(
          "Custom number-row bindings are saved here and apply on Linux. "
          "macOS still uses the default bindings until a future update."),
      tab_);
  layout->addWidget(mac_number_row_note_);
#else
  mac_number_row_note_ = nullptr;
#endif

#ifdef _WIN32
  win_number_row_note_ = MakeSecondaryLabel(
      QObject::tr(
          "Dictionary entry defaults to Ctrl+Shift+9 on Windows. Ctrl+Shift+0 "
          "is not available here: Windows often swallows it for "
          "input-language switching, even when that hotkey looks unassigned."),
      tab_);
  layout->addWidget(win_number_row_note_);
#else   // !_WIN32
  win_number_row_note_ = nullptr;
#endif  // _WIN32

  auto* shortcuts_details = MakeSecondaryLabel(
      QObject::tr(
          "Each row maps one action to a modifier and a physical key. Conflicts "
          "are checked automatically when you change a binding. Empty or "
          "duplicate chords are rejected."),
      tab_);
  shortcuts_details->setVisible(false);
  layout->addWidget(shortcuts_details);
  auto* shortcuts_disclosure =
      MakeDisclosure(QObject::tr("How custom shortcuts work"), tab_);
  layout->addWidget(shortcuts_disclosure, 0, Qt::AlignLeft);
  QObject::connect(shortcuts_disclosure, &QPushButton::toggled, tab_,
                   [shortcuts_details, shortcuts_disclosure](bool open) {
                     shortcuts_details->setVisible(open);
                     shortcuts_disclosure->setText(
                         open ? QObject::tr("Hide custom shortcut details")
                              : QObject::tr("How custom shortcuts work"));
                   });

  number_row_table_ = new QTableWidget(7, 3, tab_);
  number_row_table_->setHorizontalHeaderLabels(
      {QObject::tr("Action"), QObject::tr("Modifier"), QObject::tr("Key")});
  number_row_table_->verticalHeader()->setVisible(false);
  number_row_table_->setSelectionMode(QAbstractItemView::NoSelection);
  number_row_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  number_row_table_->setShowGrid(false);
  number_row_table_->setAlternatingRowColors(false);
  number_row_table_->setFocusPolicy(Qt::StrongFocus);
  for (int row = 0; row < 7; ++row) {
    const MarinaNumberRowAction action = ActionForRow(row);
    auto* action_item = new QTableWidgetItem(ActionLabel(action));
    number_row_table_->setItem(row, kColumnAction, action_item);

    auto* modifier_combo = new QComboBox(number_row_table_);
    modifier_combo->addItem(QObject::tr("Ctrl"),
                            MarinaShortcutModifier::MARINA_MOD_CTRL);
    modifier_combo->addItem(QObject::tr("Ctrl+Shift"),
                            MarinaShortcutModifier::MARINA_MOD_CTRL_SHIFT);
    number_row_table_->setCellWidget(row, kColumnModifier, modifier_combo);

    auto* slot_combo = new QComboBox(number_row_table_);
    PopulateSlotCombo(slot_combo, MarinaShortcutModifier::MARINA_MOD_CTRL_SHIFT,
                      MarinaPhysicalSlot::MARINA_SLOT_1);
    number_row_table_->setCellWidget(row, kColumnSlot, slot_combo);

    QObject::connect(
        modifier_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        slot_combo, [modifier_combo, slot_combo](int) {
          const MarinaShortcutModifier modifier =
              static_cast<MarinaShortcutModifier>(
                  modifier_combo->currentData().toInt());
          const MarinaPhysicalSlot preferred =
              slot_combo->count() > 0
                  ? static_cast<MarinaPhysicalSlot>(
                        slot_combo->currentData().toInt())
                  : MarinaPhysicalSlot::MARINA_SLOT_1;
          PopulateSlotCombo(slot_combo, modifier, preferred);
        });
  }
  const int row_h = QFontMetrics(number_row_table_->font()).height() + 18;
  number_row_table_->verticalHeader()->setDefaultSectionSize(row_h);
  number_row_table_->horizontalHeader()->setSectionResizeMode(
      0, QHeaderView::Stretch);
  number_row_table_->horizontalHeader()->setSectionResizeMode(
      1, QHeaderView::ResizeToContents);
  number_row_table_->horizontalHeader()->setSectionResizeMode(
      2, QHeaderView::ResizeToContents);
  // Fixed height so a parent stretch can never collapse the table under the
  // Kaeriten section (which previously painted on top of the rows).
  number_row_table_->setFixedHeight(row_h * 7 + 36);
  number_row_table_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  layout->addWidget(number_row_table_);

  // --- Kaeriten ---
  // Help lives in the Preferences ⓘ popover (keyboard.kaeriten), attached
  // after this page is embedded.
  layout->addWidget(MakeSectionHeader(QObject::tr("Kaeriten shortcuts"), tab_));

  edit_kaeriten_button_ =
      new QPushButton(QObject::tr("Edit kaeriten shortcuts..."), tab_);
  edit_kaeriten_button_->setProperty("role", QStringLiteral("secondary"));
  edit_kaeriten_button_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  layout->addWidget(edit_kaeriten_button_, 0, Qt::AlignLeft);
}

void ConfigDialogShortcutsTab::AddToTabWidget(QTabWidget* tab_widget) {
  tab_widget->addTab(tab_, QObject::tr("Shortcuts"));
}

void ConfigDialogShortcutsTab::LoadFromConfig(const config::Config& config) {
  disable_left_shift_direct_toggle_->setChecked(
      config.disable_left_shift_direct_toggle());

  const std::vector<MarinaNumberRowBinding> bindings =
      session::GetEffectiveMarinaNumberRowBindings(config);
  for (int row = 0; row < 7; ++row) {
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

    const MarinaPhysicalSlot preferred_slot =
        binding.has_slot() ? binding.slot() : MarinaPhysicalSlot::MARINA_SLOT_1;
    PopulateSlotCombo(slot_combo, modifier, preferred_slot);
  }
}

std::vector<MarinaNumberRowBinding>
ConfigDialogShortcutsTab::CollectBindingsFromTable() const {
  std::vector<MarinaNumberRowBinding> bindings;
  bindings.reserve(7);
  for (int row = 0; row < 7; ++row) {
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
  QObject::connect(disable_left_shift_direct_toggle_, SIGNAL(clicked()),
                   receiver, slot);
  for (int row = 0; row < 7; ++row) {
    if (auto* modifier_combo = qobject_cast<QComboBox*>(
            number_row_table_->cellWidget(row, kColumnModifier))) {
      QObject::connect(modifier_combo, SIGNAL(currentIndexChanged(int)),
                       receiver, slot);
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
