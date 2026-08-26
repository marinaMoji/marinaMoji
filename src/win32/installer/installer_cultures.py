# -*- coding: utf-8 -*-
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


"""The set of cultures the marinaMoji installer is built for.

Shared by build_installer.py, which builds one MSI per culture, and
embed_transforms.py, which folds them into a single multilingual MSI, so the
two cannot disagree about which cultures exist or what LCID each one is.

Codepage is deliberately 65001 (UTF-8) for every culture, including ja-JP,
which upstream Mozc built as 932. The multilingual MSI holds all three
languages' strings in one database string pool, so that pool has to encode
French accents and Japanese kana at once; no single ANSI codepage does.
Windows Installer has supported UTF-8 databases for far longer than the
Windows 10 1809 floor this package already enforces.
"""

# culture -> (LCID, database and summary-information codepage).
CULTURES = {
    'en-US': (1033, 65001),
    'fr-FR': (1036, 65001),
    'ja-JP': (1041, 65001),
}

# The culture the multilingual MSI is built from: its strings are the ones
# left in the database itself, and every other culture is applied over it as
# an embedded language transform.
BASE_CULTURE = 'en-US'
