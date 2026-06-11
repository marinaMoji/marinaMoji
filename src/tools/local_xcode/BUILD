# Checked-in Xcode version lock for macOS builds when Bazel's auto-generated
# @local_config_xcode is broken (common after `bazel clean --expunge`).
#
# Regenerate after an Xcode upgrade:
#   bash mac/fix_bazel_xcode_config.sh --write-local

load("@build_bazel_apple_support//xcode:xcode_config.bzl", "xcode_config")
load("@build_bazel_apple_support//xcode:xcode_version.bzl", "xcode_version")

package(default_visibility = ["//visibility:public"])

xcode_version(
    name = "version26_5_0_17F42",
    # Absolute path skips Bazel's xcode-locator lookup (fails in darwin sandbox on macOS 26+).
    version = "/Applications/Xcode.app/Contents/Developer",
    default_ios_sdk_version = "26.5",
    default_macos_sdk_version = "26.5",
    default_tvos_sdk_version = "26.5",
    default_visionos_sdk_version = "26.5",
    default_watchos_sdk_version = "26.5",
)

xcode_config(
    name = "host_xcodes",
    versions = [":version26_5_0_17F42"],
    default = ":version26_5_0_17F42",
)
