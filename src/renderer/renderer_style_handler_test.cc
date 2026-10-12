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

#include "protocol/renderer_style.pb.h"
#include "testing/gunit.h"

#ifdef __APPLE__
#include "config/config_handler.h"
#include "protocol/config.pb.h"
#endif  // __APPLE__

namespace mozc {
namespace renderer {

TEST(RendererStyleHandlerTest, GetRendererStyle) {
  const RendererStyle style = RendererStyleHandler::GetRendererStyle();
  EXPECT_TRUE(style.has_window_border());
  EXPECT_TRUE(style.has_infolist_style());
  EXPECT_TRUE(style.infolist_style().has_focused_border_color());
}

TEST(RendererStyleHandlerTest, CandidateBackgroundPerTheme) {
  using ColorTheme = RendererStyleHandler::ColorTheme;
  const RendererStyle light =
      RendererStyleHandler::GetRendererStyle(ColorTheme::kLight);
  ASSERT_TRUE(light.candidate_style().has_background_color());
  EXPECT_DOUBLE_EQ(light.candidate_style().background_color().r(), 255.0);

  // The dark theme must never get the light theme's opaque white fill, which
  // would hide its near-white text (GitHub issue #37).
  const RendererStyle dark =
      RendererStyleHandler::GetRendererStyle(ColorTheme::kDark);
  EXPECT_TRUE(dark.has_window_border());
  EXPECT_TRUE(!dark.candidate_style().has_background_color() ||
              dark.candidate_style().background_color().r() < 128);
}

#ifdef __APPLE__
TEST(RendererStyleHandlerTest, CandidateWindowFontSizeFromConfig) {
  config::ConfigHandler::SetConfigFileNameForTesting(
      "memory://renderer_style_handler_font_test.db");
  config::Config config = config::ConfigHandler::GetCopiedConfig();
  config.set_candidate_window_font_size(28);
  config::ConfigHandler::SetConfig(config);

  const RendererStyle style = RendererStyleHandler::GetRendererStyle();
  EXPECT_DOUBLE_EQ(style.candidate_style().font_size(), 28.0);
}
#endif  // __APPLE__

}  // namespace renderer
}  // namespace mozc
