package(default_visibility = ["//visibility:public"])

load("@build_bazel_apple_support//xcode:xcode_config.bzl", "xcode_config")
load("@build_bazel_apple_support//xcode:xcode_version.bzl", "xcode_version")

xcode_version(
    name = "version26_6_0_17F113",
    version = "/Applications/Xcode.app/Contents/Developer",
    default_ios_sdk_version = "26.5",
    default_tvos_sdk_version = "26.5",
    default_macos_sdk_version = "26.5",
    default_visionos_sdk_version = "26.5",
    default_watchos_sdk_version = "26.5",
)

xcode_config(
    name = "host_xcodes",
    versions = [":version26_6_0_17F113"],
    default = ":version26_6_0_17F113",
)
