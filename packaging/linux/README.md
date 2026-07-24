# Linux packaging

## `.deb` packages

`build_deb.sh` turns the `mozc.zip` install tree from
`bazelisk build package` into `marinamoji_<version>_<arch>.deb`. The release
workflow runs it for amd64 and arm64.

## Product version (About / update checks)

`src/marina_product_version.txt` holds the GitHub-facing version string
(e.g. `0.0.2-rc1`, no leading `v`). The release workflow overwrites it from
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
   branch** → Branch: `gh-pages` / `/ (root)`.
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
