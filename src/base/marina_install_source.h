// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: Apache-2.0
//
// marinaMoji on Windows is distributed through the Microsoft Store, which
// installs the same MSI as a direct download but passes INSTALLSOURCE=store
// (see win32/installer/installer_marinamoji_64bit.wxs). The installer records
// it in the registry so the app knows the Store updates it and must not run
// its own updater.

#ifndef MOZC_BASE_MARINA_INSTALL_SOURCE_H_
#define MOZC_BASE_MARINA_INSTALL_SOURCE_H_

#include <string_view>

namespace mozc {

// Where the Store listing of marinaMoji lives; shown to installs that did not
// come from the Store so they can switch to it.
inline constexpr char kMarinaStoreUrl[] =
    "https://apps.microsoft.com/detail/xpdm29n64d0t42";

// True if |value| (the InstallSource registry value) means "installed by the
// Microsoft Store". Separate from the registry read so it can be tested.
bool IsMarinaStoreInstallSource(std::string_view value);

// True if this installation was made by the Microsoft Store. Always false on
// platforms other than Windows.
bool MarinaIsStoreInstall();

}  // namespace mozc

#endif  // MOZC_BASE_MARINA_INSTALL_SOURCE_H_
