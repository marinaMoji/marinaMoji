#ifndef MOZC_BASE_MARINA_CURL_FETCH_H_
#define MOZC_BASE_MARINA_CURL_FETCH_H_

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace mozc {

// Fetches a URL body with curl (must be on PATH). Used for GitHub update checks.
absl::StatusOr<std::string> MarinaCurlGet(absl::string_view url);

// Downloads a URL to |dest_path| with curl.
absl::Status MarinaCurlDownload(absl::string_view url,
                                absl::string_view dest_path);

// Opens a local file with the OS default handler (Installer.app for .pkg).
bool MarinaOpenLocalPath(absl::string_view path);

}  // namespace mozc

#endif  // MOZC_BASE_MARINA_CURL_FETCH_H_
