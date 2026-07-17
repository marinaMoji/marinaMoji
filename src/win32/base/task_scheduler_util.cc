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

#include "win32/base/task_scheduler_util.h"

// clang-format off
#include <windows.h>
#include <taskschd.h>
#include <wil/com.h>
#include <wil/resource.h>
// clang-format on

#include <string>
#include <string_view>

namespace mozc {
namespace win32 {
namespace {

wil::unique_bstr MakeBstr(std::wstring_view value) {
  return wil::unique_bstr(::SysAllocStringLen(
      value.data(), static_cast<UINT>(value.size())));
}

VARIANT MakeEmptyVariant() {
  VARIANT value;
  ::VariantInit(&value);
  return value;
}

// Connects to the local Task Scheduler service.
HRESULT ConnectTaskService(wil::com_ptr_nothrow<ITaskService>& service) {
  HRESULT hr = ::CoCreateInstance(CLSID_TaskScheduler, nullptr,
                                  CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&service));
  if (FAILED(hr)) {
    return hr;
  }
  VARIANT empty = MakeEmptyVariant();
  return service->Connect(empty, empty, empty, empty);
}

// Returns the root ("\") task folder, where marinaMoji registers its sync
// task (matching how the installer's other components live directly under
// Program Files rather than in a nested folder tree).
HRESULT GetRootFolder(ITaskService* service,
                     wil::com_ptr_nothrow<ITaskFolder>& root_folder) {
  const wil::unique_bstr root_path = MakeBstr(L"\\");
  return service->GetFolder(root_path.get(), &root_folder);
}

}  // namespace

HRESULT TaskSchedulerUtil::RegisterLogonTask(std::wstring_view task_name,
                                             std::wstring_view exe_path,
                                             std::wstring_view args) {
  wil::com_ptr_nothrow<ITaskService> service;
  HRESULT hr = ConnectTaskService(service);
  if (FAILED(hr)) {
    return hr;
  }

  wil::com_ptr_nothrow<ITaskFolder> root_folder;
  hr = GetRootFolder(service.get(), root_folder);
  if (FAILED(hr)) {
    return hr;
  }

  wil::com_ptr_nothrow<ITaskDefinition> task;
  hr = service->NewTask(0, &task);
  if (FAILED(hr)) {
    return hr;
  }

  wil::com_ptr_nothrow<IRegistrationInfo> reg_info;
  if (SUCCEEDED(task->get_RegistrationInfo(&reg_info)) && reg_info) {
    const wil::unique_bstr author = MakeBstr(L"marinaMoji");
    reg_info->put_Author(author.get());
  }

  // Run only while the registering user is logged on interactively; no
  // stored password is needed (TASK_LOGON_INTERACTIVE_TOKEN), and leaving
  // IPrincipal::UserId unset means "the user registering the task" per
  // Task Scheduler's documented default.
  wil::com_ptr_nothrow<IPrincipal> principal;
  hr = task->get_Principal(&principal);
  if (FAILED(hr)) {
    return hr;
  }
  hr = principal->put_LogonType(TASK_LOGON_INTERACTIVE_TOKEN);
  if (FAILED(hr)) {
    return hr;
  }

  // Task Scheduler has no direct "keep this process alive" primitive like
  // launchd's KeepAlive or systemd's Restart=on-failure applied to a single
  // long-lived process; RestartCount/RestartInterval only cover the task's
  // own exit, which is what we rely on if the --daemon sleep-loop process
  // ever crashes. No execution time limit, since --daemon runs indefinitely.
  wil::com_ptr_nothrow<ITaskSettings> settings;
  hr = task->get_Settings(&settings);
  if (SUCCEEDED(hr) && settings) {
    settings->put_StartWhenAvailable(VARIANT_TRUE);
    settings->put_DisallowStartIfOnBatteries(VARIANT_FALSE);
    settings->put_StopIfGoingOnBatteries(VARIANT_FALSE);
    const wil::unique_bstr no_time_limit = MakeBstr(L"PT0S");
    settings->put_ExecutionTimeLimit(no_time_limit.get());
    settings->put_RestartCount(3);
    const wil::unique_bstr restart_interval = MakeBstr(L"PT1M");
    settings->put_RestartInterval(restart_interval.get());
  }

  wil::com_ptr_nothrow<ITriggerCollection> triggers;
  hr = task->get_Triggers(&triggers);
  if (FAILED(hr)) {
    return hr;
  }
  wil::com_ptr_nothrow<ITrigger> trigger;
  hr = triggers->Create(TASK_TRIGGER_LOGON, &trigger);
  if (FAILED(hr)) {
    return hr;
  }
  wil::com_ptr_nothrow<ILogonTrigger> logon_trigger;
  hr = trigger->QueryInterface(IID_PPV_ARGS(&logon_trigger));
  if (FAILED(hr)) {
    return hr;
  }
  // Give the user's sync client (Nextcloud/Syncthing/OneDrive/...) a head
  // start to mount/connect before the daemon looks for the sync folder.
  const wil::unique_bstr delay = MakeBstr(L"PT30S");
  logon_trigger->put_Delay(delay.get());

  wil::com_ptr_nothrow<IActionCollection> actions;
  hr = task->get_Actions(&actions);
  if (FAILED(hr)) {
    return hr;
  }
  wil::com_ptr_nothrow<IAction> action;
  hr = actions->Create(TASK_ACTION_EXEC, &action);
  if (FAILED(hr)) {
    return hr;
  }
  wil::com_ptr_nothrow<IExecAction> exec_action;
  hr = action->QueryInterface(IID_PPV_ARGS(&exec_action));
  if (FAILED(hr)) {
    return hr;
  }
  const wil::unique_bstr path_bstr = MakeBstr(exe_path);
  hr = exec_action->put_Path(path_bstr.get());
  if (FAILED(hr)) {
    return hr;
  }
  if (!args.empty()) {
    const wil::unique_bstr args_bstr = MakeBstr(args);
    hr = exec_action->put_Arguments(args_bstr.get());
    if (FAILED(hr)) {
      return hr;
    }
  }

  const wil::unique_bstr name_bstr = MakeBstr(task_name);
  VARIANT register_empty = MakeEmptyVariant();
  wil::com_ptr_nothrow<IRegisteredTask> registered_task;
  return root_folder->RegisterTaskDefinition(
      name_bstr.get(), task.get(), TASK_CREATE_OR_UPDATE, register_empty,
      register_empty, TASK_LOGON_INTERACTIVE_TOKEN, register_empty,
      &registered_task);
}

HRESULT TaskSchedulerUtil::UnregisterTask(std::wstring_view task_name) {
  wil::com_ptr_nothrow<ITaskService> service;
  if (FAILED(ConnectTaskService(service))) {
    // Nothing to clean up if Task Scheduler itself is unreachable.
    return S_OK;
  }
  wil::com_ptr_nothrow<ITaskFolder> root_folder;
  if (FAILED(GetRootFolder(service.get(), root_folder))) {
    return S_OK;
  }
  const wil::unique_bstr name_bstr = MakeBstr(task_name);
  // DeleteTask fails if the task does not exist; that is not an error from
  // this function's point of view (nothing left to unregister).
  root_folder->DeleteTask(name_bstr.get(), 0);
  return S_OK;
}

}  // namespace win32
}  // namespace mozc
