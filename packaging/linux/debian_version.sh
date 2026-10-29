#!/usr/bin/env bash
# Map marinaMoji product / GitHub versions onto Debian package Versions.
#
# Product tags use SemVer-style prereleases: 0.0.19-rc3. In Debian, a hyphen
# starts the debian_revision, so 0.0.19-rc3 sorts *newer* than the final
# 0.0.19 and apt will not upgrade rc testers onto the release. The tilde
# form 0.0.19~rc3 sorts before 0.0.19, which is what we want.
#
# Source this file; do not execute it.
#   debian_package_version <product-version>  -> stdout
#   debian_is_hyphen_prerelease <version>     -> exit 0/1
#   debian_upstream_base <version>            -> stdout (X.Y.Z, or empty)

debian_package_version() {
  local v="${1-}"
  [[ -n "$v" ]] || return 1
  # Already in Debian prerelease form, or a migration bump (0.0.19+1).
  if [[ "$v" == *~* || "$v" == *+* ]]; then
    printf '%s\n' "$v"
    return 0
  fi
  # Prefer sed over [[ =~ ]] so this works under bash 3.2 (macOS /bin/bash)
  # and bash 5 (Ubuntu CI) alike.
  local converted
  converted="$(printf '%s\n' "$v" | sed -E 's/^([0-9]+(\.[0-9]+){1,2})-(rc|alpha|beta)([0-9]+)$/\1~\3\4/')"
  printf '%s\n' "$converted"
}

debian_is_hyphen_prerelease() {
  printf '%s\n' "${1-}" | grep -Eq '^[0-9]+(\.[0-9]+){1,2}-(rc|alpha|beta)[0-9]+$'
}

# Strip a trailing -rcN / ~rcN / +N (and similar) so we can match a final
# release to its prerelease siblings.
debian_upstream_base() {
  local v="${1-}" base
  base="$(printf '%s\n' "$v" | sed -E -n 's/^([0-9]+(\.[0-9]+){1,2})([-+~].*)?$/\1/p')"
  [[ -n "$base" ]] || return 1
  printf '%s\n' "$base"
}
