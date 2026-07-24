#include "base/marina_curl_fetch.h"

#include <array>
#include <cstdio>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"

#if defined(__APPLE__)
#include "base/process.h"
#endif  // __APPLE__

namespace mozc {
namespace {

#if defined(__APPLE__)

std::string ShellEscapeSingleQuotes(absl::string_view value) {
  std::string out;
  out.reserve(value.size() + 2);
  out.push_back('\'');
  for (const char c : value) {
    if (c == '\'') {
      out.append("'\\''");
    } else {
      out.push_back(c);
    }
  }
  out.push_back('\'');
  return out;
}

absl::StatusOr<std::string> RunCurlCapture(absl::string_view args) {
  const std::string command = absl::StrCat("curl ", args);
  FILE* pipe = ::popen(command.c_str(), "r");
  if (pipe == nullptr) {
    return absl::UnavailableError("Could not start curl.");
  }
  std::string body;
  std::array<char, 4096> buffer;
  while (true) {
    const size_t n = ::fread(buffer.data(), 1, buffer.size(), pipe);
    if (n > 0) {
      body.append(buffer.data(), n);
    }
    if (n < buffer.size()) {
      break;
    }
  }
  const int status = ::pclose(pipe);
  if (status != 0) {
    return absl::UnavailableError("curl failed.");
  }
  return body;
}

#endif  // __APPLE__

}  // namespace

absl::StatusOr<std::string> MarinaCurlGet(absl::string_view url) {
#if defined(__APPLE__)
  const std::string args = absl::StrCat(
      "-fsSL -A marinaMoji-update-check -H ",
      ShellEscapeSingleQuotes("Accept: application/vnd.github+json"), " ",
      ShellEscapeSingleQuotes(url));
  return RunCurlCapture(args);
#else
  (void)url;
  return absl::UnimplementedError("MarinaCurlGet is only implemented on macOS.");
#endif
}

absl::Status MarinaCurlDownload(absl::string_view url,
                                absl::string_view dest_path) {
#if defined(__APPLE__)
  const std::string args = absl::StrCat(
      "-fsSL -A marinaMoji-update-check -o ", ShellEscapeSingleQuotes(dest_path),
      " ", ShellEscapeSingleQuotes(url));
  const auto body = RunCurlCapture(args);
  if (!body.ok()) {
    return body.status();
  }
  return absl::OkStatus();
#else
  (void)url;
  (void)dest_path;
  return absl::UnimplementedError(
      "MarinaCurlDownload is only implemented on macOS.");
#endif
}

bool MarinaOpenLocalPath(absl::string_view path) {
#if defined(__APPLE__)
  return Process::SpawnProcess("/usr/bin/open", path);
#else
  (void)path;
  return false;
#endif
}

}  // namespace mozc
