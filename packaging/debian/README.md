# Debian source packaging (archive readiness)

This directory is the start of a proper Debian *source* package for
marinaMoji ([#52](https://github.com/marinaMoji/marinaMoji/issues/52),
parent [#161](https://github.com/marinaMoji/marinaMoji/issues/161)).

It is **not** the path that ships today’s GitHub Releases `.deb` or the
third-party apt repo on GitHub Pages. Those stay in
[`../linux/`](../linux/) (`build_deb.sh`, `build_apt_repo.sh`). Think of
this tree as “what Debian/Ubuntu archive maintainers would eventually
build with `dpkg-buildpackage` / `sbuild`.”

## How this relates to the binary `.deb`

| | `packaging/linux/` (today) | `packaging/debian/` (this tree) |
| --- | --- | --- |
| Input | Pre-built `mozc.zip` from CI Bazel | Upstream source + Build-Depends |
| Tooling | `dpkg-deb` wrapper | debhelper (`dh`) |
| Network at build | Not needed (zip already built) | Currently **required** (Bazel fetches) |
| Users | apt repo / release assets | Future official archive / experimental |

Desktop metadata, maintainer scripts, and dependency rationale are shared
ideas: prefer reusing `../linux/data/` and
`../linux/install_desktop_metadata.sh` from `debian/rules` rather than
duplicating them.

## Layout

`dpkg-buildpackage` expects a `debian/` directory at the **root of the
source tree**. These files live under `packaging/debian/` so packaging
stays a separate concern from the IME core (same idea as
`packaging/arch/` and `packaging/linux/`).

From a checkout of the repository root:

```bash
ln -sfn packaging/debian debian
# optional binary-only try once Build-Depends and network are available:
# dpkg-buildpackage -us -uc -b
```

Remove the symlink when you are done (`rm debian`); do not commit a
`debian` symlink at the repo root.

## What works today vs what is blocked

**Scaffolded here (#52):** `control`, `rules`, `changelog`, `copyright`,
`watch`, `source/format`, maintainer scripts, and this README.

**Still required before an upload is realistic:**

1. **Offline / reproducible Bazel** ([#53](https://github.com/marinaMoji/marinaMoji/issues/53))
   — `sbuild` has no network. Debian’s experimental `mozc` packaging is
   already moving toward Bazel with vendored/`Files-Excluded` stubs; we
   will need a similar story (vendor deps, `+dfsg` orig tarball, and/or
   Debian Bazel helpers).
2. **Source-tree DEP-5 completeness** — `copyright` here is a starting
   inventory. Binary-package provenance already lives in
   `../linux/data/copyright` (#54). Official NEW review needs source
   paths plus a clear `Files-Excluded` repack story paired with #53.
3. **`sbuild` / `piuparts`** ([#55](https://github.com/marinaMoji/marinaMoji/issues/55)).
4. **Package split** ([#59](https://github.com/marinaMoji/marinaMoji/issues/59))
   — optional; the scaffold keeps a single `marinamoji` binary package
   (IBus-only), matching today’s fat `.deb`.
5. **Sponsor / NEW** ([#61](https://github.com/marinaMoji/marinaMoji/issues/61)).

Until #53 lands, treat `debian/rules` as a *networked* maintainer build
(similar to the Arch `marinamoji` PKGBUILD), not as archive-ready.

## Versioning

Product / GitHub tags keep SemVer prereleases (`0.0.21-rc11`). The Debian
package **Version** uses the tilde form (`0.0.21~rc11-1`) so the final
`0.0.21-1` sorts newer than its release candidates. See
`../linux/debian_version.sh`.
