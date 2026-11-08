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

## Local proof (2026-11-08)

### macOS host (native)

1. `vendor_deps.sh` produced a **369M** `packaging/debian/vendor/` tree.
2. With `--vendor_dir`, `--repository_disable_download`, and
   `--lockfile_mode=error`, `//server:mozc_server` **built successfully**.
3. Full macOS `//:package` compiled almost everything, then failed in the
   final `unzip`/symlink staging step (local permissions) — not a
   vendor/network failure.

### Linux via Docker (`docker_linux_proof.sh`)

On Docker Desktop (ubuntu:24.04, linux/arm64), with the container’s
network disconnected after vendoring:

1. Vendored `oss_linux` `//:package` deps (~365–378M).
2. Confirmed BCR was unreachable (`curl` to `bcr.bazel.build` failed).
3. Offline builds succeeded for **`//server:mozc_server`** and
   **`//unix:package`** using `--repository_disable_download` and a
   fresh `--output_base`.

**Docker Desktop note:** keep the vendor tree on the *container-native*
filesystem (`/var/cache/marinamoji-vendor`). Vendoring directly onto the
macOS bind-mount breaks Bazel’s hardlinks (rules_python toolchain globs
then fail). The proof script handles this; see
`docker_linux_proof.sh`.

```bash
bash packaging/debian/offline/docker_linux_proof.sh
# reuse a previous vendor-linux/ copy inside the container:
MARINAMOJI_SKIP_VENDOR=1 bash packaging/debian/offline/docker_linux_proof.sh
```

## Orig tarball / DFSG

Finished layout: [`../DFSG.md`](../DFSG.md) and
`../build_source_package.sh` (`+ds1` main orig + `orig-vendor`
component). Optional later: shrink vendor via Debian-style
`--override_module` onto system `-dev` packages.

## Network dependency inventory

See [`network_deps.md`](network_deps.md) for the categories of fetches
declared in `src/MODULE.bazel`.
