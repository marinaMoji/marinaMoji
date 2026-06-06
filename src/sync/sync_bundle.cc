#include "sync/sync_bundle.h"

#define MINIZ_NO_STDIO
#define MINIZ_NO_TIME
#include "sync/vendor/miniz/miniz.h"

#include "absl/status/status.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace sync {

absl::StatusOr<std::string> PackBundle(const SyncBundleFiles& files) {
  mz_zip_archive zip;
  memset(&zip, 0, sizeof(zip));
  if (!mz_zip_writer_init_heap(&zip, 0, 1024 * 1024)) {
    return absl::InternalError("mz_zip_writer_init_heap failed");
  }

  for (const auto& [name, content] : files) {
    if (!mz_zip_writer_add_mem(&zip, name.c_str(), content.data(),
                               content.size(), MZ_NO_COMPRESSION)) {
      mz_zip_writer_end(&zip);
      return absl::InternalError("mz_zip_writer_add_mem failed");
    }
  }

  void* heap = nullptr;
  size_t size = 0;
  if (!mz_zip_writer_finalize_heap_archive(&zip, &heap, &size)) {
    mz_zip_writer_end(&zip);
    return absl::InternalError("mz_zip_writer_finalize_heap_archive failed");
  }
  mz_zip_writer_end(&zip);

  std::string out(static_cast<const char*>(heap), size);
  mz_free(heap);
  return out;
}

absl::StatusOr<SyncBundleFiles> UnpackBundle(absl::string_view zip_data) {
  mz_zip_archive zip;
  memset(&zip, 0, sizeof(zip));
  if (!mz_zip_reader_init_mem(&zip, zip_data.data(), zip_data.size(), 0)) {
    return absl::InvalidArgumentError("Invalid sync bundle zip");
  }

  SyncBundleFiles files;
  const mz_uint num_files = mz_zip_reader_get_num_files(&zip);
  for (mz_uint i = 0; i < num_files; ++i) {
    mz_zip_archive_file_stat stat;
    if (!mz_zip_reader_file_stat(&zip, i, &stat)) {
      mz_zip_reader_end(&zip);
      return absl::DataLossError("Failed to read zip entry stat");
    }
    if (stat.m_is_directory) {
      continue;
    }
    const size_t uncomp_size = static_cast<size_t>(stat.m_uncomp_size);
    std::string content(uncomp_size, '\0');
    if (!mz_zip_reader_extract_to_mem(&zip, i, content.data(), uncomp_size,
                                      0)) {
      mz_zip_reader_end(&zip);
      return absl::DataLossError("Failed to extract zip entry");
    }
    files[stat.m_filename] = std::move(content);
  }

  mz_zip_reader_end(&zip);
  return files;
}

}  // namespace sync
}  // namespace mozc
