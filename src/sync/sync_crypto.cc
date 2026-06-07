#include "sync/sync_crypto.h"

#include <string>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "sodium.h"

namespace mozc {
namespace sync {
namespace {

constexpr absl::string_view kMagic = "MMZENC1";
constexpr size_t kMagicSize = 7;
constexpr unsigned long long kOpsLimit = crypto_pwhash_OPSLIMIT_MODERATE;
constexpr size_t kMemLimit = crypto_pwhash_MEMLIMIT_MODERATE;

bool EnsureSodiumInit() {
  static bool initialized = [] {
    if (sodium_init() < 0) {
      return false;
    }
    return true;
  }();
  return initialized;
}

}  // namespace

absl::StatusOr<std::string> EncryptWithPassphrase(absl::string_view plaintext,
                                                  absl::string_view passphrase) {
  if (!EnsureSodiumInit()) {
    return absl::InternalError("libsodium init failed");
  }
  if (passphrase.empty()) {
    return absl::InvalidArgumentError("Empty passphrase");
  }

  unsigned char salt[crypto_pwhash_SALTBYTES];
  randombytes_buf(salt, sizeof(salt));

  unsigned char key[crypto_secretstream_xchacha20poly1305_KEYBYTES];
  if (crypto_pwhash(key, sizeof(key), passphrase.data(), passphrase.size(),
                    salt, kOpsLimit, kMemLimit,
                    crypto_pwhash_ALG_DEFAULT) != 0) {
    return absl::InternalError("Passphrase key derivation failed");
  }

  crypto_secretstream_xchacha20poly1305_state state;
  unsigned char header[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
  crypto_secretstream_xchacha20poly1305_init_push(&state, header, key);

  const size_t cipher_max =
      plaintext.size() + crypto_secretstream_xchacha20poly1305_ABYTES;
  std::string cipher(cipher_max, '\0');
  unsigned long long cipher_len = 0;
  crypto_secretstream_xchacha20poly1305_push(
      &state, reinterpret_cast<unsigned char*>(cipher.data()), &cipher_len,
      reinterpret_cast<const unsigned char*>(plaintext.data()), plaintext.size(),
      nullptr, 0, crypto_secretstream_xchacha20poly1305_TAG_FINAL);
  cipher.resize(static_cast<size_t>(cipher_len));

  std::string out;
  out.reserve(kMagicSize + sizeof(salt) + sizeof(header) + cipher.size());
  out.append(kMagic.data(), kMagicSize);
  out.append(reinterpret_cast<char*>(salt), sizeof(salt));
  out.append(reinterpret_cast<char*>(header), sizeof(header));
  out += cipher;

  sodium_memzero(key, sizeof(key));
  return out;
}

absl::StatusOr<std::string> DecryptWithPassphrase(absl::string_view ciphertext,
                                                  absl::string_view passphrase) {
  if (!EnsureSodiumInit()) {
    return absl::InternalError("libsodium init failed");
  }
  if (passphrase.empty()) {
    return absl::InvalidArgumentError("Empty passphrase");
  }
  const size_t header_offset =
      kMagicSize + crypto_pwhash_SALTBYTES +
      crypto_secretstream_xchacha20poly1305_HEADERBYTES;
  if (ciphertext.size() < header_offset) {
    return absl::InvalidArgumentError("Ciphertext too short");
  }
  if (ciphertext.substr(0, kMagicSize) != kMagic) {
    return absl::InvalidArgumentError("Invalid sync file magic");
  }

  const unsigned char* salt =
      reinterpret_cast<const unsigned char*>(ciphertext.data() + kMagicSize);
  const unsigned char* header = salt + crypto_pwhash_SALTBYTES;
  const unsigned char* data =
      header + crypto_secretstream_xchacha20poly1305_HEADERBYTES;
  const size_t data_len = ciphertext.size() - header_offset;

  unsigned char key[crypto_secretstream_xchacha20poly1305_KEYBYTES];
  if (crypto_pwhash(key, sizeof(key), passphrase.data(), passphrase.size(),
                    salt, kOpsLimit, kMemLimit,
                    crypto_pwhash_ALG_DEFAULT) != 0) {
    return absl::InternalError("Passphrase key derivation failed");
  }

  crypto_secretstream_xchacha20poly1305_state state;
  if (crypto_secretstream_xchacha20poly1305_init_pull(&state, header, key) !=
      0) {
    sodium_memzero(key, sizeof(key));
    return absl::InvalidArgumentError("Wrong sync key or corrupted file");
  }

  std::string plaintext(data_len, '\0');
  unsigned long long out_len = 0;
  unsigned char tag = 0;
  const int ret = crypto_secretstream_xchacha20poly1305_pull(
      &state, reinterpret_cast<unsigned char*>(plaintext.data()), &out_len,
      &tag, data, data_len, nullptr, 0);
  sodium_memzero(key, sizeof(key));
  if (ret != 0) {
    return absl::InvalidArgumentError("Decryption failed (wrong sync key?)");
  }
  if (tag != crypto_secretstream_xchacha20poly1305_TAG_FINAL) {
    return absl::DataLossError("Incomplete sync ciphertext");
  }
  plaintext.resize(static_cast<size_t>(out_len));
  return plaintext;
}

}  // namespace sync
}  // namespace mozc
