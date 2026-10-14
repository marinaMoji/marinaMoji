#include "base/marina_curl_fetch.h"

#include <array>
#ifdef __APPLE__
#include <crt_externs.h>
#include <spawn.h>
#include <sys/wait.h>
#endif  // __APPLE__
#include <cstdio>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"

#if defined(__APPLE__)
#include "base/marina_update_log.h"
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
    // curl's own exit codes: 22 = HTTP error (-f), 6 = DNS, 7 = connect, 28 =
    // timeout, 23 = could not write the output file, 35/60 = TLS.
    const int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    MarinaUpdateLog(absl::StrCat("curl failed with exit status ", code, " for ",
                                 args.substr(0, 200)));
    return absl::UnavailableError(absl::StrCat("curl failed (exit status ", code,
                                               ")."));
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
      "-fsSL --create-dirs -A marinaMoji-update-check -o ",
      ShellEscapeSingleQuotes(dest_path),
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
  // Not Process::SpawnProcess: it splits its argument on spaces, and the
  // downloaded package lives under ~/Library/Application Support/, so `open`
  // received three bogus arguments and failed. Spawning launched `open`
  // successfully, so the failure was invisible and the updater's "Download &
  // Install" appeared to do nothing. Pass the path as one argv entry and use
  // `open`'s exit status (it returns as soon as the handler has been asked to
  // open the file).
  const std::string path_str(path);
  char open_path[] = "/usr/bin/open";
  char* const argv[] = {open_path, const_cast<char*>(path_str.c_str()),
                        nullptr};
  pid_t pid = 0;
  if (posix_spawn(&pid, open_path, nullptr, nullptr, argv,
                  *_NSGetEnviron()) != 0) {
    return false;
  }
  int wait_status = 0;
  if (waitpid(pid, &wait_status, 0) < 0) {
    return false;
  }
  return WIFEXITED(wait_status) && WEXITSTATUS(wait_status) == 0;
#else
  (void)path;
  return false;
#endif
}

std::string MarinaSha256OfFile(absl::string_view path) {
#if defined(__APPLE__)
  const std::string command =
      absl::StrCat("/usr/bin/shasum -a 256 ", ShellEscapeSingleQuotes(path));
  FILE* pipe = ::popen(command.c_str(), "r");
  if (pipe == nullptr) {
    return "";
  }
  char buf[256] = {0};
  const bool got_line = ::fgets(buf, sizeof(buf), pipe) != nullptr;
  ::pclose(pipe);
  if (!got_line) {
    return "";
  }
  const std::string line(buf);
  const size_t space = line.find(' ');
  if (space == std::string::npos) {
    return "";
  }
  return line.substr(0, space);
#else
  (void)path;
  return "";
#endif
}

}  // namespace mozc
