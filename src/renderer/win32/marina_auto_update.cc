// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0

#include "renderer/win32/marina_auto_update.h"

#include <windows.h>
#include <commctrl.h>
#include <winhttp.h>

#include <wil/resource.h>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cctype>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "absl/strings/str_cat.h"
#include "base/file_util.h"
#include "base/marina_github_releases.h"
#include "base/marina_update_throttle.h"
#include "base/process.h"
#include "base/system_util.h"
#include "base/version.h"
#include "base/win32/wide_char.h"
#include "base/win32/win_util.h"
#include "config/config_handler.h"
#include "protocol/config.pb.h"

namespace mozc {
namespace renderer {
namespace win32 {
namespace {

constexpr wchar_t kGitHubApiHost[] = L"api.github.com";
constexpr wchar_t kGitHubApiPath[] =
    L"/repos/marinaMoji/marinaMoji/releases?per_page=50";
constexpr wchar_t kUserAgent[] = L"marinaMoji-update-check";

// Re-checks whether 24h have passed at this cadence while the renderer stays
// alive, so a process that never restarts still notices the next day's
// window. Cheap: outside the 24h throttle this only reads a small file's
// mtime, no network traffic.
constexpr std::chrono::hours kRecheckInterval{2};

// Network timeouts, so a captive portal or dead proxy cannot wedge this
// thread forever -- it also owns the periodic recheck loop.
constexpr int kResolveTimeoutMs = 10'000;
constexpr int kConnectTimeoutMs = 10'000;
constexpr int kSendTimeoutMs = 15'000;
constexpr int kReceiveTimeoutMs = 20'000;

// Functor/stateless WIL deleter (not a raw function pointer): unique_ptr with
// a function-pointer deleter is not default-constructible, which made
// WinHttpGet unusable with std::optional and plain `WinHttpGet handles;`.
using UniqueHInternet =
    wil::unique_any<HINTERNET, decltype(&::WinHttpCloseHandle),
                    ::WinHttpCloseHandle>;

UniqueHInternet WrapHandle(HINTERNET handle) {
  return UniqueHInternet(handle);
}

void ApplyTimeouts(HINTERNET handle) {
  ::WinHttpSetTimeouts(handle, kResolveTimeoutMs, kConnectTimeoutMs,
                       kSendTimeoutMs, kReceiveTimeoutMs);
}

// Owns the full WinHTTP handle chain for one GET. Declared session → connect
// → request so destructors run in reverse (request, then connect, then
// session) — the safe close order. Returning only the request while letting
// session/connect go out of scope would invalidate the request: closing a
// parent HINTERNET indirectly invalidates its children
// (ERROR_INVALID_HANDLE on later ReadData / QueryDataAvailable).
struct WinHttpGet {
  UniqueHInternet session;
  UniqueHInternet connect;
  UniqueHInternet request;

  explicit operator bool() const { return static_cast<bool>(request); }
  HINTERNET get() const { return request.get(); }
};

// Opens a session/connection/request against |host| and returns the full
// handle chain once WinHttpReceiveResponse has succeeded and the status is
// 200. |extra_headers| is a single CRLF-joined header block, or nullptr.
std::optional<WinHttpGet> OpenSuccessfulGet(const std::wstring& host,
                                            INTERNET_PORT port, bool secure,
                                            const std::wstring& path,
                                            const wchar_t* extra_headers) {
  WinHttpGet handles;
  handles.session =
      WrapHandle(::WinHttpOpen(kUserAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                               WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS,
                               0));
  if (!handles.session) {
    return std::nullopt;
  }
  ApplyTimeouts(handles.session.get());

  handles.connect = WrapHandle(
      ::WinHttpConnect(handles.session.get(), host.c_str(), port, 0));
  if (!handles.connect) {
    return std::nullopt;
  }

  const DWORD flags = secure ? WINHTTP_FLAG_SECURE : 0;
  handles.request = WrapHandle(::WinHttpOpenRequest(
      handles.connect.get(), L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
      WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
  if (!handles.request) {
    return std::nullopt;
  }

  const DWORD header_len =
      extra_headers == nullptr ? 0 : static_cast<DWORD>(-1L);
  if (!::WinHttpSendRequest(handles.request.get(), extra_headers, header_len,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
    return std::nullopt;
  }
  if (!::WinHttpReceiveResponse(handles.request.get(), nullptr)) {
    return std::nullopt;
  }

  DWORD status = 0;
  DWORD status_size = sizeof(status);
  if (!::WinHttpQueryHeaders(
          handles.request.get(),
          WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr,
          &status, &status_size, nullptr) ||
      status != 200) {
    return std::nullopt;
  }
  return std::move(handles);
}

std::optional<std::string> ReadBody(HINTERNET request) {
  std::string body;
  for (;;) {
    DWORD available = 0;
    if (!::WinHttpQueryDataAvailable(request, &available)) {
      return std::nullopt;
    }
    if (available == 0) {
      break;
    }
    const size_t offset = body.size();
    body.resize(offset + available);
    DWORD read = 0;
    if (!::WinHttpReadData(request, body.data() + offset, available,
                           &read)) {
      return std::nullopt;
    }
    body.resize(offset + read);
    if (read == 0) {
      break;
    }
  }
  return body;
}

bool WriteBodyToFile(HINTERNET request, const std::wstring& dest_path) {
  wil::unique_hfile file(::CreateFileW(
      dest_path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
      FILE_ATTRIBUTE_NORMAL, nullptr));
  if (!file) {
    return false;
  }
  std::string buffer;
  for (;;) {
    DWORD available = 0;
    if (!::WinHttpQueryDataAvailable(request, &available)) {
      return false;
    }
    if (available == 0) {
      break;
    }
    buffer.resize(available);
    DWORD read = 0;
    if (!::WinHttpReadData(request, buffer.data(), available, &read)) {
      return false;
    }
    if (read == 0) {
      break;
    }
    DWORD written = 0;
    if (!::WriteFile(file.get(), buffer.data(), read, &written, nullptr) ||
        written != read) {
      return false;
    }
  }
  return true;
}

// Splits a full https URL into (host, path-and-query) for WinHttpConnect /
// WinHttpOpenRequest. Only used for GitHub's asset download URLs, which are
// always https.
struct CrackedUrl {
  std::wstring host;
  std::wstring path;
  INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
};

std::optional<CrackedUrl> CrackHttpsUrl(const std::wstring& url) {
  wchar_t host[256] = {};
  wchar_t path[2048] = {};
  URL_COMPONENTS components = {};
  components.dwStructSize = sizeof(components);
  components.lpszHostName = host;
  components.dwHostNameLength = std::size(host);
  components.lpszUrlPath = path;
  components.dwUrlPathLength = std::size(path);
  if (!::WinHttpCrackUrl(url.c_str(), 0, 0, &components)) {
    return std::nullopt;
  }
  if (components.nScheme != INTERNET_SCHEME_HTTPS) {
    // The GitHub-hosted asset redirect chain is always https; refuse to
    // follow a scheme downgrade rather than silently accepting one.
    return std::nullopt;
  }
  CrackedUrl result;
  result.host = host;
  result.path = path;
  result.port = components.nPort;
  return result;
}

std::optional<std::string> FetchGitHubReleasesJson() {
  auto handles = OpenSuccessfulGet(
      kGitHubApiHost, INTERNET_DEFAULT_HTTPS_PORT, /*secure=*/true,
      kGitHubApiPath, L"Accept: application/vnd.github+json\r\n");
  if (!handles.has_value()) {
    return std::nullopt;
  }
  return ReadBody(handles->get());
}

bool DownloadToFile(const std::string& url_utf8,
                    const std::wstring& dest_path) {
  const std::wstring url = mozc::win32::Utf8ToWide(url_utf8);
  const auto cracked = CrackHttpsUrl(url);
  if (!cracked.has_value()) {
    return false;
  }
  auto handles = OpenSuccessfulGet(cracked->host, cracked->port,
                                   /*secure=*/true, cracked->path, nullptr);
  if (!handles.has_value()) {
    return false;
  }
  return WriteBodyToFile(handles->get(), dest_path);
}

std::wstring SanitizeForFilename(const std::string& tag_name) {
  std::wstring safe = mozc::win32::Utf8ToWide(tag_name);
  for (wchar_t& c : safe) {
    const bool ok = (c < 128) &&
                    (std::isalnum(static_cast<unsigned char>(c)) ||
                     c == L'.' || c == L'-' || c == L'_');
    if (!ok) {
      c = L'_';
    }
  }
  return safe;
}

// Shows the three-button offer, mirroring mac's NSAlert
// (Download & Install… / Open release page / Later). Runs on the caller's
// thread, which is this feature's own dedicated background thread -- never
// the renderer's main message loop -- so blocking here is fine.
void PresentUpdateOffer(const MarinaGitHubRelease& release,
                        const std::string& msi_url) {
  const std::wstring message = mozc::win32::Utf8ToWide(
      absl::StrCat("A newer release is available: ", release.tag_name));

  constexpr int kInstallButtonId = 100;
  constexpr int kPageButtonId = 101;

  std::vector<TASKDIALOG_BUTTON> buttons;
  if (!msi_url.empty()) {
    buttons.push_back(TASKDIALOG_BUTTON{kInstallButtonId,
                                        L"Download && Install…"});
  }
  buttons.push_back(TASKDIALOG_BUTTON{kPageButtonId, L"Open release page"});
  // Explicit "Later" button (returns IDCANCEL) rather than relying on users
  // discovering that closing the dialog does the same thing -- mirrors mac's
  // NSAlert, which always shows all three choices as buttons.
  buttons.push_back(TASKDIALOG_BUTTON{IDCANCEL, L"Later"});

  const std::wstring content =
      msi_url.empty()
          ? L"Open the release page to download it manually?"
          : L"Download the installer and run it now? You will still need to "
            L"approve the install -- and since this build is unsigned for "
            L"now, Windows SmartScreen may warn first; choose \"More "
            L"info\" → \"Run anyway\", same as the initial install.";

  TASKDIALOGCONFIG config = {};
  config.cbSize = sizeof(config);
  config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT;
  config.pszWindowTitle = L"marinaMoji";
  config.pszMainIcon = TD_INFORMATION_ICON;
  config.pszMainInstruction = message.c_str();
  config.pszContent = content.c_str();
  config.pButtons = buttons.data();
  config.cButtons = static_cast<UINT>(buttons.size());
  config.nDefaultButton = msi_url.empty() ? kPageButtonId : kInstallButtonId;
  // No hOwner: the offer is not scoped to any one window, matching the
  // NSAlert popping up independent of whichever app the user is in.

  int clicked_id = IDCANCEL;
  if (FAILED(::TaskDialogIndirect(&config, &clicked_id, nullptr, nullptr))) {
    return;
  }

  if (clicked_id == kInstallButtonId) {
    const std::wstring dest = mozc::win32::Utf8ToWide(FileUtil::JoinPath(
        SystemUtil::GetUserProfileDirectory(),
        absl::StrCat("marinaMoji-update-",
                     mozc::win32::WideToUtf8(
                         SanitizeForFilename(release.tag_name)),
                     ".msi")));
    bool ok = DownloadToFile(msi_url, dest);
    if (ok) {
      ok = WinUtil::ShellExecuteInSystemDir(L"open", dest.c_str(), nullptr);
    }
    if (!ok) {
      ::MessageBoxW(
          nullptr,
          L"Could not download or open the installer. Try again later from "
          L"Settings → Misc → Check for updates…",
          L"marinaMoji update failed", MB_OK | MB_ICONWARNING);
    }
  } else if (clicked_id == kPageButtonId) {
    Process::OpenBrowser(release.html_url);
  }
}

void RunCheckOnce() {
  const auto json = FetchGitHubReleasesJson();
  if (!json.has_value()) {
    return;
  }
  const auto releases = ParseMarinaGitHubReleasesJson(*json);

  const std::shared_ptr<const config::Config> current_config =
      config::ConfigHandler::GetSharedConfig();
  const bool include_unstable = current_config->include_unstable_updates();

  const auto newer = SelectNewerMarinaRelease(
      releases, Version::GetProductVersion(), include_unstable);
  if (!newer.has_value()) {
    return;
  }
  std::string msi_url;
  if (const auto url =
          FindMarinaMsiDownloadUrl(*newer, MarinaHostWindowsArchToken());
      url.has_value()) {
    msi_url = *url;
  }
  PresentUpdateOffer(*newer, msi_url);
}

class AutoUpdateChecker {
 public:
  void Start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (thread_.joinable()) {
      return;  // already running
    }
    stop_requested_ = false;
    thread_ = std::thread(&AutoUpdateChecker::Run, this);
  }

  void Stop() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!thread_.joinable()) {
      return;
    }
    stop_requested_ = true;
    lock.unlock();
    wake_.notify_all();
    lock.lock();
    // Detach rather than join: Run() may be blocked inside RunCheckOnce()'s
    // network I/O, which has timeouts of up to several tens of seconds (see
    // kResolveTimeoutMs etc.), and process shutdown must not wait on that.
    // The common case -- the thread parked in the wait_for sleep -- exits
    // promptly on the notify above; the rare case of an in-flight network
    // call is simply abandoned, and the OS reclaims the thread and its
    // WinHTTP handles when the process exits regardless.
    thread_.detach();
  }

 private:
  void Run() {
    // First check happens right away (still subject to the 24h throttle
    // inside ShouldRunMarinaAutoUpdateCheck, so restarting the renderer
    // repeatedly cannot fan out checks); later iterations just re-test
    // whether the throttle window has opened since.
    for (;;) {
      const bool enabled = config::ConfigHandler::GetSharedConfig()
                               ->auto_check_for_updates();
      if (enabled && ShouldRunMarinaAutoUpdateCheck()) {
        MarkMarinaAutoUpdateCheckRan();
        RunCheckOnce();
      }

      std::unique_lock<std::mutex> lock(mutex_);
      if (wake_.wait_for(lock, kRecheckInterval,
                         [this] { return stop_requested_; })) {
        return;  // stop requested
      }
    }
  }

  std::mutex mutex_;
  std::condition_variable wake_;
  bool stop_requested_ = false;
  std::thread thread_;
};

AutoUpdateChecker& GetChecker() {
  static AutoUpdateChecker* checker = new AutoUpdateChecker();
  return *checker;
}

}  // namespace

void StartMarinaAutoUpdateChecker() { GetChecker().Start(); }

void StopMarinaAutoUpdateChecker() { GetChecker().Stop(); }

}  // namespace win32
}  // namespace renderer
}  // namespace mozc
