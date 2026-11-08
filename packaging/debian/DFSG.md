# DFSG / orig-tarball layout (#52 / #53)

This is the finished engineering story for how marinaMoji becomes a
Debian *source* package that can build offline. Sponsorship / NEW (#61)
is separate process work.

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

From the repository root (Linux vendor tree already populated, e.g.
`packaging/debian/vendor-linux/`):

```bash
bash packaging/debian/build_source_package.sh
# artifacts under dist/debian-source/
```

See that script’s `--help` for options (`--skip-vendor`, `--source-only`).

## What NEW still needs (non-engineering / process)

* A Debian sponsor (#61)  
* Possibly a finer per-directory license pass over `vendor/` during
  review  
* Optional later: shrink vendor via `--override_module` onto Debian
  `-dev` packages (Debian mozc style)
