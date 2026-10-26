#!/usr/bin/env bash
# Build a signed Debian apt repository tree from one or more .deb packages.
#
# Layout (standard dists/ + pool/):
#   pool/main/m/marinamoji/*.deb
#   dists/<suite>/main/binary-{amd64,arm64}/Packages{,.gz}
#   dists/<suite>/Release
#   dists/<suite>/InRelease          (clearsigned; requires --gpg-key-id)
#   dists/<suite>/Release.gpg        (detached; requires --gpg-key-id)
#   marinaMoji-release-public-key.asc
#   index.html                      (human-facing install blurb)
#
# One shared pool serves every suite. --suite may be repeated (default:
# unstable and stable). "stable" lists final releases only; any RC (Version
# ending in -rcN or ~rcN) is left out. Every other suite, "unstable" included,
# lists all packages.
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
  [--suite unstable --suite stable]
EOF
  exit 1
}

DEB_DIR='' OUTDIR='' PUBLIC_KEY='' GPG_KEY_ID=''
ORIGIN='marinaMoji'
SUITES=()
LABEL='marinaMoji'

while [[ $# -gt 0 ]]; do
  case "$1" in
    --deb-dir) DEB_DIR="$2"; shift 2 ;;
    --output-dir) OUTDIR="$2"; shift 2 ;;
    --public-key) PUBLIC_KEY="$2"; shift 2 ;;
    --gpg-key-id) GPG_KEY_ID="$2"; shift 2 ;;
    --origin) ORIGIN="$2"; shift 2 ;;
    --suite) SUITES+=("$2"); shift 2 ;;
    *) usage ;;
  esac
done

[[ -n "$DEB_DIR" && -n "$OUTDIR" ]] || usage
[[ ${#SUITES[@]} -gt 0 ]] || SUITES=(unstable stable)
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

# Classify each package: a release candidate has a Version ending in
# -rcN / ~rcN. The "stable" suite excludes them; every other suite takes all.
is_rc() { [[ "$(dpkg-deb -f "$1" Version)" =~ [-~+.]rc[0-9]*$ ]]; }

for deb in "$POOL"/*.deb; do
  arch="$(dpkg-deb -f "$deb" Architecture)"
  case "$arch" in
    amd64|arm64) ;;
    *)
      echo "error: unsupported Architecture '$arch' in $(basename "$deb")" >&2
      exit 1
      ;;
  esac
done

STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT

build_suite() {
  local suite="$1" deb arch
  local -a members=() arches=()
  for deb in "$POOL"/*.deb; do
    if [[ "$suite" == stable ]] && is_rc "$deb"; then continue; fi
    members+=("$deb")
  done
  if [[ ${#members[@]} -eq 0 ]]; then
    echo "error: suite '$suite' would contain no packages" >&2
    exit 1
  fi
  mapfile -t arches < <(for deb in "${members[@]}"; do dpkg-deb -f "$deb" Architecture; done | sort -u)

  # dpkg-scanpackages scans a directory, so give it a per-suite view of the
  # shared pool (symlinks) rooted so Filename: still reads pool/main/...
  local view="$STAGE/$suite"
  mkdir -p "$view/pool/main/m/marinamoji"
  for deb in "${members[@]}"; do
    ln -s "$(cd "$(dirname "$deb")" && pwd)/$(basename "$deb")" \
      "$view/pool/main/m/marinamoji/$(basename "$deb")"
  done

  for arch in "${arches[@]}"; do
    local bin_dir="$OUTDIR/dists/$suite/main/binary-$arch"
    mkdir -p "$bin_dir"
    (cd "$view" && dpkg-scanpackages --arch "$arch" --multiversion pool/main) \
      >"$bin_dir/Packages"
    gzip -9n -c "$bin_dir/Packages" >"$bin_dir/Packages.gz"
    # Empty Contents files keep apt-ftparchive / clients happier on small repos.
    : >"$OUTDIR/dists/$suite/main/Contents-$arch"
    gzip -9n -c "$OUTDIR/dists/$suite/main/Contents-$arch" \
      >"$OUTDIR/dists/$suite/main/Contents-$arch.gz"
  done

  apt-ftparchive \
    -o "APT::FTPArchive::Release::Origin=$ORIGIN" \
    -o "APT::FTPArchive::Release::Label=$LABEL" \
    -o "APT::FTPArchive::Release::Suite=$suite" \
    -o "APT::FTPArchive::Release::Codename=$suite" \
    -o "APT::FTPArchive::Release::Architectures=${arches[*]}" \
    -o "APT::FTPArchive::Release::Components=main" \
    -o "APT::FTPArchive::Release::Description=marinaMoji Debian/Ubuntu packages ($suite)" \
    release "$OUTDIR/dists/$suite" >"$OUTDIR/dists/$suite/Release"

  if [[ -n "$GPG_KEY_ID" ]]; then
    command -v gpg >/dev/null || { echo "error: gpg not found" >&2; exit 1; }
    local -a gpg_args=(--batch --yes --pinentry-mode loopback --local-user "$GPG_KEY_ID")
    if [[ -n "${APT_GPG_PASSPHRASE:-}" ]]; then
      gpg_args+=(--passphrase "$APT_GPG_PASSPHRASE")
    fi
    gpg "${gpg_args[@]}" --clearsign \
      -o "$OUTDIR/dists/$suite/InRelease" "$OUTDIR/dists/$suite/Release"
    gpg "${gpg_args[@]}" -abs \
      -o "$OUTDIR/dists/$suite/Release.gpg" "$OUTDIR/dists/$suite/Release"
  else
    echo "warning: --gpg-key-id not set; writing unsigned Release for $suite" >&2
  fi
  echo "Suite $suite: ${#members[@]} package(s), arches: ${arches[*]}"
}

for suite in "${SUITES[@]}"; do
  build_suite "$suite"
done

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
  in two channels: <strong>stable</strong> (final releases only) and
  <strong>unstable</strong> (every release, including release candidates).</p>
  <h2>Install</h2>
  <pre>curl -fsSL ${PAGES_URL_HINT}/marinaMoji-release-public-key.asc \\
  | sudo gpg --dearmor -o /usr/share/keyrings/marinamoji-archive-keyring.gpg

echo "deb [signed-by=/usr/share/keyrings/marinamoji-archive-keyring.gpg] ${PAGES_URL_HINT} stable main" \\
  | sudo tee /etc/apt/sources.list.d/marinamoji.list

sudo apt update
sudo apt install marinamoji
ibus restart</pre>
  <p>To follow release candidates too, use <code>unstable</code> instead of
  <code>stable</code> in the <code>deb</code> line.</p>
  <p>See <a href="https://github.com/marinaMoji/marinaMoji#linux">README</a> and
  <a href="https://github.com/marinaMoji/marinaMoji/blob/main/SECURITY.md">SECURITY.md</a>.</p>
</body>
</html>
EOF

echo "Built apt repo at $OUTDIR (${#DEBS[@]} package(s) in pool; suites: ${SUITES[*]})"
