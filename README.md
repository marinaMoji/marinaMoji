<img src="src/unix/ibus/toolbar_icons/logo_long_light.svg" title="" alt="" width="303">

A Japanese IME to turn back time

[Marina Pandolfino](https://www.crcao.fr/membre/marina-pandolfino/) (EPHE) | [Daniel Patrick Morgan](https://www.crcao.fr/membre/daniel-patrick-morgan/) (CNRS)

marinaMoji is a fork of [Mozc](https://github.com/google/mozc), Copyright 2010-2026 Google LLC, fine tuned for scholars of ancient and pre-modern Japanese.

===================================

Build Status
------------

(coming)

## Features

marinaMoji provides the following features for scholarly Japanese text input:

1. **Kyūjitai/Shinjitai conversion:** Automatic conversion between modern and traditional characters via OpenCC (! currently improving conversion tables !).
2. **Historical kana input:** Direct input of historical kana forms (ゐ, ゑ, and historical distinctions)
3. **Full katakana mode:** convert katakana into kanji, as in hiragana mode; quickly switch between the two with `shift_R`.
4. **Historical marks palette:** Set default repetition mark (々, 〻, 〱, ゝ, ヽ, ヾ, ゞ, ヶ, etc.) via input palette (`ctrl+shift+2`) and insert via `ctrl+shift+1`. 
5. **Kaeriten input:** directly type ㆑㆒㆓, etc., via `;r`, `;1`, `;2`, etc., to produce superscript unicode kaeriten. These can be retained or systematically replaced with the desired code in XML, LaTeX, etc. Our [plugin](https://github.com/marinaMoji/plugin) for LibreOffice and OnlyOffice handles page-setting for your word processor (testing).  
6. **Floating toolbar** - Visual mode indicator showing current input mode, shin/kyu,  with quick access to historical marks
7. **Macron vowels** - Input of macron vowels (ā, ē, ī, ō, ū) for scholarly transliteration in ASCII mode
8. **Quick dictionary injection:** type `ctrl+shift+0` in compose mode to immediately save kanji phrase and pronunciation to user dictionary.
9. **Encrypted cross-device sync:** sync your user dictionary and learning history via one encrypted file in a folder you choose (Nextcloud, Syncthing, iCloud Drive, etc.). Opt-in; see [How sync works](docs/HOW_SYNC_WORKS.md).

## Synchronisation

marinaMoji stores sync configuration in a local sidecar file (`sync.conf`), not in the main IME database. You pick a path for a single encrypted bundle (e.g. `marinamoji_sync.mmz.enc`) inside a cloud-synced folder and share a **sync key** between devices.

- **User guide:** [docs/HOW_SYNC_WORKS.md](docs/HOW_SYNC_WORKS.md) — enable sync, generate/copy key, Sync now, auto-sync intervals.
- **Developer reference:** [docs/SYNC_PLAN.md](docs/SYNC_PLAN.md) — bundle format, merge rules, IPC commands.
- **Manual QA checklist:** [docs/SYNC_MANUAL_QA.md](docs/SYNC_MANUAL_QA.md) — two-device verification steps.

Sync uses [libsodium](https://github.com/jedisct1/libsodium) and [miniz](https://github.com/richgel999/miniz); see Acknowledgements in the About dialog.

## Planned features

1. Toggle historical kana orthography
2. Integrate Jim Breen dictionaries
3. Build additional dictionary modules
4. Character composer

For policies on vocabulary and conversion results, see
[Vocabulary Policy](VOCABULARY_POLICY.md).

Build Instructions
------------------

**Recommended (marinaMoji day-to-day):**

* [Compiling marinaMoji on Ubuntu](docs/compiling_instructions_for_marina.md) — Linux: packages, build, install, uninstall, sync, IBus
* [macOS port plan](docs/MACOS_PORT_PLAN.md) — macOS: `MOZC_QT_PATH`, rebuild, `install_marinamoji.sh`, `.pkg`, troubleshooting

**Reference (Bazel details, other platforms):**

* [How to build marinaMoji for Linux](docs/build_marinamoji_for_linux.md) — test commands, install paths, Bazel options
* [How to build marinaMoji on macOS](docs/build_marinamoji_on_macos.md) — Xcode/Qt setup, cross-arch builds
* [How to build marinaMoji for Android](docs/build_marinamoji_for_android.md) — upstream Android library (not a marinaMoji product target)
* [How to build marinaMoji on Windows](docs/build_marinamoji_on_windows.md) — upstream Windows/Mozc (Windows port planned)
* [How to build marinaMoji in Docker](docs/build_marinamoji_in_docker.md) — deprecated Docker/GYP notes
* [Install branding and paths](docs/MARINAMOJI.md)

License
-------

All Mozc code written by Google is released under
[The BSD 3-Clause License](http://opensource.org/licenses/BSD-3-Clause).
For third party code under [src/third_party](src/third_party) directory,
see each sub directory to find the copyright notice.  Note also that
outside [src/third_party](src/third_party) following directories contain
third party code.

### [src/data/dictionary_oss/](src/data/dictionary_oss)

Mixed.
See [src/data/dictionary_oss/README.txt](src/data/dictionary_oss/README.txt)

### [src/data/test/dictionary/](src/data/test/dictionary)

The same as [src/data/dictionary_oss/](src/data/dictionary_oss).
See [src/data/dictionary_oss/README.txt](src/data/dictionary_oss/README.txt)

### [src/data/test/stress_test/](src/data/test/stress_test)

Public Domain.  See the comment in
[src/data/test/stress_test/sentences.txt](src/data/test/stress_test/sentences.txt)

## Install in CachyOS

Install dependencies

```
sudo pacman -S --needed \
  ibus glib2 base-devel \
  qt6-base \
  opencc \
  gtk3 \
  zip unzip jdk-openjdk
```

install bazelisk

```
# AUR (yay/paru)
yay -S bazelisk
# or
paru -S bazelisk
```

Build

```
git clone https://github.com/marinaMoji/marinaMoji.git --recursive
cd marinaMoji/src

bazelisk build package --config oss_linux --config release_build
```

Install

```
sudo unzip -o bazel-bin/unix/mozc.zip -d /
```

- **JAVA_HOME:** If you see "Could not find system javabase" or "must point to a JDK, not a JRE", install a JDK (`jdk-openjdk`), then e.g. `export JAVA_HOME=/usr/lib/jvm/java-25-openjdk` (or `default`) before building.
- **rules_swift aspect error:** If you see "required_aspect_providers, got element of type NoneType", ensure you use **bazelisk** (not system `bazel`) from the `src/` directory so the correct Bazel and dependency versions are used. This repo pins `rules_swift` to 2.5.0 and dependency overrides to avoid that failure on Linux.

Reload ibus and add marinaMoji

```
ibus write-cache
ibus restart
```