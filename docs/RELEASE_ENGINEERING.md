# Release engineering — marinaMoji & Le Jean-Baptiste

Status as of 2026-07-17. Covers both projects; lives here because the
release policy and signing key are shared.

## Policy (decided)

- **macOS**: all release packages are Developer ID-signed, notarized, and
  stapled in CI. Local/debug builds are never notarized (marinaMoji uses
  the ad-hoc pseudo identity by default; LJB has `notarize: false` in
  `electron-builder.mac.json`, re-enabled in CI via `package:mac:release`).
- **Windows**: unsigned for now; users see a SmartScreen warning. The plan
  is MSIX through the Microsoft Store, which signs at ingestion. Note MSIX
  is its own format (electron-builder target `appx`) — WiX `.msi` is *not*
  a stepping stone to it; LJB stays on NSIS `.exe` until then.
- **Linux**: the apt repository's metadata is signed by the CI subkey; deb
  and flatpak files themselves are covered by checksums + attestations.
- **Provenance**: every release asset (including `SHA256SUMS`) gets a
  GitHub build provenance attestation
  (`gh attestation verify <file> --repo <owner>/<repo>`). There is **no
  manual GPG signing in the release loop** (decided 2026-07-17; the
  platform mechanisms above carry the security load).
- **Releases publish directly** (no draft gate); tag push `v*` does
  everything.

### Key structure (one shared key for both projects)

| Component | Fingerprint | Expiry | Custody / role |
|---|---|---|---|
| Primary [C] | `5469…CFEB` | 2029-07-15 | Offline. Certifies subkeys only. |
| Signing sub | `35CC…A192` | 2028-07-15 | Offline. Out-of-band signatures (security notices, key rotation). |
| CI sub | `A1DF…09F5` | **2027-07-16** | GitHub Actions (`APT_GPG_PRIVATE_KEY`). Signs apt metadata only. |

Secret export is subkey-only: `gpg --armor --export-secret-subkeys 'A1DF…09F5!'`
(quoted — zsh eats the `!`). Public key: `marinaMoji-release-public-key.asc`
in the marinaMozc repo root; publication to keys.openpgp.org still pending.

⚠ **CI subkey expires 2027-07-16.** An expired apt key breaks `apt update`
for every installed user until they re-fetch the key. Set a reminder for
~June 2027 to extend the expiry, republish the .asc, and refresh the CI
secret — or remove the expiry and rely on revocation.

## What is in place (all uncommitted in the working trees)

### marinaMoji (`marinaMoji/marinaMoji`, branch `feature/windows`)

- `.github/workflows/release.yaml` — on `v*` tag:
  - macOS arm64 + intel64 pkgs: temp keychain from the `release`
    environment secrets, identities patched into `config.bzl`, built with
    `--define CODESIGN=release --define CHANNEL=stable`, notarized
    (`notarytool`), stapled, verified (`pkgutil`/`stapler`/`spctl`).
  - Windows x64 (windows-2025, `--config x86_simd`) + arm64
    (windows-11-arm) MSIs, unsigned, no environment needed.
  - Linux x86_64 + arm64 (native runners): binary zip + deb via
    `packaging/linux/build_deb.sh` (deps computed with `dpkg-shlibdeps`).
  - Publish job: `SHA256SUMS` over everything, attestations, release.
- Signing fixes required for notarization:
  `src/mac/fix_qt_bundled_paths.sh` (hardened runtime + timestamp, no
  swallowed errors) and `src/mac/tweak_installer_files.py` (outer bundles
  re-signed for Qt builds; keychain via search list, not `login.keychain`).
- Universal builds removed everywhere (CI + release); per-arch only.
- Packaging: `packaging/linux/build_deb.sh`;
  `packaging/arch/marinamoji/PKGBUILD` (source, Bazel) and
  `packaging/arch/marinamoji-bin/PKGBUILD` (repacks release zips).
- `SECURITY.md` rewritten (verification per channel, key table);
  `README.md` gained an Installation section.

### Le Jean-Baptiste (`lejeanbaptiste/lejeanbaptiste`)

- `.github/workflows/release.yml` — deb (amd64/arm64), flatpak (x86_64,
  kept), and new macOS job: arm64 on `macos-latest`, x64 on
  `macos-15-intel` (native per arch because the bundled Python/LemMinX
  runtimes follow `process.arch`), electron-builder signing via
  `CSC_LINK`/`CSC_INSTALLER_LINK`, app notarized by electron-builder, the
  .pkg additionally notarized + stapled explicitly. Publish job:
  checksums, attestations, direct release, then calls the apt deploy.
- `.github/workflows/publish-apt.yml` — reusable (`workflow_call` from
  release.yml — required because `GITHUB_TOKEN`-created releases don't
  fire `release: published`), plus `release`/`workflow_dispatch` triggers.
  Downloads the release .debs, rebuilds and deploys the signed apt repo to
  Pages, attaches the archive key to the release.
- Local scripts: `package:mac:release` / `build:desktop:mac:release`
  (CI-only notarization); local `package:mac` no longer notarizes.
- `SECURITY.md` added; readme Install section corrected
  (latest-release link, `signed-by` keyring apt instructions — the old
  `apt-key add` is removed from modern distros — correct Pages URL
  `https://lejeanbaptiste.github.io/lejeanbaptiste/apt`).
- Repo was transferred: canonical slug is `lejeanbaptiste/lejeanbaptiste`
  (Pages is default project Pages, confirmed via API). Local remote still
  points at `potatosinology/…` and should be updated.

## Before the first tagged release (checklist)

- [ ] Commit the working-tree changes in both repos (marinaMozc's are on
      `feature/windows` — they need to reach `master` for tag builds).
- [ ] Both repos: `release` environment with the 8 Apple secrets
      (incl. `APPLE_API_KEY_ID`), deployment rule "Selected branches and
      tags" → Tag → `v*`.
- [ ] LJB: re-add `APT_GPG_PRIVATE_KEY` (subkey-only export, see above)
      and `APT_GPG_PASSPHRASE` (repo-level is fine — the apt job runs
      under the `github-pages` environment, not `release`).
- [ ] Publish the GPG public key to keys.openpgp.org.
- [ ] `git remote set-url github git@github.com:lejeanbaptiste/lejeanbaptiste.git`
      in leaf-writer; `gh auth refresh` (current token is invalid).
- [ ] **Dry-run marinaMoji** via `workflow_dispatch` (needs the workflow on
      the default branch first): confirm notarization passes end-to-end,
      install the pkg on a clean Mac (Gatekeeper, IME registration),
      install the deb in a container
      (`docker run --rm -it ubuntu:24.04`, `apt install ./marinamoji_*.deb`,
      check deps resolve and files land), run the MSI on a Windows VM.
- [ ] **Dry-run LJB** the same way; specifically watch that
      `sign-mac-extra-resources.mjs` finds the identity from
      electron-builder's temp keychain, and that the stapled pkg passes
      `spctl` on both arches.
- [ ] After the first LJB release: verify the Pages apt repo is live and
      walk through the readme's apt instructions on a clean container.
- [ ] Calendar reminder: CI subkey expiry, June 2027.

## Next steps (roughly by value)

1. **LJB mac auto-updates** — merge the two per-arch `latest-mac.yml`
   files in the publish job, upload them, and wire `electron-updater` in
   the main process. Everything else (signed, notarized zips on GitHub
   releases) is already in place. Works fully outside the App Store.
2. **Flathub submission** for LJB (bundle assets stay as a fallback);
   Flathub is where Linux desktop users get auto-updates.
3. **Homebrew cask** for marinaMoji (and optionally LJB): `brew upgrade`
   solves mac updates for an IME without embedding Sparkle. Start with a
   personal tap; homebrew/cask once there's some notability.
4. **AUR publication** when account registration reopens — both
   `marinamoji` (source) and `marinamoji-bin`. Consider a CI step that
   bumps pkgver/checksums on release.
5. **Store/MSIX**: electron-builder `appx` target for LJB; MSIX packaging
   for marinaMoji; both blocked on Microsoft certification.
6. **marinaMoji apt repo**: reuse LJB's `build-apt-repo.mjs` + Pages
   pattern. Decide whether both projects share one repo/site or each gets
   its own (the CI subkey is shared either way).
7. **RPM**: Fedora COPR (no key custody, their build service) or openSUSE
   Build Service (deb + rpm for many distros from one spec).
8. **Emacs file conflict**: the deb declares
   `Conflicts: emacs-mozc, emacs-mozc-bin` because `mozc.el` and
   `mozc_emacs_helper` install at Debian's paths. Moving them to
   marinamoji-specific paths in `config.bzl` would let the packages truly
   coexist with stock Mozc, matching the project's stated goal.
9. **In-app update check** for marinaMoji's config dialog (GitHub releases
   API → open download page); Sparkle only if users demand in-place
   updates.

## Known risks / currently untested

- Nothing in either release workflow has run yet: first notarization
  round-trips, the electron-builder keychain interplay with the afterPack
  hook, and Windows arm64 runner availability are all unproven.
- The deb's `Depends` line comes from `dpkg-shlibdeps` and should be
  correct, but install-testing in a clean container is the real check.
- Both PKGBUILDs are untested (`makepkg -si` on a real Arch box before
  AUR); the source one needs network during `build()` for Bazel modules.
- `gh release create` uploads assets after creating the release, so there
  is a brief window where the release exists incomplete; consumers of
  `releases/latest` may race it. If this ever matters (e.g. for
  electron-updater), switch back to create-as-draft + publish-when-ready
  as a purely mechanical change.
