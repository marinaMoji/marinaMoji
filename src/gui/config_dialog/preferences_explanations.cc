// Copyright 2026, marinaMoji contributors
// SPDX-License-Identifier: BSD-3-Clause

#include "gui/config_dialog/preferences_explanations.h"

#include "gui/config_dialog/config_dialog_theme.h"

#include <QApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPaintEvent>
#include <QScreen>
#include <QSettings>
#include <QStyle>
#include <QVBoxLayout>

#include <string>
#include <utility>

#include "base/file_util.h"
#include "base/system_util.h"

namespace mozc {
namespace gui {
namespace {

namespace theme = config_dialog_theme;

QString UiSettingsPath() {
  return QString::fromStdString(FileUtil::JoinPath(
      SystemUtil::GetUserProfileDirectory(), "preferences_ui.ini"));
}

std::vector<PreferencesExplanation> BuildCatalog() {
  // Condensed from site/user/pages/04.guide (Settings and related sections).
  // Keep ids stable; titles mirror Preferences labels where practical.
  std::vector<PreferencesExplanation> items;
  auto add = [&items](const char* id, QString title, QString body,
                      QString example = QString(),
                      QString guide = QStringLiteral("section_settings")) {
    items.push_back(PreferencesExplanation{id, std::move(title), std::move(body),
                                           std::move(example),
                                           std::move(guide)});
  };

  add("writing.input_mode",
      QObject::tr("Input mode"),
      QObject::tr(
          "Sets the default writing mode when marinaMoji starts: hiragana, "
          "katakana, half-width katakana, or direct (Latin) input. You can "
          "still change mode from the toolbar or keyboard while typing."),
      QString(), QStringLiteral("section_basic_input"));

  add("writing.punctuation_style",
      QObject::tr("Punctuation style"),
      QObject::tr(
          "Chooses whether period and comma keys produce Japanese "
          "punctuation (、。) or Western forms (，．) while composing."),
      QObject::tr("Japanese  、。\nAlternative  ，．"));

  add("writing.symbol_style",
      QObject::tr("Symbol style"),
      QObject::tr(
          "Controls whether symbols such as brackets and currency signs are "
          "inserted in fullwidth or halfwidth form by default."));

  add("writing.space_style",
      QObject::tr("Space input style"),
      QObject::tr(
          "Decides whether the space bar inserts a fullwidth or halfwidth "
          "space, or follows the current character-width rules."));

  add("writing.auto_halfwidth",
      QObject::tr("Automatically switch to halfwidth"),
      QObject::tr(
          "After certain punctuation, marinaMoji can switch temporarily to "
          "halfwidth input so Latin letters and digits stay compact without "
          "leaving Japanese mode."));

  add("writing.auto_conversion",
      QObject::tr("Convert at punctuations"),
      QObject::tr(
          "When enabled, typing selected punctuation (。、? !) ends the "
          "current phrase and converts it automatically, similar to pressing "
          "Space then Enter on many Japanese IMEs. Use the checkboxes to "
          "choose which marks trigger conversion."));

  add("writing.initial_conversion_mode",
      QObject::tr("Initial conversion mode"),
      QObject::tr(
          "Manual mode waits for Space (or your convert key) before showing "
          "kanji candidates. Live mode offers conversions while you type, "
          "which feels closer to predictive input."));

  add("writing.two_step_enter",
      QObject::tr("Two-step Enter after conversion"),
      QObject::tr(
          "When enabled, the first Enter confirms the conversion candidate "
          "and the second Enter commits the text. Useful if you often refine "
          "candidates before inserting them. This option is experimental."));

  add("writing.kanji_style",
      QObject::tr("Initial kanji style"),
      QObject::tr(
          "Shin (modern) forms are everyday shinjitai. Kyū (traditional) "
          "converts dictionary results toward kyūjitai using OpenCC and "
          "marinaMoji’s custom tables—helpful for historical texts. You can "
          "still toggle shin/kyū from the toolbar while writing."),
      QObject::tr("Shin  国 学\nKyū   國 學"),
      QStringLiteral("section_kyujitai"));

  add("writing.width_rules",
      QObject::tr("Detailed width rules"),
      QObject::tr(
          "For each character group (letters, digits, punctuation, and so "
          "on), choose whether composition and conversion should prefer "
          "fullwidth or halfwidth forms. This table is the fine-grained "
          "companion to the simpler spacing and symbol settings above."));

  add("dictionaries.learning",
      QObject::tr("Learning / personalization"),
      QObject::tr(
          "Controls how strongly past conversions influence new suggestions. "
          "Clear personalization data removes that learned history; it does "
          "not delete your user dictionary entries."),
      QString(), QStringLiteral("section_dictionary_history"));

  add("dictionaries.personal",
      QObject::tr("Personal dictionary"),
      QObject::tr(
          "Your own readings and spellings—names, archaic forms, specialist "
          "terms. Edit user symbols maintains short strings that appear on "
          "the User tab of the toolbar Symbols palette."),
      QString(), QStringLiteral("section_dictionary_history"));

  add("dictionaries.usage",
      QObject::tr("Dictionary sources"),
      QObject::tr(
          "Usage and related dictionaries adjust how frequent phrases are "
          "ranked and how similar readings are disambiguated."));

  add("dictionaries.scholarly",
      QObject::tr("Scholarly vocabulary"),
      QObject::tr(
          "Optional dictionary packs add specialised vocabulary (for "
          "example historical or domain-specific terms) without changing "
          "everyday conversion when left disabled."));

  add("dictionaries.special",
      QObject::tr("Special conversions"),
      QObject::tr(
          "Fine-tunes conversions such as date/time and symbol or emoji "
          "expansion. Disable entries that interrupt scholarly typing."));

  add("keyboard.keymap",
      QObject::tr("Keymap"),
      QObject::tr(
          "Chooses a familiar shortcut layout (for example MS-IME or "
          "Kotoeri). Customize… remaps individual keys; the romaji table "
          "includes marinaMoji’s historical-kana entries."),
      QString(), QStringLiteral("section_historical_kana"));

  add("keyboard.shift_mode_switch",
      QObject::tr("Shift key mode switch"),
      QObject::tr(
          "Configures how Shift interacts with Japanese ↔ direct input and "
          "related mode changes. Shortcuts preferences can also disable the "
          "Left Shift toggle or use double-tap Left Shift to lock a mode."),
      QString(), QStringLiteral("section_settings"));

  add("keyboard.left_shift_toggle",
      QObject::tr("Left Shift mode toggle"),
      QObject::tr(
          "When enabled (default), press and release Left Shift alone to "
          "switch between hiragana and direct input only (Manyōshū, "
          "half-width, and other modes are unaffected). From direct input, "
          "Left Shift returns to the hiragana or full-katakana mode you were "
          "using before. Double-tap Left Shift to lock or unlock the current "
          "mode; on Windows, Ctrl+Alt+Right Shift does the same. Right Shift "
          "alone toggles hiragana and Manyōshū, except in Direct input, where "
          "it arms the macron dead key (tap Right Shift, then a vowel). Shift "
          "still works for capitals and shortcuts while locked."),
      QObject::tr(
          "Left Shift: Japanese ↔ direct input. Right Shift: hiragana ↔ "
          "Manyōshū, or macron dead key in direct input. Double-tap Left "
          "Shift locks the current mode."),
      QStringLiteral("section_settings"));

  add("keyboard.mode_indicator",
      QObject::tr("Mode indicator"),
      QObject::tr(
          "Shows a small on-screen cue for the current input mode near the "
          "cursor, which helps when the toolbar is hidden."));

  add("keyboard.kaeriten",
      QObject::tr("Kaeriten shortcuts"),
      QObject::tr(
          "In direct input, type semicolon then a key (for example ;r → ㆑). "
          "Edit the table to change the key portion; the leading semicolon is "
          "required. Marks not in the default set (for example linking mark "
          "㆐) can be added with New entry in the editor. Custom entries are "
          "stored with your other preferences."),
      QObject::tr(";r → ㆑"), QStringLiteral("section_kaeriten"));

  add("suggestions.history",
      QObject::tr("Use input history"),
      QObject::tr(
          "Offers candidates based on what you have converted before. Turn "
          "this off, or use Secret mode, when you do not want local history "
          "to influence suggestions."),
      QString(), QStringLiteral("section_dictionary_history"));

  add("suggestions.system_dictionary",
      QObject::tr("Use system dictionary"),
      QObject::tr(
          "Includes candidates from the main system lexicon while you type, "
          "before a full Space conversion."));

  add("suggestions.realtime",
      QObject::tr("Use realtime conversion"),
      QObject::tr(
          "Shows kanji guesses in the pre-edit line as you type romaji, "
          "before you press Space for a full conversion."));

  add("privacy.secret_mode",
      QObject::tr("Secret mode"),
      QObject::tr(
          "Temporarily disables personalization, history-based suggestions, "
          "and the user dictionary—useful on shared machines or sensitive "
          "drafts. It also skips exporting history during synchronisation."),
      QString(), QStringLiteral("section_dictionary_history"));

  add("privacy.presentation_mode",
      QObject::tr("Presentation mode"),
      QObject::tr(
          "Turns off all suggestions for demos or screen sharing so "
          "candidates do not appear unexpectedly."));

  add("privacy.sync",
      QObject::tr("Synchronisation"),
      QObject::tr(
          "Encrypted sync shares dictionary and/or learning history through "
          "a file you control (for example in Nextcloud). Generate or enter "
          "a sync key on each device; marinaMoji stores the key only locally."),
      QString(), QStringLiteral("section_sync"));

  return items;
}

const std::vector<PreferencesExplanation>& Catalog() {
  static const std::vector<PreferencesExplanation> kCatalog = BuildCatalog();
  return kCatalog;
}

}  // namespace

const PreferencesExplanation* FindPreferencesExplanation(const char* id) {
  if (id == nullptr || id[0] == '\0') {
    return nullptr;
  }
  for (const PreferencesExplanation& item : Catalog()) {
    if (item.id != nullptr && qstrcmp(item.id, id) == 0) {
      return &item;
    }
  }
  return nullptr;
}

const std::vector<PreferencesExplanation>& AllPreferencesExplanations() {
  return Catalog();
}

bool LoadShowPreferencesExplanations() {
  QSettings settings(UiSettingsPath(), QSettings::IniFormat);
  return settings.value(QStringLiteral("show_explanations"), false).toBool();
}

void SaveShowPreferencesExplanations(bool enabled) {
  QSettings settings(UiSettingsPath(), QSettings::IniFormat);
  settings.setValue(QStringLiteral("show_explanations"), enabled);
  settings.sync();
}

PreferencesInfoButton::PreferencesInfoButton(const char* explanation_id,
                                             QWidget* parent)
    : QToolButton(parent), explanation_id_(explanation_id) {
  setObjectName(QStringLiteral("preferencesInfoButton"));
  setCursor(Qt::PointingHandCursor);
  setFocusPolicy(Qt::StrongFocus);
  setAutoRaise(true);
  setFixedSize(18, 18);
  setToolTip(QObject::tr("About this setting"));
  setAccessibleName(QObject::tr("About this setting"));
}

void PreferencesInfoButton::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  const bool dark = theme::IsDark();
  const theme::Palette palette = theme::Colors(dark);
  const QColor ink = hasFocus() ? palette.focus : palette.accent;
  const QRectF circle = QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5);
  painter.setPen(QPen(ink, 1.2));
  painter.setBrush(Qt::NoBrush);
  painter.drawEllipse(circle);
  QFont font = painter.font();
  font.setFamilies({QStringLiteral("Georgia"), QStringLiteral("Times New Roman"),
                    QStringLiteral("serif")});
  font.setPointSizeF(theme::PlatformPointSize(10));
  font.setWeight(QFont::DemiBold);
  painter.setFont(font);
  painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("i"));
}

PreferencesExplanationPopover::PreferencesExplanationPopover(QWidget* parent)
    : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint) {
  setObjectName(QStringLiteral("preferencesExplanationPopover"));
  setAttribute(Qt::WA_ShowWithoutActivating, false);
  setFocusPolicy(Qt::StrongFocus);
  setMinimumWidth(300);
  setMaximumWidth(360);

  layout_ = new QVBoxLayout(this);
  layout_->setContentsMargins(16, 14, 16, 14);
  layout_->setSpacing(8);

  title_ = new QLabel(this);
  title_->setObjectName(QStringLiteral("preferencesExplanationTitle"));
  title_->setWordWrap(true);
  title_->setFont(theme::UiFont(theme::Type::section, QFont::Medium));

  body_ = new QLabel(this);
  body_->setObjectName(QStringLiteral("preferencesExplanationBody"));
  body_->setWordWrap(true);
  body_->setFont(theme::UiFont(theme::Type::label));
  body_->setTextInteractionFlags(Qt::TextSelectableByMouse);

  example_ = new QLabel(this);
  example_->setObjectName(QStringLiteral("preferencesExplanationExample"));
  example_->setWordWrap(true);
  example_->setFont(theme::UiFont(theme::Type::label));
  example_->setTextInteractionFlags(Qt::TextSelectableByMouse);

  layout_->addWidget(title_);
  layout_->addWidget(body_);
  layout_->addWidget(example_);

  qApp->installEventFilter(this);
}

void PreferencesExplanationPopover::ShowFor(
    const PreferencesExplanation& explanation, const QRect& global_anchor) {
  title_->setText(explanation.title);
  body_->setText(explanation.body);
  const bool has_example = !explanation.example.isEmpty();
  example_->setVisible(has_example);
  if (has_example) {
    example_->setText(explanation.example);
  }
  adjustSize();

  QPoint pos(global_anchor.right() + 8,
             global_anchor.center().y() - height() / 2);
  if (QScreen* screen = QGuiApplication::screenAt(global_anchor.center())) {
    const QRect avail = screen->availableGeometry();
    if (pos.x() + width() > avail.right() - 8) {
      pos.setX(global_anchor.left() - width() - 8);
    }
    if (pos.x() < avail.left() + 8) {
      pos.setX(avail.left() + 8);
    }
    if (pos.y() + height() > avail.bottom() - 8) {
      pos.setY(avail.bottom() - height() - 8);
    }
    if (pos.y() < avail.top() + 8) {
      pos.setY(avail.top() + 8);
    }
  }
  move(pos);
  show();
  raise();
  setFocus(Qt::PopupFocusReason);
}

void PreferencesExplanationPopover::Dismiss() { hide(); }

void PreferencesExplanationPopover::keyPressEvent(QKeyEvent* event) {
  if (event->key() == Qt::Key_Escape) {
    Dismiss();
    event->accept();
    return;
  }
  QFrame::keyPressEvent(event);
}

bool PreferencesExplanationPopover::eventFilter(QObject* watched,
                                                QEvent* event) {
  if (isVisible() && event->type() == QEvent::MouseButtonPress) {
    if (!rect().contains(mapFromGlobal(QCursor::pos()))) {
      Dismiss();
    }
  }
  return QFrame::eventFilter(watched, event);
}

PreferencesExplanationController::PreferencesExplanationController(
    QWidget* dialog)
    : QObject(dialog), dialog_(dialog) {
  enabled_ = LoadShowPreferencesExplanations();
  popover_ = new PreferencesExplanationPopover(dialog_);
}

PreferencesExplanationController::~PreferencesExplanationController() =
    default;

void PreferencesExplanationController::SetEnabled(bool enabled) {
  if (enabled_ == enabled) {
    return;
  }
  enabled_ = enabled;
  SaveShowPreferencesExplanations(enabled_);
  for (PreferencesInfoButton* button : buttons_) {
    if (button != nullptr) {
      button->setVisible(enabled_);
    }
  }
  if (!enabled_) {
    ClosePopover();
  }
  emit enabledChanged(enabled_);
}

void PreferencesExplanationController::ToggleEnabled() {
  SetEnabled(!enabled_);
}

QWidget* PreferencesExplanationController::MakeInfoSlot(
    const char* explanation_id, QWidget* parent) {
  auto* slot = new QWidget(parent);
  slot->setObjectName(QStringLiteral("preferencesInfoSlot"));
  slot->setFixedWidth(18);
  slot->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
  auto* layout = new QHBoxLayout(slot);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  if (FindPreferencesExplanation(explanation_id) == nullptr) {
    return slot;
  }
  auto* button = new PreferencesInfoButton(explanation_id, slot);
  button->setVisible(enabled_);
  layout->addWidget(button, 0, Qt::AlignCenter);
  RegisterInfoButton(button);
  return slot;
}

void PreferencesExplanationController::RegisterInfoButton(
    PreferencesInfoButton* button) {
  if (button == nullptr) {
    return;
  }
  buttons_.push_back(button);
  connect(button, &QToolButton::clicked, this,
          &PreferencesExplanationController::OnInfoClicked);
}

void PreferencesExplanationController::ClosePopover() {
  if (popover_ != nullptr) {
    popover_->Dismiss();
  }
}

void PreferencesExplanationController::OnPreferencesPageChanged() {
  ClosePopover();
}

void PreferencesExplanationController::OnInfoClicked() {
  auto* button = qobject_cast<PreferencesInfoButton*>(sender());
  if (button == nullptr || popover_ == nullptr) {
    return;
  }
  const PreferencesExplanation* explanation =
      FindPreferencesExplanation(button->explanation_id());
  if (explanation == nullptr) {
    return;
  }
  const QRect anchor(button->mapToGlobal(QPoint(0, 0)), button->size());
  popover_->ShowFor(*explanation, anchor);
}

}  // namespace gui
}  // namespace mozc
