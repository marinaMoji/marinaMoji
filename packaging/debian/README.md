# Debian source packaging (archive readiness)

This directory is the Debian *source* package for marinaMoji
([#161](https://github.com/marinaMoji/marinaMoji/issues/161)).

It is **not** the path that ships today’s GitHub Releases `.deb` or the
third-party apt repo — those stay in [`../linux/`](../linux/).

## Status (engineering)

| Item | Issue | Status |
| --- | --- | --- |
| `debian/` scaffold | #52 | Done |
| Offline Bazel (vendor mode) | #53 | Done (macOS + Linux/Docker proof) |
| Maintainer QA (`dpkg-buildpackage` / sbuild docs + Docker smoke) | #55 | Done |
| DFSG `+ds1` orig + `orig-vendor` component | #52/#53 | Done — see [DFSG.md](DFSG.md) |
| Package split | #59 | Optional later |
| AppStream screenshots | #58 | Optional later |
| Sponsor / NEW | #61 | Process — later |

## How to use

```bash
# 1) Linux vendor tree (networked, once per version)
bash packaging/debian/offline/vendor_deps.sh -- \
  --config oss_linux --config release_build

# 2) Orig tarballs (+ds1 source + vendor component)
bash packaging/debian/build_source_package.sh

# 3) Binary build smoke in Docker (optional)
MARINAMOJI_SKIP_VENDOR=1 bash packaging/debian/qa/docker_qa.sh
```

`dpkg-buildpackage` expects `debian/` at the source root:  
`ln -sfn packaging/debian debian` from a git checkout, or unpack the
source package produced above.

## Related

* [DFSG.md](DFSG.md) — orig layout and Files-Excluded  
* [offline/](offline/) — vendor mode + Docker offline proof  
* [qa/](qa/) — sbuild / piuparts maintainer paths  
* [../linux/](../linux/) — third-party apt `.deb` (unchanged)
