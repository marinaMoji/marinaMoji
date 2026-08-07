// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0

#include "renderer/win32/toolbar_accessible.h"

#include <oleacc.h>
#include <oleauto.h>
#include <windows.h>

#include <new>

#include "renderer/win32/toolbar_window.h"

namespace mozc {
namespace renderer {
namespace win32 {

namespace {

constexpr int kButtonCount =
    static_cast<int>(ToolbarWindow::ButtonId::kNumButtons);

// MSAA child ids are 1-based; 0 (CHILDID_SELF) is the toolbar itself.
bool ChildIdToButton(const VARIANT& var_child,
                     ToolbarWindow::ButtonId* out_button) {
  if (var_child.vt != VT_I4) {
    return false;
  }
  const LONG child_id = var_child.lVal;
  if (child_id < 1 || child_id > kButtonCount) {
    return false;
  }
  *out_button = static_cast<ToolbarWindow::ButtonId>(child_id - 1);
  return true;
}

bool IsSelf(const VARIANT& var_child) {
  return var_child.vt == VT_I4 && var_child.lVal == CHILDID_SELF;
}

class ToolbarAccessible final : public IAccessible {
 public:
  ToolbarAccessible(ToolbarWindow* window, HWND window_handle)
      : ref_count_(1), window_(window), window_handle_(window_handle) {}

  ToolbarAccessible(const ToolbarAccessible&) = delete;
  ToolbarAccessible& operator=(const ToolbarAccessible&) = delete;

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
    if (ppv == nullptr) {
      return E_INVALIDARG;
    }
    if (riid == IID_IUnknown || riid == IID_IDispatch ||
        riid == IID_IAccessible) {
      *ppv = static_cast<IAccessible*>(this);
      AddRef();
      return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
  }

  STDMETHODIMP_(ULONG) AddRef() override {
    return ::InterlockedIncrement(&ref_count_);
  }

  STDMETHODIMP_(ULONG) Release() override {
    const LONG count = ::InterlockedDecrement(&ref_count_);
    if (count == 0) {
      delete this;
    }
    return count;
  }

  // IDispatch. MSAA clients reach IAccessible through the vtable, not through
  // late binding, so the automation half is intentionally unimplemented --
  // the same thing ATL's CAccessibleProxy does for a non-scriptable object.
  STDMETHODIMP GetTypeInfoCount(UINT* count) override {
    if (count == nullptr) {
      return E_INVALIDARG;
    }
    *count = 0;
    return S_OK;
  }
  STDMETHODIMP GetTypeInfo(UINT, LCID, ITypeInfo**) override {
    return E_NOTIMPL;
  }
  STDMETHODIMP GetIDsOfNames(REFIID, LPOLESTR*, UINT, LCID,
                               DISPID*) override {
    return E_NOTIMPL;
  }
  STDMETHODIMP Invoke(DISPID, REFIID, LCID, WORD, DISPPARAMS*, VARIANT*,
                        EXCEPINFO*, UINT*) override {
    return E_NOTIMPL;
  }

  // IAccessible
  STDMETHODIMP get_accParent(IDispatch** parent) override {
    if (parent == nullptr) {
      return E_INVALIDARG;
    }
    *parent = nullptr;
    // The window object above the client area, i.e. what MSAA would have
    // returned had we not overridden OBJID_CLIENT.
    return ::AccessibleObjectFromWindow(window_handle_, OBJID_WINDOW,
                                        IID_IAccessible,
                                        reinterpret_cast<void**>(parent));
  }

  STDMETHODIMP get_accChildCount(long* count) override {
    if (count == nullptr) {
      return E_INVALIDARG;
    }
    *count = kButtonCount;
    return S_OK;
  }

  STDMETHODIMP get_accChild(VARIANT var_child, IDispatch** child) override {
    if (child == nullptr) {
      return E_INVALIDARG;
    }
    // Buttons are leaf children with no object of their own; MSAA expects
    // S_FALSE plus a null pointer for that, and the caller then addresses
    // them by (this, child_id).
    *child = nullptr;
    return S_FALSE;
  }

  STDMETHODIMP get_accName(VARIANT var_child, BSTR* name) override {
    if (name == nullptr) {
      return E_INVALIDARG;
    }
    *name = nullptr;
    if (IsSelf(var_child)) {
      wchar_t title[128] = {};
      ::GetWindowTextW(window_handle_, title, ARRAYSIZE(title));
      *name = ::SysAllocString(title);
      return *name != nullptr ? S_OK : E_OUTOFMEMORY;
    }
    ToolbarWindow::ButtonId button;
    if (!ChildIdToButton(var_child, &button)) {
      return E_INVALIDARG;
    }
    *name = ::SysAllocString(ToolbarWindow::GetButtonName(button));
    return *name != nullptr ? S_OK : E_OUTOFMEMORY;
  }

  STDMETHODIMP get_accValue(VARIANT, BSTR* value) override {
    if (value == nullptr) {
      return E_INVALIDARG;
    }
    *value = nullptr;
    return S_FALSE;
  }

  STDMETHODIMP get_accDescription(VARIANT, BSTR* description) override {
    if (description == nullptr) {
      return E_INVALIDARG;
    }
    *description = nullptr;
    return S_FALSE;
  }

  STDMETHODIMP get_accRole(VARIANT var_child, VARIANT* role) override {
    if (role == nullptr) {
      return E_INVALIDARG;
    }
    ::VariantInit(role);
    role->vt = VT_I4;
    if (IsSelf(var_child)) {
      role->lVal = ROLE_SYSTEM_TOOLBAR;
      return S_OK;
    }
    ToolbarWindow::ButtonId button;
    if (!ChildIdToButton(var_child, &button)) {
      ::VariantInit(role);
      return E_INVALIDARG;
    }
    role->lVal = ROLE_SYSTEM_PUSHBUTTON;
    return S_OK;
  }

  STDMETHODIMP get_accState(VARIANT var_child, VARIANT* state) override {
    if (state == nullptr) {
      return E_INVALIDARG;
    }
    ::VariantInit(state);
    state->vt = VT_I4;
    state->lVal = 0;
    if (!::IsWindowVisible(window_handle_)) {
      state->lVal |= STATE_SYSTEM_INVISIBLE;
    }
    if (IsSelf(var_child)) {
      return S_OK;
    }
    ToolbarWindow::ButtonId button;
    if (!ChildIdToButton(var_child, &button)) {
      ::VariantInit(state);
      return E_INVALIDARG;
    }
    // The toolbar is WS_EX_NOACTIVATE by design (taking focus would tear
    // down the application's text context), so its buttons are explicitly
    // not focusable. They remain clickable, and accDoDefaultAction below
    // gives assistive tech a way to invoke them without a click.
    return S_OK;
  }

  STDMETHODIMP get_accHelp(VARIANT, BSTR* help) override {
    if (help == nullptr) {
      return E_INVALIDARG;
    }
    *help = nullptr;
    return S_FALSE;
  }

  STDMETHODIMP get_accHelpTopic(BSTR* file, VARIANT, long* topic) override {
    if (file != nullptr) {
      *file = nullptr;
    }
    if (topic != nullptr) {
      *topic = 0;
    }
    return S_FALSE;
  }

  STDMETHODIMP get_accKeyboardShortcut(VARIANT, BSTR* shortcut) override {
    if (shortcut == nullptr) {
      return E_INVALIDARG;
    }
    *shortcut = nullptr;
    return S_FALSE;
  }

  STDMETHODIMP get_accFocus(VARIANT* focus) override {
    if (focus == nullptr) {
      return E_INVALIDARG;
    }
    ::VariantInit(focus);
    return S_FALSE;  // never focusable, see get_accState
  }

  STDMETHODIMP get_accSelection(VARIANT* selection) override {
    if (selection == nullptr) {
      return E_INVALIDARG;
    }
    ::VariantInit(selection);
    return S_FALSE;
  }

  STDMETHODIMP get_accDefaultAction(VARIANT var_child,
                                      BSTR* action) override {
    if (action == nullptr) {
      return E_INVALIDARG;
    }
    *action = nullptr;
    ToolbarWindow::ButtonId button;
    if (!ChildIdToButton(var_child, &button)) {
      return S_FALSE;
    }
    *action = ::SysAllocString(L"Press");
    return *action != nullptr ? S_OK : E_OUTOFMEMORY;
  }

  STDMETHODIMP accSelect(long, VARIANT) override { return S_FALSE; }

  STDMETHODIMP accLocation(long* left, long* top, long* width, long* height,
                             VARIANT var_child) override {
    if (left == nullptr || top == nullptr || width == nullptr ||
        height == nullptr) {
      return E_INVALIDARG;
    }
    RECT rect = {};
    if (IsSelf(var_child)) {
      if (!::GetWindowRect(window_handle_, &rect)) {
        return E_FAIL;
      }
    } else {
      ToolbarWindow::ButtonId button;
      CRect button_rect;
      if (!ChildIdToButton(var_child, &button) ||
          !window_->GetButtonScreenRect(button, &button_rect)) {
        return E_INVALIDARG;
      }
      rect = button_rect;
    }
    *left = rect.left;
    *top = rect.top;
    *width = rect.right - rect.left;
    *height = rect.bottom - rect.top;
    return S_OK;
  }

  STDMETHODIMP accNavigate(long direction, VARIANT var_start,
                             VARIANT* end) override {
    if (end == nullptr) {
      return E_INVALIDARG;
    }
    ::VariantInit(end);
    LONG index;
    switch (direction) {
      case NAVDIR_FIRSTCHILD:
        if (!IsSelf(var_start)) {
          return S_FALSE;
        }
        index = 1;
        break;
      case NAVDIR_LASTCHILD:
        if (!IsSelf(var_start)) {
          return S_FALSE;
        }
        index = kButtonCount;
        break;
      case NAVDIR_NEXT:
      case NAVDIR_RIGHT:
        if (var_start.vt != VT_I4 || var_start.lVal < 1) {
          return S_FALSE;
        }
        index = var_start.lVal + 1;
        break;
      case NAVDIR_PREVIOUS:
      case NAVDIR_LEFT:
        if (var_start.vt != VT_I4 || var_start.lVal < 1) {
          return S_FALSE;
        }
        index = var_start.lVal - 1;
        break;
      default:
        return S_FALSE;
    }
    if (index < 1 || index > kButtonCount) {
      return S_FALSE;
    }
    end->vt = VT_I4;
    end->lVal = index;
    return S_OK;
  }

  STDMETHODIMP accHitTest(long screen_x, long screen_y,
                            VARIANT* child) override {
    if (child == nullptr) {
      return E_INVALIDARG;
    }
    ::VariantInit(child);
    for (int i = 0; i < kButtonCount; ++i) {
      CRect rect;
      if (!window_->GetButtonScreenRect(
              static_cast<ToolbarWindow::ButtonId>(i), &rect)) {
        continue;
      }
      if (rect.PtInRect(CPoint(screen_x, screen_y))) {
        child->vt = VT_I4;
        child->lVal = i + 1;
        return S_OK;
      }
    }
    RECT window_rect = {};
    if (::GetWindowRect(window_handle_, &window_rect) &&
        ::PtInRect(&window_rect, POINT{screen_x, screen_y})) {
      child->vt = VT_I4;
      child->lVal = CHILDID_SELF;
      return S_OK;
    }
    return S_FALSE;
  }

  STDMETHODIMP accDoDefaultAction(VARIANT var_child) override {
    ToolbarWindow::ButtonId button;
    if (!ChildIdToButton(var_child, &button)) {
      return S_FALSE;
    }
    // MSAA delivers this on an RPC thread; hand it to the window's own thread
    // rather than running the action (which may open a menu) from here.
    if (!::PostMessageW(window_handle_, ToolbarWindow::kActivateButtonMessage,
                        static_cast<WPARAM>(button), 0)) {
      return E_FAIL;
    }
    return S_OK;
  }

  STDMETHODIMP put_accName(VARIANT, BSTR) override { return E_NOTIMPL; }
  STDMETHODIMP put_accValue(VARIANT, BSTR) override { return E_NOTIMPL; }

 private:
  ~ToolbarAccessible() = default;

  LONG ref_count_;
  ToolbarWindow* window_;
  HWND window_handle_;
};

}  // namespace

void* CreateToolbarAccessible(ToolbarWindow* window, HWND window_handle) {
  if (window == nullptr || window_handle == nullptr) {
    return nullptr;
  }
  auto* accessible =
      new (std::nothrow) ToolbarAccessible(window, window_handle);
  return static_cast<IAccessible*>(accessible);
}

}  // namespace win32
}  // namespace renderer
}  // namespace mozc
