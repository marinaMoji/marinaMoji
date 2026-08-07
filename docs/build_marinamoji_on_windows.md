# How to build marinaMoji on Windows

> **Start here for status/design:** [WINDOWS_PORT_PLAN.md](./WINDOWS_PORT_PLAN.md) —
> branding, feature-parity checklist, floating toolbar, sync, auto-update, and
> current known gaps. **marinaMoji does ship a Windows port** (branded
> `marinamoji_*.exe`/`.dll`, `marinaMoji64.msi`) — this file is the longer
> build **reference**, largely unchanged from upstream Mozc's own build
> steps since the Windows toolchain/dependencies (Qt, Bazel, `update_deps.py`)
> are shared as-is; only the target/output names below differ from stock
> Mozc.

<!-- disableFinding(LINK_RELATIVE_G3DOC) -->

[![Windows](https://github.com/marinaMoji/marinaMoji/actions/workflows/windows.yaml/badge.svg)](https://github.com/marinaMoji/marinaMoji/actions/workflows/windows.yaml)

## Summary

If you are unsure about what the following commands do, please review the
descriptions below to understand the operations before running them.

```
git clone https://github.com/marinaMoji/marinaMoji.git
cd marinaMoji\src

python build_tools/update_deps.py
python build_tools/build_qt.py --release --confirm_license
bazelisk build package --config release_build

python build_tools/open.py bazel-bin\win32\installer\marinaMoji64.msi
```

💡 Output is **`marinaMoji64.msi`** (target `//win32/installer`, same as CI
builds — see `.github/workflows/windows.yaml`), not upstream Mozc's
`Mozc64.msi`; everything else in this file (dependencies, Qt setup,
`--config x86_simd` for x64, cross-arch notes) matches stock Mozc's own
Windows build since none of that changed for this fork.

> [!TIP]
>
> You can also download `marinaMoji64.msi` (or `marinaMoji64_arm64.msi`) as a
> build artifact from GitHub Actions without building locally. Check
> [Build with GitHub Actions](#build-with-github-actions) for details. The
> installer is currently **unsigned**, so Windows SmartScreen will warn on
> first run — click "More info" → "Run anyway" (see
> [WINDOWS_PORT_PLAN.md](./WINDOWS_PORT_PLAN.md) and the repo `CHANGELOG.md`
> for why, and the plan to fix it via SignPath).

## Setup

### System Requirements

64-bit Windows 10 or later.

### Software Requirements

Building Mozc on Windows requires the following software.

*   [Visual Studio 2022 Community Edition](https://visualstudio.microsoft.com/downloads/#visual-studio-community-2022)
    with the following components.
    *   Windows 11 SDK
    *   MSVC v143 - VS 2022 C++ x64/x86 build tools (Latest)
    *   MSVC v143 - VS 2022 C++ ARM64/ARM64EC build tools (Latest)
    *   C++ ATL for latest v143 build tools (x86 & x64)
    *   C++ ATL for latest v143 build tools (ARM64/ARM64EC)
*   Python 3.12 or later.
*   `.NET 6` or later (for `dotnet` command).
*   [Bazelisk](https://github.com/bazelbuild/bazelisk)

> [!TIP]
>
> The following Visual Studio components can be skipped if you do not build Mozc
> for ARM64.
>
>  *   MSVC v143 - VS 2022 C++ ARM64/ARM64EC build tools (Latest)
>  *   C++ ATL for latest v143 build tools (ARM64/ARM64EC)

> [!TIP]
>
> Visual Studio 2026 Community Edition is also supported to build Mozc. When
> both VS 2022 and 2026 are installed, VS 2022 will be used.

> [!NOTE]
>
> Bazelisk is a wrapper of [Bazel](https://bazel.build) that allows you to use a
> specific version of Bazel.

### Download the repository from GitHub

```
git clone https://github.com/marinaMoji/marinaMoji.git
cd marinaMoji\src
```

Hereafter you can do all the operations without changing directory.

### Check out additional build dependencies

```
python build_tools/update_deps.py
```

In this step, additional build dependencies will be downloaded, including:

*   [LLVM 20.1.1](https://github.com/llvm/llvm-project/releases/tag/llvmorg-20.1.1)
*   [MSYS2 2025-02-21](https://github.com/msys2/msys2-installer/releases/tag/2025-02-21)
*   [Ninja 1.11.0](https://github.com/ninja-build/ninja/releases/tag/v1.11.0)
*   [Qt 6.9.1](https://download.qt.io/archive/qt/6.8/6.8.0/submodules/qtbase-everywhere-src-6.9.1.tar.xz)
*   [.NET tools](../dotnet-tools.json)

## Build

### Build Qt

```
python build_tools/build_qt.py --release --confirm_license
```

If you would like to manually confirm the Qt license, omit the
`--confirm_license` option.

### Build Mozc

Assuming `bazelisk` is in your `%PATH%`, run the following command to build Mozc
for Windows.

```
bazelisk build package --config release_build
```

#### Install marinaMoji

After building marinaMoji, run the following command to install it:

```
python build_tools/open.py bazel-bin/win32/installer/marinaMoji64.msi
```

#### Uninstall marinaMoji

To uninstall marinaMoji, press <kbd>Win</kbd>+<kbd>R</kbd> to open the Run dialog and
type `ms-settings:appsfeatures-app`, run the following command in the terminal:

```
start ms-settings:appsfeatures-app
```

Then, uninstall `marinaMoji` from the list of installed applications.

### Cross compilation

By default, `marinaMoji64.msi` is built for the host CPU architecture. To
explicitly specify the target CPU architecture, specify build options as
follows:

#### To build x64 installer

```
python build_tools/build_qt.py --release --confirm_license --target_arch=x64
bazelisk build package --config release_build --platforms=//:windows-x86_64
```

#### To build ARM64 installer

```
python build_tools/build_qt.py --release --confirm_license --target_arch=arm64
bazelisk build package --config release_build --platforms=//:windows-arm64
```

#### To build a universal installer for both X64 and ARM64

```
python build_tools/build_qt.py --release --confirm_license --target_arch=x64
bazelisk build package --config release_build --platforms=//:windows-x86_64 --config win_universal_installer
```

## Bazel command examples

### Bazel User Guide

*   [Build programs with Bazel](https://bazel.build/run/build)
*   [Commands and Options](https://bazel.build/docs/user-manual)
*   [Write bazelrc configuration files](https://bazel.build/run/bazelrc)

### Run all tests

```
bazelisk test ... --build_tests_only -c dbg
```

> [!NOTE]
>
> `...` means all targets under the current and subdirectories.

--------------------------------------------------------------------------------

## Build with GitHub Actions

GitHub Actions are already set up in
[windows.yaml](../.github/workflows/windows.yaml). With that, you can build and
install marinaMoji with your own commit as follows.

1.  Fork https://github.com/marinaMoji/marinaMoji to your GitHub repository.
2.  Push a new commit to your own fork.
3.  Click "Actions" tab on your fork.
4.  Wait until the action triggered by your commit succeeds.
5.  Download `marinaMoji64_x64.msi` (or `marinaMoji64_arm64.msi`) from the
    action result page.
6.  Install it. SmartScreen will warn since the installer is currently
    unsigned — click "More info" → "Run anyway".

Files on the GitHub Actions page remain available for up to 90 days.

You can also find marinaMoji installers for Windows built straight from this
repository, without forking:

1.  Sign in to GitHub.
2.  Check
    [recent successful Windows runs](https://github.com/marinaMoji/marinaMoji/actions/workflows/windows.yaml?query=is%3Asuccess)
    in the marinaMoji repository.
3.  Find an action from the last 90 days and click it.
4.  Download `marinaMoji64_x64.msi` or `marinaMoji64_arm64.msi` from the
    action result page depending on your CPU architecture.

--------------------------------------------------------------------------------

## Build with GYP (deprecated):

⚠️ The GYP build is deprecated and no longer supported.

Please check the previous version for more information.
https://github.com/google/mozc/blob/3.33.6089/docs/build_mozc_in_windows.md#build-with-gyp-maintenance-mode
