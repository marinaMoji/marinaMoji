// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "gui/character_composer/word_insert.h"

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif  // _WIN32

namespace mozc {
namespace gui {
namespace {

// A string for an AppleScript literal.
std::string AppleScriptString(std::string_view text) {
  return absl::StrCat("\"",
                      absl::StrReplaceAll(text, {{"\\", "\\\\"}, {"\"", "\\\""}}),
                      "\"");
}

#ifdef __APPLE__
// stdout of |command| (a fixed command line, no user input), "" on failure.
std::string Capture(const char *command) {
  std::string out;
  FILE *pipe = popen(command, "r");
  if (pipe == nullptr) return out;
  char buffer[256];
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) out += buffer;
  pclose(pipe);
  return out;
}

std::string BundleIdOf(const std::string &asn) {
  return ParseBundleId(Capture(
      absl::StrCat("/usr/bin/lsappinfo info -only bundleid ", asn, " 2>/dev/null")
          .c_str()));
}
#endif  // __APPLE__

}  // namespace

std::string WordInsertScript(std::string_view svg_path, std::string_view ids) {
  return absl::StrCat(
      "tell application \"Microsoft Word\"\n"
      "  if (count of documents) is 0 then error \"no document is open\"\n"
      "  set theSel to selection\n"
      "  set pt to font size of font object of theSel\n"
      "  if pt < 1 or pt > 400 then set pt to 12\n"
      "  set s to start of content of text object of theSel\n"
      "  set r to create range active document start s end s\n"
      "  set pic to make new inline picture at r with properties "
      "{file name:", AppleScriptString(svg_path),
      ", link to file:false, save with document:true}\n"
      "  set pic to inline shape 1 of (create range active document start s end (s + 1))\n"
      "  set w0 to width of pic\n"
      "  set h0 to height of pic\n"
      "  set newH to pt * ", kWordInkScale, "\n"
      "  set height of pic to newH\n"
      "  if h0 > 0 then\n"
      "    set width of pic to newH * w0 / h0\n"
      "  else\n"
      "    set width of pic to newH\n"
      "  end if\n"
      "  set alternative text of pic to ",
      AppleScriptString(absl::StrCat("MARINAMOJI:ids=", ids)), "\n"
      "  set font position of font object of (text object of pic) to -(round (pt * ",
      kWordDropEm, "))\n"
      "  select (create range active document start (s + 1) end (s + 1))\n"
      "end tell\n");
}

std::string WordInsertPowerShell(std::string_view svg_path, std::string_view ids) {
  // PowerShell single-quoted strings only need ' doubled.
  auto quote = [](std::string_view text) {
    return absl::StrCat("'", absl::StrReplaceAll(text, {{"'", "''"}}), "'");
  };
  return absl::StrCat(
      "$ErrorActionPreference = 'Stop'\n"
      "$word = [Runtime.InteropServices.Marshal]::GetActiveObject('Word.Application')\n"
      "if ($word.Documents.Count -eq 0) { throw 'no document is open' }\n"
      "$doc = $word.ActiveDocument\n"
      "$sel = $word.Selection\n"
      "$pt = [double]$sel.Font.Size\n"
      "if ($pt -lt 1 -or $pt -gt 400) { $pt = 12 }\n"
      "$start = $sel.Start\n"
      "$range = $doc.Range($start, $start)\n"
      "$pic = $doc.InlineShapes.AddPicture(", quote(svg_path),
      ", $false, $true, $range)\n"
      "$w0 = $pic.Width\n"
      "$h0 = $pic.Height\n"
      "$newH = $pt * ", kWordInkScale, "\n"
      "$pic.LockAspectRatio = 0\n"
      "$pic.Height = $newH\n"
      "if ($h0 -gt 0) { $pic.Width = $newH * $w0 / $h0 } else { $pic.Width = $newH }\n"
      "$pic.AlternativeText = ", quote(absl::StrCat("MARINAMOJI:ids=", ids)), "\n"
      "$pic.Range.Font.Position = -[Math]::Round($pt * ", kWordDropEm, ")\n"
      "$sel.SetRange($start + 1, $start + 1)\n");
}

std::string PowerShellEncodedCommand(std::string_view text) {
  std::string utf16;
  auto put = [&utf16](unsigned unit) {
    utf16.push_back(static_cast<char>(unit & 0xff));
    utf16.push_back(static_cast<char>((unit >> 8) & 0xff));
  };
  for (size_t i = 0; i < text.size();) {
    const unsigned char lead = static_cast<unsigned char>(text[i]);
    unsigned long cp = 0xFFFD;
    size_t len = 1;
    if (lead < 0x80) {
      cp = lead;
    } else if ((lead & 0xE0) == 0xC0) {
      len = 2;
      cp = lead & 0x1F;
    } else if ((lead & 0xF0) == 0xE0) {
      len = 3;
      cp = lead & 0x0F;
    } else if ((lead & 0xF8) == 0xF0) {
      len = 4;
      cp = lead & 0x07;
    }
    if (len > 1) {
      if (i + len > text.size()) {
        cp = 0xFFFD;
        len = text.size() - i;
      } else {
        for (size_t k = 1; k < len; ++k) {
          cp = (cp << 6) | (static_cast<unsigned char>(text[i + k]) & 0x3F);
        }
      }
    }
    i += len;
    if (cp >= 0x10000 && cp <= 0x10FFFF) {
      cp -= 0x10000;
      put(0xD800 + static_cast<unsigned>(cp >> 10));
      put(0xDC00 + static_cast<unsigned>(cp & 0x3FF));
    } else {
      put(static_cast<unsigned>(cp > 0xFFFF ? 0xFFFD : cp));
    }
  }
  return absl::Base64Escape(utf16);
}

#ifdef _WIN32
namespace {
bool g_word_was_foreground = false;
}  // namespace
#endif  // _WIN32

void NoteForegroundApp() {
#ifdef _WIN32
  g_word_was_foreground = false;
  const HWND window = ::GetForegroundWindow();
  if (window == nullptr) return;
  wchar_t class_name[64] = {};
  if (::GetClassNameW(window, class_name, 64) == 0) return;
  // Word's main (and document) windows.
  g_word_was_foreground = (std::wstring_view(class_name) == L"OpusApp");
#endif  // _WIN32
}

std::string ParseBundleId(std::string_view output) {
  // "bundleid"="com.microsoft.Word"
  const size_t eq = output.find('=');
  if (eq == std::string_view::npos) return "";
  std::string_view value = output.substr(eq + 1);
  const size_t open = value.find('"');
  if (open == std::string_view::npos) return "";
  value.remove_prefix(open + 1);
  const size_t close = value.find('"');
  if (close == std::string_view::npos) return "";
  return std::string(value.substr(0, close));
}

std::string IdsFromSvg(std::string_view svg) {
  constexpr std::string_view kKey = "aria-label=\"";
  const size_t start = svg.find(kKey);
  if (start == std::string_view::npos) return "composed";
  svg.remove_prefix(start + kKey.size());
  const size_t end = svg.find('"');
  if (end == std::string_view::npos || end == 0) return "composed";
  return absl::StrReplaceAll(svg.substr(0, end),
                             {{"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"},
                              {"&quot;", "\""}, {"&apos;", "'"}});
}

std::vector<std::string> ParseVisibleAsns(std::string_view output) {
  std::vector<std::string> asns;
  constexpr std::string_view kPrefix = "ASN:0x";
  size_t pos = 0;
  while ((pos = output.find(kPrefix, pos)) != std::string_view::npos) {
    size_t end = pos + kPrefix.size();
    while (end < output.size() &&
           ((output[end] >= '0' && output[end] <= '9') ||
            (output[end] >= 'a' && output[end] <= 'f') || output[end] == '-' ||
            output[end] == 'x')) {
      ++end;
    }
    std::string asn(output.substr(pos, end - pos));
    while (!asn.empty() && asn.back() == '-') asn.pop_back();
    asns.push_back(std::move(asn));
    pos = end;
  }
  return asns;
}

bool WordWasInFront() {
#ifdef __APPLE__
  for (const std::string &asn :
       ParseVisibleAsns(Capture("/usr/bin/lsappinfo visibleProcessList 2>/dev/null"))) {
    const std::string id = BundleIdOf(asn);
    // Ours (the composer is still frontmost; the IME and its tools share the
    // "inputmethod.Japanese" prefix) and unknown entries do not count.
    if (id.empty() || id.find("inputmethod.Japanese") != std::string::npos) continue;
    return id == "com.microsoft.Word";
  }
#endif  // __APPLE__
#ifdef _WIN32
  return g_word_was_foreground;
#else
  return false;
#endif  // _WIN32
}

absl::Status InsertSvgIntoWord(std::string_view svg_path, std::string_view ids) {
#ifdef __APPLE__
  const std::string script = WordInsertScript(svg_path, ids);
  FILE *pipe = popen("/usr/bin/osascript - 2>&1", "w");
  if (pipe == nullptr) return absl::InternalError("cannot start osascript");
  fwrite(script.data(), 1, script.size(), pipe);
  const int status = pclose(pipe);
  if (status != 0) {
    return absl::InternalError(
        absl::StrCat("osascript failed (status ", status,
                     "); Word may need automation permission"));
  }
  return absl::OkStatus();
#elif defined(_WIN32)
  const std::string encoded =
      PowerShellEncodedCommand(WordInsertPowerShell(svg_path, ids));
  wchar_t system_dir[MAX_PATH] = {};
  if (::GetSystemDirectoryW(system_dir, MAX_PATH) == 0) {
    return absl::InternalError("cannot find the Windows system directory");
  }
  std::wstring command =
      std::wstring(system_dir) +
      L"\\WindowsPowerShell\\v1.0\\powershell.exe";
  std::wstring command_line =
      L"\"" + command +
      L"\" -NoProfile -NonInteractive -ExecutionPolicy Bypass "
      L"-WindowStyle Hidden -EncodedCommand " +
      std::wstring(encoded.begin(), encoded.end());  // base64 is ASCII
  STARTUPINFOW startup = {};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process = {};
  if (!::CreateProcessW(command.c_str(), command_line.data(), nullptr, nullptr,
                        FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup,
                        &process)) {
    return absl::InternalError("cannot start powershell");
  }
  const DWORD waited = ::WaitForSingleObject(process.hProcess, 30000);
  DWORD exit_code = 1;
  if (waited == WAIT_OBJECT_0) {
    ::GetExitCodeProcess(process.hProcess, &exit_code);
  } else {
    ::TerminateProcess(process.hProcess, 1);
  }
  ::CloseHandle(process.hThread);
  ::CloseHandle(process.hProcess);
  if (waited != WAIT_OBJECT_0) {
    return absl::DeadlineExceededError("Word did not answer in 30 seconds");
  }
  if (exit_code != 0) {
    return absl::InternalError(absl::StrCat(
        "powershell failed (exit ", exit_code,
        "); is a document open in Word, and not in Protected View?"));
  }
  return absl::OkStatus();
#else
  return absl::UnimplementedError(
      "direct insert into Word is macOS and Windows only");
#endif  // __APPLE__
}

}  // namespace gui
}  // namespace mozc
