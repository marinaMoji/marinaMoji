# Security

## Reporting a vulnerability

Please report suspected vulnerabilities privately by email to
<daniel.morgan@college-de-france.fr>. Do not open a public issue for
security problems. You should normally receive a reply within a week.

## Release integrity

Every release asset can be verified in the following ways.

### macOS packages

The `.pkg` installers are signed with an Apple Developer ID certificate and
notarized by Apple. Gatekeeper verifies both automatically on install. To
check manually:

    pkgutil --check-signature marinaMoji-<version>-<arch>.pkg
    spctl --assess -vv --type install marinaMoji-<version>-<arch>.pkg

### Build provenance (all assets)

Every release asset, including `SHA256SUMS`, carries a GitHub build
provenance attestation binding it to the exact commit and workflow run that
produced it. Verify with the [GitHub CLI](https://cli.github.com/):

    gh attestation verify marinaMoji-<version>-<arch>.pkg --repo marinaMoji/marinaMoji

### Checksums

`SHA256SUMS`, attached to each release, contains SHA-256 digests of all
release assets:

    sha256sum --check --ignore-missing SHA256SUMS

### Windows

Windows builds are currently unsigned; SmartScreen will warn on install.
Signed MSIX packages distributed through the Microsoft Store are planned.

## Signing keys

The marinaMoji release-signing key is published in
[`marinaMoji-release-public-key.asc`](./marinaMoji-release-public-key.asc).

Primary key fingerprint (certification only; kept offline):

    5469 4B5D D8C8 DBEA 7DFE  F8F1 E40E 872A F7D7 CFEB

Subkeys:

- `35CC 9C65 3654 3F50 5825  2DDC FCDA 39EE 2E22 A192` — maintainer signing
  subkey, kept offline. Used for signatures made outside CI, such as
  security notices or key-rotation statements.
- `A1DF B08F D78B 8A8F 941E  AD5F E4A4 FB8B 462A 09F5` — CI signing subkey,
  held by GitHub Actions. Reserved for signing package-repository metadata
  (e.g. apt repository indexes); it does not sign release assets.

Release assets are authenticated by notarization and build provenance
attestations (above), not by detached GPG signatures. Subkey expiries are
extended in place; fetch the current key from this repository if a
signature reports an expired key.
