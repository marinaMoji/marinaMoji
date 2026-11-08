# Offline Bazel for Debian packaging (#53)

Debian `sbuild` has **no network**. marinaMoji’s Bazel build normally
downloads BCR modules and several `http_archive` / `http_file` deps from
`src/MODULE.bazel`. This directory is the maintainer workflow to make that
build work offline.

## Approach: Bazel vendor mode

Upstream Bazel supports [vendor mode](https://bazel.build/external/vendor):
copy every external dependency (and BCR registry files) into a local
directory, then build with `--vendor_dir=…` and no downloads.

That is lighter-weight for us than Debian mozc’s hand-maintained
`debian/third_party/*` stubs + `--override_module=…` list, and it matches
what Bazel 9 (pinned in `src/.bazeliskrc`) already provides. We can still
move to explicit overrides later if ftp-master prefers a smaller
`+dfsg` orig tarball.

```text
networked prep machine          air-gapped / sbuild
─────────────────────           ───────────────────
vendor_deps.sh  ──►  vendor/  ──►  build with --vendor_dir
                                   --repository_disable_download
```

`packaging/debian/vendor/` is **gitignored** (large). For an archive
upload it would ship inside the orig tarball or a separate component,
not as thousands of unreviewed commits on `master`.

## Scripts

| Script | Purpose |
| --- | --- |
| `vendor_deps.sh` | Populate `packaging/debian/vendor/` (needs network) |
| `verify_offline.sh` | Build `//:package` using only the vendor dir |

Both take optional Bazel config args after `--`. Defaults:

* macOS host: `--config release_build` (native `//mac:package`)
* Linux packaging: add `--config oss_linux` (needs Linux libs / typically a
  Linux host or container)

Examples from the **repository root**:

```bash
# 1) Fetch/vendor everything needed for this host's //:package
bash packaging/debian/offline/vendor_deps.sh

# 2) Prove the build does not download
bash packaging/debian/offline/verify_offline.sh
```

Linux archive shape (run on Linux, with Build-Depends installed):

```bash
bash packaging/debian/offline/vendor_deps.sh -- --config oss_linux --config release_build
bash packaging/debian/offline/verify_offline.sh -- --config oss_linux --config release_build
```

`debian/rules` picks up `debian/vendor` automatically when that directory
exists (after `ln -sfn packaging/debian debian`).

## Local proof (2026-11-08, macOS arm64)

On this machine:

1. `vendor_deps.sh` produced a **369M** `packaging/debian/vendor/` tree
   (mostly from an already-warm Bazel repository cache).
2. With `--vendor_dir`, `--repository_disable_download`, and
   `--lockfile_mode=error`, `//server:mozc_server` **built successfully**
   — no dependency downloads.
3. Full `//:package` (macOS installer zip) compiled almost everything,
   then failed in the final `unzip`/symlink staging step (`Permission
   denied` under `/var/folders/...`). That is a local packaging/sandbox
   issue, **not** a missing-network/vendor failure. Prefer
   `//server:mozc_server` as the offline smoke target on macOS.

Docker Desktop was not running here, so the Linux `oss_linux` `.deb` path
was not exercised. Start Docker (or use a Linux host) and re-run the
scripts with `-- --config oss_linux --config release_build`.

## What this does *not* solve yet

* **DFSG / `Files-Excluded`:** vendoring may pull non-DFSG bits; a
  `+dfsg` repack story still belongs with copyright work.
* **Size:** a full vendor tree is large (~hundreds of MB); shipping it in
  NEW needs a deliberate orig-tarball plan.
* **Linux `.deb` verification** on a Linux host/container (IBus/Qt
  Build-Depends).
* **Debian-style module overrides** to system `-dev` packages (smaller
  binary linkage) — optional later optimization.

## Network dependency inventory

See [`network_deps.md`](network_deps.md) for the categories of fetches
declared in `src/MODULE.bazel`.
