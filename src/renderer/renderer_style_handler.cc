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

#include "renderer/renderer_style_handler.h"

#include <algorithm>
#include <cstdint>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "base/protobuf/text_format.h"
#include "base/protobuf_util.h"
#include "base/text_normalizer.h"
#include "protocol/renderer_style.pb.h"
#include "renderer/renderer_style_appearance.h"
#include "renderer/renderer_style_scale.h"

#ifdef __APPLE__
#include "config/config_handler.h"
#endif  // __APPLE__

namespace mozc {
namespace renderer {

namespace {
// absl::string_view kStyleTextProto is defined in renderer_style.inc.
#include "renderer/renderer_style.inc"
// absl::string_view kStyleDarkTextProto is defined in renderer_style_dark.inc.
#include "renderer/renderer_style_dark.inc"

#ifdef __APPLE__
// absl::string_view kStyleDarkMacTextProto is defined in
// renderer_style_dark_mac.inc.
#include "renderer/renderer_style_dark_mac.inc"

constexpr uint32_t kDefaultCandidateWindowFontSize = 14;
constexpr uint32_t kMinCandidateWindowFontSize = 14;
constexpr uint32_t kMaxCandidateWindowFontSize = 36;

uint32_t GetCandidateWindowFontSizeFromConfig() {
  const uint32_t font_size =
      config::ConfigHandler::GetSharedConfig()->candidate_window_font_size();
  return std::clamp(font_size, kMinCandidateWindowFontSize,
                    kMaxCandidateWindowFontSize);
}
#endif  // __APPLE__

absl::string_view GetStyleTextProto(RendererStyleHandler::ColorTheme theme) {
  if (theme == RendererStyleHandler::ColorTheme::kLight) {
    return kStyleTextProto;
  }
#ifdef __APPLE__
  return kStyleDarkMacTextProto;
#else   // __APPLE__
  return kStyleDarkTextProto;
#endif  // __APPLE__
}

void SetRgbaColor(RendererStyle::RGBAColor* color, double r, double g, double b,
                  double a = 1.0) {
  color->set_r(r);
  color->set_g(g);
  color->set_b(b);
  color->set_a(a);
}

}  // namespace

RendererStyle RendererStyleHandler::GetRendererStyle() {
  return GetRendererStyle(IsDarkRendererStylePreferred() ? ColorTheme::kDark
                                                         : ColorTheme::kLight);
}

RendererStyle RendererStyleHandler::GetRendererStyle(ColorTheme theme) {
  RendererStyle style;
  CHECK(mozc::protobuf::TextFormat::ParseFromString(GetStyleTextProto(theme),
                                                    &style));

  // The macOS dark theme relies on the window's own dark background showing
  // through unselected candidate rows; only the light theme wants an
  // explicit opaque white fill here. Forcing white unconditionally painted
  // over the dark theme's near-white foreground text, making candidates
  // nearly unreadable (see GitHub issue #37).
  if (theme == ColorTheme::kLight &&
      !style.candidate_style().has_background_color()) {
    SetRgbaColor(style.mutable_candidate_style()->mutable_background_color(),
                 255, 255, 255);
  }

  protobuf_util::SanitizeMessageStrings(style, [](absl::string_view src) {
    // Limit the length of the string to 100 bytes and remove ill-formed
    // UTF-8 sequences and ASCII control characters.
    return TextNormalizer::SanitizeText(src, 100);
  });

#ifdef __APPLE__
  const uint32_t font_size = GetCandidateWindowFontSizeFromConfig();
  const double scale_factor =
      static_cast<double>(font_size) / kDefaultCandidateWindowFontSize;
  ScaleRendererStyle(&style, scale_factor);
#endif  // __APPLE__
  return style;
}

}  // namespace renderer
}  // namespace mozc
