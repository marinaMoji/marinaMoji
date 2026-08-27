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

#include "win32/tip/tip_ui_handler_conventional.h"

#include <msctf.h>
#include <wil/com.h>
#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/const.h"
#include "base/file_util.h"
#include "base/system_util.h"
#include "base/win32/com.h"
#include "base/win32/wide_char.h"
#include "base/win32/win_util.h"
#include "composer/kaeriten_table_util.h"
#include "protocol/candidate_window.pb.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "protocol/renderer_command.pb.h"
#include "renderer/win32/win32_renderer_client.h"
#include "session/marina_shortcut_list_util.h"
#include "win32/base/input_state.h"
#include "win32/base/toolbar_config.h"
#include "win32/tip/tip_composition_util.h"
#include "win32/tip/tip_dll_module.h"
#include "win32/tip/tip_input_mode_manager.h"
#include "win32/tip/tip_private_context.h"
#include "win32/tip/tip_range_util.h"
#include "win32/tip/tip_text_service.h"
#include "win32/tip/tip_thread_context.h"
#include "win32/tip/tip_ui_element_manager.h"

namespace mozc {
namespace win32 {
namespace tsf {
namespace {

using ::mozc::commands::Preedit;
using ::mozc::renderer::win32::Win32RendererClient;
using Segment = ::mozc::commands::Preedit_Segment;
using Annotation = ::mozc::commands::Preedit_Segment::Annotation;
using IndicatorInfo = ::mozc::commands::RendererCommand_IndicatorInfo;
using RendererCommand = ::mozc::commands::RendererCommand;
using ApplicationInfo = ::mozc::commands::RendererCommand::ApplicationInfo;

// True when the OS foreground window is marinaMoji renderer UI (toolbar,
// shortcuts, palette, candidate, …). Clicking a Shortcuts tab activates that
// window, which fires ITfThreadFocusSink::OnKillThreadFocus in the app. The
// handler used to treat that as "user left the IME" and hide toolbar +
// shortcuts until the app was focused again.
bool IsMarinaRendererUiForeground() {
  HWND foreground = ::GetForegroundWindow();
  if (foreground == nullptr) {
    return false;
  }
  HWND root = ::GetAncestor(foreground, GA_ROOT);
  if (root == nullptr) {
    root = foreground;
  }
  wchar_t class_name[256] = {};
  if (::GetClassNameW(root, class_name, ARRAYSIZE(class_name)) <= 0) {
    return false;
  }
  return wcscmp(class_name, kShortcutsWindowClassName) == 0 ||
         wcscmp(class_name, kToolbarWindowClassName) == 0 ||
         wcscmp(class_name, kSymbolsPaletteWindowClassName) == 0 ||
         wcscmp(class_name, kCandidateWindowClassName) == 0 ||
         wcscmp(class_name, kInfolistWindowClassName) == 0 ||
         wcscmp(class_name, kCompositionWindowClassName) == 0 ||
         wcscmp(class_name, kIndicatorWindowClassName) == 0 ||
         wcscmp(class_name, kSyncOverlayWindowClassName) == 0;
}

size_t GetTargetPos(const commands::Output& output) {
  if (!output.has_candidate_window() ||
      !output.candidate_window().has_category()) {
    return 0;
  }
  switch (output.candidate_window().category()) {
    case commands::PREDICTION:
    case commands::SUGGESTION:
      return 0;
    case commands::CONVERSION: {
      const Preedit& preedit = output.preedit();
      size_t offset = 0;
      for (int i = 0; i < preedit.segment_size(); ++i) {
        const Segment& segment = preedit.segment(i);
        const Annotation& annotation = segment.annotation();
        if (annotation == Segment::HIGHLIGHT) {
          return offset;
        }
        offset += WideCharsLen(segment.value());
      }
      return offset;
    }
    default:
      return 0;
  }
}

bool FillVisibility(ITfUIElementMgr* ui_element_manager,
                    TipPrivateContext* private_context,
                    RendererCommand* command) {
  command->set_visible(false);

  if (private_context == nullptr) {
    return false;
  }

  const bool show_suggest_window =
      private_context->GetUiElementManager()->IsVisible(
          ui_element_manager, TipUiElementManager::kSuggestWindow);
  const bool show_candidate_window =
      private_context->GetUiElementManager()->IsVisible(
          ui_element_manager, TipUiElementManager::kCandidateWindow);

  const commands::Output& output = private_context->last_output();

  bool suggest_window_visible = false;
  bool candidate_window_visible = false;

  // Check if suggest window and candidate window are actually visible.
  if (output.has_candidate_window() &&
      output.candidate_window().has_category()) {
    switch (output.candidate_window().category()) {
      case commands::SUGGESTION:
        suggest_window_visible = show_suggest_window;
        break;
      case commands::CONVERSION:
      case commands::PREDICTION:
        candidate_window_visible = show_candidate_window;
        break;
      default:
        // do nothing.
        break;
    }
  }

  if (candidate_window_visible || suggest_window_visible) {
    command->set_visible(true);
  }

  ApplicationInfo* app_info = command->mutable_application_info();

  int visibility = ApplicationInfo::ShowUIDefault;
  if (show_candidate_window) {
    // Note that |ApplicationInfo::ShowCandidateWindow| represents that the
    // application does not mind the IME showing its own candidate window.
    // This bit does not mean that |command| requires the suggest window.
    visibility |= ApplicationInfo::ShowCandidateWindow;
  }
  if (show_suggest_window) {
    // Note that |ApplicationInfo::ShowCandidateWindow| represents that the
    // application does not mind the IME showing its own candidate window.
    // This bit does not mean that |command| requires the suggest window.
    visibility |= ApplicationInfo::ShowSuggestWindow;
  }
  // marinaMoji: the floating toolbar shows whenever there is an active
  // document context, independent of candidate/suggest visibility (mirrors
  // mac/Linux: shown on focus, hidden on focus loss). UpdateCommand() clears
  // this bit below when the TSF thread loses focus.
  if (mozc::win32::LoadToolbarVisiblePreference()) {
    visibility |= ApplicationInfo::ShowToolbar;
  }
  app_info->set_ui_visibilities(visibility);

  return true;
}

// marinaMoji: reads user_symbols.txt (one symbol per line, same format/path
// mac's LoadUserSymbolsFromFile() uses: SystemUtil::GetUserProfileDirectory()
// + "/user_symbols.txt") into |out|. Missing file just means an empty list.
void LoadUserSymbolsFromFile(std::vector<std::string>* out) {
  out->clear();
  const std::string path = FileUtil::JoinPath(
      {SystemUtil::GetUserProfileDirectory(), "user_symbols.txt"});
  const auto contents = FileUtil::GetContents(path);
  if (!contents.ok()) {
    return;
  }
  const std::string& text = *contents;
  size_t pos = 0;
  while (pos < text.size()) {
    size_t eol = text.find('\n', pos);
    std::string line = text.substr(
        pos, eol == std::string::npos ? std::string::npos : eol - pos);
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (!line.empty()) {
      out->push_back(std::move(line));
    }
    if (eol == std::string::npos) {
      break;
    }
    pos = eol + 1;
  }
}

// marinaMoji: populates SymbolsPaletteInfo (Kaeriten/User tabs) while the
// Windows floating Symbols Palette is open. Odoriji and general-symbols tabs
// are static and hardcoded renderer-side, so they don't need to travel here.
void FillSymbolsPaletteInfo(TipPrivateContext* private_context,
                            ApplicationInfo* app_info) {
  if (private_context == nullptr ||
      !private_context->symbols_palette_visible()) {
    return;
  }

  RendererCommand::SymbolsPaletteInfo* info =
      app_info->mutable_symbols_palette_info();

  config::Config config;
  if (private_context->GetClient()->GetConfig(&config)) {
    std::vector<std::pair<std::string, std::string>> kaeriten_entries;
    composer::LoadKaeritenShortcutEntries(config, &kaeriten_entries);
    std::vector<std::string> seen;
    for (const auto& entry : kaeriten_entries) {
      const std::string& symbol = entry.second;
      if (symbol.empty()) {
        continue;
      }
      if (std::find(seen.begin(), seen.end(), symbol) != seen.end()) {
        continue;
      }
      seen.push_back(symbol);
      info->add_kaeriten_symbols(symbol);
    }
  }

  std::vector<std::string> user_symbols;
  LoadUserSymbolsFromFile(&user_symbols);
  for (const std::string& symbol : user_symbols) {
    info->add_user_symbols(symbol);
  }
}

// marinaMoji: populates ShortcutsInfo while the Windows Keyboard Shortcuts
// window is open. The rows are derived from the keymap table, the marina
// number-row bindings and the kaeriten table -- all config parsing the
// renderer deliberately doesn't link -- so they are built here and shipped
// whole.
void FillShortcutsInfo(TipPrivateContext* private_context,
                       ApplicationInfo* app_info) {
  if (private_context == nullptr ||
      !private_context->shortcuts_window_visible()) {
    return;
  }

  config::Config config;
  if (!private_context->GetClient()->GetConfig(&config)) {
    config.Clear();  // fall back to the bundled defaults
  }
  const auto lists = session::BuildMarinaShortcutLists(config);

  RendererCommand::ShortcutsInfo* info = app_info->mutable_shortcuts_info();
  const auto fill = [](const std::vector<session::MarinaShortcutRow>& rows,
                       auto* out) {
    for (const session::MarinaShortcutRow& row : rows) {
      auto* proto_row = out->Add();
      proto_row->set_function(row.function);
      proto_row->set_keys(row.keys);
    }
  };
  fill(lists.script, info->mutable_script());
  fill(lists.composition, info->mutable_composition());
  fill(lists.kaeriten, info->mutable_kaeriten());
}

bool FillWindowHandle(ITfContext* context, ApplicationInfo* app_info) {
  wil::com_ptr_nothrow<ITfContextView> context_view;
  if (FAILED(context->GetActiveView(&context_view)) || !context_view) {
    return false;
  }

  HWND window_handle = nullptr;
  if (FAILED(context_view->GetWnd(&window_handle))) {
    return false;
  }
  app_info->set_target_window_handle(
      WinUtil::EncodeWindowHandle(window_handle));
  return true;
}

wil::com_ptr_nothrow<ITfRange> GetCompositionRange(ITfContext* context,
                                                   TfEditCookie read_cookie) {
  wil::com_ptr_nothrow<ITfCompositionView> composition_view =
      TipCompositionUtil::GetCompositionView(context, read_cookie);
  if (!composition_view) {
    return nullptr;
  }

  wil::com_ptr_nothrow<ITfRange> composition_range;
  if (FAILED(composition_view->GetRange(&composition_range))) {
    return nullptr;
  }
  return composition_range;
}

wil::com_ptr_nothrow<ITfRange> GetSelectionRange(ITfContext* context,
                                                 TfEditCookie read_cookie) {
  wil::com_ptr_nothrow<ITfRange> selection_range;
  TfActiveSelEnd sel_end = TF_AE_NONE;
  if (FAILED(TipRangeUtil::GetDefaultSelection(context, read_cookie,
                                               &selection_range, &sel_end))) {
    return nullptr;
  }
  return selection_range;
}

// This function updates RendererCommand::CharacterPosition to emulate
// IMM32-based client. Ideally we'd better to define new field for TSF Mozc
// into which the result of ITfContextView::GetTextExt is stored.
// TODO(yukawa): Replace FillCharPosition with new one designed for TSF.
bool FillCharPosition(TipPrivateContext* private_context, ITfContext* context,
                      TfEditCookie read_cookie, bool has_composition,
                      ApplicationInfo* app_info, bool* no_layout) {
  if (private_context == nullptr) {
    return false;
  }
  bool dummy_no_layout = false;
  if (no_layout == nullptr) {
    no_layout = &dummy_no_layout;
  }
  *no_layout = false;

  if (!app_info->has_target_window_handle()) {
    return false;
  }

  (void)WinUtil::DecodeWindowHandle(app_info->target_window_handle());

  wil::com_ptr_nothrow<ITfRange> range =
      has_composition ? GetCompositionRange(context, read_cookie)
                      : GetSelectionRange(context, read_cookie);
  if (!range) {
    return false;
  }
  wil::com_ptr_nothrow<ITfRange> target_range;
  if (FAILED(range->Clone(&target_range))) {
    return false;
  }
  if (!target_range) {
    return false;
  }

  const commands::Output& output = private_context->last_output();
  LONG shifted = 0;
  if (FAILED(target_range->Collapse(read_cookie, TF_ANCHOR_START))) {
    return false;
  }
  const size_t target_pos = GetTargetPos(output);
  if (FAILED(target_range->ShiftStart(read_cookie, target_pos, &shifted,
                                      nullptr))) {
    return false;
  }
  if (FAILED(target_range->ShiftEnd(read_cookie, target_pos + 1, &shifted,
                                    nullptr))) {
    return false;
  }

  wil::com_ptr_nothrow<ITfContextView> context_view;
  if (FAILED(context->GetActiveView(&context_view)) || !context_view) {
    return false;
  }

  RECT document_rect = {};
  if (FAILED(context_view->GetScreenExt(&document_rect))) {
    return false;
  }

  RECT text_rect = {};
  bool clipped = false;
  const HRESULT hr =
      TipRangeUtil::GetTextExt(context_view.get(), read_cookie,
                               target_range.get(), &text_rect, &clipped);
  if (hr == TF_E_NOLAYOUT) {
    // This is not a critical error but the layout information is not available.
    *no_layout = true;
    return true;
  }
  if (FAILED(hr)) {
    // Any other errors are unexpected.
    return false;
  }

  RendererCommand::CharacterPosition* composition_target =
      app_info->mutable_composition_target();
  composition_target->set_position(0);

  bool vertical_writing = false;
  if (SUCCEEDED(TipRangeUtil::IsVerticalWriting(target_range.get(), read_cookie,
                                                &vertical_writing))) {
    composition_target->set_vertical_writing(vertical_writing);
  }

  RendererCommand::Point* point = composition_target->mutable_top_left();
  if (vertical_writing) {
    // [Vertical Writing]
    //    |
    //    +-----< (pt)
    //    |     |
    //    |-----+
    //    | (cLineHeight)
    //    |
    //    |
    //    v
    //   (Base Line)
    point->set_x(text_rect.right);
    point->set_y(text_rect.top);
    composition_target->set_line_height(text_rect.right - text_rect.left);
  } else {
    // [Horizontal Writing]
    //    (pt)
    //     v_____
    //     |     |
    //     |     | (cLineHeight)
    //     |     |
    //   --+-----+---------->  (Base Line)
    point->set_x(text_rect.left);
    point->set_y(text_rect.top);
    composition_target->set_line_height(text_rect.bottom - text_rect.top);
  }

  RendererCommand::Rectangle* area =
      composition_target->mutable_document_area();
  area->set_left(document_rect.left);
  area->set_top(document_rect.top);
  area->set_right(document_rect.right);
  area->set_bottom(document_rect.bottom);

  return true;
}

// marinaMoji TEMPORARY (2026-08-08): counterpart to renderer_server.cc's
// MarinaDebugLog, for "odoriji palette opens then disappears". Read with
// DebugView alongside the [marinaMoji/renderer] and [marinaMoji/toolbar]
// lines. Remove with the rest of the marinaMoji TEMPORARY logging.
void MarinaDebugLog(absl::string_view message) {
  const std::string line = absl::StrCat("[marinaMoji/tip-ui] ", message, "\n");
  ::OutputDebugStringA(line.c_str());
}

void UpdateCommand(TipTextService* text_service, ITfContext* context,
                   TfEditCookie read_cookie, RendererCommand* command,
                   bool* no_layout) {
  command->Clear();
  command->set_type(RendererCommand::UPDATE);

  TipPrivateContext* private_context = text_service->GetPrivateContext(context);
  if (private_context != nullptr) {
    *command->mutable_output() = private_context->last_output();
    private_context->GetUiElementManager()->OnUpdate(text_service, context);
  }

  ApplicationInfo* app_info = command->mutable_application_info();
  app_info->set_input_framework(ApplicationInfo::TSF);
  app_info->set_process_id(::GetCurrentProcessId());
  app_info->set_thread_id(::GetCurrentThreadId());
  app_info->set_receiver_handle(WinUtil::EncodeWindowHandle(
      text_service->renderer_callback_window_handle()));

  auto ui_element_manager =
      ComQuery<ITfUIElementMgr>(text_service->GetThreadManager());
  DCHECK(ui_element_manager);
  FillVisibility(ui_element_manager.get(), private_context, command);
  FillWindowHandle(context, app_info);
  FillCharPosition(private_context, context, read_cookie,
                   command->output().has_preedit(), app_info, no_layout);

  if (private_context != nullptr) {
    const TipInputModeManager* input_mode_manager =
        text_service->GetThreadContext()->GetInputModeManager();
    if (private_context->input_behavior().use_mode_indicator &&
        input_mode_manager->IsIndicatorVisible()) {
      command->set_visible(true);
      IndicatorInfo* info = app_info->mutable_indicator_info();
      info->mutable_status()->set_activated(
          input_mode_manager->GetEffectiveOpenClose());
      info->mutable_status()->set_mode(
          input_mode_manager->GetEffectiveConversionMode());
      // marinaMoji: TipInputModeManager tracks open/close and conversion mode
      // only, so carry the session's Left Shift lock flag over from the last
      // output. Without it this Status reports the field's default false, and
      // any consumer that prefers indicator_info over output.status() (the
      // floating toolbar does, for the mode) would draw an unlocked icon on
      // the very tap that engages the lock.
      if (command->output().has_status()) {
        info->mutable_status()->set_left_shift_direct_lock(
            command->output().status().left_shift_direct_lock());
      }
    }
  }

  FillSymbolsPaletteInfo(private_context, app_info);
  FillShortcutsInfo(private_context, app_info);

  // marinaMoji TEMPORARY (2026-08-08): the palette-visible flag lives on the
  // per-ITfContext TipPrivateContext, but the toolbar click writes it to the
  // *base* context (tip_text_service.cc GetRendererCallbackContext ->
  // GetBase) while keystroke-driven updates run on the *top* context
  // (GetTop). Where those differ -- any app using the TSF transitory
  // extension -- the click sets the flag on one context and this function
  // reads another, emits no SymbolsPaletteInfo, and the renderer hides a
  // palette that just opened. Log the context pointer with the flag so a
  // DebugView capture shows the two addresses directly: a |ctx=| that changes
  // between the click and the next keystroke confirms it.
  MarinaDebugLog(absl::StrCat(
      "UpdateCommand: ctx=", reinterpret_cast<uintptr_t>(context),
      " private_ctx=", reinterpret_cast<uintptr_t>(private_context),
      " symbols_flag=",
      private_context != nullptr && private_context->symbols_palette_visible(),
      " shortcuts_flag=",
      private_context != nullptr && private_context->shortcuts_window_visible(),
      " emitted_symbols=", app_info->has_symbols_palette_info(),
      " emitted_shortcuts=", app_info->has_shortcuts_info()));

  // Regardless of the value of |command->visible()| here, we should hide
  // all the UI elements whenever the current threads is not focused.
  BOOL thread_focus = FALSE;
  const HRESULT hr =
      text_service->GetThreadManager()->IsThreadFocus(&thread_focus);
  if (SUCCEEDED(hr) && (thread_focus == FALSE) &&
      !IsMarinaRendererUiForeground()) {
    // marinaMoji TEMPORARY (2026-08-08): the other way a just-opened palette
    // dies. If this fires on the update immediately after the toolbar click,
    // the culprit is thread focus, not the context mismatch above.
    MarinaDebugLog(absl::StrCat(
        "thread focus lost -- clearing toolbar/palette/shortcuts (was symbols=",
        app_info->has_symbols_palette_info(),
        " shortcuts=", app_info->has_shortcuts_info(), ")"));
    command->set_visible(false);
    // marinaMoji: also hide the floating toolbar, which is otherwise gated
    // by |ShowToolbar| independent of |command->visible()|.
    app_info->set_ui_visibilities(app_info->ui_visibilities() &
                                  ~static_cast<int>(ApplicationInfo::ShowToolbar));
    // marinaMoji: also close the Symbols Palette (same independent-of-
    // |command->visible()| reasoning) and clear the already-emitted
    // SymbolsPaletteInfo so this UPDATE doesn't ask the renderer to show it.
    if (private_context != nullptr) {
      private_context->set_symbols_palette_visible(false);
      private_context->set_shortcuts_window_visible(false);
    }
    app_info->clear_symbols_palette_info();
    app_info->clear_shortcuts_info();
  }
}

// This class is an implementation class for the ITfEditSession classes, which
// is an observer for exclusively read the date from the text store.
class UpdateUiEditSessionImpl final : public TipComImplements<ITfEditSession> {
 public:
  // The ITfEditSession interface method.
  // This function is called back by the TSF thread manager when an edit
  // request is granted.
  STDMETHODIMP DoEditSession(TfEditCookie edit_cookie) override {
    RendererCommand command;
    bool no_layout = false;
    UpdateCommand(text_service_.get(), context_.get(), edit_cookie, &command,
                  &no_layout);
    if (!no_layout || !command.visible()) {
      Win32RendererClient::OnUpdated(command);
    }
    return S_OK;
  }

  static bool BeginRequest(TipTextService* text_service, ITfContext* context) {
    // When RequestEditSession fails, it does not maintain the reference count.
    // So we need to ensure that AddRef/Release should be called at least once
    // per object.
    wil::com_ptr_nothrow<ITfEditSession> edit_session(
        new UpdateUiEditSessionImpl(text_service, context));

    HRESULT edit_session_result = S_OK;
    const HRESULT result = context->RequestEditSession(
        text_service->GetClientID(), edit_session.get(),
        TF_ES_ASYNCDONTCARE | TF_ES_READ, &edit_session_result);
    return SUCCEEDED(result);
  }

 private:
  UpdateUiEditSessionImpl(wil::com_ptr_nothrow<TipTextService> text_service,
                          wil::com_ptr_nothrow<ITfContext> context)
      : text_service_(std::move(text_service)), context_(std::move(context)) {}

  wil::com_ptr_nothrow<TipTextService> text_service_;
  wil::com_ptr_nothrow<ITfContext> context_;
};

}  // namespace

void TipUiHandlerConventional::OnActivate(TipTextService* text_service) {
  ITfThreadMgr* thread_mgr = text_service->GetThreadManager();
  wil::com_ptr_nothrow<ITfDocumentMgr> document;
  if (FAILED(thread_mgr->GetFocus(&document))) {
    return;
  }
  OnFocusChange(text_service, document.get());
}

void TipUiHandlerConventional::OnDeactivate() {
  // marinaMoji: explicitly tell the renderer to hide everything (including
  // the floating toolbar and Symbols Palette, which are gated by
  // |ApplicationInfo::ShowToolbar| / |has_symbols_palette_info()| rather than
  // |visible()|) before tearing down IPC. Without this, whatever the
  // renderer was last showing stays on screen after the IME deactivates.
  RendererCommand command;
  command.set_type(RendererCommand::UPDATE);
  command.set_visible(false);
  Win32RendererClient::OnUpdated(command);
  Win32RendererClient::OnUIThreadUninitialized();
}

void TipUiHandlerConventional::OnFocusChange(
    TipTextService* text_service, ITfDocumentMgr* focused_document_manager) {
  if (!focused_document_manager) {
    // Empty document (including TSF thread-focus loss). Hide the renderer,
    // unless the user is clicking marinaMoji UI in the renderer process
    // (Shortcuts tabs, palette, toolbar). That also kills thread focus but
    // is not "left the IME".
    if (IsMarinaRendererUiForeground()) {
      return;
    }
    RendererCommand command;
    command.set_type(RendererCommand::UPDATE);
    command.set_visible(false);
    Win32RendererClient::OnUpdated(command);
    return;
  }

  wil::com_ptr_nothrow<ITfContext> context;
  if (FAILED(focused_document_manager->GetBase(&context))) {
    return;
  }
  if (!context) {
    return;
  }
  UpdateUiEditSessionImpl::BeginRequest(text_service, context.get());
}

bool TipUiHandlerConventional::Update(TipTextService* text_service,
                                      ITfContext* context,
                                      TfEditCookie read_cookie) {
  RendererCommand command;
  bool no_layout = false;
  UpdateCommand(text_service, context, read_cookie, &command, &no_layout);
  if (!no_layout || !command.visible()) {
    Win32RendererClient::OnUpdated(command);
  }
  return true;
}

bool TipUiHandlerConventional::OnDllProcessAttach(HINSTANCE module_handle,
                                                  bool static_loading) {
  Win32RendererClient::OnModuleLoaded(module_handle);
  return true;
}

void TipUiHandlerConventional::OnDllProcessDetach(HINSTANCE module_handle,
                                                  bool process_shutdown) {
  Win32RendererClient::OnModuleUnloaded();
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
