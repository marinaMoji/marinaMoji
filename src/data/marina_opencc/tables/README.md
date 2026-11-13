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

The `character_conversion` project builds these tables. Where the pairs come from:

- **Characters** (`char_complete_shin_kyu_table_manual.csv`, about 500 rows):
  `jp-old-style.txt` from cjkvi-variants (MIT, declared by the kanji-database
  project: <https://kanji-database.sourceforge.net/>); the 新旧字体表 on the
  CyberLibrarian site (<https://www.asahi-net.or.jp/~ax2s-kmtn/ref/old_chara.html>),
  a list of the 常用漢字表 and 人名用漢字 old forms that states no licence; and
  a short 表外 (hyōgai) list. Reviewed and corrected by hand.
- **Phrases** (`phrases_substitution_table_final.csv`, about 980 rows): the
  Bunkachō / Kokugo Shingikai 「同音の漢字による書きかえ」 (1956) list and the
  代用字 list at <https://sound.jp/mtnest/k/tojo-daiyo-.htm>, which also states no
  licence. Reviewed by hand.
- **OpenCC** (Apache-2.0): the configuration (`marinaShin2Kyu.json`) and the
  three-layer table layout.

The source tags in the characters CSV (`jmdict`, `kd2`, `mozc`) only record in
which dictionaries each form is *attested*: they were added after the pairs were
collected, by exact string match against Mozc, JMdict and KANJIDIC2, and they
guided the manual review. No pair, definition or reading was copied from
KANJIDIC2 or JMdict. We credit the Electronic Dictionary Research and
Development Group (<https://www.edrdg.org/edrdg/licence.html>) anyway, in the
About dialog and `credits_en.html`.

Open points:

- The two web pages above state no licence. They compile official Japanese
  lists (the 常用漢字表 and the 1956 Kokugo Shingikai report) and the pairs are
  facts, so the risk looks low, but we have not asked their authors.
- The same pipeline mines Wiktionary (`kaikki`), CC-CEDICT, the MJ shrink map and
  UniDic. Their outputs are **not** read by `generate_opencc_tables.py`, which
  takes only the three CSVs in this directory, so nothing from them is in these
  tables. Keep it that way: those sources are ShareAlike.
