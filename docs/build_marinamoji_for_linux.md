# How to build marinaMoji for Linux desktop

> **Start here for day-to-day builds:** [compiling_instructions_for_marina.md](compiling_instructions_for_marina.md) — copy-paste Ubuntu packages, build, **uninstall**, sync daemon, IBus reload, and troubleshooting. This file is the longer **reference** (Bazel options, test commands, install paths). Install branding: [MARINAMOJI.md](MARINAMOJI.md).

<!-- disableFinding(LINK_RELATIVE_G3DOC) -->

[![Linux](https://github.com/google/mozc/actions/workflows/linux.yaml/badge.svg)](https://github.com/google/mozc/actions/workflows/linux.yaml)

## Summary

If you are not sure what the following commands do, read
[compiling_instructions_for_marina.md](compiling_instructions_for_marina.md) first.

```sh
git clone https://github.com/marinaMoji/marinaMoji.git --recursive
cd marinaMoji/src

bazelisk build package --config oss_linux --config release_build \
  --repo_env=CC=clang --repo_env=CXX=clang++
```

`bazel-bin/unix/mozc.zip` contains the installable files. Install with:

```sh
sudo unzip -o bazel-bin/unix/mozc.zip -d /
ibus write-cache && ibus restart
```

Then add **marinaMoji** in **Settings → Keyboard → Input Sources**. See
[compiling_instructions_for_marina.md](compiling_instructions_for_marina.md) for
the full package list (including GTK and librsvg for the toolbar), optional sync
daemon setup, and uninstall steps.

## System Requirements

Due to the diverse nature of Linux desktop ecosystem, continuous builds on
GitHub Actions are the best example on how marinaMoji (Mozc fork) executables for Linux desktop
can be built and tested against existing test cases.

*   [`.github/workflows/linux.yaml`](../.github/workflows/linux.yaml)
*   [CI for Linux](https://github.com/google/mozc/actions/workflows/linux.yaml)

The following sections describe relevant software components that are necessary
to build marinaMoji for Linux desktop.

### Bazelisk (required)

[Bazelisk](https://github.com/bazelbuild/bazelisk) is a wrapper of
[Bazel](https://bazel.build) to use the specific version of Bazel.

The Bazel version specified in [`src/.bazeliskrc`](../src/.bazeliskrc) is what
continuous builds are testing against.

As of this writing, the repository is pinned to:

`USE_BAZEL_VERSION=9.0.2`

If your distro `bazel` package is older, builds may fail with module/toolchain
errors. Prefer `bazelisk` so the correct Bazel version is downloaded
automatically.

See the following document for detail on how Bazelisk determines the Bazel
version.

*   [How does Bazelisk know which Bazel version to run?](https://github.com/bazelbuild/bazelisk/blob/master/README.md#how-does-bazelisk-know-which-bazel-version-to-run)

⚠️ Bazel version mismatch is a major source of build failures. If you manually
install Bazel and use it instead of Bazelisk, make sure it matches
`src/.bazeliskrc` exactly.

### C++ toolchain

GCC or Clang is needed to build marinaMoji.

While Linux continuous builds currently use GCC, the C++ code is designed to
be compatible with Clang (for macOS, Windows, Android, and Google internal use).
For marinaMoji on Ubuntu, **Clang is recommended** — see
[compiling_instructions_for_marina.md](compiling_instructions_for_marina.md).

💡 See [`.github/workflows/linux.yaml`](../.github/workflows/linux.yaml) on which
version of GCC is tested against.

💡 Like many other Bazel-based C++ projects, marinaMoji relies on
[`rules_cc`](https://github.com/bazelbuild/rules_cc/) specified in
[`MODULE.bazel`](../src/MODULE.bazel) to automatically detect C++ toolchains in
the host environment.

### Packages

Development packages referenced in `pkg_config_repository` at
[`src/MODULE.bazel`](../src/MODULE.bazel) need to be installed beforehand.

```
# iBus
pkg_config_repository(
    name = "ibus",
    packages = [
        "glib-2.0",
        "gobject-2.0",
        "ibus-1.0",
    ],
)
```

```
# Qt for Linux
pkg_config_repository(
    name = "qt_linux",
    packages = [
        "Qt6Core",
        "Qt6Gui",
        "Qt6Widgets",
    ],
)
```

```
# OpenCC (optional: for Traditional kanji / kyūjitai conversion)
pkg_config_repository(
    name = "opencc",
    packages = ["opencc"],
)
```

Install the OpenCC development package so the build can link libopencc and enable the Shin/Kyū (traditional kanji) rewriter: e.g. `libopencc-dev` (Debian/Ubuntu), `opencc-devel` (Fedora), or `opencc` (Arch). If OpenCC is not installed, the build still succeeds; the kyūjitai toggle and menu option will have no effect on conversion output.

```
# GTK3 + librsvg (marinaMoji floating toolbar)
pkg_config_repository(
    name = "gtk3",
    packages = ["gtk+-3.0"],
)
```

Install `libgtk-3-dev` and `librsvg2-dev` (Debian/Ubuntu) before building the toolbar. See [compiling_instructions_for_marina.md](compiling_instructions_for_marina.md).

💡 `pkg_config_repository` is not a bazel standard functionality. It is a custom
macro defined in
[`src/bazel/pkg_config_repository.bzl`](../src/bazel/pkg_config_repository.bzl).

## Build instructions

### Get the Code

You can download marinaMoji source code as follows.

```sh
git clone https://github.com/marinaMoji/marinaMoji.git --recursive
cd marinaMoji/src
```

Hereafter you can do all the operations without changing directory.

### Build marinaMoji

You should be able to build marinaMoji for Linux desktop as follows, assuming
`bazelisk` is in your `$PATH`.

```sh
bazelisk build package --config oss_linux --config release_build \
  --repo_env=CC=clang --repo_env=CXX=clang++
```

`package` is an alias to build marinaMoji executables and archive them into
`mozc.zip`.

### Install and register with IBus (marinaMoji)

After building, install the package and make marinaMoji appear in your input method list. For uninstall, sync daemon, and troubleshooting, see [compiling_instructions_for_marina.md](compiling_instructions_for_marina.md).

1. **Install files** (from `src/` directory):
   ```bash
   sudo unzip -o bazel-bin/unix/mozc.zip -d /
   ```

2. **Verify** the component and engine are in place:
   ```bash
   test -f /usr/share/ibus/component/marinamoji.xml && echo "Component OK"
   test -x /usr/lib/ibus-marinamoji/ibus-engine-marinamoji && echo "Engine OK"
   ```

3. **Reload IBus** so it picks up the new component:
   ```bash
   ibus write-cache
   ibus restart
   ```
   If you use a desktop session, logging out and back in is an alternative.

4. **Add marinaMoji** in your system input settings:
   - **GNOME:** Settings → Keyboard → Input Sources → Add (+) → select **Japanese** → choose **marinaMoji** (or “marinaMoji (Japanese Input Method)”).
   - **Other (IBus):** Add input method and pick the engine named **marinaMoji** / **Japanese (marinaMoji)**.

If it still does not appear, confirm that `ibus-daemon` is running and that no errors show when running `ibus engine` or when opening the input method configuration.

**Display name:** marinaMoji uses its own config directory (`~/.config/marinamoji/`), so it appears as **Japanese (marinaMoji)** and does not share settings with stock Mozc. After reinstalling, run `ibus write-cache && ibus restart` and add the input source again; the new config will show "marinaMoji" in the list.

### Usage and known behavior

- **Toolbar:** When you focus a text field, the marinaMoji toolbar window shows the current schema (あ, ア, etc.), Shin/Kyū (traditional kanji) toggle, Odoriji button, and Half/Full button. If the window was empty before, rebuild and reinstall so the fix (showing all widgets) is applied.

- **Keyboard shortcuts** (from the keymap, e.g. MS-IME / ATOK style):
  - **Ctrl+Shift+1 / !** – Insert default odoriji
  - **Ctrl+Shift+2 / @** – Odoriji (iteration marks) palette
  - **Ctrl+Shift+3 / #** – Toggle traditional (kyūjitai) / modern (shinjitai) kanji
  - **Ctrl+Shift+4 / $** – Toggle Hiragana <-> Manyoshu
  - **Ctrl+Shift+5 / %** – Toggle Hiragana <-> Direct
  - **Ctrl+Shift+F / f** – Traditional-kanji toggle alias on some layouts  
  If these do nothing, your desktop or IBus may be capturing them; use the **IBus menu** (click the icon in the panel) or the **toolbar** instead.

- **Odoriji from the menu:** The "Odoriji (iteration marks)" menu item works when the engine is active (e.g. focus in a text field). If it only works after you start typing, click inside a text field first so the engine has focus, then open the menu and choose Odoriji.

- **Traditional kanji (Shin/Kyū):** Toggle via the toolbar button or the "Traditional kanji (Kyūjitai)" option in the IBus menu. The effect applies to the **next conversion**; reconvert or type again after toggling to see kyūjitai/shinjitai. Requires the build to have OpenCC enabled (install `libopencc-dev` or equivalent before building; see Packages above).

### Clean Bazel's build cache

💡 You may have some build errors when you update the build environment or
configurations. Try the following command to
[clean Bazel's build cache](https://bazel.build/docs/user-manual#clean).

```sh
bazelisk clean --expunge
```

### Troubleshooting: Linker error `relocation refers to a discarded section`

If you encounter linker errors related to `.sframe` sections (e.g., `relocation
refers to a discarded section` when using GCC 15+ and LLD 19+ in non-release
builds), you can append `--config no_sframe` to disable SFrame generation:

```sh
bazelisk build package --config no_sframe
```

### How to customize installation locations

This fork is branded **marinaMoji** and uses distinct paths so it can be
installed **next to** stock Mozc (e.g. for testing). The IBus component is
registered as **marinaMoji** and uses `marinamoji.xml` so it does not
overwrite `mozc.xml`.

Here is a table of contents in `mozc.zip` and their installation locations for
marinaMoji:

build target                     | installation location (marinaMoji)
-------------------------------- | ----------------------------------
`//server:mozc_server`           | `/usr/lib/marinamoji/mozc_server`
`//gui/tool:mozc_tool`           | `/usr/lib/marinamoji/mozc_tool`
`//renderer:mozc_renderer`       | `/usr/lib/marinamoji/mozc_renderer`
`//unix/ibus/ibus_mozc`          | `/usr/lib/ibus-marinamoji/ibus-engine-marinamoji`
`//unix/ibus:gen_mozc_xml`       | `/usr/share/ibus/component/marinamoji.xml`
`//unix:icons`                   | `/usr/share/ibus-marinamoji/...`
`//unix:icons`                   | `/usr/share/icons/marinamoji/...`
`//unix/emacs:mozc.el`           | `/usr/share/emacs/site-lisp/emacs-mozc/mozc.el`
`//unix/emacs:mozc_emacs_helper` | `/usr/bin/mozc_emacs_helper`

To customize installation locations, modify [`src/config.bzl`](../src/config.bzl).

💡 The following command makes the specified file untracked by Git.

```sh
git update-index --assume-unchanged src/config.bzl
```

💡 This command reverts the above change.

```sh
git update-index --no-assume-unchanged src/config.bzl
```

## Bazel command examples

### Bazel User Guide

*   [Build programs with Bazel](https://bazel.build/run/build)
*   [Commands and Options](https://bazel.build/docs/user-manual)
*   [Write bazelrc configuration files](https://bazel.build/run/bazelrc)

### Run all tests

```sh
bazelisk test ... --build_tests_only -c dbg
```

*   `...` means all targets under the current and subdirectories.

### Run tests under the specific directories

```sh
bazelisk test base/... composer/... --build_tests_only -c dbg
```

*   `<dir>/...` means all targets under the `<dir>/` directory.

### Run tests without the specific directories

```sh
bazelisk test ... --build_tests_only -c dbg -- -base/...
```

*   `--` means the end of the flags which start from `-`.
*   `-<dir>/...` means exclusion of all targets under the `dir`.

### Run the specific test

```sh
bazelisk test base:util_test -c dbg
```

*   `util_test` is defined in `base/BUILD.bazel`.

### Output logs to stderr

```
bazelisk test base:util_test --test_arg=--stderrthreshold=0 --test_output=all
```

*   The `--test_arg=--stderrthreshold=0 --test_output=all` flags show the
    output of unitests to stderr.

### Examples of environment-specific options

#### JDK options

```sh
bazelisk test ... --java_runtime_version=remotejdk_21
```

#### C/C++ compiler options

```sh
bazelisk test ... --repo_env=CC=gcc-14 --repo_env=CXX=g++-14
```

--------------------------------------------------------------------------------

## Build marinaMoji for Linux Desktop with GYP (deprecated):

⚠️ The GYP build is deprecated and no longer supported.

Please check the previous version for more information.
https://github.com/google/mozc/blob/2.29.5374.102/docs/build_mozc_in_docker.md#build-mozc-for-linux-desktop-with-gyp-maintenance-mode
