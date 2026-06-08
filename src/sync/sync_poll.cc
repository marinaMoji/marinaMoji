#include "sync/sync_poll.h"

#include <fstream>
#include <string>

#include "base/config_file_stream.h"
#include "base/file_util.h"
#include "sodium/crypto_hash_sha256.h"

namespace mozc {
namespace sync {
namespace {

std::string BytesToHex(const unsigned char* data, size_t len) {
  static const char kHex[] = "0123456789abcdef";
  std::string out;
  out.reserve(len * 2);
  for (size_t i = 0; i < len; ++i) {
    out.push_back(kHex[data[i] >> 4]);
    out.push_back(kHex[data[i] & 0x0f]);
  }
  return out;
}

void UpdateSha256(crypto_hash_sha256_state* state, absl::string_view label,
                  absl::string_view bytes) {
  crypto_hash_sha256_update(state,
                            reinterpret_cast<const unsigned char*>(label.data()),
                            label.size());
  crypto_hash_sha256_update(state,
                            reinterpret_cast<const unsigned char*>(bytes.data()),
                            bytes.size());
}

absl::StatusOr<std::string> ReadFileBytes(absl::string_view path) {
  std::ifstream ifs(std::string(path), std::ios::binary);
  if (!ifs) {
    return absl::NotFoundError("File not found");
  }
  return std::string(std::istreambuf_iterator<char>(ifs),
                       std::istreambuf_iterator<char>());
}

void MaybeHashFile(crypto_hash_sha256_state* state, absl::string_view path,
                   absl::string_view label) {
  const auto bytes_or = ReadFileBytes(path);
  if (!bytes_or.ok()) {
    return;
  }
  UpdateSha256(state, label, *bytes_or);
}

}  // namespace

absl::StatusOr<std::string> Sha256HexFile(absl::string_view path) {
  const auto bytes_or = ReadFileBytes(path);
  if (!bytes_or.ok()) {
    return bytes_or.status();
  }
  unsigned char digest[crypto_hash_sha256_BYTES];
  crypto_hash_sha256(digest,
                     reinterpret_cast<const unsigned char*>(bytes_or->data()),
                     bytes_or->size());
  return BytesToHex(digest, sizeof(digest));
}

absl::StatusOr<std::string> LocalSyncDataSha256(
    const commands::UserSyncConfig& config) {
  crypto_hash_sha256_state state;
  crypto_hash_sha256_init(&state);

  if (config.sync_dictionary()) {
    const std::string dict_path =
        ConfigFileStream::GetFileName("user://user_dictionary.db");
    MaybeHashFile(&state, dict_path, "dictionary:");
  }

  if (config.sync_history()) {
    const std::string history_path =
        ConfigFileStream::GetFileName("user://.history.db");
    MaybeHashFile(&state, history_path, "history:");
  }

  unsigned char digest[crypto_hash_sha256_BYTES];
  crypto_hash_sha256_final(&state, digest);
  return BytesToHex(digest, sizeof(digest));
}

absl::StatusOr<SyncFingerprintSnapshot> CaptureSyncFingerprints(
    const commands::UserSyncConfig& config) {
  SyncFingerprintSnapshot snapshot;
  if (!config.sync_file_path().empty()) {
    const auto remote_or = Sha256HexFile(config.sync_file_path());
    if (remote_or.ok()) {
      snapshot.remote_bundle_sha256 = *remote_or;
    }
  }
  const auto local_or = LocalSyncDataSha256(config);
  if (!local_or.ok()) {
    return local_or.status();
  }
  snapshot.local_data_sha256 = *local_or;
  return snapshot;
}

IntervalSyncDecision EvaluateIntervalSync(
    const SyncFingerprintSnapshot& baseline,
    const SyncFingerprintSnapshot& current) {
  const bool baseline_missing = baseline.remote_bundle_sha256.empty() &&
                                baseline.local_data_sha256.empty();
  if (baseline_missing) {
    return IntervalSyncDecision::kBaselineOnly;
  }

  const bool remote_changed =
      !current.remote_bundle_sha256.empty() &&
      current.remote_bundle_sha256 != baseline.remote_bundle_sha256;
  const bool remote_removed =
      current.remote_bundle_sha256.empty() &&
      !baseline.remote_bundle_sha256.empty();
  const bool remote_added =
      !current.remote_bundle_sha256.empty() &&
      baseline.remote_bundle_sha256.empty();
  const bool local_changed =
      current.local_data_sha256 != baseline.local_data_sha256;

  if (remote_changed || remote_removed || remote_added || local_changed) {
    return IntervalSyncDecision::kSync;
  }
  return IntervalSyncDecision::kSkip;
}

}  // namespace sync
}  // namespace mozc
