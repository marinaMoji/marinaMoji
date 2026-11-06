// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "base/font_coverage.h"

#include <cstdint>

#include "absl/base/const_init.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "base/util.h"

#if defined(__linux__)
#include <dlfcn.h>
#endif

namespace mozc {
namespace {

// Characters that never need a font of their own: ASCII, joiners, variation
// selectors and the like are drawn by whatever font draws their neighbours.
bool IsAlwaysRenderable(char32_t cp) {
  return cp < 0x80 || cp == 0x200C || cp == 0x200D ||
         (cp >= 0xFE00 && cp <= 0xFE0F) || (cp >= 0xE0100 && cp <= 0xE01EF);
}

}  // namespace

#if defined(__linux__)
namespace internal {
namespace {

// The few fontconfig entry points used, resolved with dlopen so the server
// does not gain a link-time dependency. Types are opaque on purpose.
struct FontConfigApi {
  using Pattern = void;
  using CharSet = void;
  int (*init)() = nullptr;
  Pattern *(*pattern_create)() = nullptr;
  void (*pattern_destroy)(Pattern *) = nullptr;
  CharSet *(*charset_create)() = nullptr;
  int (*charset_add_char)(CharSet *, uint32_t) = nullptr;
  int (*charset_has_char)(const CharSet *, uint32_t) = nullptr;
  void (*charset_destroy)(CharSet *) = nullptr;
  int (*pattern_add_charset)(Pattern *, const char *, const CharSet *) =
      nullptr;
  int (*pattern_get_charset)(const Pattern *, const char *, int, CharSet **) =
      nullptr;
  int (*config_substitute)(void *, Pattern *, int) = nullptr;
  void (*default_substitute)(Pattern *) = nullptr;
  Pattern *(*font_match)(void *, Pattern *, int *) = nullptr;
  bool ok = false;
};

template <typename T>
bool Load(void *lib, const char *name, T *out) {
  *out = reinterpret_cast<T>(dlsym(lib, name));
  return *out != nullptr;
}

const FontConfigApi &Api() {
  static const FontConfigApi *api = [] {
    auto *a = new FontConfigApi;
    void *lib = dlopen("libfontconfig.so.1", RTLD_LAZY | RTLD_LOCAL);
    if (lib == nullptr) return a;
    a->ok = Load(lib, "FcInit", &a->init) &&
            Load(lib, "FcPatternCreate", &a->pattern_create) &&
            Load(lib, "FcPatternDestroy", &a->pattern_destroy) &&
            Load(lib, "FcCharSetCreate", &a->charset_create) &&
            Load(lib, "FcCharSetAddChar", &a->charset_add_char) &&
            Load(lib, "FcCharSetHasChar", &a->charset_has_char) &&
            Load(lib, "FcCharSetDestroy", &a->charset_destroy) &&
            Load(lib, "FcPatternAddCharSet", &a->pattern_add_charset) &&
            Load(lib, "FcPatternGetCharSet", &a->pattern_get_charset) &&
            Load(lib, "FcConfigSubstitute", &a->config_substitute) &&
            Load(lib, "FcDefaultSubstitute", &a->default_substitute) &&
            Load(lib, "FcFontMatch", &a->font_match);
    if (a->ok) a->init();
    return a;
  }();
  return *api;
}

}  // namespace

bool CanRenderCodePointUncached(char32_t code_point) {
  const FontConfigApi &fc = Api();
  if (!fc.ok) return true;  // No fontconfig: never hide on a guess.

  constexpr int kFcMatchPattern = 0;
  bool renderable = true;
  FontConfigApi::CharSet *wanted = fc.charset_create();
  FontConfigApi::Pattern *pattern = fc.pattern_create();
  if (wanted != nullptr && pattern != nullptr &&
      fc.charset_add_char(wanted, code_point) != 0 &&
      fc.pattern_add_charset(pattern, "charset", wanted) != 0) {
    fc.config_substitute(nullptr, pattern, kFcMatchPattern);
    fc.default_substitute(pattern);
    int result = 0;
    FontConfigApi::Pattern *match = fc.font_match(nullptr, pattern, &result);
    if (match != nullptr) {
      FontConfigApi::CharSet *has = nullptr;
      // FcResultMatch == 0. The best match is the font whose charset covers
      // the most of what we asked for; if even it lacks the character, no
      // installed font has it.
      if (fc.pattern_get_charset(match, "charset", 0, &has) == 0 &&
          has != nullptr) {
        renderable = fc.charset_has_char(has, code_point) != 0;
      }
      fc.pattern_destroy(match);
    }
  }
  if (pattern != nullptr) fc.pattern_destroy(pattern);
  if (wanted != nullptr) fc.charset_destroy(wanted);
  return renderable;
}

}  // namespace internal
#elif !defined(__APPLE__)
namespace internal {
// Windows and others: no coverage query here yet, so never hide anything.
bool CanRenderCodePointUncached(char32_t) { return true; }
}  // namespace internal
#endif
// macOS: internal::CanRenderCodePointUncached lives in base/mac/font_coverage_mac.mm.

bool CanRenderCodePoint(char32_t code_point) {
  if (IsAlwaysRenderable(code_point)) return true;

  ABSL_CONST_INIT static absl::Mutex mutex(absl::kConstInit);
  static auto *cache = new absl::flat_hash_map<char32_t, bool>();
  {
    absl::MutexLock lock(&mutex);
    if (auto it = cache->find(code_point); it != cache->end()) {
      return it->second;
    }
  }
  const bool renderable = internal::CanRenderCodePointUncached(code_point);
  absl::MutexLock lock(&mutex);
  cache->emplace(code_point, renderable);
  return renderable;
}

bool CanRenderText(absl::string_view text) {
  if (!Util::IsValidUtf8(text)) return true;
  for (ConstChar32Iterator iter(text); !iter.Done(); iter.Next()) {
    if (!CanRenderCodePoint(iter.Get())) return false;
  }
  return true;
}

}  // namespace mozc
