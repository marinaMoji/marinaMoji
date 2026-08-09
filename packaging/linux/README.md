# Linux packaging

## `.deb` packages

`build_deb.sh` turns the `mozc.zip` install tree from
`bazelisk build package` into `marinamoji_<version>_<arch>.deb`. The release
workflow runs it for amd64 and arm64.

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
gh-pages site). Tracked in the project TODO under Block 2: Deployment.

## Product version (About / update checks)

`src/marina_product_version.txt` holds the GitHub-facing version string
(e.g. `0.0.2-rc2`, no leading `v`). The release workflow overwrites it from
the tag before building so About and Win/mac update checks match the release.
The Mozc four-part engine version in `version.bzl` stays separate (IPC /
installer PE metadata).

## Apt repository

`build_apt_repo.sh` builds a signed `dists/` + `pool/` tree from one or more
`.deb` files. On each `v*` tag, `.github/workflows/release.yaml` collects every
`.deb` attached to GitHub Releases, rebuilds the repository, signs the indexes
with the CI GPG subkey, and deploys the tree to the `gh-pages` branch (GitHub
Pages).

The published suite is currently **`unstable`**. A separate **`stable`**
channel can be added later (same pool, second `dists/<suite>/` tree, or a
second Pages path).

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
private key into your GnuPG home to produce `InRelease` / `Release.gpg`. Pass
`--suite stable` when you introduce a stable channel.
