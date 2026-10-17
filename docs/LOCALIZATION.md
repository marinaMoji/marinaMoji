# marinaMoji UI localization (EN / FR / JA)

UI language follows the **system locale** only (no in-app language picker).

## macOS

| Surface | Mechanism |
|---------|-----------|
| Input-source name in menu bar / System Settings | [`src/mac/{English,French,Japanese}.lproj/InfoPlist.strings`](../src/mac/English.lproj/InfoPlist.strings) (`com.apple.inputmethod.Japanese` → `marinaMoji`). French is required; without it macOS shows the raw ID and may hide the IME. |
| IME menu, floating toolbar | [`src/mac/Resources/*/Localizable.strings`](../src/mac/Resources/en.lproj/Localizable.strings) + `MarinaLocalizedString()` |
| Qt tools (Preferences, Dictionary, About, …) | Qt `.qm` files (`*_en`, `*_fr`, `*_ja`) |
| Applications folder name in `.pkg` | `installer/Resources/{en,fr,ja}.lproj/Localizable.strings` |

**Test:** System Settings → General → Language → French (or Japanese). Restart apps after rebuild. Open marinaMoji menu, toolbar, Preferences.

## Linux (IBus)

Menu labels use [`message_translator.cc`](../src/unix/ibus/message_translator.cc) for `ja_JP.UTF-8` and `fr_FR.UTF-8`.

The floating toolbar, Symbols Palette and Keyboard Shortcuts window use [`marina_localized_string.cc`](../src/unix/ibus/marina_localized_string.cc): the same `MM.*` keys and wordings as mac, in a table with en/fr/ja columns. The language comes from `LANGUAGE`, `LC_ALL`, `LC_MESSAGES` or `LANG` (first of `ja`/`fr`/`en`); anything else is English.

```bash
LC_ALL=fr_FR.UTF-8 ibus-daemon -drx   # example; then select marinaMoji
```

To add a toolbar string: add a row to `kEntries` in `marina_localized_string.cc` (reuse the mac key if one exists), add the key to `marina_localized_string_test.cc`, and call `MarinaLocalizedString("MM.YourKey")` in `mozc_toolbar.cc`.

## Qt translation workflow

1. Edit UI source (`.ui`, `tr()` in `.cc`).
2. Update `.qtts`: `lupdate` (see [`src/gui/config_dialog/README.md`](../src/gui/config_dialog/README.md)).
3. Add or refresh French: `python3 src/gui/tools/generate_fr_qtts.py` (draft from English + [`en_fr_glossary.json`](../src/gui/tools/en_fr_glossary.json)), then edit `*_fr.qtts` by hand as needed.
4. When you settle on a French wording (e.g. `Startup` → `Démarrage`), put the whole-string pair in `en_fr_glossary.json` so the next regenerate keeps it.
5. Compile: `lrelease foo_fr.qtts -qm foo_fr.qm` (and the matching `*_ja.qm` / `*_en.qm` if those `.qtts` changed).
6. Register `*_fr.qm` in the component’s `.qrc` and `BUILD.bazel` (already done for EN / FR / JA on Preferences).

Commit both `.qtts` and `.qm` (Bazel does not run `lrelease` automatically).

**Japanese Preferences:** `config_dialog_ja.qtts` / `.qm` are already wired the same way as French (`InstallTranslator("config_dialog")` + `QLocale::system()`). Empty JA entries are intentional for punctuation and brand names (ATOK, MS-IME, etc.).

## Adding a new Mac UI string

1. Add key to `Resources/en.lproj/Localizable.strings` (English value).
2. Mirror in `fr.lproj` and `ja.lproj`.
3. Call `MarinaLocalizedString(@"MM.YourKey")` in Objective-C++.

## Adding a new IBus menu string

1. Keep English text in `property_handler.cc` / menu code.
2. Add entries to `kUTF8JapaneseMap` and `kUTF8FrenchMap` in `message_translator.cc`.
3. Extend `message_translator_test.cc`.
