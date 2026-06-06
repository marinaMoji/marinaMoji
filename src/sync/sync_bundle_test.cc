#include "sync/sync_bundle.h"

#include "gtest/gtest.h"

namespace mozc {
namespace sync {
namespace {

TEST(SyncBundleTest, PackUnpack) {
  SyncBundleFiles files;
  files["manifest.txt"] = "version=1\n";
  files["dictionary.tsv"] = "# test\nfoo\tbar\t名詞\t\n";
  const auto packed = PackBundle(files);
  ASSERT_TRUE(packed.ok()) << packed.status();
  const auto unpacked = UnpackBundle(*packed);
  ASSERT_TRUE(unpacked.ok()) << unpacked.status();
  EXPECT_EQ(unpacked->at("manifest.txt"), "version=1\n");
  EXPECT_EQ(unpacked->at("dictionary.tsv"), "# test\nfoo\tbar\t名詞\t\n");
}

}  // namespace
}  // namespace sync
}  // namespace mozc
