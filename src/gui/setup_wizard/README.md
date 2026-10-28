# gui/setup_wizard

First-run setup wizard (`mozc_tool --mode=setup_wizard`).

## .qtts files

We use `.qtts` for Qt translation files rather than `.ts` to avoid the confusion
with TypeScript files.

## .qm files

`.qm` files are binary data converted from the `.qtts` files. Since the build
process does not convert the files, you need to manually update `.qm` files when
you update `.qtts` files.

```shell
lrelease setup_wizard_en.qtts -qm setup_wizard_en.qm
lrelease setup_wizard_fr.qtts -qm setup_wizard_fr.qm
lrelease setup_wizard_ja.qtts -qm setup_wizard_ja.qm
```
