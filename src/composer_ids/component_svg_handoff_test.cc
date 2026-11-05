// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "composer_ids/component_svg_handoff.h"

#include <string>
#include <vector>

#include "composer_ids/composer_result.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace composer_ids {
namespace {

class ComponentSvgHandoffTest : public testing::TestWithTempUserProfile {};

TEST_F(ComponentSvgHandoffTest, RenderContainsPartsAndIds) {
  const std::vector<std::string> parts = {"水", "毎"};
  const std::string svg = RenderComponentSelectionSvg("⿰", parts);
  ASSERT_FALSE(svg.empty());
  EXPECT_NE(svg.find("<svg"), std::string::npos);
  EXPECT_NE(svg.find("水"), std::string::npos);
  EXPECT_NE(svg.find("毎"), std::string::npos);
  EXPECT_NE(svg.find("⿰"), std::string::npos);
}

TEST_F(ComponentSvgHandoffTest, HandOffWritesSvgFile) {
  const std::vector<std::string> parts = {"木", "目"};
  ASSERT_TRUE(HandOffComponentSvg("⿰", parts).ok());
  const auto read = ReadComposerSvgResult(DefaultSvgResultFilePath());
  ASSERT_TRUE(read.has_value());
  EXPECT_NE(read->find("<svg"), std::string::npos);
}

}  // namespace
}  // namespace composer_ids
}  // namespace mozc
