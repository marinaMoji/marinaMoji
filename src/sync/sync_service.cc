#include "sync/sync_service.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/hash/hash.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "base/file_util.h"
#include "config/config_handler.h"
#include "dictionary/user_dictionary_importer.h"
#include "dictionary/user_dictionary_storage.h"
#include "dictionary/user_pos.h"
#include "prediction/user_history_storage.h"
#include "protocol/config.pb.h"
#include "sync/sync_bundle.h"
#include "sync/sync_config.h"
#include "sync/sync_crypto.h"
#include "sync/sync_merge.h"

namespace mozc {
namespace sync {
namespace {

std::string ExportDictionaryTsv() {
  UserDictionaryStorage storage;
  if (!storage.Load().ok()) {
    return "# marinaMoji sync dictionary\n";
  }
  std::string out = "# marinaMoji sync dictionary\n";
  for (int d = 0; d < storage.GetProto().dictionaries_size(); ++d) {
    const auto& dic = storage.GetProto().dictionaries(d);
    for (int i = 0; i < dic.entries_size(); ++i) {
      const auto& entry = dic.entries(i);
      out += entry.key();
      out += '\t';
      out += entry.value();
      out += '\t';
      out += dictionary::UserPos::GetStringPosType(entry.pos());
      out += '\t';
      out += entry.comment();
      if (!entry.locale().empty()) {
        out += '\t';
        out += entry.locale();
      }
      out += '\n';
    }
  }
  return out;
}

std::string ExportHistoryTsv() {
  prediction::UserHistoryStorage storage;
  if (!storage.Load()) {
    return "# marinaMoji sync history\n";
  }
  std::string out = "# marinaMoji sync history\n";
  storage.ForEach([&](uint64_t /*fp*/,
                      const prediction::UserHistoryStorage::Entry& entry) {
    out += absl::StrCat(entry.key(), "\t", entry.value(), "\t",
                        entry.suggestion_freq(), "\t", entry.shown_freq(),
                        "\t", entry.last_access_time(), "\n");
    return true;
  });
  return out;
}

size_t DictionaryEntryHash(const user_dictionary::UserDictionary::Entry& e) {
  return absl::HashOf(e.key(), e.value(), e.pos(), e.locale());
}

absl::Status ImportDictionaryTsv(absl::string_view tsv) {
  UserDictionaryStorage storage;
  if (!storage.Lock()) {
    return absl::InternalError("Cannot lock user dictionary");
  }
  if (!storage.Load().ok()) {
    storage.GetProto().Clear();
  }

  user_dictionary::UserDictionary* dic = nullptr;
  if (storage.GetProto().dictionaries_size() > 0) {
    dic = storage.GetProto().mutable_dictionaries(0);
  } else {
    const auto id_or = storage.CreateDictionary("User dictionary");
    if (!id_or.ok()) {
      storage.UnLock();
      return id_or.status();
    }
    for (int i = 0; i < storage.GetProto().dictionaries_size(); ++i) {
      if (storage.GetProto().dictionaries(i).id() == *id_or) {
        dic = storage.GetProto().mutable_dictionaries(i);
        break;
      }
    }
  }
  if (dic == nullptr) {
    storage.UnLock();
    return absl::InternalError("Cannot access user dictionary");
  }

  absl::flat_hash_set<size_t> existing;
  for (int i = 0; i < dic->entries_size(); ++i) {
    existing.insert(DictionaryEntryHash(dic->entries(i)));
  }

  user_dictionary::StringTextLineIterator line_iter(tsv);
  user_dictionary::TextInputIterator input_iter(user_dictionary::MOZC,
                                                &line_iter);
  user_dictionary::RawEntry raw;
  while (input_iter.Next(&raw)) {
    user_dictionary::UserDictionary::Entry entry;
    if (!user_dictionary::ConvertEntry(raw, &entry)) {
      continue;
    }
    const size_t h = DictionaryEntryHash(entry);
    if (existing.contains(h)) {
      continue;
    }
    existing.insert(h);
    *dic->add_entries() = entry;
  }

  const absl::Status save_status = storage.Save();
  storage.UnLock();
  return save_status;
}

absl::Status ImportHistoryTsv(absl::string_view tsv) {
  prediction::UserHistoryStorage storage;
  storage.Load();
  for (absl::string_view line : absl::StrSplit(tsv, '\n')) {
    line = absl::StripAsciiWhitespace(line);
    if (line.empty() || absl::StartsWith(line, "#")) {
      continue;
    }
    const std::vector<std::string> fields = absl::StrSplit(line, '\t');
    if (fields.size() < 2) {
      continue;
    }
    prediction::UserHistoryStorage::Entry entry;
    entry.set_key(fields[0]);
    entry.set_value(fields[1]);
    if (fields.size() > 2) {
      uint32_t v = 0;
      if (absl::SimpleAtoi(fields[2], &v)) {
        entry.set_suggestion_freq(v);
      }
    }
    if (fields.size() > 3) {
      uint32_t v = 0;
      if (absl::SimpleAtoi(fields[3], &v)) {
        entry.set_shown_freq(v);
      }
    }
    if (fields.size() > 4) {
      uint64_t v = 0;
      if (absl::SimpleAtoi(fields[4], &v)) {
        entry.set_last_access_time(v);
      }
    }
    storage.Insert(std::move(entry));
  }
  if (!storage.Save()) {
    return absl::InternalError("Failed to save user history");
  }
  return absl::OkStatus();
}

std::string BuildManifest(const commands::UserSyncConfig& config) {
  return absl::StrCat("version=1\n"
                      "device_id=",
                      config.device_id(), "\n"
                      "updated_at=",
                      absl::FormatTime(absl::Now()), "\n");
}

absl::Status ReadFileToString(absl::string_view path, std::string* out) {
  std::ifstream ifs(std::string(path), std::ios::binary);
  if (!ifs) {
    return absl::NotFoundError("Sync file not found");
  }
  *out = std::string(std::istreambuf_iterator<char>(ifs),
                     std::istreambuf_iterator<char>());
  return absl::OkStatus();
}

absl::Status WriteStringToFileAtomically(absl::string_view path,
                                         absl::string_view content) {
  const std::string tmp = absl::StrCat(path, ".tmp");
  {
    std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
    if (!ofs) {
      return absl::PermissionDeniedError("Cannot write sync file");
    }
    ofs.write(content.data(), content.size());
  }
  return FileUtil::AtomicRename(tmp, path);
}

}  // namespace

absl::StatusOr<commands::UserSyncReport> PerformSync(
    const PerformSyncOptions& options, SyncProgressCallback progress) {
  auto notify = [&](const std::string& phase, double p,
                    const std::string& msg) {
    if (progress) {
      progress(phase, p, msg);
    }
  };

  commands::UserSyncReport report;
  if (!options.config.enabled()) {
    report.set_success(false);
    report.set_error_message("Sync is disabled");
    return report;
  }
  if (options.config.sync_file_path().empty()) {
    report.set_success(false);
    report.set_error_message("Sync file path is not set");
    return report;
  }
  if (options.passphrase.empty()) {
    report.set_success(false);
    report.set_error_message("Sync key is not set");
    return report;
  }

  const commands::UserSyncConfig config =
      EnsureDeviceId(options.config);
  const auto direction = options.direction;

  notify("export", 0.4, "Exporting local data…");

  SyncBundleFiles local_files;
  local_files[std::string(kManifestFile)] = BuildManifest(config);

  if (config.sync_settings()) {
    config::Config settings =
        ExtractSyncSettings(*config::ConfigHandler::GetSharedConfig());
    std::string settings_pb;
    if (!settings.SerializeToString(&settings_pb)) {
      report.set_success(false);
      report.set_error_message("Failed to serialize settings");
      return report;
    }
    local_files[std::string(kSettingsFile)] = std::move(settings_pb);
  }

  if (config.sync_dictionary()) {
    local_files[std::string(kDictionaryFile)] = ExportDictionaryTsv();
  }

  if (config.sync_history() &&
      !config::ConfigHandler::GetSharedConfig()->incognito_mode()) {
    local_files[std::string(kHistoryFile)] = ExportHistoryTsv();
  }

  SyncBundleFiles merged = local_files;

  if (direction != commands::UserSyncConfig::UPLOAD) {
    notify("download", 0.5, "Reading remote bundle…");
    std::string remote_encrypted;
    const absl::Status read_status =
        ReadFileToString(config.sync_file_path(), &remote_encrypted);
    if (read_status.ok()) {
      absl::StatusOr<std::string> remote_zip =
          DecryptWithPassphrase(remote_encrypted, options.passphrase);
      if (!remote_zip.ok()) {
        report.set_success(false);
        report.set_error_message(remote_zip.status().ToString());
        return report;
      }
      absl::StatusOr<SyncBundleFiles> remote_files =
          UnpackBundle(*remote_zip);
      if (!remote_files.ok()) {
        report.set_success(false);
        report.set_error_message(remote_files.status().ToString());
        return report;
      }

      if (config.sync_settings() && remote_files->contains(kSettingsFile)) {
        config::Config remote_settings;
        config::Config local_settings;
        if (!local_settings.ParseFromString(local_files[kSettingsFile])) {
          report.set_success(false);
          report.set_error_message("Failed to parse local settings payload");
          return report;
        }
        if (!remote_settings.ParseFromString((*remote_files)[kSettingsFile])) {
          report.set_success(false);
          report.set_error_message("Failed to parse remote settings payload");
          return report;
        }
        const config::Config merged_settings =
            MergeSettingsConfig(local_settings, remote_settings);
        std::string settings_pb;
        if (!merged_settings.SerializeToString(&settings_pb)) {
          report.set_success(false);
          report.set_error_message("Failed to serialize merged settings");
          return report;
        }
        merged[kSettingsFile] = std::move(settings_pb);
      }

      if (config.sync_dictionary()) {
        DictionaryMergeStats dict_stats;
        std::string merged_dict;
        const std::string local_dict =
            local_files.contains(kDictionaryFile)
                ? local_files[kDictionaryFile]
                : "# marinaMoji sync dictionary\n";
        const std::string remote_dict =
            remote_files->contains(kDictionaryFile)
                ? (*remote_files)[kDictionaryFile]
                : "# marinaMoji sync dictionary\n";
        const absl::Status merge_status =
            MergeDictionaryTsv(remote_dict, local_dict, &merged_dict, &dict_stats);
        if (!merge_status.ok()) {
          report.set_success(false);
          report.set_error_message(merge_status.ToString());
          return report;
        }
        merged[kDictionaryFile] = std::move(merged_dict);
        report.set_dictionary_added(dict_stats.added);
        report.set_dictionary_skipped(dict_stats.skipped);
      }

      if (config.sync_history()) {
        HistoryMergeStats hist_stats;
        std::string merged_hist;
        const std::string local_hist =
            local_files.contains(kHistoryFile)
                ? local_files[kHistoryFile]
                : "# marinaMoji sync history\n";
        const std::string remote_hist =
            remote_files->contains(kHistoryFile)
                ? (*remote_files)[kHistoryFile]
                : "# marinaMoji sync history\n";
        const absl::Status merge_status =
            MergeHistoryTsv(remote_hist, local_hist, &merged_hist, &hist_stats);
        if (!merge_status.ok()) {
          report.set_success(false);
          report.set_error_message(merge_status.ToString());
          return report;
        }
        merged[kHistoryFile] = std::move(merged_hist);
        report.set_history_merged(hist_stats.merged);
      }

      merged[kManifestFile] = BuildManifest(config);
    } else if (!options.force &&
               direction == commands::UserSyncConfig::DOWNLOAD) {
      report.set_success(false);
      report.set_error_message("Remote sync file not found");
      return report;
    }
  }

  if (direction != commands::UserSyncConfig::DOWNLOAD) {
    notify("upload", 0.7, "Writing sync bundle…");
    absl::StatusOr<std::string> zip = PackBundle(merged);
    if (!zip.ok()) {
      report.set_success(false);
      report.set_error_message(zip.status().ToString());
      return report;
    }
    absl::StatusOr<std::string> encrypted =
        EncryptWithPassphrase(*zip, options.passphrase);
    if (!encrypted.ok()) {
      report.set_success(false);
      report.set_error_message(encrypted.status().ToString());
      return report;
    }
    const absl::Status write_status =
        WriteStringToFileAtomically(config.sync_file_path(), *encrypted);
    if (!write_status.ok()) {
      report.set_success(false);
      report.set_error_message(write_status.ToString());
      return report;
    }
  }

  if (direction != commands::UserSyncConfig::UPLOAD) {
    notify("import", 0.8, "Importing merged data…");
    if (config.sync_settings() && merged.contains(kSettingsFile)) {
      config::Config remote_settings;
      if (!remote_settings.ParseFromString(merged[kSettingsFile])) {
        report.set_success(false);
        report.set_error_message("Failed to parse merged settings payload");
        return report;
      }
      config::Config applied = ApplySyncSettings(
          *config::ConfigHandler::GetSharedConfig(), remote_settings);
      config::ConfigHandler::SetConfig(applied);
    }
    if (config.sync_dictionary() && merged.contains(kDictionaryFile)) {
      const absl::Status import_status = ImportDictionaryTsv(merged[kDictionaryFile]);
      if (!import_status.ok()) {
        report.set_success(false);
        report.set_error_message(import_status.ToString());
        return report;
      }
    }
    if (config.sync_history() && merged.contains(kHistoryFile)) {
      const absl::Status import_status = ImportHistoryTsv(merged[kHistoryFile]);
      if (!import_status.ok()) {
        report.set_success(false);
        report.set_error_message(import_status.ToString());
        return report;
      }
    }
  }

  report.set_success(true);
  report.set_status("OK");
  return report;
}

}  // namespace sync
}  // namespace mozc
