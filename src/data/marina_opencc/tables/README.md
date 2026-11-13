These CSVs are normally an **imported copy** of `character_conversion`'s
`tables_output/` — edit shin/kyu pairs there (it has the fuller tooling),
then run `bash import_opencc_tables.sh` from `marinaMozc/src` to pull the
change in and rebuild the dictionaries.

If you need to hand-edit a CSV here directly instead (e.g. a quick one-off
fix), run `bash export_opencc_tables.sh` afterward to push it back to
`character_conversion` — otherwise the next `import_opencc_tables.sh` run
will silently overwrite your edit with the stale upstream version, and
`character_conversion`'s own copy (which also feeds the Rime-based
marinaMoji IME) will be left out of sync.

See [`docs/OPENCC_INTEGRATION.md`](../../../../docs/OPENCC_INTEGRATION.md).

## Sources and licences

The pairs in these tables were collected by the `character_conversion` project
from several sources, and the source tags in `char_complete_shin_kyu_table_manual.csv`
record which ones attest each character pair (`jmdict`, `kd2`, `mozc`):

- **OpenCC** (Apache-2.0).
- **Mozc** dictionary data (BSD-3-Clause).
- **KANJIDIC2 and JMdict** (`kd2`, `jmdict`), from the Electronic Dictionary
  Research and Development Group, **CC BY-SA 4.0**
  (<https://www.edrdg.org/edrdg/licence.html>). Of the 500 character rows, 112
  have no Mozc attestation and 16 rest only on KANJIDIC2 or JMdict. Many of the
  phrase pairs were confirmed against JMdict headwords.

The tables contain only character and word pairs, no definitions or readings,
but EDRDG's licence covers "data files which are derived from" its files, so we
treat these tables as CC BY-SA 4.0 data (ShareAlike: they cannot be relicensed
more permissively). They are read as data at run time and are not linked into
any binary. The acknowledgement the licence asks for is in the About dialog,
`src/data/installer/credits_en.html` and both copyright files.

Still to audit: the `character_conversion` pipeline also mines Wiktionary
(`kaikki`), CC-CEDICT, the MJ shrink map and UniDic. Wiktionary, CC-CEDICT and
the MJ data are ShareAlike licences too. How much of each reached these tables
has not been checked.
