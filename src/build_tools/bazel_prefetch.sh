#!/bin/bash
# Pre-fetch Bazel external dependencies, retrying on network failures.
#
# GitHub's release-asset CDN intermittently returns 504 for the archives that
# Bazel Central Registry modules are pinned to (rules_cc, bazel-skylib,
# apple_support, ...). Bazel's built-in retries are short, so one bad moment
# fails the whole release build. Fetched archives land in Bazel's repository
# cache, so each retry only downloads what is still missing and the real build
# that follows finds everything locally.
#
# Usage: bazel_prefetch.sh [bazel fetch args, e.g. --config release_build]
set -u
attempts=8
delay=20
for ((i = 1; i <= attempts; i++)); do
  if bazelisk fetch //:package "$@"; then
    exit 0
  fi
  echo "bazel fetch failed (attempt $i/$attempts)" >&2
  if ((i < attempts)); then
    echo "retrying in ${delay}s" >&2
    sleep "$delay"
    delay=$((delay * 2 > 240 ? 240 : delay * 2))
  fi
done
echo "bazel fetch still failing after $attempts attempts" >&2
exit 1
