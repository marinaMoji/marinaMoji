load("@build_bazel_apple_support//xcode:xcode_config.bzl", "xcode_config")
load("@build_bazel_apple_support//xcode:xcode_version.bzl", "xcode_version")

package(default_visibility = ["//visibility:public"])

xcode_version(
    name = "version27_2",
    default_ios_sdk_version = "27.2",
    default_macos_sdk_version = "27.2",
    default_tvos_sdk_version = "27.2",
    default_visionos_sdk_version = "27.2",
    default_watchos_sdk_version = "27.2",
    version = "/Applications/Xcode.app/Contents/Developer",
)

xcode_config(
    name = "host_xcodes",
    default = ":version27_2",
    versions = [":version27_2"],
)
