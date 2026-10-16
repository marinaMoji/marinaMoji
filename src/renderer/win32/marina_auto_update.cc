// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0

#include "renderer/win32/marina_auto_update.h"

#include <windows.h>
#include <bcrypt.h>
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
#include "absl/strings/string_view.h"
#include "base/file_util.h"
#include "base/marina_github_releases.h"
#include "base/marina_update_cleanup.h"
#include "base/marina_update_log.h"
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
  // The response's Content-Length header, if the server sent one. Used to
  // catch a connection that drops mid-transfer but still leaves
  // WinHttpQueryDataAvailable/WinHttpReadData looking like a clean EOF.
  std::optional<int64_t> content_length;

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

  DWORD content_length = 0;
  DWORD content_length_size = sizeof(content_length);
  if (::WinHttpQueryHeaders(
          handles.request.get(),
          WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, nullptr,
          &content_length, &content_length_size, nullptr)) {
    handles.content_length = content_length;
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

// Writes the full response body to |dest_path|. If |expected_length| is set
// (from the response's Content-Length header), the total bytes actually
// written must match it exactly -- WinHttpQueryDataAvailable/WinHttpReadData
// returning 0 can otherwise look like a clean end-of-stream even when the
// connection dropped mid-transfer, which would silently hand the caller a
// truncated installer.
bool WriteBodyToFile(HINTERNET request, const std::wstring& dest_path,
                     std::optional<int64_t> expected_length) {
  wil::unique_hfile file(::CreateFileW(
      dest_path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
      FILE_ATTRIBUTE_NORMAL, nullptr));
  if (!file) {
    return false;
  }
  std::string buffer;
  int64_t total_written = 0;
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
    total_written += written;
  }
  if (expected_length.has_value() && total_written != *expected_length) {
    return false;
  }
  return true;
}

// Lowercase-hex SHA-256 of |path|'s contents, or nullopt on any I/O or
// BCrypt failure. Re-reads the file after WriteBodyToFile rather than
// hashing while streaming: installers are a few tens of MB at most, so the
// extra pass is cheap, and it keeps the download loop above unchanged.
std::optional<std::string> Sha256HexOfFile(const std::wstring& path) {
  wil::unique_hfile file(::CreateFileW(path.c_str(), GENERIC_READ,
                                       FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                       FILE_ATTRIBUTE_NORMAL, nullptr));
  if (!file) {
    return std::nullopt;
  }

  wil::unique_bcrypt_algorithm alg_owner;
  if (!BCRYPT_SUCCESS(::BCryptOpenAlgorithmProvider(
          &alg_owner, BCRYPT_SHA256_ALGORITHM, nullptr, 0))) {
    return std::nullopt;
  }
  const BCRYPT_ALG_HANDLE alg = alg_owner.get();

  DWORD hash_object_size = 0;
  DWORD result_size = 0;
  if (!BCRYPT_SUCCESS(::BCryptGetProperty(
          alg, BCRYPT_OBJECT_LENGTH,
          reinterpret_cast<PUCHAR>(&hash_object_size), sizeof(hash_object_size),
          &result_size, 0))) {
    return std::nullopt;
  }
  DWORD hash_length = 0;
  if (!BCRYPT_SUCCESS(::BCryptGetProperty(
          alg, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hash_length),
          sizeof(hash_length), &result_size, 0))) {
    return std::nullopt;
  }

  std::vector<UCHAR> hash_object(hash_object_size);
  wil::unique_bcrypt_hash hash_owner;
  if (!BCRYPT_SUCCESS(::BCryptCreateHash(alg, &hash_owner, hash_object.data(),
                                         hash_object_size, nullptr, 0, 0))) {
    return std::nullopt;
  }
  const BCRYPT_HASH_HANDLE hash = hash_owner.get();

  std::vector<UCHAR> buffer(1 << 16);
  for (;;) {
    DWORD read = 0;
    if (!::ReadFile(file.get(), buffer.data(),
                    static_cast<DWORD>(buffer.size()), &read, nullptr)) {
      return std::nullopt;
    }
    if (read == 0) {
      break;
    }
    if (!BCRYPT_SUCCESS(
            ::BCryptHashData(hash, buffer.data(), read, 0))) {
      return std::nullopt;
    }
  }

  std::vector<UCHAR> digest(hash_length);
  if (!BCRYPT_SUCCESS(
          ::BCryptFinishHash(hash, digest.data(), hash_length, 0))) {
    return std::nullopt;
  }

  static constexpr char kHex[] = "0123456789abcdef";
  std::string hex;
  hex.reserve(digest.size() * 2);
  for (UCHAR byte : digest) {
    hex.push_back(kHex[byte >> 4]);
    hex.push_back(kHex[byte & 0xF]);
  }
  return hex;
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

// Downloads |url_utf8| to |dest_path|, verifying the transfer completed (via
// Content-Length) and, if |expected_sha256_hex| is non-empty, that the
// downloaded bytes hash to it. A mismatch on either check deletes the
// partial/untrusted file and returns false rather than leaving something on
// disk the caller might go on to execute.
bool DownloadToFile(const std::string& url_utf8, const std::wstring& dest_path,
                    absl::string_view expected_sha256_hex) {
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
  if (!WriteBodyToFile(handles->get(), dest_path, handles->content_length)) {
    ::DeleteFileW(dest_path.c_str());
    return false;
  }
  if (!expected_sha256_hex.empty()) {
    // Both sides are lowercase hex by construction (Sha256HexOfFile above;
    // GitHub's asset "digest" field), so a plain compare is enough.
    const auto actual = Sha256HexOfFile(dest_path);
    if (!actual.has_value() || *actual != expected_sha256_hex) {
      ::DeleteFileW(dest_path.c_str());
      return false;
    }
  }
  return true;
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

// True unless this process is in a job that forbids creating child processes.
//
// The renderer is started by RendererClient through WinSandbox with
// use_locked_down_job (ActiveProcessLimit = 1, low integrity, restricted
// token), so it can neither launch the downloaded installer nor open a
// browser: ShellExecute / CreateProcess simply fail. The offer's "Download &
// Install" and "Open release page" buttons used to be dead for that reason,
// with the failure swallowed. Settings runs at normal integrity and can do
// both, so the offer points there instead.
bool CanSpawnProcesses() {
  JOBOBJECT_BASIC_LIMIT_INFORMATION info = {};
  // A null job handle queries the job the calling process belongs to.
  if (!::QueryInformationJobObject(nullptr, JobObjectBasicLimitInformation,
                                   &info, sizeof(info), nullptr)) {
    return true;  // not in a job (or cannot tell): assume unrestricted
  }
  const bool limited = (info.LimitFlags & JOB_OBJECT_LIMIT_ACTIVE_PROCESS) &&
                       info.ActiveProcessLimit <= 1;
  return !limited;
}

// Plain MessageBoxW rather than TaskDialogIndirect: this code now runs in
// marinamoji_sync.exe, which has no comctl32 v6 manifest. A binary that
// imports TaskDialogIndirect without one fails to load at all ("entry point
// not found"), and that would have taken sync down with it. MB_TOPMOST and
// MB_SETFOREGROUND make sure a dialog raised by a background process is seen.
constexpr UINT kDialogFlags = MB_SETFOREGROUND | MB_TOPMOST;

// Used when this process cannot spawn children (see CanSpawnProcesses()).
// Not the case in the sync daemon, but kept as a safe fallback.
void PresentUpdateNotice(const MarinaGitHubRelease& release) {
  const std::wstring text = mozc::win32::Utf8ToWide(absl::StrCat(
      "A newer release is available: ", release.tag_name,
      "\n\nTo install it, open marinaMoji Preferences (language bar menu > "
      "Properties), go to the Misc tab and choose \"Check for updates...\"."));
  ::MessageBoxW(nullptr, text.c_str(), L"marinaMoji",
                MB_OK | MB_ICONINFORMATION | kDialogFlags);
}

// Offers the update. Runs on this feature's own background thread, never a UI
// thread, so blocking in the modal box is fine.
void PresentUpdateOffer(const MarinaGitHubRelease& release,
                        const std::string& msi_url) {
  std::wstring text = mozc::win32::Utf8ToWide(
      absl::StrCat("A newer release is available: ", release.tag_name, "\n\n"));
  UINT type = MB_ICONINFORMATION | kDialogFlags;
  if (!msi_url.empty()) {
    text +=
        L"Download the installer and run it now? You will still need to "
        L"approve the install. The installer is signed, but Windows "
        L"SmartScreen may still warn first while the certificate builds "
        L"reputation; choose \"More info\" > \"Run anyway\".\n\n"
        L"Yes: download and install\nNo: open the release page\n"
        L"Cancel: remind me tomorrow";
    type |= MB_YESNOCANCEL;
  } else {
    text += L"Open the release page to download it manually?";
    type |= MB_YESNO;
  }
  const int clicked = ::MessageBoxW(nullptr, text.c_str(), L"marinaMoji", type);

  if (clicked == IDYES && !msi_url.empty()) {
    const std::wstring dest = mozc::win32::Utf8ToWide(FileUtil::JoinPath(
        SystemUtil::GetUserProfileDirectory(),
        absl::StrCat("marinaMoji-update-",
                     mozc::win32::WideToUtf8(
                         SanitizeForFilename(release.tag_name)),
                     ".msi")));
    const std::string expected_sha256 =
        FindMarinaMsiSha256Digest(release, MarinaHostWindowsArchToken());
    bool ok = DownloadToFile(msi_url, dest, expected_sha256);
    if (ok) {
      ok = WinUtil::ShellExecuteInSystemDir(L"open", dest.c_str(), nullptr);
    }
    if (!ok) {
      MarinaUpdateLog(absl::StrCat("update: could not download or open the ",
                                   "installer for ", release.tag_name));
      ::MessageBoxW(
          nullptr,
          L"Could not download or open the installer. Try again later from "
          L"Preferences > Misc > Check for updates...",
          L"marinaMoji update failed",
          MB_OK | MB_ICONWARNING | kDialogFlags);
    }
  } else if ((msi_url.empty() && clicked == IDYES) ||
             (!msi_url.empty() && clicked == IDNO)) {
    Process::OpenBrowser(release.html_url);
  }
}

// Returns true if the releases were fetched and parsed, whether or not a newer
// one exists; false if the check could not be completed (offline, DNS, proxy,
// GitHub error). Only a completed check may consume the 24h throttle window --
// otherwise one failed attempt, e.g. at login before the network is up, would
// silence update checks for a whole day.
bool RunCheckOnce() {
  // Earlier updates leave their downloaded MSI behind; drop the ones that are
  // no longer newer than this version (base/marina_update_cleanup.h).
  const int removed = RemoveStaleMarinaUpdateInstallers(
      SystemUtil::GetUserProfileDirectory(), Version::GetProductVersion());
  if (removed > 0) {
    MarinaUpdateLog(
        absl::StrCat("update check: removed ", removed, " old installer(s)"));
  }
  const auto json = FetchGitHubReleasesJson();
  if (!json.has_value()) {
    MarinaUpdateLog("update check: could not fetch releases");
    return false;
  }
  const auto releases = ParseMarinaGitHubReleasesJson(*json);
  if (releases.empty()) {
    MarinaUpdateLog("update check: no releases parsed");
    return false;
  }

  const std::shared_ptr<const config::Config> current_config =
      config::ConfigHandler::GetSharedConfig();
  const bool include_unstable = current_config->include_unstable_updates();

  const std::string current_version = Version::GetProductVersion();
  const auto newer =
      SelectNewerMarinaRelease(releases, current_version, include_unstable);
  if (!newer.has_value()) {
    MarinaUpdateLog(
        absl::StrCat("update check: ", current_version, " is up to date"));
    return true;
  }
  MarinaUpdateLog(absl::StrCat("update check: ", newer->tag_name,
                               " is newer than ", current_version));
  // Checking is frequent, bothering the user is not: the same release is shown
  // at most once a day (whatever was answered), while a newer one is shown at
  // the next check.
  if (!ShouldOfferMarinaUpdate(newer->tag_name)) {
    MarinaUpdateLog(absl::StrCat("update check: ", newer->tag_name,
                                 " was already offered recently; not asking",
                                 " again yet"));
    return true;
  }
  MarkMarinaUpdateOffered(newer->tag_name);
  if (!CanSpawnProcesses()) {
    PresentUpdateNotice(*newer);
    return true;
  }
  std::string msi_url;
  if (const auto url =
          FindMarinaMsiDownloadUrl(*newer, MarinaHostWindowsArchToken());
      url.has_value()) {
    msi_url = *url;
  }
  PresentUpdateOffer(*newer, msi_url);
  return true;
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
        if (RunCheckOnce()) {
          MarkMarinaAutoUpdateCheckRan();
        }
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
