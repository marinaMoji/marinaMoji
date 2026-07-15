// Copyright 2010-2021, Google Inc.
// All rights reserved.

#include "renderer/win32/symbols_palette_window.h"

#include <commctrl.h>
#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "base/file_util.h"
#include "base/system_util.h"
#include "base/win32/wide_char.h"
#include "protocol/commands.pb.h"
#include "protocol/renderer_command.pb.h"

namespace mozc {
namespace renderer {
namespace win32 {

namespace {

constexpr int kNumTabs = 4;
constexpr int kColumns = 8;
constexpr int kButtonWidth = 40;
constexpr int kButtonHeight = 32;
constexpr int kGap = 4;
constexpr int kMargin = 8;
constexpr int kTabStripHeight = 28;
constexpr int kWindowWidth =
    kMargin * 2 + kColumns * kButtonWidth + (kColumns - 1) * kGap;
constexpr int kPinCheckboxHeight = 24;

// marinaMoji: mirrors src/session/odoriji_palette.cc's kOdorijiChars and
// mac's BuildDefaultOdorijiSymbols()/BuildDefaultGeneralSymbols(). Kept as a
// Windows-local copy for scope containment; sharing one source across
// platforms is a nice-to-have follow-up.
const wchar_t* const kOdorijiSymbols[] = {
    L"\x3005", L"\x309D", L"\x309E", L"\x30FD",
    L"\x30FE", L"\x303B", L"\x3031", L"\x3032",
};

const wchar_t* const kGeneralSymbols[] = {
    L"\x3014", L"\x3015", L"\xFF3B", L"\xFF3D", L"\x3010", L"\x3011",
    L"\x3008", L"\x3009", L"\x300A", L"\x300B", L"\xFF08", L"\xFF09",
    L"\xFF5B", L"\xFF5D", L"\x25A1", L"\x25A0", L"\x25CB", L"\x25B3",
    L"\xFF0D", L"\x203B", L"\x3013", L"\x25C6", L"\x25C7", L"\x25CE",
    L"\x25B2", L"\x25BD", L"\x2026", L"\x2014",
};

std::vector<std::wstring> BuildOdorijiSymbols() {
  return std::vector<std::wstring>(std::begin(kOdorijiSymbols),
                                   std::end(kOdorijiSymbols));
}

std::vector<std::wstring> BuildGeneralSymbols() {
  return std::vector<std::wstring>(std::begin(kGeneralSymbols),
                                   std::end(kGeneralSymbols));
}

constexpr int kTabControlId = 1001;
constexpr int kPinCheckboxId = 1002;
constexpr int kButtonIdBase = 2000;
constexpr int kButtonsPerTab = 1000;  // must exceed any tab's symbol count

}  // namespace

SymbolsPaletteWindow::SymbolsPaletteWindow()
    : send_command_interface_(nullptr),
      tab_control_(nullptr),
      pin_checkbox_(nullptr),
      active_tab_(0),
      pinned_(false),
      has_shown_once_(false) {}

SymbolsPaletteWindow::~SymbolsPaletteWindow() {}

void SymbolsPaletteWindow::Initialize() {
  INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_TAB_CLASSES | ICC_STANDARD_CLASSES};
  ::InitCommonControlsEx(&icc);

  // Must run before Create() so OnCreate()'s CreateTabControl() picks up the
  // persisted |active_tab_| when it calls TabCtrl_SetCurSel.
  LoadPreferences();

  Create(nullptr);

  RebuildButtonsForTab(Tab::kOdoriji, BuildOdorijiSymbols());
  RebuildButtonsForTab(Tab::kSymbols, BuildGeneralSymbols());

  ShowWindow(SW_HIDE);
}

void SymbolsPaletteWindow::Destroy() {
  if (IsWindow()) {
    DestroyWindow();
  }
}

void SymbolsPaletteWindow::SetSendCommandInterface(
    client::SendCommandInterface* send_command_interface) {
  send_command_interface_ = send_command_interface;
}

void SymbolsPaletteWindow::Hide() { ShowWindow(SW_HIDE); }

LRESULT SymbolsPaletteWindow::OnCreate(LPCREATESTRUCT create_struct) {
  SetWindowTextW(L"marinaMoji Symbols");
  CreateTabControl();

  pin_checkbox_ = ::CreateWindowExW(
      0, L"BUTTON", L"Pin", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
      kMargin, kTabStripHeight, 100, kPinCheckboxHeight, m_hWnd,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPinCheckboxId)),
      nullptr, nullptr);
  if (pin_checkbox_ != nullptr) {
    Button_SetCheck(pin_checkbox_, pinned_ ? BST_CHECKED : BST_UNCHECKED);
  }

  return 0;
}

void SymbolsPaletteWindow::CreateTabControl() {
  tab_control_ = ::CreateWindowExW(
      0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE,
      0, 0, kWindowWidth, kTabStripHeight, m_hWnd,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kTabControlId)),
      nullptr, nullptr);
  if (tab_control_ == nullptr) {
    return;
  }

  const wchar_t* labels[] = {L"Odoriji", L"Kaeriten", L"Symbols", L"User"};
  for (int i = 0; i < kNumTabs; ++i) {
    TCITEMW item = {};
    item.mask = TCIF_TEXT;
    item.pszText = const_cast<wchar_t*>(labels[i]);
    TabCtrl_InsertItem(tab_control_, i, &item);
  }
  TabCtrl_SetCurSel(tab_control_, active_tab_);
}

void SymbolsPaletteWindow::RebuildButtonsForTab(
    Tab tab, const std::vector<std::wstring>& symbols) {
  const size_t idx = static_cast<size_t>(tab);
  for (HWND button : tab_buttons_[idx]) {
    if (button != nullptr) {
      ::DestroyWindow(button);
    }
  }
  tab_buttons_[idx].clear();
  tab_symbols_[idx] = symbols;

  const int button_top = kTabStripHeight + kPinCheckboxHeight + kMargin;
  for (size_t i = 0; i < symbols.size(); ++i) {
    const int row = static_cast<int>(i) / kColumns;
    const int col = static_cast<int>(i) % kColumns;
    const int x = kMargin + col * (kButtonWidth + kGap);
    const int y = button_top + row * (kButtonHeight + kGap);
    const int control_id =
        kButtonIdBase + static_cast<int>(idx) * kButtonsPerTab +
        static_cast<int>(i);
    HWND button = ::CreateWindowExW(
        0, L"BUTTON", symbols[i].c_str(),
        WS_CHILD | (static_cast<int>(tab) == active_tab_ ? WS_VISIBLE : 0),
        x, y, kButtonWidth, kButtonHeight, m_hWnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(control_id)), nullptr,
        nullptr);
    tab_buttons_[idx].push_back(button);
  }
}

void SymbolsPaletteWindow::ShowOnlyActiveTab() {
  for (int t = 0; t < kNumTabs; ++t) {
    const bool visible = (t == active_tab_);
    for (HWND button : tab_buttons_[static_cast<size_t>(t)]) {
      if (button != nullptr) {
        ::ShowWindow(button, visible ? SW_SHOW : SW_HIDE);
      }
    }
  }
}

LRESULT SymbolsPaletteWindow::OnNotify(UINT msg_id, WPARAM wparam,
                                       LPARAM lparam, BOOL& handled) {
  const auto* header = reinterpret_cast<const NMHDR*>(lparam);
  if (header != nullptr && header->hwndFrom == tab_control_ &&
      header->code == TCN_SELCHANGE) {
    active_tab_ = TabCtrl_GetCurSel(tab_control_);
    ShowOnlyActiveTab();
    SavePreferences();
    handled = TRUE;
    return 0;
  }
  handled = FALSE;
  return 0;
}

LRESULT SymbolsPaletteWindow::OnCommand(UINT msg_id, WPARAM wparam,
                                        LPARAM lparam, BOOL& handled) {
  const int control_id = LOWORD(wparam);
  const int notification = HIWORD(wparam);
  handled = TRUE;

  if (control_id == kPinCheckboxId && notification == BN_CLICKED) {
    OnPinToggled();
    return 0;
  }

  if (control_id >= kButtonIdBase && notification == BN_CLICKED) {
    const int offset = control_id - kButtonIdBase;
    const int tab = offset / kButtonsPerTab;
    const int index = offset % kButtonsPerTab;
    if (tab >= 0 && tab < kNumTabs) {
      OnSymbolClicked(static_cast<Tab>(tab), index);
    }
    return 0;
  }

  handled = FALSE;
  return 0;
}

void SymbolsPaletteWindow::OnSymbolClicked(Tab tab, int index) {
  const auto& symbols = tab_symbols_[static_cast<size_t>(tab)];
  if (index < 0 || static_cast<size_t>(index) >= symbols.size()) {
    return;
  }

  if (tab == Tab::kOdoriji) {
    SendShowOdorijiPaletteAndSubmit(index);
  } else {
    SendInsertSymbolText(symbols[index]);
  }

  if (!pinned_) {
    SendHidePaletteSignal();
    Hide();
  }
}

void SymbolsPaletteWindow::OnPinToggled() {
  pinned_ = (Button_GetCheck(pin_checkbox_) == BST_CHECKED);
  SavePreferences();
}

LRESULT SymbolsPaletteWindow::OnCloseMessage(UINT msg_id, WPARAM wparam,
                                             LPARAM lparam, BOOL& handled) {
  handled = TRUE;
  SendHidePaletteSignal();
  Hide();
  return 0;
}

void SymbolsPaletteWindow::SendShowOdorijiPaletteAndSubmit(int index) {
  if (send_command_interface_ == nullptr) {
    return;
  }
  commands::SessionCommand show;
  show.set_type(commands::SessionCommand::SHOW_ODORIJI_PALETTE);
  commands::Output show_output;
  send_command_interface_->SendCommand(show, &show_output);

  commands::SessionCommand submit;
  submit.set_type(commands::SessionCommand::SUBMIT_CANDIDATE);
  submit.set_id(index);
  commands::Output submit_output;
  send_command_interface_->SendCommand(submit, &submit_output);
}

void SymbolsPaletteWindow::SendInsertSymbolText(const std::wstring& symbol) {
  if (send_command_interface_ == nullptr) {
    return;
  }
  commands::SessionCommand command;
  command.set_type(commands::SessionCommand::INSERT_SYMBOL_TEXT);
  command.set_text(mozc::win32::WideToUtf8(symbol));
  commands::Output output;
  send_command_interface_->SendCommand(command, &output);
}

void SymbolsPaletteWindow::SendHidePaletteSignal() {
  if (send_command_interface_ == nullptr) {
    return;
  }
  commands::SessionCommand command;
  command.set_type(commands::SessionCommand::HIDE_SYMBOLS_PALETTE);
  commands::Output output;
  send_command_interface_->SendCommand(command, &output);
}

void SymbolsPaletteWindow::OnUpdate(const commands::RendererCommand& command) {
  if (!command.has_application_info() ||
      !command.application_info().has_symbols_palette_info()) {
    // TIP has closed the palette (e.g. focus loss) -- follow suit.
    if (IsWindowVisible()) {
      Hide();
    }
    return;
  }

  const auto& info = command.application_info().symbols_palette_info();

  std::vector<std::wstring> kaeriten;
  for (const std::string& s : info.kaeriten_symbols()) {
    kaeriten.push_back(mozc::win32::Utf8ToWide(s));
  }
  RebuildButtonsForTab(Tab::kKaeriten, kaeriten);

  std::vector<std::wstring> user;
  for (const std::string& s : info.user_symbols()) {
    user.push_back(mozc::win32::Utf8ToWide(s));
  }
  RebuildButtonsForTab(Tab::kUser, user);

  if (!has_shown_once_) {
    // marinaMoji: mirrors mac's [window center] -- the palette isn't
    // anchored to the toolbar's position, unlike the toolbar's own
    // bottom-right default.
    RECT work_area = {0, 0, 1920, 1080};
    ::SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0);
    const int total_height = kTabStripHeight + kPinCheckboxHeight + kMargin +
                             4 * (kButtonHeight + kGap) + kMargin;
    const int x =
        work_area.left + (work_area.right - work_area.left - kWindowWidth) / 2;
    const int y = work_area.top +
                  (work_area.bottom - work_area.top - total_height) / 2;
    SetWindowPos(HWND_TOPMOST, x, y, kWindowWidth, total_height,
                 SWP_NOACTIVATE);
    has_shown_once_ = true;
  }

  if (!IsWindowVisible()) {
    ShowWindow(SW_SHOWNA);
  }
}

std::string SymbolsPaletteWindow::ConfigFilePath() const {
  return FileUtil::JoinPath(
      {SystemUtil::GetUserProfileDirectory(), "symbols_palette.conf"});
}

void SymbolsPaletteWindow::LoadPreferences() {
  pinned_ = false;
  active_tab_ = 0;
  const auto contents = FileUtil::GetContents(ConfigFilePath());
  if (!contents.ok()) {
    return;
  }
  const std::string& text = *contents;
  size_t pos = 0;
  while (pos < text.size()) {
    const size_t eol = text.find('\n', pos);
    const std::string line = text.substr(
        pos, eol == std::string::npos ? std::string::npos : eol - pos);
    const size_t eq = line.find('=');
    if (eq != std::string::npos) {
      const std::string key = line.substr(0, eq);
      const std::string value = line.substr(eq + 1);
      if (key == "pinned") {
        pinned_ = (value == "1");
      } else if (key == "last_tab") {
        const int tab = std::atoi(value.c_str());
        if (tab >= 0 && tab < kNumTabs) {
          active_tab_ = tab;
        }
      }
    }
    if (eol == std::string::npos) {
      break;
    }
    pos = eol + 1;
  }
}

void SymbolsPaletteWindow::SavePreferences() const {
  const std::string content = absl::StrCat(
      "pinned=", pinned_ ? "1" : "0", "\nlast_tab=", active_tab_, "\n");
  const std::string path = ConfigFilePath();
  (void)FileUtil::CreateDirectory(FileUtil::Dirname(path));
  (void)FileUtil::SetContents(path, content);
}

}  // namespace win32
}  // namespace renderer
}  // namespace mozc
