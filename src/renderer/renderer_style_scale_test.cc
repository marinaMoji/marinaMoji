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
#include "renderer/renderer_style_scale.h"

#include "protocol/renderer_style.pb.h"
#include "testing/gunit.h"

namespace mozc {
namespace renderer {

TEST(RendererStyleScaleTest, ScaleRendererStyleDoublesFontSize) {
  RendererStyle style;
  RendererStyleHandler::GetRendererStyle(&style);
  const double base_candidate_size = style.candidate_style().font_size();
  const double base_description_size = style.description_style().font_size();
  ASSERT_GT(base_candidate_size, 0);

  ScaleRendererStyle(&style, 2.0);
  EXPECT_DOUBLE_EQ(style.candidate_style().font_size(), base_candidate_size * 2.0);
  EXPECT_DOUBLE_EQ(style.description_style().font_size(),
                   base_description_size * 2.0);
}

TEST(RendererStyleScaleTest, ScaleFactorOneIsNoOp) {
  RendererStyle style;
  RendererStyleHandler::GetRendererStyle(&style);
  const double base_candidate_size = style.candidate_style().font_size();

  ScaleRendererStyle(&style, 1.0);
  EXPECT_DOUBLE_EQ(style.candidate_style().font_size(), base_candidate_size);
}

}  // namespace renderer
}  // namespace mozc
