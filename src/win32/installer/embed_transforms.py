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


"""Folds the per-culture MSIs into one multilingual MSI.

build_installer.py produces one MSI per culture, identical except for the
five installer strings and the declared language. BUILD.bazel names them
here, each keyed by its LCID. This script turns the extra
cultures into MSI language transforms embedded in the base (en-US) package,
so a single .msi picks its language from the machine at install time.

Everything here goes through msi.dll directly rather than through msidb.exe
or WiSubStg.vbs: msi.dll is on every Windows machine, whereas the Windows SDK
tools are an extra build dependency and the SDK's VBScript samples are no
longer shipped. It is Windows-only by nature and cannot run on the host that
builds the rest of marinaMoji on macOS or Linux.
"""

import argparse
import ctypes
import pathlib
import shutil
import tempfile

# A language transform for this package carries five strings and a summary
# stream: a few KB. If one comes back large, the two MSIs differed in their
# embedded cab as well -- WiX did not produce a byte-identical cab from
# identical inputs -- and the transform is dragging a whole second copy of the
# payload with it. Better to fail the build than to ship a package several
# times the intended size.
_MAX_TRANSFORM_BYTES = 512 * 1024

# Windows Installer database open modes (MsiOpenDatabase szPersist values,
# passed as integers cast to LPCWSTR).
_MSIDBOPEN_READONLY = 0
_MSIDBOPEN_TRANSACT = 1

# Summary information property IDs and types.
_PID_TEMPLATE = 7
_VT_LPSTR = 30


# MSIHANDLE is `typedef unsigned long MSIHANDLE`, i.e. 32 bits wide on both
# x86 and x64 -- it is a table index, not a pointer. Declaring it explicitly
# keeps the out-parameters the right size instead of relying on the upper
# half of a c_void_p happening to stay zero.
MSIHANDLE = ctypes.c_uint32


def _msi():
  """Returns the msi.dll binding, or raises on a non-Windows host."""
  try:
    library = ctypes.WinDLL('msi.dll')
  except AttributeError as e:
    raise RuntimeError(
        'embed_transforms.py needs msi.dll and can only run on Windows.'
    ) from e
  # Returns a handle, not the default int, so it must be declared.
  library.MsiCreateRecord.restype = MSIHANDLE
  return library


def _check(rc: int, what: str) -> None:
  if rc != 0:
    raise ChildProcessError(f'{what} failed with Windows Installer error {rc}.')


def _open_database(msi, path: pathlib.Path, mode: int) -> MSIHANDLE:
  handle = MSIHANDLE()
  _check(
      msi.MsiOpenDatabaseW(
          ctypes.c_wchar_p(str(path)),
          ctypes.cast(ctypes.c_void_p(mode), ctypes.c_wchar_p),
          ctypes.byref(handle),
      ),
      f'MsiOpenDatabaseW({path.name})',
  )
  return handle


def generate_transform(
    msi, base: pathlib.Path, localized: pathlib.Path, out: pathlib.Path
) -> None:
  """Writes the transform that turns `base` into `localized`."""
  h_base = _open_database(msi, base, _MSIDBOPEN_READONLY)
  h_localized = _open_database(msi, localized, _MSIDBOPEN_READONLY)
  try:
    # The reference database is the one the transform gets applied to, so the
    # localized database is the "current" side and the base is the reference.
    _check(
        msi.MsiDatabaseGenerateTransformW(
            h_localized, h_base, ctypes.c_wchar_p(str(out)), 0, 0
        ),
        f'MsiDatabaseGenerateTransformW({out.name})',
    )
    # A transform without summary information cannot be applied. Both
    # validation arguments are zero: a language transform legitimately changes
    # the package language, so validating language or product code against the
    # base would reject it.
    _check(
        msi.MsiCreateTransformSummaryInfoW(
            h_localized, h_base, ctypes.c_wchar_p(str(out)), 0, 0
        ),
        f'MsiCreateTransformSummaryInfoW({out.name})',
    )
  finally:
    msi.MsiCloseHandle(h_localized)
    msi.MsiCloseHandle(h_base)

  size = out.stat().st_size
  if size > _MAX_TRANSFORM_BYTES:
    raise ChildProcessError(
        f'{out.name} is {size} bytes, over the {_MAX_TRANSFORM_BYTES} byte'
        ' ceiling. A language transform should hold a handful of strings, so'
        ' this almost certainly means the per-culture MSIs embedded'
        ' byte-different cabs and the transform now carries a second copy of'
        ' the payload. Fix the cab difference rather than raising the ceiling.'
    )


def embed_transform(
    msi, package: pathlib.Path, transform: pathlib.Path, lcid: int
) -> None:
  """Stores `transform` in `package` under the name Windows Installer looks
  up for `lcid`."""
  h_db = _open_database(msi, package, _MSIDBOPEN_TRANSACT)
  try:
    for sql, set_stream in (
        (f'DELETE FROM `_Storages` WHERE `Name` = \'{lcid}\'', False),
        ('INSERT INTO `_Storages` (`Name`, `Data`) VALUES (?, ?)', True),
    ):
      h_view = MSIHANDLE()
      _check(
          msi.MsiDatabaseOpenViewW(
              h_db, ctypes.c_wchar_p(sql), ctypes.byref(h_view)
          ),
          f'MsiDatabaseOpenViewW({sql})',
      )
      try:
        h_rec = MSIHANDLE()
        if set_stream:
          h_rec = MSIHANDLE(msi.MsiCreateRecord(2))
          _check(
              msi.MsiRecordSetStringW(h_rec, 1, ctypes.c_wchar_p(str(lcid))),
              'MsiRecordSetStringW',
          )
          _check(
              msi.MsiRecordSetStreamW(
                  h_rec, 2, ctypes.c_wchar_p(str(transform))
              ),
              'MsiRecordSetStreamW',
          )
        _check(msi.MsiViewExecute(h_view, h_rec), f'MsiViewExecute({sql})')
        if set_stream:
          msi.MsiCloseHandle(h_rec)
      finally:
        msi.MsiViewClose(h_view)
        msi.MsiCloseHandle(h_view)
    _check(msi.MsiDatabaseCommit(h_db), 'MsiDatabaseCommit')
  finally:
    msi.MsiCloseHandle(h_db)


def declare_languages(msi, package: pathlib.Path, lcids: list[int]) -> None:
  """Lists `lcids` in the summary Template, which is how Windows Installer
  learns the package has embedded language transforms to choose between."""
  h_sum = MSIHANDLE()
  _check(
      msi.MsiGetSummaryInformationW(
          None, ctypes.c_wchar_p(str(package)), 1, ctypes.byref(h_sum)
      ),
      'MsiGetSummaryInformationW',
  )
  try:
    # Template is "<platform>;<language list>"; the platform half has to
    # survive, so read the current value and replace only the languages.
    data_type = ctypes.c_uint()
    value = ctypes.c_int()
    buf = ctypes.create_unicode_buffer(512)
    size = ctypes.c_uint(len(buf))
    _check(
        msi.MsiSummaryInfoGetPropertyW(
            h_sum,
            _PID_TEMPLATE,
            ctypes.byref(data_type),
            ctypes.byref(value),
            None,
            buf,
            ctypes.byref(size),
        ),
        'MsiSummaryInfoGetPropertyW(Template)',
    )
    platform = buf.value.split(';')[0]
    template = platform + ';' + ','.join(str(lcid) for lcid in lcids)
    _check(
        msi.MsiSummaryInfoSetPropertyW(
            h_sum, _PID_TEMPLATE, _VT_LPSTR, 0, None, ctypes.c_wchar_p(template)
        ),
        'MsiSummaryInfoSetPropertyW(Template)',
    )
    _check(msi.MsiSummaryInfoPersist(h_sum), 'MsiSummaryInfoPersist')
  finally:
    msi.MsiCloseHandle(h_sum)
  print(f'Template set to "{template}".')


def main() -> None:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--output', type=str, required=True)
  parser.add_argument(
      '--base',
      type=str,
      required=True,
      help=(
          'The MSI whose strings stay in the database itself. Every other'
          ' culture is applied over it as an embedded language transform.'
      ),
  )
  parser.add_argument(
      '--base_language',
      type=int,
      required=True,
      help='LCID of --base, e.g. 1033. Listed first in the summary Template.',
  )
  # The culture set lives in win32/installer/BUILD.bazel, which passes each
  # non-base culture's LCID and MSI here. See build_installer.py for why the
  # table is not a shared Python module.
  parser.add_argument(
      '--transform',
      action='append',
      default=[],
      required=True,
      metavar='LCID=PATH',
      help=(
          'A per-culture MSI to fold in as a language transform, keyed by its'
          ' LCID. Repeat once per culture beyond the base.'
      ),
  )
  args = parser.parse_args()

  transforms = {}
  for pair in args.transform:
    lcid_text, separator, path = pair.partition('=')
    if not separator or not lcid_text.isdigit():
      raise ValueError(f'--transform expects LCID=PATH, got {pair!r}.')
    lcid = int(lcid_text)
    if lcid == args.base_language:
      raise ValueError(
          f'--transform {lcid} is the base language; it is already the'
          ' database itself and must not also be a transform.'
      )
    transforms[lcid] = pathlib.Path(path).resolve()

  base = pathlib.Path(args.base).resolve()
  output = pathlib.Path(args.output).resolve()
  msi = _msi()

  # Work on the copy from the start, so a failure part-way through cannot
  # leave a half-transformed package behind under the output name.
  shutil.copyfile(base, output)

  with tempfile.TemporaryDirectory() as tmp:
    for lcid in sorted(transforms):
      transform = pathlib.Path(tmp).joinpath(f'{lcid}.mst')
      generate_transform(msi, base, transforms[lcid], transform)
      embed_transform(msi, output, transform, lcid)
      print(f'Embedded {lcid}, {transform.stat().st_size} bytes.')

  # Base language first: Windows Installer falls back to it when the machine
  # language matches no transform.
  declare_languages(msi, output, [args.base_language] + sorted(transforms))


if __name__ == '__main__':
  main()
