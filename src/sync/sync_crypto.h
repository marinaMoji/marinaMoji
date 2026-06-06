#ifndef MOZC_SYNC_SYNC_CRYPTO_H_
#define MOZC_SYNC_SYNC_CRYPTO_H_

#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace sync {

// Encrypt plaintext with passphrase-derived key (libsodium secretstream).
absl::StatusOr<std::string> EncryptWithPassphrase(absl::string_view plaintext,
                                                  absl::string_view passphrase);

// Decrypt blob produced by EncryptWithPassphrase.
absl::StatusOr<std::string> DecryptWithPassphrase(absl::string_view ciphertext,
                                                  absl::string_view passphrase);

}  // namespace sync
}  // namespace mozc

#endif  // MOZC_SYNC_SYNC_CRYPTO_H_
