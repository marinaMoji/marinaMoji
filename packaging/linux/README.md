# Linux packaging

## `.deb` packages

`build_deb.sh` turns the `mozc.zip` install tree from
`bazelisk build package` into `marinamoji_<version>_<arch>.deb`. The release
workflow runs it for amd64 and arm64.

GitHub tags and the About dialog keep SemVer prereleases (`0.0.19-rc3`). The
`.deb` **Version** uses Debian’s tilde form (`0.0.19~rc3`) via
`debian_version.sh`, so apt treats the final `0.0.19` as newer than its
release candidates. Plain `-rcN` in the package Version would sort *after* the
final and block upgrades for unstable-channel testers.

### Installing a local `.deb`

`dpkg -i` never fetches dependencies; it only unpacks and then fails
configuration if something is missing. Use apt, which resolves them:

```bash
sudo apt install ./marinamoji_*.deb
```

(If `dpkg -i` was already run and left the package unconfigured,
`sudo apt -f install` finishes the job.) Installing from the apt repository
below resolves dependencies the same way.

### Desktop metadata

`data/` holds the files that make the package presentable to GNOME Software /
Ubuntu App Center / KDE Discover and to application menus.
`install_desktop_metadata.sh --root <tree> --version <v>` stages them into an
install tree; `build_deb.sh` and both Arch PKGBUILDs
(`packaging/arch/marinamoji{,-bin}`) call it, so every Linux package presents
itself identically:

| Source | Installed as |
| --- | --- |
| `io.github.marinamoji.marinaMoji.desktop` | `/usr/share/applications/` (settings) |
| `io.github.marinamoji.marinaMoji.Dictionary.desktop` | `/usr/share/applications/` (dictionary tool) |
| `io.github.marinamoji.marinaMoji.metainfo.xml.in` | `/usr/share/metainfo/…metainfo.xml` (`@VERSION@` / `@DATE@` substituted) |
| `copyright` | `/usr/share/doc/marinamoji/copyright` (`.deb` only; Arch uses `/usr/share/licenses`) |
| `usr/share/icons/marinamoji/mozc.{png,svg}` from the zip | `/usr/share/icons/hicolor/{128x128,scalable}/apps/io.github.marinamoji.marinaMoji.{png,svg}` |

The AppStream metainfo is what supplies the publisher name, licence, summary,
description and release date in software centres; the hicolor copies are what
make the icon resolve (an `Icon=` name is looked up in icon *themes*, and
`/usr/share/icons/marinamoji` is not one). When the build host has
`desktop-file-validate` or `appstreamcli`, the script validates the files.

Keep `<release>` and the `.desktop` `Exec=` paths in step with any change to
the install layout in `src/unix/build_package.py`.

`marinamoji-bin` repacks the release zip, which contains only the install
tree, so it fetches `install_desktop_metadata.sh` and `data/*` from the release
tag over raw.githubusercontent. Those four extra `source=()` entries need real
`sha256sums` before an AUR upload (`makepkg -g`), like the rest.

The metainfo has no `<screenshots>` yet — software centres rank and display
entries much better with them, but they must be served from stable URLs (the
gh-pages site). Tracked as a GitHub issue under the
[`debian packaging`](https://github.com/marinaMoji/marinaMoji/issues?q=is%3Aissue+label%3A%22debian+packaging%22)
label (and on the Docs Wiki Deployment list).

### Maintainer scripts

`data/postinst` and `data/postrm` run `ibus write-cache` when `ibus` is
available (install/upgrade and remove/purge). They never call `ibus restart`
and never fail the package if IBus is missing or the cache refresh fails.
Users may still need `ibus restart` or a re-login before the engine appears
in the input-source list.

## Install smoke checklist (packaging)

Packaging-only checks after building or downloading a `.deb`. For IME
behaviour, use the project’s WP7 testing checklist.

1. Install with `sudo apt install ./marinamoji_*.deb` (or from the apt repo).
2. Confirm `/usr/share/ibus/component/marinamoji.xml` exists.
3. If marinaMoji is missing from input sources, run `ibus write-cache` and
   `ibus restart` (or re-login), then add **marinaMoji** in Settings.
4. Type in a text field and confirm conversion works.
5. Install a newer `.deb` over the old one (upgrade path).
6. `sudo apt remove marinamoji` — engine gone after cache refresh; user
   config under `~/.config/marinamoji/` should remain.
7. Reinstall, then `sudo apt purge marinamoji` — same as remove for this
   package (no purge-specific config deletion).

## Known gaps

This packaging path is a **binary `.deb`** repack of `mozc.zip`, plus a
signed third-party apt repo on GitHub Pages. It is **not** an official
Debian/Ubuntu archive source package.

- Build uses Bazel and needs network access for modules and some archives.
- Linux integration is **IBus only** (no Fcitx/Fcitx5 package).
- Maintainer scripts refresh the IBus component cache only.
- Fuller DFSG copyright, `debian/` source packaging, offline/`sbuild`
  builds, and related archive work live as GitHub issues labeled
  [`debian packaging`](https://github.com/marinaMoji/marinaMoji/issues?q=is%3Aissue+label%3A%22debian+packaging%22).

## Product version (About / update checks)

`src/marina_product_version.txt` holds the GitHub-facing version string
(e.g. `0.0.18-rc7`, no leading `v`). You do **not** need to bump this file
for a tagged release: `.github/workflows/release.yaml` overwrites it from the
`v*` tag on every macOS / Windows / Linux job before `bazel build`, then runs
`src/build_tools/verify_product_version.sh` so the generated
`product_version_def.h` (and `mozc_tool` on Linux/Windows) must contain that
same string. If the overwrite is skipped or the build still embeds a stale
value, the job fails instead of shipping a wrong About dialog.

Local / non-tag builds still use whatever is in the tree (useful for
day-to-day development). The Mozc four-part engine version in `version.bzl`
stays separate (IPC / installer PE metadata).

## Apt repository

`build_apt_repo.sh` builds a signed `dists/` + `pool/` tree from one or more
`.deb` files. On each `v*` tag, `.github/workflows/release.yaml` collects every
`.deb` attached to GitHub Releases, rebuilds the repository, signs the indexes
with the CI GPG subkey, and deploys the tree to the `gh-pages` branch (GitHub
Pages).

Two suites are published from one shared pool, each with its own signed
`Release` / `InRelease` / `Release.gpg`:

- **`stable`** — final releases only. A package whose Version ends in `-rcN` or
  `~rcN` is excluded.
- **`unstable`** — everything, including release candidates.

On each rebuild, `.deb` assets still tagged with SemVer `-rcN` in their
control Version are rewritten to `~rcN` in the published pool. If a final
`X.Y.Z` shares the pool with those historical hyphen-form RCs, it is
published as `X.Y.Z+1` so machines that already installed `X.Y.Z-rcN` can
`apt upgrade` onto the final (hyphen-rc sorts newer than a plain final). New
builds from `build_deb.sh` already emit `~rcN` and need no bump.

Consequences of that migration:

- A machine that installed a hyphen-form RC (`0.0.19-rc1`) will not be offered
  later tilde-form RCs through apt: `0.0.19~rc3` sorts *older* than the
  installed `0.0.19-rc1`. It moves forward only when the final is published.
- While any hyphen-form RC of `X.Y.Z` is still attached to a GitHub release,
  the final is always published as `X.Y.Z+1`. Someone who installed the plain
  `X.Y.Z` from apt before this change gets a no-op upgrade to the same
  contents.

Both are built in a single run (the `gh-pages` deploy uses `force_orphan`, so
they must be), and the build fails if a suite would be empty.

### One-time GitHub setup

1. **Pages:** Settings → Pages → Build and deployment → Source: **Deploy from a
   branch** → Branch: `gh-pages` / `/ (root)`. Without this, `publish_apt` can
   push a complete `gh-pages` tree while
   `https://marinamoji.github.io/marinaMoji` still returns **404** (“Site not
   found”). The release workflow now fails the apt job if Pages is not enabled.
2. **Repository secrets** (already named this way in the marinaMoji repo):
   - `APT_GPG_PRIVATE_KEY` — ASCII-armored private key material for the
     **CI signing subkey**
     (`A1DF B08F D78B 8A8F 941E  AD5F E4A4 FB8B 462A 09F5`). Export only that
     subkey (plus the stub of the primary) for Actions; keep the primary and
     maintainer subkey offline. See [SECURITY.md](../../SECURITY.md).
     Base64-wrapped key material is also accepted.
   - `APT_GPG_PASSPHRASE` — passphrase for that private key (omit or leave
     empty if the key is unprotected).

The public key already lives in-repo as
[`marinaMoji-release-public-key.asc`](../../marinaMoji-release-public-key.asc)
and is copied into the published apt root.

### Local dry-run (unsigned)

```bash
bash packaging/linux/build_apt_repo.sh \
  --deb-dir /path/to/debs \
  --output-dir /tmp/marinamoji-apt \
  --public-key marinaMoji-release-public-key.asc
```

Add `--gpg-key-id A1DFB08FD78B8A8F941EAD5FE4A4FB8B462A09F5` after importing the
private key into your GnuPG home to produce `InRelease` / `Release.gpg`. Repeat
`--suite` to choose suites (default: `unstable` and `stable`).
