# Copyright 2010-2021, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Genrule wrapper around build_installer.py.

Factored out of BUILD.bazel because marinaMoji builds one MSI per culture and
BUILD files cannot define functions. The upstream Mozc and GoogleJapaneseInput
brandings keep their own hardcoded language and pass no culture at all.
"""

_TARGET_COMPATIBLE_WITH = [
    "@platforms//os:windows",
]

def mozc_win_installer(
        name,
        out,
        wxs_file,
        branding,
        culture = None,
        loc_file = None):
    """Builds one MSI with wix.exe.

    Args:
      name: target name.
      out: the MSI filename to produce.
      wxs_file: label of the .wxs to compile.
      branding: BRANDING, forwarded to build_installer.py for the UpgradeCode.
      culture: WiX culture, e.g. "fr-FR". Must be one of build_installer.py's
        _CULTURES. When set, loc_file is required.
      loc_file: label of the .wxl holding this culture's strings.
    """
    if (culture == None) != (loc_file == None):
        fail("culture and loc_file must be given together (got %r / %r)." %
             (culture, loc_file))

    srcs = [
        "//gui/tool:mozc_tool_win",
        "//renderer/win32:win32_renderer_main",
        "//server:mozc_server_win",
        "//win32/broker:mozc_broker_main",
        "//win32/cache_service:mozc_cache_service",
        "//win32/tip:mozc_tip32",
        "//win32/tip:mozc_tip64",
        "//win32/custom_action",
        "//sync:marinaMojiSync_win",
        "//data/marina_opencc:marinaShin2Kyu.json",
        "//data/marina_opencc:marinaShin2KyuCharacters.ocd2",
        "//data/marina_opencc:marinaShin2KyuPhrases.ocd2",
        "//data/marina_opencc:marinaShin2KyuVariants.ocd2",
        "//data/images/win:toolbar_icons",
        "//data/images/win:toolbar_icons/logo_long_light_24.png",
        wxs_file,
        "//base:mozc_version_txt",
        "//data/images/win:product_icon.ico",
        "//data/installer:credits_en.html",
        "@qt_win//:bin/Qt6Core.dll",
        "@wix//:wix.exe",
    ] + ([loc_file] if loc_file else []) + select({
        ":build_arm64_binaries": [
            "//win32/tip:mozc_tip64arm",
            "//win32/tip:mozc_tip64x",
        ],
        "//conditions:default": [],
    })

    cmd = " ".join([
        "$(location :build_msi)",
        "--output=$@",
        "--version_file=$(location //base:mozc_version_txt)",
        "--mozc_tool=$(location //gui/tool:mozc_tool_win)",
        "--mozc_renderer=$(location //renderer/win32:win32_renderer_main)",
        "--mozc_server=$(location //server:mozc_server_win)",
        "--mozc_broker=$(location //win32/broker:mozc_broker_main)",
        "--mozc_cache_service=$(location //win32/cache_service:mozc_cache_service)",
        "--mozc_tip32=$(location //win32/tip:mozc_tip32)",
        "--mozc_tip64=$(location //win32/tip:mozc_tip64)",
        "--custom_action=$(location //win32/custom_action)",
        "--marinamoji_sync=$(location //sync:marinaMojiSync_win)",
        "--opencc_config=$(location //data/marina_opencc:marinaShin2Kyu.json)",
        "--opencc_characters=$(location //data/marina_opencc:marinaShin2KyuCharacters.ocd2)",
        "--opencc_phrases=$(location //data/marina_opencc:marinaShin2KyuPhrases.ocd2)",
        "--opencc_variants=$(location //data/marina_opencc:marinaShin2KyuVariants.ocd2)",
        "--toolbar_icons_sample=$(location //data/images/win:toolbar_icons/logo_long_light_24.png)",
        "--icon_path=$(location //data/images/win:product_icon.ico)",
        "--credit_file=$(location //data/installer:credits_en.html)",
        "--qt_core_dll=$(location @qt_win//:bin/Qt6Core.dll)",
        "--wxs_path=$(location " + wxs_file + ")",
        "--wix_path=$(location @wix//:wix.exe)",
        "--branding=" + branding,
        "--vs_install_dir=\"$$BAZEL_VC\"",
    ] + ([
        "--culture=" + culture,
        "--loc_file=$(location " + loc_file + ")",
    ] if loc_file else [])) + select({
        ":build_arm64_binaries": " " + " ".join([
            "--mozc_tip64arm=$(location //win32/tip:mozc_tip64arm)",
            "--mozc_tip64x=$(location //win32/tip:mozc_tip64x)",
        ]),
        "//conditions:default": "",
    }) + select({
        ":debug_build": " --debug_build",
        "//conditions:default": "",
    }) + select({
        "@platforms//cpu:x86_64": " --arch=x64",
        "@platforms//cpu:arm64": " --arch=arm64",
        "//conditions:default": "",
    }) + select({
        "//:enable_win_universal_installer": " --enable_win_universal_installer",
        "//conditions:default": "",
    })

    native.genrule(
        name = name,
        srcs = srcs,
        outs = [out],
        cmd = cmd,
        target_compatible_with = _TARGET_COMPATIBLE_WITH,
        tools = [":build_msi"],
        visibility = ["//:__subpackages__"],
    )
