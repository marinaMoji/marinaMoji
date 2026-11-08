# Debian QA: sbuild + piuparts (#55)

After the source scaffold (#52) and offline Bazel vendor mode (#53), the
next quality gate is building in a clean environment and checking
install / upgrade / remove.

## What “good” looks like

| Gate | Tool | Purpose |
| --- | --- | --- |
| Clean build | `sbuild` (or `dpkg-buildpackage` in a fresh chroot) | No leftover host state; no network if `debian/vendor/` is present |
| Install smoke | `piuparts` (or a scripted apt cycle) | Install, upgrade, remove, purge without leaving a broken system |

Linux CI today only builds the **binary** `.deb` from a pre-built zip and
runs `lintian`. This directory is the **source-package** maintainer path.

## Maintainer path A — Docker smoke (macOS / laptop)

Docker Desktop cannot run full `sbuild`/`schroot` comfortably. Use the
stand-in script, which:

1. Starts Ubuntu 24.04 with Build-Depends + Bazelisk  
2. Copies the tree to a container-native build dir (writable parent for
   `dpkg-buildpackage` artifacts)  
3. Places a Linux vendor tree at `debian/vendor/` (from
   `packaging/debian/vendor-linux/` or by running vendor inside)  
4. Runs `dpkg-buildpackage -us -uc -b`  
5. Runs an apt install → remove → install → purge smoke (piuparts-shaped)

```bash
# from repo root; Docker Desktop running
# optional: reuse an existing vendor-linux tree
MARINAMOJI_SKIP_VENDOR=1 bash packaging/debian/qa/docker_qa.sh
```

First run without `MARINAMOJI_SKIP_VENDOR` will vendor `oss_linux` deps
(networked). Expect a long build (Bazel compile inside `debian/rules`).

## Maintainer path B — real sbuild + piuparts (Debian/Ubuntu host)

On a machine with [`sbuild`](https://wiki.debian.org/sbuild) already set
up (chroot for your target suite):

```bash
cd /path/to/marinaMoji
ln -sfn packaging/debian debian

# Populate debian/vendor for offline builds (Linux host):
bash packaging/debian/offline/vendor_deps.sh -- \
  --config oss_linux --config release_build
# vendor_deps writes packaging/debian/vendor/ by default; if you used
# vendor-linux for Docker, copy/symlink it to packaging/debian/vendor.

# Build in the chroot (no network once vendor is present):
sbuild --no-run-lintian -d unstable   # or: -d noble

# Install/upgrade/remove torture test:
sudo piuparts ../marinamoji_*.deb
```

Install Bazelisk on the host (or in the chroot tarball) so
`debian/rules` finds it; it is not yet a satisfiable archive
Build-Depends on every suite.

## CI note

Wiring these as required GitHub Actions checks is **out of scope** until
the Docker/maintainer path is green and cheap enough. This issue’s
acceptance is a **documented command path**; automation can follow.
