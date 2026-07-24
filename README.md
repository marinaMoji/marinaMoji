<img src="src/unix/ibus/toolbar_icons/logo_long_light.svg" title="" alt="" width="303">

A Japanese IME for historical and pre-modern Japanese text.

[Marina Pandolfino](https://www.crcao.fr/membre/marina-pandolfino/) (EPHE) | [Daniel Patrick Morgan](https://www.crcao.fr/membre/daniel-patrick-morgan/) (CNRS)

marinaMoji is a fork of [Mozc](https://github.com/google/mozc), tuned for scholarly workflows with kyūjitai/shinjitai conversion, historical kana, kaeriten, macron vowels, and encrypted cross-device sync.

===================================

## CI

[![Linux CI](https://github.com/marinaMoji/marinaMoji/actions/workflows/linux.yaml/badge.svg)](https://github.com/marinaMoji/marinaMoji/actions/workflows/linux.yaml)
[![macOS CI](https://github.com/marinaMoji/marinaMoji/actions/workflows/macos.yaml/badge.svg)](https://github.com/marinaMoji/marinaMoji/actions/workflows/macos.yaml)
[![Windows CI](https://github.com/marinaMoji/marinaMoji/actions/workflows/windows.yaml/badge.svg)](https://github.com/marinaMoji/marinaMoji/actions/workflows/windows.yaml)

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

## Support

- Bugs and reproducible issues: [Issues](https://github.com/marinaMoji/marinaMoji/issues)
- Questions, ideas, and discussion: [Discussions](https://github.com/marinaMoji/marinaMoji/discussions)
- Vocabulary policy: [VOCABULARY_POLICY.md](VOCABULARY_POLICY.md)

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

## Installation

Prebuilt installers are attached to each
[release](https://github.com/marinaMoji/marinaMoji/releases/latest). All
release assets can be verified as described in [SECURITY.md](SECURITY.md).

### macOS

Download the `.pkg` for your machine — `arm64` (Apple silicon), `intel64`,
or `universal` (both, larger download) — and run it. The packages are
signed and notarized, so Gatekeeper accepts them without warnings. Log out
and back in, then add marinaMoji in
System Settings → Keyboard → Input Sources.

In **Preferences → Misc**, marinaMoji can check GitHub Releases for you:

- **Automatically check for updates (once a day)** is on by default. When you
  switch to marinaMoji (or open Preferences), it looks for a newer notarized
  `.pkg`, then offers **Download & Install…** (opens the macOS Installer; you
  still approve with your password).
- **Check for updates…** runs the same check immediately.
- By default only final (non-rc) releases are offered; enable **Include
  unstable (rc) releases…** to consider `-rc` / prerelease tags.

### Windows

Download the `.msi` for your machine — `x64` or `arm64` — and run it. The
installer is not yet signed, so SmartScreen will warn — choose
"More info" → "Run anyway". Signed packages through the Microsoft Store
are planned.

In **Settings → Misc**, use **Check for updates…** the same way as on macOS
(stable-only by default; optional unstable/rc channel).

### Linux

**Register the apt repository and install (Debian/Ubuntu, amd64 or arm64)**

This is the **unstable** channel (tracks release builds, including rc tags).
A separate stable channel will be added later. Indexes are GPG-signed; see
[SECURITY.md](SECURITY.md).

```bash
# 1. Install the archive signing key
curl -fsSL https://marinamoji.github.io/marinaMoji/marinaMoji-release-public-key.asc \
  | sudo gpg --dearmor -o /usr/share/keyrings/marinamoji-archive-keyring.gpg

# 2. Register the repository
echo "deb [signed-by=/usr/share/keyrings/marinamoji-archive-keyring.gpg] https://marinamoji.github.io/marinaMoji unstable main" \
  | sudo tee /etc/apt/sources.list.d/marinamoji.list

# 3. Install
sudo apt update
sudo apt install marinamoji
ibus restart   # or log out and back in
```

Then add "marinaMoji" in your desktop's input-source settings (or with
`ibus-setup`). Packaging details:
[packaging/linux/README.md](packaging/linux/README.md).

**Manual `.deb`:** each
[release](https://github.com/marinaMoji/marinaMoji/releases/latest) also
attaches `marinamoji_<version>_<arch>.deb` if you prefer not to add the
repository:

```bash
sudo apt install ./marinamoji_<version>_amd64.deb
ibus restart
```

**Arch Linux (x86_64, aarch64):** PKGBUILDs are provided in
[packaging/arch](packaging/arch) — `marinamoji` builds from source,
`marinamoji-bin` repacks the release binaries. Build and install with
`makepkg -si` after updating `pkgver` and checksums for the release.
(AUR publication is planned.)

**Other distributions:** releases also include plain binary archives
(`marinaMoji-<version>-linux-<arch>.zip`) that mirror the install tree
(`usr/...`); see the build guides below for manual installation.

## Build and Install

For release builds and installation steps, use the platform guides:

- Linux day-to-day build and install: [Compiling marinaMoji on Ubuntu](docs/compiling_instructions_for_marina.md)
- Linux Bazel reference: [How to build marinaMoji for Linux](docs/build_marinamoji_for_linux.md)
- macOS build and porting: [macOS port plan](docs/MACOS_PORT_PLAN.md)
- macOS Bazel reference: [How to build marinaMoji on macOS](docs/build_marinamoji_on_macos.md)
- Android reference build: [How to build marinaMoji for Android](docs/build_marinamoji_for_android.md)
- Windows reference build: [How to build marinaMoji on Windows](docs/build_marinamoji_on_windows.md)
- Docker and legacy notes: [How to build marinaMoji in Docker](docs/build_marinamoji_in_docker.md)
- Branding and install paths: [Install branding and paths](docs/MARINAMOJI.md)

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

## Notes

- Release verification instructions are available in [SECURITY.md](SECURITY.md).
