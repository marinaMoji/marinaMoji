#!/usr/bin/env bash
# Build a signed Debian apt repository tree from one or more .deb packages.
#
# Layout (standard dists/ + pool/):
#   pool/main/m/marinamoji/*.deb
#   dists/unstable/main/binary-{amd64,arm64}/Packages{,.gz}
#   dists/unstable/Release
#   dists/unstable/InRelease          (clearsigned; requires --gpg-key-id)
#   dists/unstable/Release.gpg        (detached; requires --gpg-key-id)
#   marinaMoji-release-public-key.asc
#   index.html                      (human-facing install blurb)
#
# The default suite is "unstable" (pre-stable channel). Pass --suite stable
# later when a stable channel is published alongside it.
#
# Requires: dpkg-dev, apt-utils (apt-ftparchive), gzip; gnupg when signing.
set -euo pipefail

usage() {
  cat >&2 <<'EOF'
usage: build_apt_repo.sh \
  --deb-dir <dir with .deb files> \
  --output-dir <repo root> \
  [--public-key <marinaMoji-release-public-key.asc>] \
  [--gpg-key-id <fingerprint>] \
  [--origin marinaMoji] \
  [--suite unstable]
EOF
  exit 1
}

DEB_DIR='' OUTDIR='' PUBLIC_KEY='' GPG_KEY_ID=''
ORIGIN='marinaMoji'
SUITE='unstable'
LABEL='marinaMoji'
CODENAME='unstable'

while [[ $# -gt 0 ]]; do
  case "$1" in
    --deb-dir) DEB_DIR="$2"; shift 2 ;;
    --output-dir) OUTDIR="$2"; shift 2 ;;
    --public-key) PUBLIC_KEY="$2"; shift 2 ;;
    --gpg-key-id) GPG_KEY_ID="$2"; shift 2 ;;
    --origin) ORIGIN="$2"; shift 2 ;;
    --suite) SUITE="$2"; CODENAME="$2"; shift 2 ;;
    *) usage ;;
  esac
done

[[ -n "$DEB_DIR" && -n "$OUTDIR" ]] || usage
[[ -d "$DEB_DIR" ]] || { echo "error: --deb-dir is not a directory: $DEB_DIR" >&2; exit 1; }

shopt -s nullglob
DEBS=("$DEB_DIR"/*.deb)
if [[ ${#DEBS[@]} -eq 0 ]]; then
  echo "error: no .deb files in $DEB_DIR" >&2
  exit 1
fi

rm -rf "$OUTDIR"
POOL="$OUTDIR/pool/main/m/marinamoji"
mkdir -p "$POOL"
cp -f "${DEBS[@]}" "$POOL/"

ARCHES=()
for deb in "$POOL"/*.deb; do
  arch="$(dpkg-deb -f "$deb" Architecture)"
  case "$arch" in
    amd64|arm64) ARCHES+=("$arch") ;;
    *)
      echo "error: unsupported Architecture '$arch' in $(basename "$deb")" >&2
      exit 1
      ;;
  esac
done
# Unique sorted arches.
mapfile -t ARCHES < <(printf '%s\n' "${ARCHES[@]}" | sort -u)

for arch in "${ARCHES[@]}"; do
  bin_dir="$OUTDIR/dists/$SUITE/main/binary-$arch"
  mkdir -p "$bin_dir"
  # Paths in Packages must be relative to the archive root (OUTDIR).
  (
    cd "$OUTDIR"
    dpkg-scanpackages --arch "$arch" --multiversion pool/main \
      >"dists/$SUITE/main/binary-$arch/Packages"
  )
  gzip -9n -c "$bin_dir/Packages" >"$bin_dir/Packages.gz"
done

# Empty Contents files keep apt-ftparchive / clients happier on small repos.
for arch in "${ARCHES[@]}"; do
  : >"$OUTDIR/dists/$SUITE/main/Contents-$arch"
  gzip -9n -c "$OUTDIR/dists/$SUITE/main/Contents-$arch" \
    >"$OUTDIR/dists/$SUITE/main/Contents-$arch.gz"
done

ARCH_LIST="$(IFS=' '; echo "${ARCHES[*]}")"
apt-ftparchive \
  -o "APT::FTPArchive::Release::Origin=$ORIGIN" \
  -o "APT::FTPArchive::Release::Label=$LABEL" \
  -o "APT::FTPArchive::Release::Suite=$SUITE" \
  -o "APT::FTPArchive::Release::Codename=$CODENAME" \
  -o "APT::FTPArchive::Release::Architectures=$ARCH_LIST" \
  -o "APT::FTPArchive::Release::Components=main" \
  -o "APT::FTPArchive::Release::Description=marinaMoji Debian/Ubuntu packages" \
  release "$OUTDIR/dists/$SUITE" >"$OUTDIR/dists/$SUITE/Release"

if [[ -n "$GPG_KEY_ID" ]]; then
  command -v gpg >/dev/null || { echo "error: gpg not found" >&2; exit 1; }
  gpg_args=(--batch --yes --pinentry-mode loopback --local-user "$GPG_KEY_ID")
  if [[ -n "${APT_GPG_PASSPHRASE:-}" ]]; then
    gpg_args+=(--passphrase "$APT_GPG_PASSPHRASE")
  fi
  gpg "${gpg_args[@]}" --clearsign \
    -o "$OUTDIR/dists/$SUITE/InRelease" "$OUTDIR/dists/$SUITE/Release"
  gpg "${gpg_args[@]}" -abs \
    -o "$OUTDIR/dists/$SUITE/Release.gpg" "$OUTDIR/dists/$SUITE/Release"
else
  echo "warning: --gpg-key-id not set; writing unsigned Release only" >&2
fi

if [[ -n "$PUBLIC_KEY" ]]; then
  [[ -f "$PUBLIC_KEY" ]] || { echo "error: public key not found: $PUBLIC_KEY" >&2; exit 1; }
  cp -f "$PUBLIC_KEY" "$OUTDIR/marinaMoji-release-public-key.asc"
fi

PAGES_URL_HINT='https://marinamoji.github.io/marinaMoji'
cat >"$OUTDIR/index.html" <<EOF
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>marinaMoji apt repository</title>
  <style>
    body { font-family: system-ui, sans-serif; max-width: 42rem; margin: 2rem auto; padding: 0 1rem; line-height: 1.5; }
    code, pre { font-family: ui-monospace, monospace; }
    pre { background: #f4f4f4; padding: 1rem; overflow-x: auto; }
  </style>
</head>
<body>
  <h1>marinaMoji apt repository</h1>
  <p>Debian/Ubuntu packages for <a href="https://github.com/marinaMoji/marinaMoji">marinaMoji</a>
  (<strong>unstable</strong> channel).</p>
  <h2>Install</h2>
  <pre>curl -fsSL ${PAGES_URL_HINT}/marinaMoji-release-public-key.asc \\
  | sudo gpg --dearmor -o /usr/share/keyrings/marinamoji-archive-keyring.gpg

echo "deb [signed-by=/usr/share/keyrings/marinamoji-archive-keyring.gpg] ${PAGES_URL_HINT} unstable main" \\
  | sudo tee /etc/apt/sources.list.d/marinamoji.list

sudo apt update
sudo apt install marinamoji
ibus restart</pre>
  <p>See <a href="https://github.com/marinaMoji/marinaMoji#linux">README</a> and
  <a href="https://github.com/marinaMoji/marinaMoji/blob/main/SECURITY.md">SECURITY.md</a>.</p>
</body>
</html>
EOF

echo "Built apt repo at $OUTDIR (${#DEBS[@]} package(s), arches: ${ARCHES[*]})"
