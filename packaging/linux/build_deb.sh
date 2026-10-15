#!/usr/bin/env bash
# Build a Debian package from the install tree in mozc.zip (the output of
# `bazelisk build package` on Linux, see src/unix/build_package.py).
#
# The zip already mirrors the filesystem layout (usr/lib/marinamoji, ...),
# so this script stages it, normalizes permissions, computes shared-library
# dependencies with dpkg-shlibdeps (the same mechanism debhelper uses), and
# wraps the result with dpkg-deb.
#
# Requires: unzip, dpkg-dev, binutils (strip).
set -euo pipefail

usage() {
  echo "usage: $0 --zip <mozc.zip> --version <x.y.z> --arch <amd64|arm64> --output-dir <dir>" >&2
  exit 1
}

ZIP='' VERSION='' ARCH='' OUTDIR=''
while [[ $# -gt 0 ]]; do
  case "$1" in
    --zip) ZIP="$2"; shift 2 ;;
    --version) VERSION="$2"; shift 2 ;;
    --arch) ARCH="$2"; shift 2 ;;
    --output-dir) OUTDIR="$2"; shift 2 ;;
    *) usage ;;
  esac
done
[[ -n "$ZIP" && -n "$VERSION" && -n "$ARCH" && -n "$OUTDIR" ]] || usage

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
ROOT="$WORK/root"
mkdir -p "$ROOT"
unzip -q "$ZIP" -d "$ROOT"
# tmp/icons.zip is a staging artifact of build_package.py, not an installed file.
rm -rf "$ROOT/tmp"

# Desktop entries, hicolor icons and AppStream metadata (shared with the Arch
# packages), plus the Debian copyright file and man page.
bash "$HERE/install_desktop_metadata.sh" --root "$ROOT" --version "$VERSION"
install -d "$ROOT/usr/share/doc/marinamoji"
install -m 644 "$HERE/data/copyright" "$ROOT/usr/share/doc/marinamoji/copyright"
install -d "$ROOT/usr/share/man/man1"
gzip -9n < "$HERE/data/mozc_emacs_helper.1" \
  > "$ROOT/usr/share/man/man1/mozc_emacs_helper.1.gz"

# Policy 12.7: a native package ships its Debian changelog as changelog.gz.
# A single generated entry pointing at CHANGELOG.md; gzip -n keeps it
# reproducible (no embedded name or timestamp).
CHANGELOG_DATE="$(date -R ${SOURCE_DATE_EPOCH:+-d "@$SOURCE_DATE_EPOCH"})"
cat <<EOF | gzip -9n > "$ROOT/usr/share/doc/marinamoji/changelog.gz"
marinamoji ($VERSION) unstable; urgency=medium

  * Release $VERSION. See CHANGELOG.md in the source tree:
    https://github.com/marinaMoji/marinaMoji/blob/master/CHANGELOG.md

 -- Daniel Patrick Morgan <daniel.morgan@college-de-france.fr>  $CHANGELOG_DATE
EOF

find "$ROOT" -type d -exec chmod 755 {} +
find "$ROOT" -type f -exec chmod 644 {} +
BINARIES=(
  usr/lib/marinamoji/mozc_server
  usr/lib/marinamoji/mozc_tool
  usr/lib/marinamoji/mozc_renderer
  usr/lib/marinamoji/mozc_sync
  usr/lib/ibus-marinamoji/ibus-engine-marinamoji
  usr/bin/mozc_emacs_helper
)
for bin in "${BINARIES[@]}"; do
  chmod 755 "$ROOT/$bin"
  # Same flags dh_strip uses. CI builds natively on each arch, so the host
  # strip matches the binaries.
  strip --remove-section=.comment --remove-section=.note --strip-unneeded \
    "$ROOT/$bin"
done

# dpkg-shlibdeps needs a debian/control in the working directory.
mkdir -p "$ROOT/debian"
touch "$ROOT/debian/control"
SHLIB_DEPS="$(cd "$ROOT" && dpkg-shlibdeps -O --ignore-missing-info "${BINARIES[@]}" | sed 's/^shlibs:Depends=//')"
rm -rf "$ROOT/debian"

# CI builds on Ubuntu 24.04 (noble), where the 64-bit time_t transition
# left packages like libqt6gui6t64 with a "t64" suffix. Releases where the
# transition has completed drop the suffix again (libqt6gui6), so pin
# dpkg-shlibdeps' t64 names as alternatives rather than hard requirements,
# letting apt pick whichever name the target release actually has.
SHLIB_DEPS="$(echo "$SHLIB_DEPS" | awk -F', ' '{
  for (i = 1; i <= NF; i++) {
    dep = $i
    n = split(dep, parts, " ")
    pkg = parts[1]
    if (pkg ~ /t64$/) {
      alt = pkg
      sub(/t64$/, "", alt)
      dep = dep " | " alt
    }
    printf "%s%s", dep, (i < NF ? ", " : "")
  }
}')"
DEPS="ibus (>= 1.5.0), hicolor-icon-theme${SHLIB_DEPS:+, $SHLIB_DEPS}"

INSTALLED_SIZE="$(du -sk "$ROOT" | cut -f1)"

mkdir -p "$ROOT/DEBIAN"
cat > "$ROOT/DEBIAN/control" <<EOF
Package: marinamoji
Version: $VERSION
Section: utils
Priority: optional
Architecture: $ARCH
Maintainer: Daniel Patrick Morgan <daniel.morgan@college-de-france.fr>
Installed-Size: $INSTALLED_SIZE
Depends: $DEPS
Conflicts: emacs-mozc, emacs-mozc-bin
Replaces: emacs-mozc, emacs-mozc-bin
Homepage: https://github.com/marinaMoji/marinaMoji
Description: Japanese input method for classical and literary Japanese
 marinaMoji is a fork of Mozc focused on classical, literary, and
 pre-reform Japanese. It provides an IBus engine that installs
 alongside stock Mozc, GUI configuration tools, and an Emacs helper.
EOF

# Maintainer scripts: refresh IBus component cache only (never restart).
install -m 755 "$HERE/data/postinst" "$ROOT/DEBIAN/postinst"
install -m 755 "$HERE/data/postrm" "$ROOT/DEBIAN/postrm"

# DEBIAN/md5sums lists every shipped file (paths relative to /), as
# dh_md5sums does; dpkg --verify / debsums read it.
(cd "$ROOT" && find . -path ./DEBIAN -prune -o -type f -printf '%P\0' \
  | LC_ALL=C sort -z | xargs -0 md5sum) > "$ROOT/DEBIAN/md5sums"
chmod 644 "$ROOT/DEBIAN/md5sums"

mkdir -p "$OUTDIR"
dpkg-deb --build --root-owner-group -Zxz "$ROOT" \
  "$OUTDIR/marinamoji_${VERSION}_${ARCH}.deb"
