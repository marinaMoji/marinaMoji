// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "gui/character_composer/word_insert.h"

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"

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
  return false;
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
#else
  return absl::UnimplementedError("direct insert into Word is macOS only");
#endif  // __APPLE__
}

}  // namespace gui
}  // namespace mozc
