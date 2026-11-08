#!/usr/bin/env bash
# Prove offline Bazel for Linux (oss_linux) inside Docker (#53).
#
# Phase 1 (network): Ubuntu 24.04, install Build-Depends + Bazelisk, vendor
#                    //:package deps into a *container-native* directory
#                    (/var/cache/marinamoji-vendor). Vendoring onto the
#                    Docker Desktop bind-mount breaks Bazel's hardlinks.
# Phase 2 (no network): --network=none + --repository_disable_download
#                       + fresh --output_base.
#
# Usage (from repo root, Docker Desktop running):
#   bash packaging/debian/offline/docker_linux_proof.sh
#
# Optional:
#   MARINAMOJI_SKIP_VENDOR=1  reuse host packaging/debian/vendor-linux by
#                             copying it into the container-native path
#                             (still requires a prior successful vendor).
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEB_DIR="$(cd "$HERE/.." && pwd)"
REPO_ROOT="$(cd "$DEB_DIR/../.." && pwd)"
IMAGE="${MARINAMOJI_DOCKER_IMAGE:-ubuntu:24.04}"
# Host mirror of the vendor tree. Deliberately NOT MARINAMOJI_VENDOR_DIR (used
# by vendor_deps.sh/verify_offline.sh) because this script rm -rf's it.
VENDOR_HOST="${MARINAMOJI_VENDOR_HOST_DIR:-$DEB_DIR/vendor-linux}"
case "$VENDOR_HOST" in
  "$DEB_DIR"/vendor|"$DEB_DIR"/vendor-linux|"$DEB_DIR"/vendor-*) ;;
  *) echo "docker_linux_proof.sh: refusing to manage $VENDOR_HOST (must be under $DEB_DIR/vendor*)" >&2; exit 1 ;;
esac
VENDOR_CONTAINER="/var/cache/marinamoji-vendor"
CONTAINER_NAME="marinamoji-debian-offline-proof"
BAZELISK_VERSION="${MARINAMOJI_BAZELISK_VERSION:-v1.30.0}"

if ! command -v docker >/dev/null 2>&1; then
  echo "docker_linux_proof.sh: docker not found" >&2
  exit 1
fi
if ! docker info >/dev/null 2>&1; then
  echo "docker_linux_proof.sh: cannot talk to Docker daemon (is Desktop running?)" >&2
  exit 1
fi

ARCH="$(uname -m)"
case "$ARCH" in
  arm64|aarch64) BAZELISK_ASSET="bazelisk-linux-arm64" ;;
  x86_64|amd64) BAZELISK_ASSET="bazelisk-linux-amd64" ;;
  *)
    echo "docker_linux_proof.sh: unsupported host arch: $ARCH" >&2
    exit 1
    ;;
esac

mkdir -p "$VENDOR_HOST"
# Hide Darwin-only third_party/ndk from the Linux container (CI has none).
EMPTY_NDK="$(mktemp -d "${TMPDIR:-/tmp}/marinamoji-empty-ndk.XXXXXX")"

docker rm -f "$CONTAINER_NAME" >/dev/null 2>&1 || true

echo "=== Phase 1: networked image setup + vendor ($IMAGE, $BAZELISK_ASSET) ==="
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
  ca-certificates curl \
  build-essential clang git unzip zip \
  openjdk-17-jdk python3 pkgconf \
  libibus-1.0-dev qt6-base-dev libgtk-3-dev librsvg2-dev libopencc-dev \
  >/dev/null
update-ca-certificates >/dev/null 2>&1 || true
'

docker exec "$CONTAINER_NAME" bash -lc "
set -euo pipefail
curl -fsSL -o /usr/local/bin/bazelisk \
  https://github.com/bazelbuild/bazelisk/releases/download/${BAZELISK_VERSION}/${BAZELISK_ASSET}
chmod +x /usr/local/bin/bazelisk
cd /src/src && bazelisk version
"

if [[ "${MARINAMOJI_SKIP_VENDOR:-0}" == "1" ]] && [[ -d "$VENDOR_HOST/rules_python+" ]]; then
  echo "docker_linux_proof.sh: copying host vendor-linux -> $VENDOR_CONTAINER (native FS)"
  docker exec "$CONTAINER_NAME" bash -lc "
set -euo pipefail
rm -rf '$VENDOR_CONTAINER'
mkdir -p '$(dirname "$VENDOR_CONTAINER")'
# Materialize a real copy; bind-mount hardlinks are unsafe for Bazel vendor.
cp -a /src/packaging/debian/vendor-linux '$VENDOR_CONTAINER'
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
  echo "docker_linux_proof.sh: mirroring vendor tree to host $VENDOR_HOST"
  rm -rf "$VENDOR_HOST"
  docker cp "$CONTAINER_NAME:$VENDOR_CONTAINER/." "$VENDOR_HOST/"
fi

echo "=== Phase 2: offline build with --network=none ==="
docker exec -e ANDROID_NDK_HOME= "$CONTAINER_NAME" bash -lc '
set -euo pipefail
unset ANDROID_NDK_HOME || true
cd /src/src && bazelisk shutdown || true
' || true

docker network disconnect bridge "$CONTAINER_NAME" 2>/dev/null || \
  docker network disconnect "$(docker inspect -f '{{range $k,$v := .NetworkSettings.Networks}}{{$k}}{{end}}' "$CONTAINER_NAME" | awk '{print $1}')" "$CONTAINER_NAME"

docker exec -e MARINAMOJI_VENDOR_DIR="$VENDOR_CONTAINER" \
  -e MARINAMOJI_OFFLINE_TARGET=//server:mozc_server \
  -e MARINAMOJI_OFFLINE_OUTPUT_BASE=/tmp/marinamoji-offline-out \
  -e ANDROID_NDK_HOME= \
  "$CONTAINER_NAME" bash -lc '
set -euo pipefail
unset ANDROID_NDK_HOME || true
if curl -fsS --max-time 3 https://bcr.bazel.build/ >/dev/null 2>&1; then
  echo "docker_linux_proof.sh: ERROR: network still reachable after disconnect" >&2
  exit 1
fi
echo "docker_linux_proof.sh: no external network (curl to BCR failed as expected)"
# Sanity: the path Bazel previously complained about must exist on native FS
# (architecture-independent glob: aarch64 or x86_64 interpreter).
compgen -G '"'"''"$VENDOR_CONTAINER"'/rules_python++python+python_3_11_*/lib/python3.11/site-packages/pip/_internal/operations/build'"'"' >/dev/null
cd /src
bash packaging/debian/offline/verify_offline.sh -- --config oss_linux --config release_build
'

echo "=== Optional: offline //unix:package (full Linux zip) ==="
docker exec -e MARINAMOJI_VENDOR_DIR="$VENDOR_CONTAINER" \
  -e MARINAMOJI_OFFLINE_TARGET=//unix:package \
  -e MARINAMOJI_OFFLINE_OUTPUT_BASE=/tmp/marinamoji-offline-out \
  -e ANDROID_NDK_HOME= \
  "$CONTAINER_NAME" bash -lc '
set -euo pipefail
unset ANDROID_NDK_HOME || true
cd /src
bash packaging/debian/offline/verify_offline.sh -- --config oss_linux --config release_build
' || {
  echo "docker_linux_proof.sh: //unix:package offline build failed (mozc_server already proved deps)." >&2
  exit 1
}

echo "=== SUCCESS: Linux offline vendor + build proved ==="
docker exec "$CONTAINER_NAME" du -sh "$VENDOR_CONTAINER"
du -sh "$VENDOR_HOST" 2>/dev/null || true
