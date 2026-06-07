#include "sync/sync_crypto.h"

#include "gtest/gtest.h"

namespace mozc {
namespace sync {
namespace {

TEST(SyncCryptoTest, RoundTrip) {
  const std::string plaintext = "hello marinaMoji sync bundle";
  const std::string passphrase = "anchor-amber-bridge-coral-delta-ember";
  const auto encrypted = EncryptWithPassphrase(plaintext, passphrase);
  ASSERT_TRUE(encrypted.ok()) << encrypted.status();
  const auto decrypted = DecryptWithPassphrase(*encrypted, passphrase);
  ASSERT_TRUE(decrypted.ok()) << decrypted.status();
  EXPECT_EQ(*decrypted, plaintext);
}

TEST(SyncCryptoTest, WrongKeyFails) {
  const auto encrypted = EncryptWithPassphrase("secret", "key-one");
  ASSERT_TRUE(encrypted.ok());
  const auto decrypted = DecryptWithPassphrase(*encrypted, "key-two");
  EXPECT_FALSE(decrypted.ok());
}

}  // namespace
}  // namespace sync
}  // namespace mozc
