#!/usr/bin/env bash
# Point marinaMoji / Mozc at the curated OpenCC data in this directory.
# Usage: source opencc_runtime/env.sh
OPENCC_DATA_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export OPENCC_DATA_DIR
