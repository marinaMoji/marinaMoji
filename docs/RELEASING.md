# Releasing marinaMoji

Checklist for a release. Steps marked **manual** are not automated.

1. **Tag.** Push a `v*` tag. `.github/workflows/release.yaml` builds the macOS
   `.pkg` files (signed and notarized), the Windows MSIs (unsigned), and the
   Linux `.deb`/`.zip` assets, publishes the GitHub Release with `SHA256SUMS`,
   and rebuilds the signed apt repository.
2. **Sign the Windows MSIs (manual).** On the maintainer machine, with Certum
   SimplySign running and logged in:

   ```bash
   node scripts/sign-windows-release.mjs <tag> --upload
   ```

   This replaces the two MSIs and the `SHA256SUMS` lines, verifies the result,
   and adds the tag to `versions.txt` in `marinaMoji/downloads` (stable releases
   only). `--mirror-only` mirrors a release that is already signed.
3. **Check the mirror.** The `marinaMoji/downloads` Pages workflow must pass for
   the tag, and both MSIs must answer 200 at
   `https://marinamoji.github.io/downloads/windows/<tag>/`. The workflow rejects
   an unsigned or wrongly signed MSI (tested in #51).
4. **Update the Microsoft Store submission (manual).** The Store does not follow
   the mirror: it keeps the installer URL and hash from its last submission. In
   Partner Center, open the marinaMoji app, start an update submission, set the
   x64 and arm64 installer URLs to the new tag's mirror URLs (silent switches
   `/qn /norestart`), and submit. The Store does not update MSI installs, so
   this only affects new installs.

   To see what the live listing currently points at:

   ```bash
   curl -s 'https://storeedgefd.dsx.mp.microsoft.com/v9.0/products/XPDM29N64D0T42?market=US&locale=en-US&deviceFamily=Windows.Desktop' \
     | python3 -c 'import json,sys; i=json.load(sys.stdin)["Payload"]["Installer"]["Architectures"]; [print(a, v["SourceUri"], v["Hash"]) for a,v in i.items()]'
   ```

   The printed hash should equal the tag's MSI line in `SHA256SUMS`.
5. **Website (manual).** Update the version and links on the downloads page of
   `marinamoji.crcao.fr`.
6. **Changelog.** Add the entry to `CHANGELOG.md` under the release.
