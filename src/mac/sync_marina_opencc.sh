#!/bin/bash
# Backward-compatible wrapper; see src/sync_marina_opencc.sh.
set -euo pipefail
exec "$(cd "$(dirname "$0")/.." && pwd)/sync_marina_opencc.sh" "$@"
