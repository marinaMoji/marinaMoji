# Network fetches used by marinaMoji Bazel

Source of truth: `src/MODULE.bazel` (and the BCR graph it pulls in).
This list is for Debian packaging review (#53), not a second pin file.

## Bazel Central Registry modules (`bazel_dep`)

Resolved via `https://bcr.bazel.build` unless overridden. Direct deps
include (versions change — read MODULE.bazel):

* abseil-cpp, abseil-py, protobuf, googletest, google_benchmark, re2
* platforms, bazel_skylib, libsodium, rules_pkg, rules_python
* rules_apple, apple_support, rules_cc
* rules_go, gazelle, rules_android, rules_android_ndk (mostly transitive
  pins for Bazel 9)

Vendor mode copies their sources and the registry metadata under
`vendor/_registries/`.

## Direct `http_file` / `http_archive` (non-BCR)

| Repo name | Used for | Needed on Linux package? |
| --- | --- | --- |
| material_icon_* | GUI icons | yes |
| jigmo_fonts | Rare-kanji fonts in mac/Win installers | no (Linux Recommends distro fonts) |
| com_microsoft_wil | Windows | no |
| google_toolbox_for_mac | macOS tests/tooling | mac only |
| ja_usage_dict | usage rewriter data | yes |
| zip_code_jigyosyo / zip_code_ken_all | postal code data | yes |
| opencc_source (+ marisa_trie, rapidjson, darts_clone, tclap) | OpenCC static build (mac/Win); Linux prefers system OpenCC via pkg-config | mac/Win; Linux may still fetch if selected |
| windows_sdk / wix (custom repo rules) | Windows installer | no |

## Host tools still required (not vendored)

From Build-Depends / compiling docs: C++ toolchain, pkg-config libs
(`libibus-1.0-dev`, `qt6-base-dev`, `libgtk-3-dev`, `librsvg2-dev`,
`libopencc-dev` on Linux), Python 3, unzip/zip, and Bazelisk providing
the Bazel version in `src/.bazeliskrc`.
