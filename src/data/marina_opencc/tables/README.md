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
