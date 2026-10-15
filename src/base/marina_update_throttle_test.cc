#include "base/marina_update_throttle.h"

#include "absl/time/time.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

absl::Time At(int64_t unix_sec) { return absl::FromUnixSeconds(unix_sec); }

constexpr int64_t kHour = 3600;

TEST(MarinaUpdateThrottleTest, NoRecordMeansOffer) {
  EXPECT_TRUE(ShouldOfferMarinaUpdateGiven("", "v0.0.14", At(1000)));
}

TEST(MarinaUpdateThrottleTest, UnreadableRecordMeansOffer) {
  EXPECT_TRUE(ShouldOfferMarinaUpdateGiven("garbage", "v0.0.14", At(1000)));
  EXPECT_TRUE(
      ShouldOfferMarinaUpdateGiven("notanumber v0.0.14", "v0.0.14", At(1000)));
}

TEST(MarinaUpdateThrottleTest, SameReleaseIsHeldBackWithinTheOfferInterval) {
  const int64_t offered = 1'000'000;
  EXPECT_FALSE(ShouldOfferMarinaUpdateGiven("1000000 v0.0.14\n", "v0.0.14",
                                            At(offered + 1)));
  EXPECT_FALSE(ShouldOfferMarinaUpdateGiven("1000000 v0.0.14\n", "v0.0.14",
                                            At(offered + 23 * kHour)));
}

TEST(MarinaUpdateThrottleTest, SameReleaseIsOfferedAgainAfterTheInterval) {
  const int64_t offered = 1'000'000;
  EXPECT_TRUE(ShouldOfferMarinaUpdateGiven("1000000 v0.0.14\n", "v0.0.14",
                                           At(offered + 24 * kHour)));
  EXPECT_TRUE(ShouldOfferMarinaUpdateGiven("1000000 v0.0.14\n", "v0.0.14",
                                           At(offered + 48 * kHour)));
}

TEST(MarinaUpdateThrottleTest, NewerReleaseIsNeverHeldBack) {
  const int64_t offered = 1'000'000;
  EXPECT_TRUE(ShouldOfferMarinaUpdateGiven("1000000 v0.0.14\n", "v0.0.15",
                                           At(offered + 1)));
}

TEST(MarinaUpdateThrottleTest, TolerantOfSurroundingWhitespace) {
  EXPECT_FALSE(ShouldOfferMarinaUpdateGiven("  1000000 v0.0.14  \n\n",
                                            "v0.0.14", At(1'000'001)));
}

}  // namespace
}  // namespace mozc
