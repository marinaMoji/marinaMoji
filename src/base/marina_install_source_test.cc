#include "base/marina_install_source.h"

#include "testing/gunit.h"

namespace mozc {
namespace {

TEST(MarinaInstallSourceTest, StoreValueIsRecognised) {
  EXPECT_TRUE(IsMarinaStoreInstallSource("store"));
  EXPECT_TRUE(IsMarinaStoreInstallSource("Store"));
  EXPECT_TRUE(IsMarinaStoreInstallSource(" STORE "));
}

TEST(MarinaInstallSourceTest, AnythingElseIsNotTheStore) {
  EXPECT_FALSE(IsMarinaStoreInstallSource(""));
  EXPECT_FALSE(IsMarinaStoreInstallSource("github"));
  EXPECT_FALSE(IsMarinaStoreInstallSource("microsoft store"));
  EXPECT_FALSE(IsMarinaStoreInstallSource("stores"));
}

}  // namespace
}  // namespace mozc
