#!/usr/bin/env bash
# Tiny self-check for debian_version.sh (run from anywhere).
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=debian_version.sh
source "$HERE/debian_version.sh"

fail=0
expect() {
  local got="$1" want="$2" label="$3"
  if [[ "$got" != "$want" ]]; then
    echo "FAIL $label: got '$got', want '$want'" >&2
    fail=1
  fi
}

expect "$(debian_package_version '0.0.19')" '0.0.19' 'final'
expect "$(debian_package_version '0.0.19-rc3')" '0.0.19~rc3' 'rc hyphen'
expect "$(debian_package_version '0.0.19~rc3')" '0.0.19~rc3' 'rc tilde passthrough'
expect "$(debian_package_version '0.0.19+1')" '0.0.19+1' 'migration passthrough'
expect "$(debian_package_version '1.2-beta1')" '1.2~beta1' 'beta'
expect "$(debian_upstream_base '0.0.19-rc3')" '0.0.19' 'base from hyphen rc'
expect "$(debian_upstream_base '0.0.19~rc3')" '0.0.19' 'base from tilde rc'
expect "$(debian_upstream_base '0.0.19+1')" '0.0.19' 'base from +1'

debian_is_hyphen_prerelease '0.0.19-rc3' || { echo 'FAIL hyphen rc detect' >&2; fail=1; }
debian_is_hyphen_prerelease '0.0.19~rc3' && { echo 'FAIL tilde should not be hyphen' >&2; fail=1; }
debian_is_hyphen_prerelease '0.0.19' && { echo 'FAIL final should not be hyphen' >&2; fail=1; }

if [[ "$fail" -ne 0 ]]; then
  exit 1
fi
echo 'debian_version_test.sh: ok'
