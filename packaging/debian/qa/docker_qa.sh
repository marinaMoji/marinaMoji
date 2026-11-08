#!/usr/bin/env bash
# Source-package QA smoke in Docker (#55): build the real source package
# (orig + orig-vendor + dsc), unpack it, dpkg-buildpackage, apt cycle.
#
# This is the laptop-friendly stand-in for sbuild + piuparts. Full sbuild
# on a Debian host is documented in qa/README.md.
#
# Usage (repo root, Docker Desktop running):
#   bash packaging/debian/qa/docker_qa.sh
#   MARINAMOJI_SKIP_VENDOR=1 bash packaging/debian/qa/docker_qa.sh
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEB_DIR="$(cd "$HERE/.." && pwd)"
REPO_ROOT="$(cd "$DEB_DIR/../.." && pwd)"
IMAGE="${MARINAMOJI_DOCKER_IMAGE:-ubuntu:24.04}"
VENDOR_HOST_LINUX="${DEB_DIR}/vendor-linux"
VENDOR_HOST="${DEB_DIR}/vendor"
VENDOR_CONTAINER="/var/cache/marinamoji-vendor"
CONTAINER_NAME="marinamoji-debian-qa"
BAZELISK_VERSION="${MARINAMOJI_BAZELISK_VERSION:-v1.30.0}"
BUILD_ROOT="/build/marinamoji"

if ! command -v docker >/dev/null 2>&1; then
  echo "docker_qa.sh: docker not found" >&2
  exit 1
fi
if ! docker info >/dev/null 2>&1; then
  echo "docker_qa.sh: cannot talk to Docker daemon" >&2
  exit 1
fi

ARCH="$(uname -m)"
case "$ARCH" in
  arm64|aarch64) BAZELISK_ASSET="bazelisk-linux-arm64" ;;
  x86_64|amd64) BAZELISK_ASSET="bazelisk-linux-amd64" ;;
  *)
    echo "docker_qa.sh: unsupported arch: $ARCH" >&2
    exit 1
    ;;
esac

EMPTY_NDK="$(mktemp -d "${TMPDIR:-/tmp}/marinamoji-empty-ndk.XXXXXX")"
docker rm -f "$CONTAINER_NAME" >/dev/null 2>&1 || true

echo "=== QA Phase 1: image + Build-Depends ($IMAGE) ==="
docker run -d --name "$CONTAINER_NAME" \
  -v "$REPO_ROOT:/src:rw" \
  -v "$EMPTY_NDK:/src/src/third_party/ndk:ro" \
  -e DEBIAN_FRONTEND=noninteractive \
  -e ANDROID_NDK_HOME= \
  -w /src \
  "$IMAGE" \
  sleep infinity

cleanup() {
  docker rm -f "$CONTAINER_NAME" >/dev/null 2>&1 || true
  rm -rf "$EMPTY_NDK"
}
trap cleanup EXIT

docker exec "$CONTAINER_NAME" bash -lc '
set -euo pipefail
apt-get update -qq
apt-get install -y -qq \
  ca-certificates curl rsync fakeroot \
  build-essential clang git unzip zip \
  openjdk-17-jdk python3 pkgconf \
  debhelper dpkg-dev lintian desktop-file-utils \
  libibus-1.0-dev qt6-base-dev libgtk-3-dev librsvg2-dev libopencc-dev \
  >/dev/null
'

docker exec "$CONTAINER_NAME" bash -lc "
set -euo pipefail
curl -fsSL -o /usr/local/bin/bazelisk \
  https://github.com/bazelbuild/bazelisk/releases/download/${BAZELISK_VERSION}/${BAZELISK_ASSET}
chmod +x /usr/local/bin/bazelisk
cd /src/src && bazelisk version
"

echo "=== QA Phase 2: vendor tree (container-native) ==="
if [[ "${MARINAMOJI_SKIP_VENDOR:-0}" == "1" ]]; then
  SRC_VENDOR=""
  if [[ -d "$VENDOR_HOST_LINUX/rules_python+" ]]; then
    SRC_VENDOR="/src/packaging/debian/vendor-linux"
  elif [[ -d "$VENDOR_HOST/rules_python+" ]]; then
    SRC_VENDOR="/src/packaging/debian/vendor"
  else
    echo "docker_qa.sh: MARINAMOJI_SKIP_VENDOR=1 but no vendor(-linux) tree found" >&2
    exit 1
  fi
  echo "docker_qa.sh: copying $SRC_VENDOR -> $VENDOR_CONTAINER"
  docker exec "$CONTAINER_NAME" bash -lc "
set -euo pipefail
rm -rf '$VENDOR_CONTAINER'
cp -a '$SRC_VENDOR' '$VENDOR_CONTAINER'
du -sh '$VENDOR_CONTAINER'
"
else
  docker exec -e MARINAMOJI_VENDOR_DIR="$VENDOR_CONTAINER" \
    -e ANDROID_NDK_HOME= \
    "$CONTAINER_NAME" bash -lc '
set -euo pipefail
unset ANDROID_NDK_HOME || true
mkdir -p '"$VENDOR_CONTAINER"'
cd /src
bash packaging/debian/offline/vendor_deps.sh -- --config oss_linux --config release_build
'
fi

echo "=== QA Phase 3: source package -> dpkg-source -x -> dpkg-buildpackage ==="
# Build the .orig / .orig-vendor / .dsc exactly as a maintainer would, from
# the committed tree (git archive), then unpack them with dpkg-source and
# build the result. Commit your changes first: uncommitted work is not
# part of the source package.
docker exec -e ANDROID_NDK_HOME= "$CONTAINER_NAME" bash -lc '
set -euo pipefail
unset ANDROID_NDK_HOME || true
git config --global --add safe.directory /src
apt-get install -y -qq xz-utils >/dev/null
rm -rf '"$BUILD_ROOT"'
mkdir -p '"$BUILD_ROOT"'/out
cd /src
bash packaging/debian/build_source_package.sh \
  --vendor-src '"$VENDOR_CONTAINER"' \
  --output-dir '"$BUILD_ROOT"'/out \
  --with-dpkg-source
cd '"$BUILD_ROOT"'/out
dpkg-source -x marinamoji_*.dsc '"$BUILD_ROOT"'/pkg
cd '"$BUILD_ROOT"'/pkg
test -d vendor/rules_python+
# Binary-only build; UNRELEASED changelog is fine for smoke.
dpkg-buildpackage -us -uc -b -rfakeroot
ls -la '"$BUILD_ROOT"'/marinamoji_*.deb
'

echo "=== QA Phase 4: apt install/remove/purge smoke (piuparts-shaped) ==="
docker exec "$CONTAINER_NAME" bash -lc '
set -euo pipefail
DEB=$(ls '"$BUILD_ROOT"'/marinamoji_*.deb | head -1)
echo "docker_qa.sh: using $DEB"
apt-get install -y -qq "$DEB"
dpkg -s marinamoji | grep -q "^Status: install ok installed"
test -f /usr/share/ibus/component/marinamoji.xml
apt-get remove -y -qq marinamoji
apt-get install -y -qq "$DEB"
apt-get purge -y -qq marinamoji
dpkg -s marinamoji >/dev/null 2>&1 && exit 1 || true
echo "docker_qa.sh: install/remove/purge smoke OK"
'

echo "=== QA Phase 5: lintian (informational) ==="
docker exec "$CONTAINER_NAME" bash -lc '
set -euo pipefail
DEB=$(ls '"$BUILD_ROOT"'/marinamoji_*.deb | head -1)
lintian -i "$DEB" || true
'

mkdir -p "$REPO_ROOT/dist"
DEB_IN_CONTAINER="$(docker exec "$CONTAINER_NAME" bash -lc "ls $BUILD_ROOT/marinamoji_*.deb | head -1")"
DEB_BASENAME="$(basename "$DEB_IN_CONTAINER")"
docker cp "$CONTAINER_NAME:$DEB_IN_CONTAINER" "$REPO_ROOT/dist/$DEB_BASENAME"

echo "=== SUCCESS: source-package QA smoke passed ==="
ls -la "$REPO_ROOT/dist/$DEB_BASENAME"
