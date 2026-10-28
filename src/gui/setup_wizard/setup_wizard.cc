// Copyright 2010-2021, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "gui/setup_wizard/setup_wizard.h"

#include <algorithm>

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QScreen>
#include <QScrollArea>
#include <QShowEvent>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWindow>

#include "absl/log/log.h"
#include "client/client.h"
#include "config/config_handler.h"
#include "gui/base/util.h"
#include "gui/setup_wizard/setup_wizard_harbour.h"
#include "gui/setup_wizard/setup_wizard_mark.h"
#include "gui/setup_wizard/setup_wizard_style.h"
#include "gui/setup_wizard/setup_wizard_theme.h"

#ifdef __APPLE__
#include "base/mac/mac_util.h"
#include "gui/setup_wizard/setup_wizard_mac.h"
#endif  // __APPLE__

namespace mozc {
namespace gui {
namespace {

QPushButton* MakeButton(const QString& text, const QString& name,
                        QWidget* parent) {
  auto* button = new QPushButton(text, parent);
  button->setObjectName(name);
  button->setFont(setup_theme::BodyFont());
  button->setCursor(Qt::PointingHandCursor);
  button->setFocusPolicy(Qt::StrongFocus);
  return button;
}

}  // namespace

class Keycap : public QWidget {
 public:
  explicit Keycap(QWidget* parent) : QWidget(parent) {
    setFixedSize(30, 28);
    setAccessibleName(QStringLiteral("Shift"));
  }

 protected:
  void paintEvent(QPaintEvent* /*event*/) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(setup_theme::kMuted);
    pen.setWidthF(1.0);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 4.0,
                            4.0);
    pen.setColor(setup_theme::kInk);
    pen.setWidthF(1.4);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    const QPointF c = QRectF(rect()).center();
    painter.drawLine(QPointF(c.x(), c.y() - 6), QPointF(c.x() - 5, c.y() - 1));
    painter.drawLine(QPointF(c.x(), c.y() - 6), QPointF(c.x() + 5, c.y() - 1));
    painter.drawLine(QPointF(c.x(), c.y() - 5), QPointF(c.x(), c.y() + 6));
  }
};

SetupWizard::SetupWizard() : client_(client::ClientFactory::NewClient()) {
  control_style_ = new SetupWizardStyle(nullptr);
  control_style_->setParent(this);
  setWindowTitle(tr("%1 setup").arg(GuiUtil::ProductName()));
  setAccessibleName(windowTitle());
  setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
  setAttribute(Qt::WA_TranslucentBackground);
  setAutoFillBackground(false);
  setFixedSize(setup_theme::kWindowWidth, setup_theme::kWindowHeight);
  setStyleSheet(setup_theme::Stylesheet());

  QPalette palette = this->palette();
  palette.setColor(QPalette::Window, setup_theme::kIvory);
  palette.setColor(QPalette::WindowText, setup_theme::kInk);
  palette.setColor(QPalette::Text, setup_theme::kInk);
  palette.setColor(QPalette::ButtonText, setup_theme::kInk);
  palette.setColor(QPalette::Base, setup_theme::kIvory);
  palette.setColor(QPalette::Highlight, setup_theme::kPrussian);
  palette.setColor(QPalette::HighlightedText, setup_theme::kIvory);
  setPalette(palette);

  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(32, 16, 32, 20);
  root->setSpacing(10);

  auto* chrome = new QHBoxLayout();
  chrome->setSpacing(8);
  close_button_ = MakeButton(QStringLiteral(u"\u00D7"),
                             QStringLiteral("closeButton"), this);
  close_button_->setAccessibleName(tr("Close"));
  close_button_->setFixedSize(28, 28);
  close_button_->setFont(setup_theme::FontAt(18));
  chrome_mark_ = new MarkView(QStringLiteral(":/wordmark.png"), 19, false, this);
  chrome_mark_->setAccessibleName(GuiUtil::ProductName());
  progress_ = new QLabel(this);
  progress_->setObjectName(QStringLiteral("muted"));
  progress_->setFont(setup_theme::ProgressFont());
  progress_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  chrome->addWidget(close_button_);
  chrome->addWidget(chrome_mark_);
  chrome->addStretch(1);
  chrome->addWidget(progress_);
  root->addLayout(chrome);

  chrome_line_ = new QFrame(this);
  chrome_line_->setObjectName(QStringLiteral("line"));
  chrome_line_->setFrameShape(QFrame::NoFrame);
  chrome_line_->setFixedHeight(1);
  root->addWidget(chrome_line_);

  stack_ = new QStackedWidget(this);
  stack_->setAttribute(Qt::WA_TranslucentBackground);
  root->addWidget(stack_, 1);

  auto* nav = new QHBoxLayout();
  back_ = MakeButton(tr("\u2190  Back"), QStringLiteral("quiet"), this);
  next_ = MakeButton(tr("Get started  \u2192"), QStringLiteral("primary"), this);
  next_->setDefault(true);
  nav->addWidget(back_);
  nav->addStretch(1);
  nav->addWidget(next_);
  root->addLayout(nav);

  connect(close_button_, &QPushButton::clicked, this, &QDialog::reject);
  connect(back_, &QPushButton::clicked, this, &SetupWizard::OnBack);
  connect(next_, &QPushButton::clicked, this, &SetupWizard::OnNext);

  stack_->addWidget(MakeWelcomePage());
  stack_->addWidget(MakeKeyboardPage());
  stack_->addWidget(MakeConversionPage());
  stack_->addWidget(MakePreferencesPage());
  stack_->addWidget(MakeReadyPage());

  config::Config config;
  config_loaded_ = GetConfig(&config);
  if (!config_loaded_) {
    config = config::ConfigHandler::DefaultConfig();
  }
  LoadChoices(config, config_loaded_);
  ShowPage(kWelcome);
}

SetupWizard::~SetupWizard() = default;

void SetupWizard::paintEvent(QPaintEvent* /*event*/) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setPen(Qt::NoPen);
  painter.setBrush(setup_theme::kIvory);
  painter.drawRoundedRect(rect(), setup_theme::kCornerRadius,
                          setup_theme::kCornerRadius);
}

void SetupWizard::showEvent(QShowEvent* event) {
  QDialog::showEvent(event);
  if (!sized_) {
    QScreen* screen = this->screen();
    const QRect available =
        screen != nullptr ? screen->availableGeometry() : QRect(0, 0, 1440, 900);
    const int width =
        std::min(setup_theme::kWindowWidth, std::max(640, available.width() - 48));
    const int height = std::min(setup_theme::kWindowHeight,
                                std::max(480, available.height() - 48));
    setFixedSize(width, height);
    sized_ = true;
  }
#ifdef __APPLE__
  ApplySetupWizardMacShadow(reinterpret_cast<void*>(winId()));
#endif  // __APPLE__
}

bool SetupWizard::CanDragFrom(QWidget* widget) const {
  for (QWidget* cursor = widget; cursor != nullptr && cursor != this;
       cursor = cursor->parentWidget()) {
    if (qobject_cast<QAbstractButton*>(cursor) != nullptr ||
        qobject_cast<QComboBox*>(cursor) != nullptr) {
      return false;
    }
  }
  return true;
}

void SetupWizard::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton &&
      CanDragFrom(childAt(event->position().toPoint()))) {
    if (windowHandle() != nullptr && windowHandle()->startSystemMove()) {
      event->accept();
      return;
    }
  }
  QDialog::mousePressEvent(event);
}

bool SetupWizard::GetConfig(config::Config* config) {
  if (!client_->CheckVersionOrRestartServer()) {
    LOG(ERROR) << "CheckVersionOrRestartServer failed";
    return false;
  }
  if (!client_->GetConfig(config)) {
    LOG(ERROR) << "GetConfig failed";
    return false;
  }
  return true;
}

bool SetupWizard::SetConfig(const config::Config& config) {
  if (!client_->CheckVersionOrRestartServer()) {
    LOG(ERROR) << "CheckVersionOrRestartServer failed";
    return false;
  }
  if (!client_->SetConfig(config)) {
    LOG(ERROR) << "SetConfig failed";
    return false;
  }
  return true;
}

void SetupWizard::LoadChoices(const config::Config& config, bool from_server) {
#ifdef __APPLE__
  const bool qwerty = from_server && config.use_japanese_layout();
  qwerty_layout_->setChecked(qwerty);
  follow_layout_->setChecked(!qwerty);
  // A fresh preview starts the login item on. Reopening a live install shows
  // whether that login item is already registered.
  if (from_server) {
    login_checkbox_->setChecked(MacUtil::CheckPrelauncherLoginItemStatus());
  } else {
    login_checkbox_->setChecked(true);
  }
#endif  // __APPLE__
#ifdef _WIN32
  const int layout = static_cast<int>(config.marina_keyboard_layout());
  if (layout >= 0 && layout < layout_combo_->count()) {
    layout_combo_->setCurrentIndex(layout);
  } else {
    layout_combo_->setCurrentIndex(0);
  }
#endif  // _WIN32

  int keymap_choice = kKeymapUnknown;
  const config::Config::SessionKeymap keymap = config.session_keymap();
  const config::Config::SessionKeymap fallback =
      config::ConfigHandler::GetDefaultKeyMap();
  if (keymap == config::Config::CUSTOM) {
    keymap_combo_->addItem(tr("Custom (leave unchanged)"), kKeymapKeepCustom);
    keymap_choice = kKeymapKeepCustom;
  } else if (keymap == config::Config::ATOK ||
             keymap == config::Config::MSIME ||
             keymap == config::Config::KOTOERI) {
    if (keymap == fallback) {
      keymap_choice = kKeymapUnknown;
    } else {
      keymap_choice = static_cast<int>(keymap);
    }
  }
  const int keymap_index = keymap_combo_->findData(keymap_choice);
  keymap_combo_->setCurrentIndex(keymap_index >= 0 ? keymap_index : 0);

  const bool live = from_server && config.use_live_conversion();
  live_on_->setChecked(live);
  live_off_->setChecked(!live);

  const bool two_step = from_server && config.use_two_step_commit();
  two_step_on_->setChecked(two_step);
  two_step_off_->setChecked(!two_step);
}

QWidget* SetupWizard::MakeScrollPage(QVBoxLayout** layout) {
  auto* scroll = new QScrollArea(stack_);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scroll->setAttribute(Qt::WA_TranslucentBackground);
  scroll->viewport()->setAutoFillBackground(false);
  auto* inner = new QWidget(scroll);
  inner->setAutoFillBackground(false);
  auto* inner_layout = new QVBoxLayout(inner);
  inner_layout->setContentsMargins(0, 6, 8, 6);
  inner_layout->setSpacing(6);
  scroll->setWidget(inner);
  *layout = inner_layout;
  return scroll;
}

QLabel* SetupWizard::AddTitle(QVBoxLayout* layout, QWidget* parent,
                              const QString& text, const QFont& font) {
  auto* label = new QLabel(text, parent);
  label->setFont(font);
  label->setWordWrap(true);
  layout->addWidget(label);
  return label;
}

QLabel* SetupWizard::AddMuted(QVBoxLayout* layout, QWidget* parent,
                              const QString& text) {
  auto* label = new QLabel(text, parent);
  label->setObjectName(QStringLiteral("muted"));
  label->setFont(setup_theme::SecondaryFont());
  label->setWordWrap(true);
  layout->addWidget(label);
  return label;
}

void SetupWizard::AddSection(QVBoxLayout* layout, QWidget* parent,
                             const QString& title, const QString& description) {
  auto* heading = new QLabel(title, parent);
  heading->setFont(setup_theme::SectionFont());
  heading->setWordWrap(true);
  heading->setContentsMargins(0, setup_theme::kSectionGap, 0, 2);
  layout->addWidget(heading);
  AddMuted(layout, parent, description);
}

QRadioButton* SetupWizard::AddChoice(QVBoxLayout* layout, QWidget* parent,
                                     QButtonGroup* group, const QString& title,
                                     const QString& detail) {
  auto* radio = new QRadioButton(title, parent);
  radio->setFont(setup_theme::BodyFont());
  radio->setFocusPolicy(Qt::StrongFocus);
  radio->setStyle(control_style_);
  radio->setAttribute(Qt::WA_MacShowFocusRect, false);
  radio->setAccessibleDescription(detail);
  group->addButton(radio);
  layout->addWidget(radio);
  auto* note = new QLabel(detail, parent);
  note->setObjectName(QStringLiteral("muted"));
  note->setFont(setup_theme::SecondaryFont());
  note->setWordWrap(true);
  note->setContentsMargins(setup_theme::kChoiceIndent, 0, 0, 4);
  layout->addWidget(note);
  return radio;
}

QWidget* SetupWizard::MakeWelcomePage() {
  auto* page = new QWidget(stack_);
  page->setAutoFillBackground(false);
  auto* layout = new QVBoxLayout(page);
  layout->setContentsMargins(8, 22, 8, 0);
  layout->setSpacing(0);

  auto* title = AddTitle(layout, page, tr("Welcome to"),
                         setup_theme::WelcomeTitleFont());
  title->setAlignment(Qt::AlignHCenter);

  // Equal air above and below the logo: Welcome → logo and logo → slogan.
  constexpr int kBrandGap = 16;
  layout->addSpacing(kBrandGap);
  auto* wordmark =
      new MarkView(QStringLiteral(":/wordmark.png"), 37, true, page);
  wordmark->setAccessibleName(GuiUtil::ProductName());
  layout->addWidget(wordmark);

  layout->addSpacing(kBrandGap);
  auto* slogan = new QLabel(tr("Make room for character"), page);
  slogan->setObjectName(QStringLiteral("muted"));
  slogan->setFont(setup_theme::SloganFont());
  slogan->setAlignment(Qt::AlignHCenter);
  slogan->setWordWrap(false);
  // No clip of italic descenders against a fixed-height image box.
  slogan->setMinimumHeight(slogan->fontMetrics().height() + 4);
  layout->addWidget(slogan);

  // No extra gap: the harbour fades into the page, so the slogan may sit in
  // that soft edge.
  layout->addWidget(new HarbourView(HarbourView::kHarbour, page), 1);
  return page;
}

QWidget* SetupWizard::MakeKeyboardPage() {
  QVBoxLayout* layout = nullptr;
  QWidget* page = MakeScrollPage(&layout);
  QWidget* inner = qobject_cast<QScrollArea*>(page)->widget();
  AddTitle(layout, inner, tr("Keyboard"), setup_theme::PageTitleFont());

  AddSection(layout, inner, tr("Keyboard layout"),
             tr("Choose how Japanese input follows your keyboard."));

#if defined(__APPLE__)
  auto* layout_group = new QButtonGroup(inner);
  follow_layout_ = AddChoice(
      layout, inner, layout_group, tr("Follow my current keyboard"),
      tr("Use your normal keyboard layout. Recommended."));
  qwerty_layout_ = AddChoice(
      layout, inner, layout_group, tr("Use QWERTY positions"),
      tr("Use QWERTY key positions when typing Japanese."));
  auto* qwerty_note = AddMuted(
      layout, inner,
      tr("QWERTY positions also apply during direct input, for as long as %1 "
         "is selected.")
          .arg(GuiUtil::ProductName()));
  qwerty_note->setContentsMargins(setup_theme::kChoiceIndent, 0, 0, 0);
#elif defined(_WIN32)
  layout_combo_ = new QComboBox(inner);
  layout_combo_->setFont(setup_theme::BodyFont());
  layout_combo_->setAccessibleName(tr("Keyboard layout"));
  layout_combo_->setMaximumWidth(420);
  // Index must match MarinaKeyboardLayout in config.proto.
  layout_combo_->addItem(tr("System default (current OS layout)"));
  layout_combo_->addItem(tr("US QWERTY"));
  layout_combo_->addItem(tr("UK QWERTY"));
  layout_combo_->addItem(tr("French AZERTY"));
  layout_combo_->addItem(tr("German QWERTZ"));
  layout_combo_->addItem(tr("Spanish"));
  layout_combo_->addItem(tr("Italian"));
  layout_combo_->addItem(tr("Dutch"));
  layout_combo_->addItem(tr("Dvorak"));
  layout_combo_->addItem(tr("BÉPO"));
  layout_combo_->addItem(tr("Japanese JIS"));
  layout->addWidget(layout_combo_);
#else
  AddMuted(layout, inner,
           tr("%1 follows the keyboard layout you already chose in the system. "
              "AZERTY stays AZERTY. To type in QWERTY positions, add a second "
              "layout in your system settings and switch to it while typing "
              "Japanese.")
               .arg(GuiUtil::ProductName()));
#endif

  AddSection(layout, inner, tr("Typing conventions"),
             tr("Choose the typing conventions you're familiar with."));
  keymap_combo_ = new QComboBox(inner);
  keymap_combo_->setFont(setup_theme::BodyFont());
  keymap_combo_->setAccessibleName(tr("Typing conventions"));
  keymap_combo_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  keymap_combo_->setMinimumWidth(220);
  keymap_combo_->setMaximumWidth(420);
#ifdef __APPLE__
  keymap_combo_->addItem(tr("I don't know (Kotoeri)"), kKeymapUnknown);
#else
  keymap_combo_->addItem(tr("I don't know (MS-IME)"), kKeymapUnknown);
#endif
  keymap_combo_->addItem(tr("Kotoeri"),
                         static_cast<int>(config::Config::KOTOERI));
  keymap_combo_->addItem(tr("MS-IME"), static_cast<int>(config::Config::MSIME));
  keymap_combo_->addItem(tr("ATOK"), static_cast<int>(config::Config::ATOK));
  layout->addWidget(keymap_combo_);
  AddMuted(layout, inner, tr("New to Japanese input? Keep the recommended setting."));
  layout->addStretch(1);
  return page;
}

QWidget* SetupWizard::MakeConversionPage() {
  QVBoxLayout* layout = nullptr;
  QWidget* page = MakeScrollPage(&layout);
  QWidget* inner = qobject_cast<QScrollArea*>(page)->widget();
  AddTitle(layout, inner, tr("Conversion"), setup_theme::PageTitleFont());

  AddSection(layout, inner, tr("Automatic conversion"),
             tr("Choose when to convert hiragana into kanji."));
  auto* live_group = new QButtonGroup(inner);
  live_off_ = AddChoice(layout, inner, live_group,
                        tr("Off — convert with Space"),
                        tr("Press Space when you're ready to convert. Recommended."));
  live_on_ = AddChoice(layout, inner, live_group, tr("On — convert as I type"),
                       tr("Kanji suggestions appear automatically."));

  AddSection(layout, inner, tr("Two-step Enter"),
             tr("Choose how Enter behaves when selecting a conversion candidate."));
  auto* enter_group = new QButtonGroup(inner);
  two_step_off_ = AddChoice(
      layout, inner, enter_group, tr("Off"),
      tr("Insert the selected candidate with one Enter. Recommended."));
  two_step_on_ = AddChoice(
      layout, inner, enter_group, tr("On"),
      tr("Press Enter once to confirm the candidate, then again to insert it."));
  AddMuted(layout, inner, tr("Unconverted kana still inserts with one Enter."));
  layout->addStretch(1);
  return page;
}

QWidget* SetupWizard::MakePreferencesPage() {
  QVBoxLayout* layout = nullptr;
  QWidget* page = MakeScrollPage(&layout);
  QWidget* inner = qobject_cast<QScrollArea*>(page)->widget();
  AddTitle(layout, inner, tr("Preferences"), setup_theme::PageTitleFont());

#ifdef __APPLE__
  AddSection(layout, inner, tr("Start at login"),
             tr("Keep %1 ready when you start your Mac.")
                 .arg(GuiUtil::ProductName()));
  login_checkbox_ =
      new QCheckBox(tr("Start the conversion engine when I log in"), inner);
  login_checkbox_->setFont(setup_theme::BodyFont());
  login_checkbox_->setStyle(control_style_);
  login_checkbox_->setAttribute(Qt::WA_MacShowFocusRect, false);
  login_checkbox_->setAccessibleDescription(
      tr("Recommended. This does not turn Japanese input on."));
  layout->addWidget(login_checkbox_);
  AddMuted(layout, inner,
           tr("Recommended. macOS may still ask you to allow this under "
              "Login Items. It does not turn Japanese input\u00A0on."));
#endif  // __APPLE__

  layout->addSpacing(8);
  AddSection(layout, inner, tr("A few useful shortcuts"),
             tr("These already work, and can be changed later in Properties."));
  auto add_shortcut = [&](const QString& key, const QString& action) {
    auto* row = new QWidget(inner);
    auto* row_layout = new QHBoxLayout(row);
    row_layout->setContentsMargins(0, 8, 0, 8);
    row_layout->setSpacing(12);
    auto* cap = new Keycap(row);
    auto* key_label = new QLabel(key, row);
    key_label->setFont(setup_theme::BodyFont());
    auto* dash = new QLabel(QStringLiteral("—"), row);
    dash->setObjectName(QStringLiteral("muted"));
    dash->setFont(setup_theme::SecondaryFont());
    auto* action_label = new QLabel(action, row);
    action_label->setObjectName(QStringLiteral("muted"));
    action_label->setFont(setup_theme::SecondaryFont());
    row_layout->addWidget(cap, 0, Qt::AlignVCenter);
    row_layout->addWidget(key_label, 0, Qt::AlignVCenter);
    row_layout->addWidget(dash, 0, Qt::AlignVCenter);
    row_layout->addWidget(action_label, 1, Qt::AlignVCenter);
    layout->addWidget(row);
  };
  add_shortcut(tr("Left Shift"), tr("Japanese  \u2194  Direct input"));
  add_shortcut(tr("Right Shift"), tr("Hiragana  \u2194  Man'y\u014dsh\u016b"));
  layout->addStretch(1);
  return page;
}

QWidget* SetupWizard::MakeReadyPage() {
  auto* page = new QWidget(stack_);
  page->setAutoFillBackground(false);
  auto* layout = new QVBoxLayout(page);
  layout->setContentsMargins(8, 10, 8, 0);
  layout->setSpacing(6);

  AddTitle(layout, page, tr("You're ready to write."),
           setup_theme::ReadyTitleFont());

  layout->addSpacing(2);
  auto* japanese = new QLabel(QStringLiteral(u"日本語を始めましょう。"), page);
  japanese->setFont(setup_theme::JapaneseFont(20));
  layout->addWidget(japanese);
  layout->addSpacing(6);

  layout->addWidget(new HarbourView(HarbourView::kOpenSea, page), 1);

#if defined(__APPLE__)
  const QString next_step =
      tr("Select %1 from your Mac's input menu to begin typing Japanese.")
          .arg(GuiUtil::ProductName());
#elif defined(_WIN32)
  const QString next_step =
      tr("Turn on %1 from the language indicator in the taskbar to begin "
         "typing Japanese.")
          .arg(GuiUtil::ProductName());
#else
  const QString next_step =
      tr("Select %1 in your input menu to begin typing Japanese.")
          .arg(GuiUtil::ProductName());
#endif
  AddMuted(layout, page, next_step);
  AddMuted(layout, page,
           tr("Settings can be changed later in Properties."));
  return page;
}

void SetupWizard::ShowPage(int index) {
  if (index < 0 || index >= stack_->count()) {
    return;
  }
  page_index_ = index;
  stack_->setCurrentIndex(index);

  const bool welcome = index == kWelcome;
  const bool ready = index == kReady;
  const bool configuring = !welcome && !ready;
  chrome_mark_->setVisible(!welcome);
  progress_->setVisible(configuring);
  chrome_line_->setVisible(configuring);
  back_->setVisible(!welcome);
  back_->setEnabled(index > 0);

  if (welcome) {
    next_->setText(tr("Get started  \u2192"));
  } else if (ready) {
    next_->setText(tr("Start using %1  \u2192").arg(GuiUtil::ProductName()));
  } else {
    next_->setText(tr("Continue  \u2192"));
    // Three configuration pages, numbered from the keyboard page.
    progress_->setText(tr("%1  /  03").arg(index, 2, 10, QChar('0')));
  }

  QWidget* page = stack_->currentWidget();
  QRadioButton* radio = page->findChild<QRadioButton*>();
  if (!welcome && radio != nullptr && radio->isVisible()) {
    radio->setFocus();
  } else {
    next_->setFocus();
  }
}

void SetupWizard::OnBack() { ShowPage(page_index_ - 1); }

void SetupWizard::OnNext() {
  if (page_index_ + 1 < stack_->count()) {
    ShowPage(page_index_ + 1);
    return;
  }
  if (Save()) {
    accept();
  }
}

bool SetupWizard::Save() {
  // A failed first contact leaves the client in a fatal state. Reset so the
  // button can try again, including another attempt to start the engine.
  client_->Reset();
  config::Config config;
  if (!GetConfig(&config)) {
    QMessageBox::critical(
        this, windowTitle(),
        tr("Nothing was saved. %1's conversion engine could not be reached, "
           "so these choices are still only in this window. Start %1, then "
           "press the button again.")
            .arg(GuiUtil::ProductName()));
    return false;
  }

#ifdef __APPLE__
  config.set_use_japanese_layout(qwerty_layout_->isChecked());
#endif  // __APPLE__
#ifdef _WIN32
  config.set_marina_keyboard_layout(static_cast<config::MarinaKeyboardLayout>(
      layout_combo_->currentIndex()));
#endif  // _WIN32

  const int keymap_choice = keymap_combo_->currentData().toInt();
  if (keymap_choice == kKeymapUnknown) {
    config.set_session_keymap(config::ConfigHandler::GetDefaultKeyMap());
  } else if (keymap_choice != kKeymapKeepCustom) {
    config.set_session_keymap(
        static_cast<config::Config::SessionKeymap>(keymap_choice));
  }

  config.set_use_live_conversion(live_on_->isChecked());
  config.set_use_two_step_commit(two_step_on_->isChecked());

  if (!SetConfig(config)) {
    QMessageBox::critical(
        this, windowTitle(),
        tr("Nothing was saved. %1's conversion engine did not accept the "
           "settings. The window will stay open so you can try again.")
            .arg(GuiUtil::ProductName()));
    return false;
  }

#ifdef __APPLE__
  const bool want_login = login_checkbox_->isChecked();
  const bool has_login = MacUtil::CheckPrelauncherLoginItemStatus();
  if (want_login && !has_login) {
    MacUtil::AddPrelauncherLoginItem();
  } else if (!want_login && has_login) {
    MacUtil::RemovePrelauncherLoginItem();
  }
#endif  // __APPLE__
  config_loaded_ = true;
  return true;
}

}  // namespace gui
}  // namespace mozc
