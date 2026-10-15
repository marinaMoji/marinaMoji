// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0

#include "base/marina_install_source.h"

#include <string_view>

#include "absl/strings/ascii.h"

#ifdef _WIN32
#include <windows.h>

#include <string>
#endif  // _WIN32

namespace mozc {

bool IsMarinaStoreInstallSource(std::string_view value) {
  return absl::AsciiStrToLower(absl::StripAsciiWhitespace(value)) == "store";
}

bool MarinaIsStoreInstall() {
#ifdef _WIN32
  // The installer is 64-bit and writes the 64-bit registry view.
  wchar_t buffer[32] = {};
  DWORD size = sizeof(buffer) - sizeof(wchar_t);
  const LSTATUS result = ::RegGetValueW(
      HKEY_LOCAL_MACHINE, L"SOFTWARE\\marinaMoji", L"InstallSource",
      RRF_RT_REG_SZ | RRF_SUBKEY_WOW64_64KEY, nullptr, buffer, &size);
  if (result != ERROR_SUCCESS) {
    return false;
  }
  std::string narrow;
  for (const wchar_t* p = buffer; *p != L'\0'; ++p) {
    narrow.push_back(*p < 128 ? static_cast<char>(*p) : '?');
  }
  return IsMarinaStoreInstallSource(narrow);
#else
  return false;
#endif  // _WIN32
}

}  // namespace mozc
