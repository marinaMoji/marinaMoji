# marinaMoji localization plan

This document turns the current localization setup into a practical roadmap.
The goal is to make marinaMoji solid for English, French, and Japanese first,
then reduce the cost of adding more languages later.

## Goals

- Keep the existing system-locale behavior.
- Finish and harden the current EN / FR / JA coverage.
- Make new strings cheaper to localize across all platforms.
- Leave room for future languages without a redesign.

## Canonical contract

The safest long-term approach is to standardize the semantic contract for every
user-facing string, while still letting each platform use its native resource
format.

### Key rules

- Prefix every shared key with `MM.`
- Use dot-separated namespaces for grouping
- Keep keys stable even if the English source text changes slightly
- Never use translated text as the key
- Avoid duplicate keys for the same meaning

### Placeholder rules

- Prefer named placeholders, such as `{productName}` or `{item}`
- Keep placeholder names identical across all locales
- Allow placeholder order to vary by language
- Define escaping rules once and apply them consistently

### Fallback behavior

| Situation | Behavior |
|-----------|----------|
| Requested locale has the key | Use the localized value |
| Requested locale is missing the key | Fall back to English |
| English is missing too | Show the raw key only in debug builds |
| Translation is incomplete | Surface it in QA / CI, do not hide it |

### Example

| Key | English | French | Japanese |
|-----|---------|--------|----------|
| `MM.About.Title` | `About {productName}` | `A propos de {productName}` | `{productName} ni tsuite` |
| `MM.Toolbar` | `Toolbar` | `Barre d'outils` | `tsu-ru baa` |
| `MM.DictionaryTool` | `Dictionary Tool...` | `Outil dictionnaire...` | `jisho tsu-ru...` |

### Platform mapping

- Qt keeps its `.qtts/.qm` flow, but the source strings should map cleanly to
  the shared key contract.
- macOS keeps native `Localizable.strings` files, with keys matching the shared
  contract.
- Linux/IBus should use a data-driven lookup table rather than hard-coded
  sentence-by-sentence branching.

## Current state

The repo already has a real localization pipeline:

- Qt UI files exist for `*_en`, `*_fr`, and `*_ja`.
- macOS resources already ship `en`, `fr`, and `ja` bundles.
- Linux/IBus menu translation is implemented for Japanese and French.
- French draft generation is already scripted for Qt translations.

The main gaps are:

- Some translation files still have unfinished or blank entries.
- Platform coverage is uneven between Qt, macOS, and Linux.
- Linux/IBus language support is explicit rather than data-driven.
- Generated `.qm` files must be kept in sync manually.

## Recommended rollout

### Phase 1: Establish the localization contract

Define the shared key format and placeholder rules first:

- one canonical key naming scheme
- identical shape across the language files
- consistent placeholder interpolation
- defined fallback behavior when a translation is missing

Treat this as the contract that all surfaces must follow.

### Phase 2: Sweep the codebase

Replace hard-coded user-facing text as the code is touched.

Focus the sweep on:

- Qt dialogs and tools
- macOS menu and toolbar strings
- macOS installer and uninstaller strings
- Linux/IBus menu strings

For each string, mark:

- source-only
- translated in EN / FR / JA
- missing
- unfinished

Deliverable:

- a growing migration map that shows what still needs to move onto the contract

### Phase 3: Finish the current languages

Bring the current languages to parity before adding more locales.

Focus areas:

- finish unfinished Qt translation entries
- confirm `.qm` files match their `.qtts` sources
- verify macOS `en/fr/ja` resources stay aligned
- check installer and uninstaller text coverage
- review layout overflow in translated dialogs and menus

Deliverable:

- a stable EN / FR / JA release-quality baseline

### Phase 4: Make localization extensible

Reduce the amount of code that knows about specific locales.

Good first refactors:

- make Linux/IBus menu translations table-driven instead of hard-coded to
  Japanese and French
- keep string keys stable across Qt and macOS surfaces
- standardize the workflow for updating source strings and translated files

Deliverable:

- adding one new locale should mostly be content work, not code work

### Phase 5: Add process and QA

Set a repeatable workflow so localization does not drift over time.

Suggested checklist for every new UI string:

- add the source string
- update the translation files
- regenerate binaries where needed
- review UI fit in each supported locale

Suggested quality checks:

- run the app in each supported locale
- open the main menu, toolbar, Preferences, Dictionary, and About surfaces
- verify installer and uninstaller text on macOS
- verify Linux/IBus menu labels under `fr_FR.UTF-8` and `ja_JP.UTF-8`

## File areas to touch

- `docs/LOCALIZATION.md`
- `src/gui/**/*.qtts`
- `src/gui/**/*.qm`
- `src/mac/Resources/*/Localizable.strings`
- `src/mac/installer/Resources/*/Localizable.strings`
- `src/unix/ibus/message_translator.cc`
- `src/unix/ibus/message_translator.h`

## Feasibility assessment

- EN / FR / JA completion: high
- one additional language: medium
- many additional languages: medium to high, but only with a better translation
  workflow and more review capacity

The biggest cost is not the text translation itself.
It is keeping all surfaces, generated assets, and layout behavior aligned over
time.

## Suggested next step

Start by defining the localization contract and the shared lookup behavior, then
sweep the codebase against that contract while you harden the current EN / FR /
JA coverage.
