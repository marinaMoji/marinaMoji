# DFSG / orig-tarball layout (#52 / #53)

How marinaMoji becomes a Debian *source* package that builds offline.
This works for maintainer builds (Docker QA, local `dpkg-buildpackage`)
but is **not yet archive-ready**: see "Known archive blockers" below.
Sponsorship / NEW (#61) is separate process work.

## Version naming

| Meaning | Example |
| --- | --- |
| Product / GitHub tag | `0.0.21-rc11` / `v0.0.21-rc11` |
| Debian upstream (tilde RC) | `0.0.21~rc11` |
| Repacked upstream (`Files-Excluded`) | `0.0.21~rc11+ds1` |
| Binary package | `0.0.21~rc11+ds1-1` |

`+ds1` means “Debian source repack 1” (unused Windows trees removed),
following Debian mozc — not a claim that every vendored file has been
line-audited for NEW yet.

## Two upstream components

`3.0 (quilt)` additional tarball:

| File | Unpacks as | Contents |
| --- | --- | --- |
| `marinamoji_${V}+ds1.orig.tar.xz` | source root | Upstream git tree after `Files-Excluded` |
| `marinamoji_${V}+ds1.orig-vendor.tar.xz` | `vendor/` | Linux Bazel vendor tree (`vendor_deps.sh --config oss_linux`) |

`debian/rules` looks for a vendor directory in this order:

1. top-level `vendor/` (from the additional orig tarball)  
2. `debian/vendor`  
3. `debian/vendor-linux` (Docker/maintainer convenience)

and passes `--vendor_dir` + `--repository_disable_download` when one
exists.

## Files-Excluded (main orig)

Recorded in `debian/copyright`:

* `src/win32/` and `src/renderer/win32/` — Windows IME / TIP  
* Windows `.rc` resource scripts listed there  

Not excluded (kept in orig even if unused on Linux): macOS sources,
docs, Arch packaging — reviewers can still read them; they are DFSG
Free and small compared to the vendor tree.

## Building the tarballs

From the repository root, with a Linux vendor tree already populated
(e.g. `packaging/debian/vendor-linux/`) and your changes **committed**:

```bash
bash packaging/debian/build_source_package.sh
# artifacts under dist/debian-source/
```

The main orig is made with `git archive`, so only tracked files can end
up in it (no caches or local credentials), and the paths removed are read
from `Files-Excluded` in `debian/copyright` — that header is the single
source of truth, and it is also what `uscan` applies when repacking.
Options: `--skip-vendor`, `--vendor-src DIR`, `--output-dir DIR`,
`--ref GIT_REF` (e.g. a tag) and `--with-dpkg-source` (also produces the
`.dsc` and debian tarball). `qa/docker_qa.sh` runs the whole chain:
build tarballs, `dpkg-source -x`, `dpkg-buildpackage`, install smoke.

## Known archive blockers

These are real; the offline *mechanism* is proven, the archive *policy*
is not met yet.

1. **Pre-built binaries in `orig-vendor`.** Vendor mode snapshots
   downloaded toolchains: rules_python's standalone Python interpreter,
   a pre-built `protoc`, Go/JDK-related toolchains, Kotlin worker jars,
   jq/yq/zstd/coreutils toolchains, and so on. The archive requires
   building from source with packaged tools. Fix direction: point Bazel
   at Debian's `python3`, `protobuf-compiler`, etc. (`--override_module`,
   toolchain flags; this is what Debian's mozc packaging does) and drop
   those repositories from the vendor tree.
2. **Bazel itself.** Bazel is not in Debian main and Bazelisk downloads
   a Bazel binary at run time. A network-less `sbuild` has neither.
   Needs a packaged Bazel (or a vendored, source-built one) as a
   Build-Depends.
3. **The vendor tree is architecture-specific.** The current tree
   contains `linux_aarch_64` protoc and an aarch64 Python; it will not
   build on amd64. Until item 1 removes the per-arch toolchains, one
   `orig-vendor` per architecture would be needed, which is not viable
   for a single source package. `control` lists `amd64 arm64` but only
   the arch the vendor tree was made on has been built.
4. **Licence inventory.** `debian/copyright` records `vendor/*` as a
   mix of licences, not a per-file audit. NEW will want a real one,
   which gets much smaller once item 1 is done.
5. **Hardening flags.** `DEB_BUILD_MAINT_OPTIONS` does not reach Bazel,
   so dpkg-buildflags (hardening, reproducible prefix maps) are not
   applied to the build. Pass them through `--copt`/`--linkopt` once the
   toolchain story is settled; expect lintian hardening warnings until
   then.

## What NEW still needs (non-engineering / process)

* A Debian sponsor (#61)  
* Possibly a finer per-directory license pass over `vendor/` during
  review  
* Optional later: shrink vendor via `--override_module` onto Debian
  `-dev` packages (Debian mozc style)
