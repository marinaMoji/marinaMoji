// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "gui/character_composer/glyphwiki_client.h"

#include <array>
#include <cctype>
#include <cstdio>
#include <string>
#include <string_view>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "base/file_util.h"
#include "base/system_util.h"
#include "kage/kage_engine.h"

namespace mozc {
namespace gui {
namespace {

constexpr int kFetchTimeoutSec = 3;
constexpr int kMaxFetchDepth = 24;

bool IsSafeGlyphName(std::string_view name) {
  if (name.empty() || name.size() > 64) return false;
  for (const char c : name) {
    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-' ||
          c == '_' || c == '@')) {
      return false;
    }
  }
  return true;
}

std::string CachePathFor(std::string_view name) {
  return FileUtil::JoinPath(GlyphWikiCacheDirectory(),
                            absl::StrCat(name, ".kage"));
}

std::string ShellEscapeSingleQuotes(std::string_view value) {
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

}  // namespace

std::string GlyphWikiCacheDirectory() {
  return FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(),
                            "glyphwiki_cache");
}

std::optional<std::string> LoadGlyphWikiCache(std::string_view name) {
  if (!IsSafeGlyphName(name)) return std::nullopt;
  absl::StatusOr<std::string> contents =
      FileUtil::GetContents(CachePathFor(name));
  if (!contents.ok() || contents->empty()) return std::nullopt;
  while (!contents->empty() &&
         (contents->back() == '\n' || contents->back() == '\r')) {
    contents->pop_back();
  }
  if (contents->empty()) return std::nullopt;
  return *contents;
}

bool StoreGlyphWikiCache(std::string_view name, std::string_view data) {
  if (!IsSafeGlyphName(name) || data.empty()) return false;
  const std::string dir = GlyphWikiCacheDirectory();
  if (!FileUtil::DirectoryExists(dir).ok()) {
    if (!FileUtil::CreateDirectory(dir).ok()) {
      LOG(WARNING) << "glyphwiki cache mkdir failed: " << dir;
      return false;
    }
  }
  if (const absl::Status status =
          FileUtil::SetContents(CachePathFor(name), data);
      !status.ok()) {
    LOG(WARNING) << "glyphwiki cache write failed: " << status;
    return false;
  }
  return true;
}

std::optional<std::string> ParseGlyphWikiApiData(std::string_view json) {
  // GlyphWiki returns a small object: {"name":"...","data":"...","version":N}
  // or {} when the name is unknown. Avoid a JSON dependency for this field.
  constexpr std::string_view kKey = "\"data\":\"";
  const size_t start = json.find(kKey);
  if (start == std::string_view::npos) return std::nullopt;
  size_t i = start + kKey.size();
  std::string data;
  data.reserve(64);
  while (i < json.size()) {
    const char c = json[i++];
    if (c == '\\') {
      if (i >= json.size()) return std::nullopt;
      data.push_back(json[i++]);
      continue;
    }
    if (c == '"') {
      return data.empty() ? std::nullopt
                          : std::optional<std::string>(std::move(data));
    }
    data.push_back(c);
  }
  return std::nullopt;
}

bool GlyphWikiFetchAvailable() {
  static const bool available = [] {
#ifdef _WIN32
    FILE *pipe = ::_popen("curl --version 2>NUL", "r");
    if (pipe == nullptr) return false;
    const int status = ::_pclose(pipe);
#else
    FILE *pipe = ::popen("curl --version 2>/dev/null", "r");
    if (pipe == nullptr) return false;
    const int status = ::pclose(pipe);
#endif
    return status == 0;
  }();
  return available;
}

std::optional<std::string> FetchGlyphWikiKage(std::string_view name) {
  if (!IsSafeGlyphName(name)) return std::nullopt;
  if (!GlyphWikiFetchAvailable()) return std::nullopt;

  const std::string url =
      absl::StrCat("https://glyphwiki.org/api/glyph?name=", name);
  // curl is on PATH on macOS/Linux and modern Windows. -f fails on HTTP errors.
  const std::string command = absl::StrCat(
      "curl -fsSL --max-time ", kFetchTimeoutSec,
      " -A marinaMoji-character-composer ", ShellEscapeSingleQuotes(url));
#ifdef _WIN32
  FILE *pipe = ::_popen(command.c_str(), "r");
#else
  FILE *pipe = ::popen(command.c_str(), "r");
#endif
  if (pipe == nullptr) {
    LOG(WARNING) << "GlyphWiki fetch: could not start curl for " << name;
    return std::nullopt;
  }
  std::string body;
  std::array<char, 4096> buffer;
  while (true) {
    const size_t n = ::fread(buffer.data(), 1, buffer.size(), pipe);
    if (n > 0) body.append(buffer.data(), n);
    if (n < buffer.size()) break;
  }
#ifdef _WIN32
  const int status = ::_pclose(pipe);
#else
  const int status = ::pclose(pipe);
#endif
  if (status != 0) {
    LOG(WARNING) << "GlyphWiki fetch curl failed for " << name
                 << " (status " << status << ")";
    return std::nullopt;
  }
  return ParseGlyphWikiApiData(body);
}

bool EnsureGlyphWikiGlyph(std::string_view name, kage::Buhin *scratch,
                          bool allow_network) {
  if (scratch == nullptr || !IsSafeGlyphName(name)) return false;

  auto ensure_one = [&](auto &&self, std::string_view glyph,
                        int depth) -> bool {
    if (depth > kMaxFetchDepth) return false;
    if (!scratch->Search(glyph).empty()) return true;

    std::string data;
    if (auto cached = LoadGlyphWikiCache(glyph)) {
      data = *std::move(cached);
    } else if (allow_network) {
      auto fetched = FetchGlyphWikiKage(glyph);
      if (!fetched) return false;
      data = *std::move(fetched);
      StoreGlyphWikiCache(glyph, data);
    } else {
      return false;
    }
    scratch->Set(std::string(glyph), data);

    const kage::Kage engine(scratch);
    for (const std::string &missing : engine.UnresolvedComponents(data)) {
      if (!self(self, missing, depth + 1)) return false;
    }
    return true;
  };

  return ensure_one(ensure_one, name, 0);
}

}  // namespace gui
}  // namespace mozc
