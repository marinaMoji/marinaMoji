# marinaMoji UI localization (EN / FR / JA)

UI language follows the **system locale** only (no in-app language picker).

## macOS

| Surface | Mechanism |
|---------|-----------|
| IME menu, floating toolbar | [`src/mac/Resources/*/Localizable.strings`](../src/mac/Resources/en.lproj/Localizable.strings) + `MarinaLocalizedString()` |
| Qt tools (Preferences, Dictionary, About, …) | Qt `.qm` files (`*_en`, `*_fr`, `*_ja`) |
| Installer ActivatePane | `ActivatePane/{English,Japanese,fr}.lproj/Localizable.strings` |
| Applications folder name in `.pkg` | `installer/Resources/{en,fr,ja}.lproj/Localizable.strings` |

**Test:** System Settings → General → Language → French (or Japanese). Restart apps after rebuild. Open marinaMoji menu, toolbar, Preferences.

## Linux (IBus)

Menu labels use [`message_translator.cc`](../src/unix/ibus/message_translator.cc) for `ja_JP.UTF-8` and `fr_FR.UTF-8`.

```bash
LC_ALL=fr_FR.UTF-8 ibus-daemon -drx   # example; then select marinaMoji
```

## Qt translation workflow

1. Edit UI source (`.ui`, `tr()` in `.cc`).
2. Update `.qtts`: `lupdate` (see [`src/gui/config_dialog/README.md`](../src/gui/config_dialog/README.md)).
3. Add or refresh French: `python3 src/gui/tools/generate_fr_qtts.py` (draft from English + glossary), then edit `*_fr.qtts` by hand as needed.
4. Compile: `lrelease foo_fr.qtts -qm foo_fr.qm`
5. Register `*_fr.qm` in the component’s `.qrc` and `BUILD.bazel`.

Commit both `.qtts` and `.qm` (Bazel does not run `lrelease` automatically).

## Adding a new Mac UI string

1. Add key to `Resources/en.lproj/Localizable.strings` (English value).
2. Mirror in `fr.lproj` and `ja.lproj`.
3. Call `MarinaLocalizedString(@"MM.YourKey")` in Objective-C++.

## Adding a new IBus menu string

1. Keep English text in `property_handler.cc` / menu code.
2. Add entries to `kUTF8JapaneseMap` and `kUTF8FrenchMap` in `message_translator.cc`.
3. Extend `message_translator_test.cc`.
