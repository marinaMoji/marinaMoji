# Single Kanji data

The files are passed to
[rewriter/gen_single_kanji_rewriter_data.py](../../rewriter/gen_single_kanji_rewriter_data.py)
as a part of [mozc_data](../../data_manager/mozc_data.bzl).

The generated data is used by
[rewriter/single_kanji_rewriter](../../rewriter/single_kanji_rewriter.h).

## Files

### single_kanji.tsv

Data for single kanji entries.

This file is passed to the `--single_kanji_file` flag of
[rewriter/gen_single_kanji_rewriter_data.py](../../rewriter/gen_single_kanji_rewriter_data.py)

### variant_rule.txt

Data for single kanji variants. (e.g. variant:髙 and base:高).

Update this file to add descriptions for single kanji characters
(e.g. 〜の旧字体).

This file is passed to the `--variant_file` flag of
[rewriter/gen_single_kanji_rewriter_data.py](../../rewriter/gen_single_kanji_rewriter_data.py)
