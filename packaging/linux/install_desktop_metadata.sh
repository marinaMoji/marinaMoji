#!/usr/bin/env bash
# Stage the freedesktop/AppStream metadata into an install tree.
#
# The mozc.zip install tree produced by `bazelisk build package` contains only
# binaries, IBus glue and raw icons. Software centres (GNOME Software, Ubuntu
# App Center, KDE Discover) need a .desktop entry, an AppStream metainfo file
# and icons in an icon *theme* directory; without them the application shows a
# placeholder icon and "unknown" publisher/licence.
#
# This is shared by packaging/linux/build_deb.sh and the Arch PKGBUILDs so the
# .deb and the Arch packages present themselves identically.
#
# usage: install_desktop_metadata.sh --root <install-tree> --version <x.y.z>
#                                    [--data-dir <dir>] [--date <YYYY-MM-DD>]
#
# --root is the directory holding the "usr" hierarchy (${pkgdir} on Arch, the
# staged package root for dpkg-deb). --data-dir defaults to the "data"
# directory beside this script.
set -euo pipefail

APP_ID='io.github.marinamoji.marinaMoji'

usage() {
  echo "usage: $0 --root <install-tree> --version <x.y.z> [--data-dir <dir>] [--date <YYYY-MM-DD>]" >&2
  exit 1
}

ROOT='' VERSION='' DATA_DIR='' RELEASE_DATE=''
while [[ $# -gt 0 ]]; do
  case "$1" in
    --root) ROOT="$2"; shift 2 ;;
    --version) VERSION="$2"; shift 2 ;;
    --data-dir) DATA_DIR="$2"; shift 2 ;;
    --date) RELEASE_DATE="$2"; shift 2 ;;
    *) usage ;;
  esac
done
[[ -n "$ROOT" && -n "$VERSION" ]] || usage

if [[ -z "$DATA_DIR" ]]; then
  DATA_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/data" && pwd)"
fi
if [[ -z "$RELEASE_DATE" ]]; then
  # Honour SOURCE_DATE_EPOCH so reproducible builds get a stable date.
  RELEASE_DATE="$(date -u ${SOURCE_DATE_EPOCH:+-d "@$SOURCE_DATE_EPOCH"} +%Y-%m-%d)"
fi

install -d "$ROOT/usr/share/applications" \
           "$ROOT/usr/share/metainfo" \
           "$ROOT/usr/share/icons/hicolor/128x128/apps" \
           "$ROOT/usr/share/icons/hicolor/scalable/apps"

install -m 644 "$DATA_DIR/$APP_ID.desktop" \
               "$DATA_DIR/$APP_ID.Dictionary.desktop" \
               "$ROOT/usr/share/applications/"

sed -e "s/@VERSION@/$VERSION/" -e "s/@DATE@/$RELEASE_DATE/" \
    "$DATA_DIR/$APP_ID.metainfo.xml.in" \
    > "$ROOT/usr/share/metainfo/$APP_ID.metainfo.xml"
chmod 644 "$ROOT/usr/share/metainfo/$APP_ID.metainfo.xml"

# The install tree ships icons under /usr/share/icons/marinamoji, which is not
# an icon theme, so Icon= lookups miss. Publish them into hicolor as well.
install -m 644 "$ROOT/usr/share/icons/marinamoji/mozc.png" \
               "$ROOT/usr/share/icons/hicolor/128x128/apps/$APP_ID.png"
if [[ -f "$ROOT/usr/share/icons/marinamoji/mozc.svg" ]]; then
  install -m 644 "$ROOT/usr/share/icons/marinamoji/mozc.svg" \
                 "$ROOT/usr/share/icons/hicolor/scalable/apps/$APP_ID.svg"
fi

# Best-effort validation; these tools are not installed on every builder.
if command -v desktop-file-validate >/dev/null; then
  desktop-file-validate "$ROOT/usr/share/applications/$APP_ID.desktop" \
                        "$ROOT/usr/share/applications/$APP_ID.Dictionary.desktop"
fi
if command -v appstreamcli >/dev/null; then
  appstreamcli validate --no-net --pedantic \
    "$ROOT/usr/share/metainfo/$APP_ID.metainfo.xml" || true
fi
