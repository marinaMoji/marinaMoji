# How to build marinaMoji on macOS

> **Start here for day-to-day builds:** [MACOS_PORT_PLAN.md](./MACOS_PORT_PLAN.md) — `MOZC_QT_PATH`, `//mac:mozc_macos`, `install_marinamoji.sh`, `.pkg` for VM, LaunchAgents, and troubleshooting. This file is the longer **reference** (Xcode/Qt setup, cross-arch builds, unit tests). Install branding: [MARINAMOJI.md](./MARINAMOJI.md).

<!-- disableFinding(LINK_RELATIVE_G3DOC) -->

[![macOS](https://github.com/google/mozc/actions/workflows/macos.yaml/badge.svg)](https://github.com/google/mozc/actions/workflows/macos.yaml)

## Summary

If you are not sure what the following commands do, read
[MACOS_PORT_PLAN.md](./MACOS_PORT_PLAN.md) first.

```bash
git clone https://github.com/marinaMoji/marinaMoji.git --recursive
cd marinaMoji/src

export MOZC_QT_PATH=/opt/homebrew/opt/qt   # Homebrew Qt; required each build shell
bazelisk build --config=oss_macos //mac:mozc_macos
bash mac/install_marinamoji.sh
```

Dev install copies `bazel-bin/mac/mozc_macos_archive-root/marinaMoji.app` into
`/Library/Input Methods/`, registers the IME, fixes bundled Qt paths, and
restarts converter/renderer processes.

**Optional `.pkg` installer** (VM / machine without Homebrew Qt):

```bash
export MOZC_QT_PATH=/opt/homebrew/opt/qt
bazelisk build --config=oss_macos --spawn_strategy=local //mac:package
open bazel-bin/mac/marinaMoji.pkg
```

💡 Binaries match the build machine’s CPU unless you pass `--macos_cpus=…` (see
below). Upstream stock Mozc used `bazelisk build package` → `Mozc.pkg`; marinaMoji
uses **`--config oss_macos`**, target **`//mac:mozc_macos`** or **`//mac:package`**, and
output **`marinaMoji.app`** / **`marinaMoji.pkg`**.

## Setup

### System Requirements

64-bit macOS 12 and later versions are supported.

### Software Requirements

Building on Mac requires the following software.

*   [Xcode](https://apps.apple.com/us/app/xcode/id497799835)
    *   Xcode 16.0 or later
    *   ⚠️Xcode Command Line Tools aren't sufficient.
*   [Bazelisk](https://github.com/bazelbuild/bazelisk)
*   Python 3.12 or later.
*   **Qt 6** — marinaMoji dev builds typically use Homebrew: `brew install qt`. Set `export MOZC_QT_PATH=/opt/homebrew/opt/qt` before every Bazel build (see [MACOS_PORT_PLAN.md](./MACOS_PORT_PLAN.md)). Alternatively, build Qt from source with `build_tools/build_qt.py` (CMake required).

## Get the Code

You can download marinaMoji source code as follows:

```
git clone https://github.com/marinaMoji/marinaMoji.git --recursive
cd marinaMoji/src
```

Hereafter you can do all the operations without changing directory.

### Check out additional build dependencies

```
python build_tools/update_deps.py
```

In this step, additional build dependencies will be downloaded.

*   [Ninja 1.11.0](https://github.com/ninja-build/ninja/releases/download/v1.11.0/ninja-mac.zip)
*   [Qt 6.9.1](https://download.qt.io/archive/qt/6.8/6.8.0/submodules/qtbase-everywhere-src-6.9.1.tar.xz)

You can specify `--noqt` option if you would like to use your own Qt binaries.

### Build Qt

```
python3 build_tools/build_qt.py --release --confirm_license
```

Drop `--confirm_license` option if you would like to manually confirm the Qt
license.

You can also specify `--debug` option to build debug version of Mozc.

```
python3 build_tools/build_qt.py --release --debug --confirm_license
```

You can also specify `--macos_cpus` option, which has the same semantics as the
[same name option in Bazel](https://bazel.build/reference/command-line-reference#flag--macos_cpus),
for cross-build including building a Universal macOS Binary.

```
# Building x86_64 binaries regardless of the host CPU architecture.
python3 build_tools/build_qt.py --release --debug --confirm_license --macos_cpus=x86_64

# Building Universal macOS Binary for both x86_64 and arm64.
python3 build_tools/build_qt.py --release --debug --confirm_license --macos_cpus=x86_64,arm64
```

You can skip this process if you have already installed Qt prebuilt binaries.

CMake is also required to build Qt. If you use `brew`, you can install `cmake`
as follows.

```
brew install cmake
```

--------------------------------------------------------------------------------

## Build with Bazel

### Build marinaMoji (dev install)

With Homebrew Qt (recommended for daily development):

```
export MOZC_QT_PATH=/opt/homebrew/opt/qt
bazelisk build --config=oss_macos //mac:mozc_macos
bash mac/install_marinamoji.sh
```

See [MACOS_PORT_PLAN.md](./MACOS_PORT_PLAN.md) for manual `ditto`, LaunchAgents, and `bazel-bin` paths.

### Build installer (`.pkg`)

```
export MOZC_QT_PATH=/opt/homebrew/opt/qt
bazelisk build --config=oss_macos --spawn_strategy=local //mac:package
open bazel-bin/mac/marinaMoji.pkg
```

Stock upstream Mozc used `bazelisk build package --config release_build` → `Mozc.pkg`. marinaMoji requires **`--config oss_macos`** and produces **`marinaMoji.pkg`**.

#### How to specify target CPU architectures

To build an Intel64 macOS binary regardless of the host CPU architecture.

```
python3 build_tools/build_qt.py --release --debug --confirm_license --macos_cpus=x86_64
export MOZC_QT_PATH=/path/to/built/qt   # if not using Homebrew
bazelisk build --config=oss_macos --macos_cpus=x86_64 //mac:package
open bazel-bin/mac/marinaMoji.pkg
```

To build a Universal macOS Binary both x86_64 and arm64.

```
python3 build_tools/build_qt.py --release --debug --confirm_license --macos_cpus=x86_64,arm64
bazelisk build --config=oss_macos --macos_cpus=x86_64,arm64 //mac:package
open bazel-bin/mac/marinaMoji.pkg
```

### Unit tests

```
bazelisk test ... --build_tests_only -c dbg
```

See [build marinaMoji in Docker](build_marinamoji_in_docker.md#unittests) for details.

### Edit src/config.bzl

You can modify variables in `src/config.bzl` to fit your environment. Note: `~`
does not represent the home directory. The exact path should be specified (e.g.
`MACOS_QT_PATH = "/Users/mozc/myqt"`).

Tips: the following command makes the specified file untracked by Git.

```
git update-index --assume-unchanged src/config.bzl
```

This command reverts the above change.

```
git update-index --no-assume-unchanged src/config.bzl
```

--------------------------------------------------------------------------------

## Build with GitHub Actions

GitHub Actions steps are already set up in
[macos.yaml](../.github/workflows/macos.yaml). With that, you can build and
install Mozc with your own commit as follows.

1.  Fork https://github.com/google/mozc to your GitHub repository.
2.  Push a new commit to your own fork.
3.  Click "Actions" tab on your fork.
4.  Wait until the action triggered with your commit succeeds.
5.  Download `Mozc.pkg` from the action result page.
6.  Install `Mozc.pkg`.

Files in the GitHub Actions page remain available up to 90 days.

You can also find Mozc Installers for macOS in google/mozc repository. Please
keep in mind that Mozc is not an officially supported Google product, even if
downloaded from https://github.com/google/mozc/.

1.  Sign in GitHub.
2.  Check
    [recent successful macOS runs](https://github.com/google/mozc/actions/workflows/macos.yaml?query=is%3Asuccess)
    in google/mozc repository.
3.  Find action in last 90 days and click it.
4.  Download `Mozc.pkg` from the action result page.

--------------------------------------------------------------------------------

## Build with GYP (deprecated):

⚠️ The GYP build is deprecated and no longer supported.

Please check the previous version for more information.
https://github.com/google/mozc/blob/3.33.6089/docs/build_mozc_in_osx.md#build-with-gyp-maintenance-mode

--------------------------------------------------------------------------------

## marinaMoji feature notes

For marinaMoji-specific macOS behavior and toolbar features (including odoriji, symbols palette, and custom symbol settings), see:

- [MACOS_PORT_PLAN.md](MACOS_PORT_PLAN.md) — **primary** rebuild/install/troubleshooting guide
- [SYMBOLS_PALETTE.md](SYMBOLS_PALETTE.md)
- [compiling_instructions_for_marina.md](compiling_instructions_for_marina.md) — Linux counterpart
