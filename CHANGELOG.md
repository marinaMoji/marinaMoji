# Changelog

Notable marinaMoji-specific changes, kept by hand. This is not upstream
Mozc's release history (see [`docs/release_history.md`](docs/release_history.md)
for that) — it tracks work specific to this fork, most usefully the parts
that touch more than one file or aren't obvious from a commit subject line.

Format: newest entry first, grouped by date. Each entry should say what
changed and, where it isn't obvious, why.

## Unreleased

### Windows CI: wrong namespace on WinUtil::DecodeWindowHandle (2026-08-27)

The Windows build broke on master (run 33020383634, both x64 and arm64) with
`'mozc::WinUtil' is not a class, namespace, or enumeration` from
`shortcuts_window.cc`. `DecodeWindowHandle` is a static member of
`mozc::WinUtil` in `base/win32/win_util.h`, but two of the windows added in
"Big Windows batch" qualified it as `::mozc::win32::WinUtil` — the namespace
that `base/win32/wide_char.h`'s `Utf8ToWide` / `WideToUtf8` live in, which is
probably where the habit came from. Fixed in both `shortcuts_window.cc` and
`symbols_palette_window.cc`; only the first had been reached before Bazel
aborted, so the second would have failed the next run. The BUILD deps were
already correct.

Note the build stopped at the first error, so targets after it were never
compiled — a green run is the only real confirmation there was nothing else.

### Drop shadows for the frameless popup windows (2026-08-27)

Both popups are frameless (`Qt::ToolTip | Qt::FramelessWindowHint`), so no
window manager decorates them and they sat on whatever was behind them with
no visible edge. Qt clips a `QGraphicsDropShadowEffect` to its top-level
widget's bounds, so a frameless popup cannot cast one itself; it has to sit
inside a slightly larger translucent parent for the shadow to spill into.
`CreateShadowFrame()` in
[`qt_window_manager.cc`](src/renderer/qt/qt_window_manager.cc) builds that
parent, and geometry now goes through `MoveContent` / `ResizeContent` /
`ContentRect`, which all speak in terms of the *content* rect. The
positioning maths and the rect reported back to the IME are unchanged — the
12px margin never leaks into either.

The frame deliberately has **no layout**. The first attempt used a
`QVBoxLayout` with contents margins, which imposed the table's
`minimumSizeHint()` — 90x90 for a `QAbstractScrollArea` — on the frame. A
top-level `resize()` was never clamped that way, so short candidate lists
(a single row is ~24px tall) would have been silently inflated to 90px.
Positioning the child by hand keeps the requested size exact. This was
caught by modelling the call sequence in PySide6 and comparing old against
new across a range of sizes: 180x60 came back as 180x90 and 64x24 as 90x90
under the layout, and exact without it.

Translucency needs a compositor. Wayland and GNOME/X11 always composite; on
bare X11 with no compositing manager the margin degrades to an unpainted
border rather than breaking the popup.

On Windows the candidate and infolist windows already carried
`CS_DROPSHADOW`, and the symbols palette and shortcuts window have real
chrome that DWM shadows. The toolbar had none: it is `WS_EX_LAYERED`, which
`CS_DROPSHADOW` does not apply to, and DWM does not decorate a caption-less
`WS_POPUP`. It now gets one from a separate click-through window,
[`shadow_window.{h,cc}`](src/renderer/win32/shadow_window.h), stacked
immediately behind it.

The alternative — inflating the toolbar's own bitmap and drawing the shadow
into the margin — was rejected because it would move the origin of every
button rect, and with it hit testing, dragging, screen-edge clamping and the
MSAA rects in `toolbar_accessible.cc`. A separate window leaves all of that
untouched; `WS_EX_TRANSPARENT` keeps it out of hit testing entirely. It is
driven from `ToolbarWindow::UpdateShadow()`, called from `Redraw()`, from
the first show, and from `WM_WINDOWPOSCHANGED` so it stays glued during a
drag (the system moves the toolbar itself, so no redraw happens until
`WM_EXITSIZEMOVE`). The bitmap is rebuilt only when size, radius or DPI
scale changed, so a hover repaint costs one `SetWindowPos`. Restacking the
shadow behind the toolbar can send the toolbar its own
`WM_WINDOWPOSCHANGED`, so `updating_shadow_` guards against recursing.

The shadow is a signed-distance rounded rect with a quadratic falloff rather
than a real Gaussian convolution — visually equivalent for a solid rounded
rect, at one `sqrt` per pixel.

The Windows sync overlay is deliberately left alone. It is uniformly
translucent (`SetLayeredWindowAttributes`), so a shadow behind it would show
*through* it.

On macOS the renderer panels are `NSPanel`s with `hasShadow` left at its
default of YES and nothing turning it off, so they should already have one.

None of the Windows code here has been compiled or run; it was written on
macOS. The shadow's appearance was checked by rendering the same constants
and formulas offscreen, but the window plumbing — z-order, the drag path,
DPI changes, and that clicks still reach the toolbar — needs a Windows test.

### Settings: Advanced tab was vertically centred, not top-aligned (2026-08-27)

The Advanced tab (`inputSupportTab`, "Avancé") was the only one of the six
`.ui` tabs whose `QVBoxLayout` had no trailing expanding spacer. With
nothing in the layout able to absorb the leftover height, Qt spreads it
around the items instead, so the two setting groups drifted apart and away
from the top of the page as the dialog grew. Added the same
`Minimum`/`Expanding` spacer the other five tabs already end with.

Measured before and after by loading `config_dialog.ui` through PySide6 with
the dialog's real stylesheet and the Linux visibility rules applied, at the
same size as the reported screenshot. Advanced went from first item at
y=134 with a 124px inter-group gap to y=11 with a 1px gap; the other five
tabs were already at y=11 and were unchanged. The two tabs built in code,
Shortcuts and Sync, both end in `addStretch()` and were never affected.

### Remove Ctrl+Alt+vowel macron shortcuts (2026-08-27)

The default way to type ā ē ī ō ū is **Right Shift tap, then a vowel** in
Direct input, on Windows, macOS, and Linux. `Ctrl+Alt`+vowel (and the
`Ctrl+RightAlt` / `Ctrl+AltGr` aliases, plus Shift for capitals) is no longer
bound: that chord collided with desktop shortcuts and with AZERTY `AltGr`
(which Windows reports as Ctrl+Alt).

Removed from every default keymap TSV (`ms-ime`, `atok`, `kotoeri`,
`chromeos`, `mobile`). Dropped the client hacks that existed only for the
chord: macOS `tryMacronVowelChord` / `macron_shift` in
`mac/mozc_imk_input_controller.mm` and `mac/KeyCodeMap.mm`; the Windows
uppercase `key_code` fixup in `win32/base/keyevent_handler.cc`; IBus
Ctrl+Alt+Hiragana → `a` in `unix/ibus/key_translator.cc`. Tests that pinned
those hacks are gone.

Unchanged: **`Ctrl+Alt+Right Shift`** on Windows (Left Shift mode lock);
**AltGr+¨** / **AltGr+^** as a layout dead key. ASCII composition still does
not arm Right Shift — switch to Direct input (or use AltGr+¨) for macrons
there.

Docs: `docs/MACRON_VOWELS.md`; ShareDocs `WINDOWS_TESTING.md`,
`MACOS_TESTING.md`, `Testing_Checklist_WP7.md`, `Keybinding_notes.md`,
`Fonctions.md`.

### Windows: multilingual MSI (en-US / fr-FR / ja-JP) (2026-08-27)

`marinaMoji64.msi` is now a single package that installs in English, French
or Japanese according to the machine's language. It had been English-only
since 2026-08-08, when the four user-visible installer strings inherited
from upstream Mozc were translated out of Japanese and `<Package>` moved
from `1041`/`932` to `1033`/`1252`; that entry noted a genuinely
multilingual installer as a larger change, and this is it.

**Strings.** The four messages — the Windows-version gate, the
administrator-privileges gate, the ARM64 gate, and the
newer-version-installed error — plus the summary-information description now
come from [`win32/installer/loc/*.wxl`](src/win32/installer/loc/) via
`!(loc.*)`, with `<Package>`'s `Language`/`Codepage` from
`$(var.InstallerLanguage)` / `$(var.InstallerCodepage)`. Those defines have
no fallback, so a culture whose `.wxl` is missing fails the build instead of
producing a mislabelled MSI.
[`installer_cultures.py`](src/win32/installer/installer_cultures.py) holds
the one culture table that `build_installer.py` and `embed_transforms.py`
both read, so they cannot disagree about which cultures exist.

**Packaging.** `build_installer.py` gained `--culture` and `--loc_file` and
runs once per culture, producing `marinaMoji64.<culture>.msi`. Those are
intermediates — individually buildable for debugging, but not what ships.
[`embed_transforms.py`](src/win32/installer/embed_transforms.py) then diffs
the fr-FR and ja-JP databases against the en-US one with
`MsiDatabaseGenerateTransformW`, stores each result in the en-US database's
`_Storages` table under its LCID, and rewrites the summary `Template` to
`<platform>;1033,1036,1041`, which is what tells Windows Installer there are
language transforms to choose between. It drives msi.dll through ctypes
rather than shelling out to `msidb.exe` or the SDK's `WiSubStg.vbs`: msi.dll
is on every Windows machine, the SDK tools would be a new build dependency,
and the VBScript samples are no longer shipped at all. Both validation
arguments to `MsiCreateTransformSummaryInfoW` are zero — a language
transform legitimately changes the package language, so validating it
against the base would reject it.

The genrule body moved to
[`installer.bzl`](src/win32/installer/installer.bzl) because BUILD files
cannot define functions, and `//win32/installer` is a filegroup over the
combined package. The output path is unchanged, so `//:package`, the CI
artifact paths, the release asset name and the docs all keep working; the
Windows CI workflow is byte-identical to before this work, and the release
workflow differs only by a comment. The upstream Mozc and
GoogleJapaneseInput brandings pass no culture and still build a single
unlocalized MSI from their own .wxs.

**Two consequences worth knowing about.**

*ProductCode is derived, not minted per build.* A language transform may not
change the ProductCode, so all three cultures of a version must share one,
ruling out the fresh GUID WiX generates by default. It is now
`uuid5(namespace, branding-version-arch)`. That preserves what upstream
relied on — the ProductCode changes with the version, so upgrades do not
trip "Another version of this product is already installed" — and adds
reproducibility: rebuilding a version yields the same ProductCode rather
than a new one.

*Codepage is 65001 everywhere,* including ja-JP, which was 932. A single
database string pool has to hold French accents and Japanese kana at once,
and no ANSI codepage does. UTF-8 MSI support long predates the Windows 10
1809 floor this package already enforces.

`embed_transforms.py` also refuses to embed a transform over 512 KB. One
should be a few KB here; a large one would mean WiX did not produce
byte-identical cabs across the three passes and the transform is carrying a
second copy of the payload. Failing the build beats shipping a package
several times the intended size.

**Untested on Windows.** None of the msi.dll code has executed — it cannot
run on the macOS host the rest of this was written on. What was checked
offline: ProductCode identical across cultures and distinct per
version/arch, the culture tables and `.wxl` files agreeing on LCID and
codepage, every `!(loc.*)` and `$(var.*)` resolving, and the
argument-validation paths. Still to check on the first Windows build, in
order: that transform sizes come back small, that `Template` reads
`x64;1033,1036,1041`, that **upgrading over an already-installed version
still works** (the ProductCode change lands here), and that a non-elevated
run on a French or Japanese machine shows a translated message.

**Scope.** This only ever covered the installer's own strings, all of which
are error-path: a successful install looks the same in every language. The
progress and uninstall chrome is drawn by msiexec in the OS UI language, and
the cache service's display name and description come from the binary's
string resources (`@[#marinamoji_cache_service.exe],-100`/`-101`), so
neither was the MSI's to localize. The installed credits file is still
`credits_en.html` in all three languages.

### Windows: Left Shift double tap looked like it needed a third tap (2026-08-26)

On Windows the mode lock appeared to engage only on the *third* Left
Shift tap: tap 1 flipped Hiragana → Direct, tap 2 flipped back with no
padlock, tap 3 showed the padlock. The gesture itself was fine all
along — the lock engages on tap 2 as designed, and the toolbar was
drawing the wrong flag.

`left_shift_direct_lock` only ever exists on `output.status()`, where
`Session::OutputMode` sets it. But `ToolbarWindow::OnUpdate` prefers
`application_info().indicator_info().status()` whenever it is present
(deliberately, for mode/activated: the TIP's `TipInputModeManager` is
the authoritative state there and renderer output can lag during focus
transitions). That Status is built field by field in
[`tip_ui_handler_conventional.cc`](src/win32/tip/tip_ui_handler_conventional.cc)
-- `activated` and `mode`, nothing else — so the lock flag read back as
its proto default `false`.

The timing is what disguised it as a lost keystroke: `indicator_info`
is attached exactly when `IsIndicatorVisible()`, i.e. just after a mode
change. Tap 2 changes the mode *and* engages the lock, so that update
carried `indicator_info` and the toolbar drew the unlocked icon. Tap 3
changes nothing (`ToggleLeftShiftDirect` early-returns once locked), the
indicator has gone, the toolbar falls back to `output.status()` — and
the padlock finally appears.

`ToolbarWindow::OnUpdate` now reads the lock from `output.status()`
only, never from whichever Status supplied mode/activated, and keeps it
sticky when an update carries no status at all (the same pattern as
`use_traditional_kanji_` beside it). The TIP also copies the flag into
`indicator_info.status()` so the two sources agree and the next reader
of `indicator_info` does not fall into the same partial-Status trap.

Ruled out first, with a test rather than by reading: `session_test.cc`
gained `LeftShiftDoubleTapLocksWithWindowsClientKeyShape`, which replays
the Windows key shape through the engine — every tap as a *key-up*
whose `KeyEvent` carries the client's cached `mode`/`activated`,
`TestSendKey` before each `SendKey`, each reply's status fed into the
next event — and two taps lock. The pre-existing tests sent a bare
modifier-only event, which macOS and Linux do but the TSF client never
does, so the mode-dependent early returns in `ToggleLeftShiftDirect` and
`ToggleLeftShiftModeLock` had no Windows-shaped coverage. Neither Win32
change is compiled here (macOS tree); the session test passes.

### Windows: taskbar mode indicator returned after a Deactivate/Activate cycle (2026-08-26)

The fix from "Actually hide taskbar mode icon by removing it from the
langbar" worked only until TSF cycled the text service. `TipLangBar`
started `mode_icon_registered_` at `true`, `UninitLangBar()` never reset
it, and `InitLangBar()` re-added both input-mode items unconditionally.
TSF calls `Deactivate()` and `ActivateEx()` repeatedly on the *same*
text service object — switching keyboard layouts and back is enough --
so after the first cycle the flag read `false` while the items had just
been re-registered. `UpdateMenu()` then saw its state as already matching
the preference, removed nothing, and the mode icon stayed in the taskbar
regardless of the toolbar setting.

Registration is now derived in one place, `SyncModeIconRegistration()`,
which reconciles the two items against `LoadToolbarVisiblePreference()`.
`InitLangBar()` only *creates* them and calls the helper, so when the
toolbar is visible they are never added in the first place (no window
where the icon flashes up between activation and the first focus
change); `UninitLangBar()` clears the flag, which now starts `false`
since nothing is registered before init. Unverified from macOS.

Still open if the icon persists: the Windows 10/11 tray indicator may
not be the langbar item at all but the shell's own input indicator
reading the TSF open/close and conversion-mode compartments, in which
case no `ITfLangBarItemMgr` juggling can hide it.

### Windows toolbar: Shortcuts window opened behind the focused app (2026-08-26)

The toolbar's **Shortcuts** button looked dead on Windows for the same
reason it did on macOS, expressed in Win32 terms.
`ShortcutsWindowTraits` was `WS_OVERLAPPEDWINDOW, WS_EX_TOOLWINDOW` —
no `WS_EX_TOPMOST` — while `ShortcutsWindow::OnUpdate` shows the window
with `ShowWindow(SW_SHOWNA)` (show without activating). The renderer is
not the foreground process, so the window was created and shown
*underneath* the application being typed into: no error, nothing
visible.

The Symbols Palette already documents this trap in
[`symbols_palette_window.h`](src/renderer/win32/symbols_palette_window.h)
("The toolbar is topmost, so the palette must be as well. Otherwise
`SW_SHOWNA` succeeds but the palette is immediately covered by the
focused application") — the shortcuts window was the one window that
had not been given the same treatment. It is now `WS_EX_TOPMOST` too.
`WS_OVERLAPPEDWINDOW` and `SW_SHOWNA` are kept, so it stays resizable
and focusable but does not steal focus from the text field until the
user clicks it. Not yet built or run: this is Win32/ATL, unverified
from a macOS tree.

### Windows toolbar: wordmark rendered soft at every DPI (2026-08-26)

The long marinaMoji logo looked noticeably less crisp than the six
square icons beside it. The generated logo PNGs are one pixel too tall
— `logo_long_light_24.png` is **108x25**, not 108x24 (likewise 162x37
and 216x49) — because `resvg` honours the requested *width* and derives
the height from the aspect ratio, rounding up (108 / 4.4901 = 24.05 ->
25).

`ToolbarWindow` fits the logo to the icon height, so it computes a draw
size of `round(108 * 24 / 25) x 24 = 104x24`, which differs from the
asset's `108x25` and therefore trips `BlendIcon`'s resample path: a
0.96x bilinear pass over letterforms whose stems are about a pixel
wide, at *every* DPI including 100%. The square icons are `24x24` drawn
at 24 and are blitted 1:1, which is why only the wordmark looked soft.

[`generate_toolbar_icons.py`](src/data/images/win/generate_toolbar_icons.py)
now renders the logo at 4x and does one Lanczos downsample to exactly
`(width, size)`, so the shipped PNG is exactly the tier height whatever
resvg rounds to; `verify_pngs()` asserts the dimensions (square icons
exactly `size x size`, logo exactly `size` tall) so this cannot regress
silently. **The PNGs themselves have not been regenerated** — that needs
`pip install resvg-py pillow` and a run of the script.

Not addressed: at 125% and 175% scaling everything is resampled anyway,
because `kIconSizeTiers` is {24, 36, 48} while `icon_draw_size_` becomes
30 or 42. Adding 30 and 42 tiers would make the two most common Windows
laptop scale factors 1:1, at the cost of two more PNGs per icon and a
full asset regeneration.

### macOS toolbar: Shortcuts button opened a window nobody could see (2026-08-26)

The toolbar's **Shortcuts** button appeared dead: clicking it did
nothing. `MozcShortcutsWindowController` built a plain `NSWindow`, and
the IME bundle is `LSBackgroundOnly` + `LSUIElement`
([`src/mac/Info.plist`](src/mac/Info.plist)) — a background-only process
cannot order an ordinary window to the front, so
`makeKeyAndOrderFront:` silently did nothing. Every other window the IME
shows (the toolbar itself, the symbols palette) is a non-activating
floating `NSPanel`, which is why those work.

The shortcuts window is now built like the symbols palette:
`NSWindowStyleMaskNonactivatingPanel`, `floatingPanel:YES`,
`NSPopUpMenuWindowLevel`, `hidesOnDeactivate:NO`,
`becomesKeyOnlyIfNeeded:YES` (so its three tables stay scrollable and
selectable without stealing focus from the app being typed into), and
`CanJoinAllSpaces`.

### macOS: Right Shift in Direct input toggled Manyōshū instead of arming the macron (2026-08-26)

A Right Shift tap in Direct input switched to katakana rather than
arming the macron dead key. `-dispatchRightShiftAlone:` called
`-ensureConverterActivated:` before sending the tap, so the session had
already left `ImeContext::DIRECT` by the time it arrived;
`Session::IsMacronEligibleContext()` was then false and the tap fell
through to `ToggleManyoshuHiragana`. Because that path does not consume
the key, `macronDeadKeyPending_` was cleared too — the dead key could
never arm on macOS at all.

The client no longer activates the session while `mode_` is DIRECT:
`Session::SendKey` is answered whether or not the session is activated,
so the tap reaches the server either way. The same activation was
removed from the macron *completion* key in `-handleEvent:client:`
(previously `mode_ != DIRECT || macronWasPending`), since
`Session::InsertMacronVowel` commits the vowel directly only while the
session is still in `ImeContext::DIRECT` — otherwise it falls through to
`InsertCharacter` and would have composed あ instead of committing ā.
Linux never had this bug: `mozc_engine.cc`'s inactive-key gate lets the
tap and the completion through without any `TURN_ON_IME`.

### Number-row shortcuts: unbound slots beep (not a macOS bug) (2026-08-26)

Investigated `Ctrl+Shift+2` (odoriji palette) and `Ctrl+Shift+5`
(hiragana/direct) beeping on macOS while `Ctrl+Shift+1` and `3` worked.
The key path was innocent: `KeyCodeMap` maps *physical* `kVK_ANSI_1`…`0`
under Ctrl+Shift to US digits before any layout translation, so Dvorak
and AZERTY match QWERTY slots. The saved config simply had the palette
on slot **6** and hiragana/direct on slot **9**;
`GetEffectiveMarinaNumberRowBindings` falls back to the defaults only
when the stored list is *empty*, so a saved list is used verbatim and
the unbound chords fall through to the ordinary keymap, are consumed by
nobody, and beep. No code changed. Worth knowing that there is no
"restore defaults" control in the Shortcuts tab — a slot moved by
accident has to be moved back by hand.

### macOS: diagnosed ad-hoc-signed Converter/Renderer getting SIGKILLed (2026-08-26)

Investigated a local install where the Input Sources list showed the raw
mode key (`com.apple.inputmethod.J…`) instead of "marinaMoji", and the
input source had no mode submenu or candidate toolbar at all.

The bundle's own localization was fine (`English.lproj/InfoPlist.strings`
correctly maps `com.apple.inputmethod.Japanese` → `marinaMoji`), so the
menu-label issue is HIToolbox caching the mode name from an earlier,
incomplete registration — cleared by removing the input source, deleting
`~/Library/Preferences/com.apple.HIToolbox.plist`, logging out/in, and
re-adding it.

The missing submenu/toolbar had a different, more consequential cause:
`~/Library/Logs/DiagnosticReports/marinaMoji{Converter,Renderer}-*.ips`
showed both helper processes killed on launch with `SIGKILL (Code
Signature Invalid)`, `CODESIGNING` / Launch Constraint Violation — the
main `marinaMoji` binary loads leniently via IMKit, but `launchd`-spawned
agents (`org.mozc.inputmethod.Japanese.Converter`/`.Renderer`) hit a
stricter signature check on current macOS that plain ad-hoc signing
(`codesign -s -`) doesn't satisfy. [`src/config.bzl`](src/config.bzl)
hardcodes every codesign identity — release, testing, and installer — to
`MACOS_CODESIGN_IDENTITY_PSEUDO` ("-"). No code changed yet; the fix is
to sign with a real (free, self-generated) Apple Development certificate
and point `MACOS_CODESIGN_IDENTITY_TESTING` at it before rebuilding.

Also noted: the `.pkg` installer being unsigned only blocks the initial
double-click (right-click → Open, or `xattr -d com.apple.quarantine`
works around it) and is unrelated to the launch-constraint crash above.

### macOS: IME process crash while typing (empty preedit / `;r`) (2026-08-26)

Typing sometimes stopped until the input source was switched away and
back. Crash reports (`marinaMoji-*.ips`) showed the IME process
(`marinaMoji`) dying with `EXC_BAD_ACCESS` in `objc_retain` /
`objc_storeStrong`, not the converter daemon. Stack:

`handleEvent` → `updateComposedString:` → `-selectionRange` →
`[IMKInputController selectionRange]` → `composedString:`.

After a commit with an empty preedit (including `;r` inserting a
kaeriten then clearing marked text), `cursorPosition_` is `-1` and the
code called Apple's `selectionRange`, which immediately retains
`composedString_`. Word and other hosts also retain the marked-text
object; we were handing them the same `NSMutableAttributedString` we
keep mutating, so that pointer could already be bad. Switching IMEs
relaunches the IMK process, which is why that recovered it.

`mac/mozc_imk_input_controller.mm`: compute the caret from our own
preedit (`markedTextSelectionRange`) instead of `[super selectionRange]`;
`composedString:` and `setMarkedText:` now use an immutable snapshot
(`composedStringSnapshot`).

### macOS Preferences: leftover English and clipped descenders (2026-08-26)

The Qt Preferences window still showed English on Shortcuts, Sync,
privacy copy, updates, Apply/Cancel/OK, and the experimental-vocabulary
checkbox: those strings used `tr()` / `QObject::tr()` but had never been
extracted into `config_dialog_fr.qtts` / `.qm`. Filled unfinished FR/JA
catalogs (including long Shortcuts/Sync strings), regenerated
`config_dialog_{en,fr,ja}.qm` via `lupdate`/`lrelease` (`.qtts` copied
through a temporary `.ts`). Shortcut-table actions now use explicit
`QObject::tr(...)` in `ActionLabel()`; dictionary-pack checkbox uses a
literal `tr("Experimental vocabulary…")`; OK is `tr("OK")`.

Descenders (`g`, `p`, `y`) were clipped: stylesheet padding on labels,
checkboxes, buttons, tab bar, and table items; `AdjustTabBarForDescenders`;
`usageStatsMessageGroup` size policy Fixed → Minimum; wrapped-label
contentsMargins; shortcuts table row height = font height + 14. Relabelled
"Start in at system startup" → "Initial kanji style".

### macOS IME menu: localize leftover English; ellipsis only for dialogs (2026-08-26)

After the Config nib loaded, several extra-menu titles stayed English
because they were hardcoded in the xib; code-inserted items already used
`MarinaLocalizedString`. `applyLocalizedImeMenuTitles` now maps actions to
`MM.*` keys after nib load and in `-menu`. Added
`MM.Reconversion`, `MM.Preferences`, `MM.AddWord`, `MM.About`,
`MM.DictionaryTool` in `mac/Resources/{en,fr,ja}.lproj/Localizable.strings`.

Ellipsis follows the macOS convention: **…** means the item opens another
window; immediate actions have no dots. "Add a word" got an ellipsis.

### macOS IME extra menu missing under French (and similar) locales (2026-08-26)

marinaMoji was selected in the input menu but only system items appeared
(no Traditional kanji, Odoriji, Toolbar, Preferences, …). Extra commands
come from `Config.nib`. That nib lived only in `English.lproj` /
`Japanese.lproj`. `fr.lproj` and `French.lproj` had strings but **no**
`Config.nib`. As the main bundle, `loadNibNamed:@"Config"` returns `NO`
when preferred localizations are `fr`/`French`, so `menu_` stayed nil.

Japanese usually still found `Japanese.lproj/Config.nib`. Languages
without their own Config nib (e.g. Chinese) would fail the same way;
adding a strings-only lproj without a Base/nib would reproduce it.

Fix: `mac/Base.lproj/Config.xib` (always found); File's Owner in
English/Japanese/Base xibs is `MozcImkInputController` (was
`GoogleJapaneseInputController`); nib load retains `topLevelObjects_`,
searches known localizations, and falls back to a programmatic menu.
`mac/BUILD.bazel` includes `Base.lproj/Config.xib`.

### macOS toolbar restored off-screen after an external monitor (2026-08-26)

After a source build the floating toolbar seemed missing. The IME was
running; `~/Library/Application Support/marinaMoji/toolbar.conf` still
had `x=1591` from an external display, off a 1512-wide built-in screen
(`onscreen=false`). Immediate workaround: move the saved origin on-screen.

`mac/mozc_toolbar.mm` now clamps restored and dragged origin onto a
visible screen (same idea as Windows `ClampToVisibleArea`).
`MozcToolbarShow` / `EnsureToolbar` call `clampWindowToVisibleScreen`.

### Shortcuts tab: Right Shift macron dead key and mode lock (2026-08-26)

The Shortcuts help paragraph still described Right Shift only as the
hiragana ↔ Manyōshū toggle, and mentioned only the Windows
`Ctrl+Alt+Right Shift` lock chord. It now matches the engine: in Direct
input a Right Shift tap arms the macron dead key; in Japanese modes it
still toggles Manyōshū; double-tap Left Shift locks the mode on all
platforms (Windows can also use `Ctrl+Alt+Right Shift`). Updated
`config_dialog_shortcuts_tab.cc` and the FR/JA `config_dialog_*.qtts` /
`.qm` catalogs.

### Settings: clarify Emoticon vs. Emoji conversion with an example (2026-08-15)

[#10](https://github.com/marinaMoji/marinaMoji/issues/10): the two "special
conversions" checkboxes (顔文字変換/Emoticon and 絵文字変換/Emoji) were
easy to mix up from the label alone. Added a `toolTip` to each in
`gui/config_dialog/config_dialog.ui` with a concrete example: Emoticon is
text-based faces like `(T_T)`, `＼(^o^)／`; Emoji is pictograph characters
like `😀`, `🐶` — each tooltip also points at the other checkbox by name so
the distinction is visible without needing both tooltips open at once.
Added matching `<source>`/`<translation>` entries to
`config_dialog_{en,fr,ja}.qtts` and regenerated the corresponding `.qm`
catalogs with `lrelease` (verified via `strings`/UTF-16 decode that all
three landed in the compiled binaries). Couldn't build the actual GUI
target locally to see the tooltip rendered (`//gui/config_dialog:config_dialog`
fails here on a pre-existing, unrelated Qt toolchain gap — the Bazel-fetched
`qt_mac` repository doesn't have `libexec/uic`/`QtCore.framework` on this
machine) — validated `config_dialog.ui` and all three `.qtts` as well-formed
XML instead, and matched the existing `text` property's exact tag structure
for the new `toolTip` property.

### Symbols Palette: close on Escape or on focus loss, all three platforms (2026-08-15)

Leaving the palette open without clicking a symbol left it stuck on screen
permanently on Linux (Ubuntu/Wayland), reportedly triggering a compositor
error — an always-on-top, undecorated, never-focusable window with no way to
dismiss it. Root cause on all three platforms is the same design choice: the
palette window deliberately never takes keyboard focus/activation (GTK
`accept_focus=FALSE`, macOS `NSWindowStyleMaskNonactivatingPanel`, Windows
`WS_EX_NOACTIVATE` + `SW_SHOWNA`), so it doesn't interrupt the text field
being composed in — but that also means it can never receive an Escape key
press directly, and "click outside to dismiss" can't rely on the window
losing focus the normal way, since it never has it.

- **Linux** (`unix/ibus/mozc_engine.cc`, `unix/ibus/mozc_toolbar.{h,cc}`):
  Escape is intercepted early in `ProcessKeyEventInternal` — since the
  palette window can't receive it, it has to be caught in the engine's
  normal key pipeline instead, via a new `MozcToolbarHideSymbolsPalette()`.
  "Click outside" is handled in `FocusOut`, which now closes the palette
  unconditionally before its existing (unrelated) toolbar-hide-scheduling
  check — as a side effect this also unblocks that check, which previously
  stayed blocked forever once the palette opened, since it explicitly skips
  scheduling the toolbar hide while the palette is visible.
- **macOS** (`mac/mozc_toolbar.mm`): added `-cancelOperation:` (the standard
  NSResponder hook for Escape) and `-windowDidResignKey:` on
  `MozcSymbolsPaletteWindowController`, both calling a new shared
  `-dismissPalette` (extracted from the existing "close after inserting a
  symbol, unless pinned" path). This works because the panel *is* made key
  on open (`-symbolsClicked:`'s `makeKeyAndOrderFront:`) despite being
  non-activating — nonactivating panels can become key for their own
  controls without stealing app activation from the composing field's app,
  so both Escape and losing key status route to us reliably.
- **Windows** (`win32/tip/tip_keyevent_handler.cc`): added
  `TryCloseSymbolsPaletteOnEscape`, intercepted in `OnKey` the same way as
  the existing marina number-row shortcuts, directly flipping
  `TipPrivateContext::symbols_palette_visible()` and forcing a layout
  refresh (`TipEditSession::OnLayoutChangedAsync`) — no round trip to
  `mozc_server`, matching how `SHOW_SYMBOLS_PALETTE`/`HIDE_SYMBOLS_PALETTE`
  are already local-only UI signals never forwarded to the session
  (`win32/tip/tip_edit_session.cc`). "Click outside" needed **no code
  change**: `OnKillThreadFocus` → `TipUiHandlerConventional::OnFocusChange`
  already sends a `RendererCommand` with no `application_info` on real
  focus loss, and `SymbolsPaletteWindow::OnUpdate` already treats that as
  "close the palette" — that path just wasn't being exercised by Escape
  because Escape doesn't change focus at all.

**Verification:** `//mac:mozc_toolbar` builds clean. The Linux and Windows
changes could not be compile-verified on this host (GTK headers and the
Win32/TSF toolchain both unavailable on macOS) — brace/paren balance and
every touched dependency were checked by hand, but both need a real
Linux/Windows build before merging, same limitation as the other Linux/
Windows fixes in this batch.

### Diagnosed: new kaeriten shortcuts not taking effect was a stale Bazel build, not a code bug (2026-08-15)

Reported after the `;jo`/`;c`/`;g`/`;to` remap (#21, above): the new shortcuts
didn't work when actually typed on Linux. Traced end-to-end through the real
`composer::Table` (not just the display-only shortcut list) and found the
compiled-in `system://kaeriten.tsv` resource — embedded at build time via
`base/gen_config_file_stream_data.py` into `config_file_stream_data.inc` —
was serving the **old** `;u`/`;m`/`;d`/`;t` mapping under this repo's test
build configuration (`bazel-out/*-ST-*/`), while a plain `bazel build` of
the same genrule, and the default `bazel-out/*-fastbuild/` config, both
correctly reflected the new mapping already committed to
`data/preedit/kaeriten.tsv`. I.e. the source and default build were correct
throughout; one specific cached build-graph configuration had a stale
`config_file_stream_data.inc` left over from before the remap. `bazel clean`
followed by a full rebuild resolved it — every config now agrees.

Given this affects the same file (`data/preedit/kaeriten.tsv`), and Linux
package builds are also plain Bazel builds subject to the same staleness
risk, this most likely explains the Linux report directly: rebuild from a
clean state before repackaging.

Added `TableTest.KaeritenShortcuts` (`composer/table_test.cc`) as a
permanent regression test — it initializes a real `Table` through
`InitializeWithRequestAndConfig` (the production path, not synthetic test
data) and checks every current kaeriten shortcut resolves correctly, and
that none of the four retired `;u`/`;m`/`;d`/`;t` inputs still resolve. This
is the first test coverage of the kaeriten table through the actual
composer, rather than just the TSV-parsing helpers or the shortcuts-display
list — it would have caught this class of staleness (or a real mapping
regression) immediately.

### Symbols Palette: added ヶ to the Symbols tab (2026-08-15)

[#19](https://github.com/marinaMoji/marinaMoji/issues/19): ヶ (katakana
small ke, as in 三ヶ日) isn't a repetition mark, so it doesn't belong on the
Odoriji tab, but it's common enough in place names to deserve one-click
access alongside the `xke` romaji chord. Added to the three duplicated
`BuildDefaultGeneralSymbols`/`kGeneralSymbols` literal lists that back the
Symbols Palette's "Symbols" tab: `unix/ibus/mozc_toolbar.cc`,
`mac/mozc_toolbar.mm`, `renderer/win32/symbols_palette_window.cc`. Also
noted in `docs/SYMBOLS_PALETTE.md`. In passing: the Windows list uses
`－` (fullwidth hyphen-minus, `\xFF0D`) at the position where mac/Linux use
`×` (multiplication sign, U+00D7) — a pre-existing mismatch, left alone
since it's out of scope here.

### Linux: odoriji palette flashed top-left and vanished when opened from the IME menu (2026-08-15)

[#20](https://github.com/marinaMoji/marinaMoji/issues/20): `MozcEngine::FocusOut`
(`unix/ibus/mozc_engine.cc`) unconditionally hides the candidate window and
calls `RevertSession` on every focus loss. Clicking "Odoriji" in the ibus
panel/property menu — a separate surface from the focused text field —
tends to bounce keyboard focus off and back, so `FocusOut` fires right after
`PropertyActivate` shows the palette. `Session::Revert` (`session/session.cc`)
is a genuine no-op for the odoriji palette when there's no preedit (it only
touches undo-context/converter state in `PRECOMPOSITION`), so the palette
stays alive server-side — but `FocusOut`'s own `Hide()` call tears it down
client-side anyway, orphaning it until the next real keystroke (e.g. Space)
resyncs client and server state. That resync is what made the bug look like
"press Space once and it works": the underlying session was fine the whole
time, only the client's window got prematurely hidden. Also explains the
top-left flash: candidate window positioning is fully delegated to ibus
(`IBusCandidateWindowHandler::UpdateCursorRect` is a no-op by design), which
falls back to a default position when the palette opens without a
recently-established cursor context — as happens when triggered from the
menu rather than a real keystroke.

Fix: added `odoriji_property_show_time_` (`unix/ibus/mozc_engine.h`), set
when the property-menu handler sends `SHOW_ODORIJI_PALETTE`. `FocusOut`
skips its `Hide()`/`RevertSession()` calls if it lands within 500ms of that
timestamp, treating it as the spurious focus bounce rather than a genuine
focus change — everything else in `FocusOut` (toolbar-hide scheduling,
`ResetContentType`, `SyncData`, tracked-modifier release) still runs
normally. Follows the same "don't tear down a just-opened palette on a
focus blip" precedent already used for the Symbols Palette
(`!MozcToolbarIsSymbolsPaletteVisible()`, one line above), and the same
debounce-window shape already used elsewhere in this file (300ms
autorepeat suppression, 150ms toolbar-hide delay).

### Linux: Ctrl+Shift+5 only toggled Hiragana→ASCII, not the reverse (2026-08-15)

[#13](https://github.com/marinaMoji/marinaMoji/issues/13): `MozcEngine::ProcessKeyEventInternal`
(`unix/ibus/mozc_engine.cc`) only called `DispatchMarinaNumberRowShortcut` —
which already handles both toggle directions correctly via
`TURN_ON_IME`/`TURN_OFF_IME` — *after* the "IME inactive, forward to
application" early-return gate. That gate only lets a key through while
inactive if it's in the keymap's `DirectInput` table, and `Ctrl Shift 5`
isn't (only backtick is, for plain `IMEOn`). So with the IME off (ASCII),
Ctrl+Shift+5 never reached the dispatcher at all and was forwarded straight
to the focused application; with the IME on (Hiragana), the gate doesn't
apply and the toggle worked. Moved the dispatch call above the gate so it
runs unconditionally, matching `win32/tip/tip_keyevent_handler.cc`'s `OnKey`
(added during the recent Windows number-row port), whose "not gated on
`open`" comment already claimed this was how ibus behaved — it wasn't, until
now.

### Kaeriten shortcuts: fix `;t` prefix collision, remap to readings, drop duplicate default tables (2026-08-15)

[#21](https://github.com/marinaMoji/marinaMoji/issues/21): `;t` (丁) was a
strict prefix of `;te` (天) and `;ti` (地) in `data/preedit/kaeriten.tsv`, so
pressing space right after `;t` committed 丁 even when typing toward `;te`/`;ti`.
Remapped the colliding/less-intuitive entries to equal-length, reading-based
keys: `;jo` 上 (was `;u`), `;c` 中 (was `;m`), `;g` 下 (was `;d`), `;to` 丁 (was
`;t`) — mirrors the existing `;te`/`;ti` pattern, so no entry is a prefix of
another.

While auditing this, found the same default-shortcut list hardcoded three
times beyond the actual source of truth: `unix/ibus/mozc_toolbar.cc`,
`mac/mozc_toolbar.mm`, and `session/marina_shortcut_list_util.cc` each had a
literal `FillDefaultKaeritenShortcuts` array used only as a fallback when
`composer::LoadKaeritenShortcutEntries` returned empty. Since
`data/preedit/kaeriten.tsv` is compiled into every process via
`config_file_stream`'s embedded-data generation (`base/gen_config_file_stream_data.py`),
that fallback path is unreachable in practice — it just meant three
hand-copied literals to keep in sync (or, as here, forget to). Removed all
three; the tsv (overridable per-user via `config.custom_kaeriten_table`,
already wired to the "Edit kaeriten shortcuts..." dialog) is now the sole
source, with a `LOG(ERROR)` in each call site if it's ever actually empty
instead of a silent stale substitute.

### Linux: user symbols didn't appear in the Symbols Palette until reboot (2026-08-15)

[#18](https://github.com/marinaMoji/marinaMoji/issues/18): `ShowSymbolsPalette()`
in `unix/ibus/mozc_toolbar.cc` caches the whole palette window as a static
singleton and only builds the "User" tab (from `user_symbols.txt`) the first
time it's opened after `mozc_toolbar` starts; every later click just re-raised
the same cached window, so symbols added later via Preferences were saved
correctly but never re-read until the long-lived toolbar process itself
restarted. macOS and Windows were unaffected — both already rebuild/reload the
palette on every open. Added `RefreshUserSymbolsTab()`, which rebuilds just
the User tab's notebook page from disk before raising the cached window.

### Windows: avoid the StickyKeys popup our own Shift-tap shortcuts trigger (2026-08-09)

The Left/Right Shift tap gestures (mode toggle, hiragana/manyōshū toggle,
macron dead key) are exactly the "press Shift 5 times" pattern Windows uses
to offer turning StickyKeys on, so switching modes briskly could pop up that
system dialog mid-typing. New `win32/base/sticky_keys_util.{h,cc}`
(`StickyKeysUtil`) wraps `SystemParametersInfo(SPI_*STICKYKEYS)` to disable
just that activation hotkey -- never StickyKeys itself -- for the lifetime of
the renderer process (`renderer/win32/win32_renderer_main.cc`), restoring the
previous setting on exit (also via the destructor as a safety net). It is
this one long-lived, single-instance process (unlike the TIP DLL, which loads
into every focused application) that makes a clean disable/restore lifecycle
possible.

This only stops the popup; it does not touch StickyKeys itself, which is a
deliberate accessibility choice for some users -- and if it's already on, a
plain Shift tap latches Shift for the next key (so a macron vowel can come out
capitalized). `gui/config_dialog/config_dialog_shortcuts_tab.cc` now detects
that case (Windows only, via `StickyKeysUtil::IsCurrentlyOn`) and shows a
warning with a button to open Ease of Access keyboard settings, rather than
silently overriding it.

### Windows: docket button did nothing; macron placeholder redrawn (2026-08-09)

Second on-hardware round. Two fixes from the same session:

`LAUNCH_DOCKET_DIALOG` was missing from both allowlists on the Windows
renderer→TIP path, so clicking the toolbar's docket button was silently
dropped and the dialog never opened. The command type was added to
`protocol/commands.proto`, `session/session.cc`, `client/client.cc` and
`win32/base/keyevent_handler.cc` when the docket landed, but not to
`renderer/renderer_server.cc`'s `SendCommand` switch (which returns false for
unlisted types before the PostMessage) or
`win32/tip/tip_edit_session.cc`'s `OnRendererCallbackAsync`. Both now list it
next to `LAUNCH_CONFIG_DIALOG`/`LAUNCH_DICTIONARY_TOOL`. Worth remembering
that a new toolbar→session command needs *five* edits, not three.

The armed-macron placeholder changed from `◌̄` (U+25CC DOTTED CIRCLE +
U+0304 COMBINING MACRON) to a plain `¯` (U+00AF MACRON). The dotted circle is
the textbook way to display a diacritic with no base character, and it looks
right on Linux/macOS, but Windows renders it at full glyph size *beside* the
bar rather than under it — a big circle followed by a stray macron.

### Docket: a review queue for unregistered committed vocabulary (2026-08-08)

Added the **docket** — a persistent queue of dictionary-unknown compounds
committed in passing (e.g. 大元宮/daigenguu), reviewed later in a new
`docket_tool` table UI with per-row Yes/No/Never triage, replacing what the
Windows toolbar's "Add Word" button opens (`Ctrl+Shift+0`'s single-entry
dialog is unchanged). Grew out of `LaunchWordRegisterDialog`'s existing
single-slot commit-buffer prefill, which is fine for "register this right
now" but gets overwritten before there's leisure to deal with it. See
[docs/DOCKET.md](docs/DOCKET.md) for the full design.

New: `dictionary/docket_store.{h,cc}` (locked JSON store, mirrors
`UserDictionaryStorage`'s `ProcessMutex` pattern), `gui/docket/` (the review
dialog, built without a `.ui`/`moc` step since it needs neither). Touched:
`engine/engine.{h,cc}` (`IsKnownWord`/`RecordDocketCandidate`, backed by
`Modules::GetDictionary()`/`GetUserDictionary()`/`GetPosMatcher()`),
`session/session.cc` (`CommitInternal` gate, reusing `lid`/`rid` already
carried on `commands::Result.tokens`), `protocol/commands.proto`
(`DOCKET_DIALOG` tool mode), `renderer/win32/toolbar_window.cc` (button
repoint + a presence-dot badge — deliberately not a digit count, since GDI
text in the toolbar's raw alpha-blended pixel buffer needs its own
alpha-channel bookkeeping to composite correctly).

### Windows: first on-hardware bug round (English installer + diagnostics)

First real install of a CI artifact on Windows hardware (2026-08-08). The
build is broadly functional; four bugs came back. One is fixed, one is
diagnosed but needs a design decision, and two are instrumented rather than
guessed at.

**Fixed — installer was entirely in Japanese** (`win32/installer/
installer_marinamoji_64bit.wxs`). The `<Package>` element still carried
upstream Mozc's `Language="1041" Codepage="932"` (Japanese / Shift-JIS), and
all four user-visible strings — the Windows-version gate, the
administrator-privileges gate, the ARM64 gate, and the
newer-version-installed error — were Japanese text inherited unchanged from
upstream. On an English Windows the install and uninstall dialogs came up in
Japanese. Switched to `1033`/`1252` and translated the strings. Note this
makes it an English-only MSI: a genuinely multilingual installer needs WiX
`.wxl` localization files and a per-culture build, which is a larger change
tracked separately.

**Diagnosed, not yet fixed — macron vowels dead on Dvorak.**
`Ctrl+Alt+O` on a Dvorak machine typed nothing and fell through to the host
app (Notepad opened its Open dialog). Root cause is a **double translation**:
`win32/base/keyboard_layout_tables.h` documents its premise as "Windows
assigns VK_A..VK_Z by physical key position (not by printed character)", and
that premise is false for Dvorak and AZERTY, where Windows remaps virtual-key
codes to follow the *printed* character. So with the OS layout set to Dvorak
*and* marinaMoji's `MarinaKeyboardLayout` also set to Dvorak, the key that
types `o` reports `VK_O`, and `RomajiKeyboardLayoutEmulator::
GetCharacterForKeyDown` then maps `VK_O` through the Dvorak table as though
it were a physical QWERTY-O key, yielding a different letter entirely. No
macron rule matches and the chord passes through to the application.
Interestingly `WINDOWS_TESTING.md` already had this right (Part 3.2: "Windows
assigns virtual-key codes by the character a key produces"); it is the code's
own header comment that is wrong.

- **Workaround, no rebuild needed**: set marinaMoji's keyboard layout to *OS
  default* whenever the OS layout already is the layout you want. The
  emulator exists for the opposite case (wanting AZERTY behaviour on a
  US-layout machine), and is actively harmful when the two agree.
- **The real fix** is to index the layout tables by **scan code** (genuinely
  physical and layout-independent) rather than by virtual key. `VirtualKey`
  (`win32/base/keyboard.h`) does not currently carry a scan code, so this
  means plumbing it through the TIP key path — a real refactor of the most
  test-sensitive code in the port, deliberately not attempted blind with no
  Windows toolchain here.

**Related, separate, also not fixed — `RIGHT_ALT` is never emitted on
Windows.** `grep` finds zero `RIGHT_ALT` in `win32/`, while `unix/ibus`
emits it in three places (`key_event_handler.cc:222`, `key_translator.cc:402`,
`mozc_engine.cc:514`) — with a comment at `key_event_handler.cc:215-218`
about this *specifically for Dvorak*. `KeyParser` maps the `RightAlt` token
to both `ALT` and `RIGHT_ALT`, so the 20 `Ctrl RightAlt …` rows per keymap
TSV can never match on Windows; they are dead weight there. The comment at
`win32/base/keyevent_handler.cc:429-430` claiming left/right Alt fidelity "is
not required by any marina shortcut today" is contradicted by those rows.
Not fixed this pass because `KeyEventUtil::NormalizeModifiers` strips
`RIGHT_ALT` on some paths and not others, so adding the emission could change
which keymap row wins and regress the lowercase macrons that currently work
— needs a Windows build to verify, not a guess.

**Instrumented — Symbols Palette insert, odoriji palette flicker, shortcuts
button.** Three reported failures that could not be diagnosed from source
alone, so rather than guess, added temporary `OutputDebugString` tracing at
the three points that would distinguish the candidate causes. All sites are
marked `marinaMoji TEMPORARY (2026-08-08)` with removal instructions.

- `renderer/renderer_server.cc` — `[marinaMoji/renderer]`: logs the target
  HWND, our PID, payload size, and whether `SendMessageTimeout` actually
  succeeded, distinguishing "never sent" from "sent but rejected."
- `win32/tip/tip_text_service.cc` — `[marinaMoji/tip]`: the `WM_COPYDATA`
  handler now reports **each rejection gate separately** (null struct, wrong
  tag, empty payload, over the size cap, failed sender check) instead of one
  silent compound `if`. This specifically covers the possibility that the
  new `IsTrustedRendererSender` check from the entry below is itself the
  thing dropping legitimate inserts — e.g. if `GetProcessInitialNtPath` or
  `GetNtPath` fails under a sandboxed host application.
- `renderer/win32/toolbar_window.cc` — `[marinaMoji/toolbar]`: logs the three
  visibility bits plus previous state on every `RendererCommand`, which
  should show whether a palette is being closed by a follow-up command that
  simply omits its info field (the race already flagged in
  `renderer_server.cc`) or whether the show request never arrives.

Read with Sysinternals DebugView, run as administrator with "Capture Global
Win32" enabled — required to see output from the TIP, which runs in-process
inside whatever application has focus. See `WINDOWS_TESTING.md` Part 6.

### Windows: verify sender of the TIP's renderer-callback window

A third review pass (Explore agent + manual verification) found that fixing
the WM_COPYDATA UIPI-filter bug two rounds back had an unintended side
effect: `TipTextServiceImpl`'s renderer-callback window
(`win32/tip/tip_text_service.cc`) now accepts `WM_COPYDATA` from *any*
process at or below the box's integrity level, not just
`marinamoji_renderer.exe`. `kMessageReceiverClassName` and the symbol-text
copy-data tag are plain public constants (`base/const.h`), so any other
process could `FindWindow` the class and send a spoofed Symbols-Palette
"commit," inserting arbitrary text into whatever the user is currently
typing into -- a password field, an elevated installer prompt, anything.
The registered-message path (candidate clicks, mode switches) has the same
lack of sender validation but a much smaller blast radius (only affects
mozc's own in-session state), and structurally can't carry a sender handle
in-band the way `WM_COPYDATA` can -- left as a known, documented residual
gap rather than redesigned this pass.

- `renderer/renderer_server.cc`: `RendererServerSendCommand::SendCommand`'s
  `WM_COPYDATA` call now puts this process's own PID in `wParam` (was
  `nullptr`) instead of leaving it empty.
- `win32/tip/tip_text_service.cc`: new `IsTrustedRendererSender(DWORD)`
  resolves that PID's *initial* process image path via
  `WinUtil::GetProcessInitialNtPath` (NT path, so a later same-name
  substitution doesn't fool it) and compares it against
  `SystemUtil::GetRendererPath()`. `RendererCallbackWidnowProc` now requires
  this to pass, plus a 4096-byte cap on the inserted text, before calling
  `OnRendererSymbolTextCallback`.
- This is **not** a cryptographic guarantee -- a sender can claim any PID it
  likes, and if that PID happens to belong to a real
  `marinamoji_renderer.exe` (discoverable without special privilege), the
  check passes even though the claimed PID isn't actually who sent the
  message. It raises the bar from "any process, trivially" to "a process
  that already knows how to find a running renderer," which is the
  practical ceiling reachable without redesigning this channel around a
  shared secret carried over the existing (already-authenticated) TIP<->
  renderer IPC connection -- worth doing before a wider release, not done
  this pass.
- Untested against a real Windows build -- no Windows compiler available
  here, same caveat as everything else in this file.

### Windows: auto-updater downloaded-installer integrity checks

`renderer/win32/marina_auto_update.cc` downloaded the update MSI over HTTPS
and ran it straight away: no hash check against release metadata, no
signature check (the UI copy already admits the build is unsigned), and the
download loop treated `WinHttpQueryDataAvailable`/`WinHttpReadData` returning
0 as "done" even though that's indistinguishable from a connection that
dropped mid-transfer -- a truncated download could still get executed as a
"success."

- **Truncation check**: `OpenSuccessfulGet()` now also reads the response's
  `Content-Length` header; `WriteBodyToFile()` takes it as an expected byte
  count and fails if the total actually written doesn't match. `DownloadToFile()`
  deletes the partial file on that failure rather than leaving it for the
  caller to launch.
- **Hash check**: GitHub has published a `digest` (`sha256:...`) field on
  release assets since 2024. `base/marina_github_releases.{h,cc}` now parses
  it into `MarinaGitHubAsset::digest_sha256` and exposes
  `FindMarinaMsiSha256Digest()` alongside the existing
  `FindMarinaMsiDownloadUrl()` (both now share one internal asset lookup so
  they can't disagree on which asset they mean). `marina_auto_update.cc`
  hashes the downloaded file with BCrypt (`Sha256HexOfFile()`, new `bcrypt`
  win32-lib target) and deletes it on a mismatch. When GitHub hasn't
  published a digest for a given asset (older releases), the check is
  skipped rather than treated as an error -- there's nothing to compare
  against, and refusing every pre-2024 release's installer would be worse
  than not checking.
- This is **not** a substitute for code signing -- it only guards against a
  corrupted/truncated transfer, since the digest itself is fetched over the
  same GitHub API channel as the download. The SmartScreen warning in the
  install-offer dialog (from the unsigned-build decision above) is still the
  only thing standing between the user and an unverified publisher at
  present.
- Added test coverage for the new digest parsing/lookup path
  (`marina_semver_test.cc`, `marina_github_releases_test.cc`). The WinHTTP/
  BCrypt plumbing in `marina_auto_update.cc` itself is untested here, same
  caveat as everything else in this file -- no Windows compiler available.

### Windows: indicator DPI handling and installer sync-task rollback

Two more items off the same "gaps" list as the round below.

- **Indicator DPI** (`renderer/win32/indicator_window.cc`): the mode-switch
  indicator balloon read its DPI scaling once at construction
  (`GetDPIScaling()`/`dpi_scaling_`) and never again, unlike the toolbar and
  sync overlay, which both got `WM_DPICHANGED` handling in the round below.
  Effect: drag the window (or its monitor) to a display with a different
  scale factor and the indicator's sprites stay sized for the old DPI.
  Added a `WM_DPICHANGED` handler that recomputes `dpi_scaling_` from the new
  DPI and reloads all six mode sprites, mirroring `ToolbarWindow::OnDpiChanged`
  / `LoadIcons()`.
- **Installer sync-task rollback** (`win32/installer/installer_marinamoji_64bit.wxs`,
  `win32/custom_action/custom_action.{h,cc,def}`): every other state-mutating
  install step (`RegisterTIP64`, `RestoreServiceState`, ...) has a paired
  `Execute="rollback"` action, but `RegisterSyncTask` -- which creates the
  `marinamoji_sync.exe --daemon` Task Scheduler logon task -- didn't. It also
  ran as `Execute="commit"`, which sits outside the rollback transaction
  entirely. Effect: a failed or aborted install/upgrade could leave the sync
  logon task registered with nothing else installed to back it. Switched
  `RegisterSyncTask` to `Execute="deferred"` and added
  `RegisterSyncTaskRollback` (a thin wrapper around the existing
  `UnregisterSyncTask`), scheduled `Before="RegisterSyncTask"`, matching the
  `RegisterTIP64`/`RegisterTIPRollback64` pattern. Untested against a real
  MSI build -- no Windows toolchain available here.

### Windows: docs, test coverage, sync overlay, and toolbar hide follow-ups

Second pass on the same review as the bug-fix/perf entry below, working
through the "gaps" list that pass had deliberately left untouched.

- **Docs**: `docs/build_marinamoji_on_windows.md` still opened with "marinaMoji
  does not yet ship a Windows port" and pointed every command at
  `google/mozc`; fixed the banner (now points at
  [WINDOWS_PORT_PLAN.md](docs/WINDOWS_PORT_PLAN.md)) and every clone/output/
  GitHub Actions reference to use this repo and `marinaMoji64.msi`.
  `docs/WINDOWS_PORT_PLAN.md`'s status block still said Phase 4 (toolbar)
  hadn't started and that the Symbols Palette/Shortcuts buttons were
  disabled placeholders; both were true when written but false since the
  toolbar parity pass. Corrected in place (old entries kept, with an
  "Update (2026-08-07)" note) rather than rewritten, so the doc still reads
  as a history. Same treatment for the sync-QA checklist item that claimed
  "no Windows TIP-side implementation at all" for input-blocking during
  sync -- `win32/base/sync_lock_util` implements exactly that.
- **Tests**: added `renderer/win32/marina_localized_string_test.cc`, the
  first test for any of the ~3,300 lines of new Windows UI code from this
  branch. Checks that a sample of "MM.*" keys resolve to a non-empty,
  actually-translated string in all three languages, that Japanese wording
  differs from English (catches an untranslated copy-paste), and the
  unknown-key/null-key fallback paths. Broader coverage (`PickIconSizeTier`,
  `ClampToVisibleArea`, the `toolbar.conf` round-trip) would need those
  helpers exposed from anonymous-namespace/private scope first -- left as a
  follow-up rather than done as a blind refactor with no Windows compiler
  available to verify it.
- **Sync overlay multi-monitor** (`renderer/win32/sync_overlay_window.cc`):
  the "synchronising…" overlay always centred on the *primary* monitor at
  the *primary's* DPI (`GetDpiForPoint(0, 0)` / `SPI_GETWORKAREA`), so on a
  multi-monitor setup it could appear on a screen the user isn't even
  looking at. Added `TargetScreenPoint()` (center of the foreground
  window's rect) and route both the DPI lookup and
  `GetWorkingAreaFromPoint()` through it, refreshing font/layout in
  `UpdateLayout()` if the target monitor's DPI changed -- the same pattern
  the toolbar already used for its own drag handling.
- **Toolbar delayed hide** (`renderer/win32/toolbar_window.{h,cc}`): the
  toolbar hid immediately on any focus-loss signal (`ShowToolbar` bit
  clearing in `OnUpdate()`), unlike `unix/ibus`'s 150ms grace period
  (`MozcToolbarScheduleHideDelayed`). Windows 11 focus transitions (e.g.
  Alt+Tab bouncing through an intermediate window) can flicker the toolbar
  as a result. Added a matching `SchedulePendingHide()`/
  `CancelPendingHide()` pair on a `WM_TIMER`, composing with the existing
  `menu_open_`-deferred-hide logic unchanged (a still-open context menu at
  the end of the delay still defers via the existing path). The public
  `Hide()` entry point (used for a hard hide-all, e.g. renderer shutdown)
  stays immediate, matching `unix/ibus`'s split between
  `MozcToolbarHide()` (instant) and the delayed variant.

### Windows: bug fixes and hot-path perf found in a full-code review pass

A fresh review of `src/win32/` and `src/renderer/win32/` against the mac and
Linux/ibus implementations, done after the toolbar/sync/macron/auto-update
work above had all landed. Three real bugs and three hot-path perf issues
fixed; a longer list of untouched gaps (test coverage, multi-monitor sync
overlay, stale docs, etc.) recorded in ShareDocs for later triage rather
than done here. Full writeup: ShareDocs
`documentation/Implementation/Windows_Improvement_Opportunities.md`.

**Bugs:**

- `win32/tip/tip_text_service.cc`: Symbols Palette text commits travel
  renderer→TIP via `WM_COPYDATA`. A comment claimed `WM_COPYDATA` is on
  Windows' default UIPI-allowed message list and skipped the
  `ChangeMessageFilterEx` call the neighbouring registered message gets --
  backwards; `WM_COPYDATA` is MSDN's canonical example of a message that
  *needs* the filter call to cross an integrity-level boundary. Effect: every
  Kaeriten/Symbols/User palette insertion silently failed whenever the
  focused application ran elevated (admin PowerShell, an elevated editor,
  etc.), because the medium-IL renderer's message never reached the high-IL
  TIP. Odoriji was unaffected (different, already-filtered transport). Fixed
  by adding the missing `ChangeMessageFilter(..., WM_COPYDATA)` call.
- `renderer/renderer_server.cc`: the toolbar/palette/shortcuts round-trip
  added in this branch used plain `SendMessage` (synchronous) into a window
  owned by the *focused application's* process, unlike upstream's
  `PostMessage`-only design. A stalled host app (Word mid-save, Excel
  recalculating, any temporarily-hung process) would freeze the entire
  renderer UI -- toolbar, candidate window, every palette, for every
  application -- with no way back. Switched both call sites to
  `SendMessageTimeout` with `SMTO_ABORTIFHUNG` (500ms), keeping the
  synchronous ordering these commands need while bounding the damage to one
  hung app.
- Investigated and **not a bug**: "Hide toolbar" (the toolbar's own context
  menu) looked one-way at first read, since nothing on the toolbar itself
  can turn it back on. It isn't -- the TSF language bar's existing "Tool"
  menu already has a `kToolbarVisibility` entry
  (`win32/tip/tip_lang_bar.cc`/`tip_text_service.cc`) that correctly flips
  `SaveToolbarVisiblePreference(!LoadToolbarVisiblePreference())` both ways,
  predating this review. Real open question, moved to the Windows testing
  checklist rather than code: Windows 11 hides the classic language bar by
  default (consolidated into the taskbar input indicator), so this toggle's
  practical reachability needs on-hardware confirmation, not a code fix.

**Perf (all three are per-keystroke or per-update costs that scale with how
much the user types, inside the host application's process):**

- `renderer/win32/toolbar_window.cc`: `OnUpdate()` (called on every
  `RendererCommand`, i.e. every keystroke while the toolbar is visible) was
  re-reading `AppsUseLightTheme` from the registry on every call. The value
  is already tracked correctly via `WM_SETTINGCHANGE`
  (`OnSettingChange()`), so the per-update poll was pure redundancy. Removed.
- `renderer/win32/toolbar_window.{h,cc}`: `LoadIcons()` re-read and
  WIC-decoded all seven toolbar PNGs from disk on every mode/theme change
  and every re-show, so e.g. toggling hiragana↔direct hit the disk each
  time. Added `GetOrLoadCachedIcon()` + an `icon_disk_cache_` map keyed on
  the resolved icon path, so each distinct (name, size) is decoded once for
  the life of the renderer process. `icon_cache_`/`logo_cache_` changed from
  owning `wil::unique_hbitmap` to non-owning `HBITMAP` borrowed from the
  cache (ownership now lives in `icon_disk_cache_`, which outlives any
  single `LoadIcons()` call).
- `win32/base/sync_lock_util.cc`: `IsSyncLockActive()` is polled from both
  `OnTestKey` and `OnKey` on the TSF thread with a 250ms TTL, so a sustained
  typist caused ~4 `sync.status.json` opens/second *whether or not the user
  has ever configured sync*. Added a much-longer-TTL (10s) gate on
  `sync.conf`'s existence -- free for users who never open the Sync tab,
  since that file is only ever written by explicit user action in the
  config dialog.

### Decision: Windows ships unsigned; Microsoft Store ruled out

No code change — recorded here because it determines what we release.

The Windows `.exe` ships **unsigned** for now, with the SmartScreen warning
documented in the README. Code signing comes later via
[SignPath Foundation](https://signpath.org/), which is free for open-source
projects and signs the existing MSI unchanged. SignPath require an existing
release history and don't accept brand-new projects, so shipping first and
applying later is the correct order rather than a compromise.

**The Microsoft Store was investigated and ruled out.** It cannot supply free
signing for this project, for two independent reasons:

- Microsoft only re-signs **MSIX** packages during certification. An MSI/EXE
  Store submission [must already be signed](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/code-signing-options)
  with a CA-chained certificate.
- MSIX cannot host a TSF text service at all. Per Microsoft's
  [MSIX preparation docs](https://learn.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-prepare):
  *"Your app's modules are loaded in-process to processes that are not in your
  Windows app package. This isn't permitted, which means that in-process
  extensions, like shell extensions, aren't supported."* A TIP DLL is loaded
  in-process into every application that takes text input — architecturally the
  same shape as a shell extension. Packaged COM does not help: it exposes
  **out-of-process** servers only, and explicitly "will not work for
  application extensions that rely upon directly reading the registry", which
  is how TSF profile registration works. MSIX also blocks the HKLM writes that
  registration needs, and the Store rejects apps requiring elevation for any
  part of their functionality.

Sparse packages (`allowExternalContent`) don't rescue it either: Microsoft
signs only the MSIX package, never the external content, so the actual
binaries would remain unsigned.

A Store listing may still be worth doing later purely for discoverability, as
a plain MSI/EXE submission once a SignPath certificate exists. It is not a
route to obtaining one.

### Windows: self-hosted auto-update, on the same model as macOS

Windows previously had no update mechanism at all: `win32/installer/`
produces an unsigned `.msi` with no update path, and the "Decision: Windows
ships unsigned" entry above ruled out the Microsoft Store as a source of one
(the Store only gives free signing/auto-update to MSIX packages, and MSIX
cannot host a TSF text service). **That ruling is about the Store
specifically and has no bearing on this feature** — this mirrors macOS's own
`marina_auto_update.mm`, which has nothing to do with the App Store either:
both are a plain HTTPS poll of GitHub Releases plus a one-click
download-and-launch of the installer, entirely self-hosted, entirely free.

Reused as-is, unmodified: `base/marina_github_releases.{h,cc}` (JSON parsing,
version comparison) and `base/marina_update_throttle.{h,cc}` (24h throttle,
persisted under the user profile dir) were already platform-agnostic — mac
was simply the only caller. Added Windows-equivalent asset lookup alongside
the existing mac-only functions, without touching them:
`FindMarinaMsiDownloadUrl()` and `MarinaHostWindowsArchToken()` ("x64" /
"arm64"), matching the `marinaMoji-<tag>-<arch>.msi` naming
`.github/workflows/release.yaml` actually publishes (confirmed by reading
the workflow, not assumed — the README says ".exe" but the real artifact is
an `.msi`). Covered by a new `base/marina_github_releases_test.cc`, the
first tests this file has had; **passes on macOS**, so the parsing/lookup
logic behind this feature is verified by execution.

**Deliberately hosted in the renderer, not the TIP** — this needed a real
architectural decision, not just a port:

- `TaskDialogIndirect` (used for the update-offer prompt) requires the
  *process's* manifest to declare comctl32 v6. The renderer's own
  (`mozc_renderer.exe.manifest`) already does, because the toolbar work
  earlier in this session depends on it. The TIP DLL has no such guarantee:
  it loads in-process into every application that takes text input --
  Notepad, Chrome, banking software, sandboxed app-container processes --
  and their manifests aren't ours to control. A TaskDialog call there would
  be unreliable at best.
- It also keeps a network stack (WinHTTP) out of a component that runs
  inside arbitrary, possibly low-integrity host processes -- a materially
  larger trust footprint than putting it in a single process we own.

New `renderer/win32/marina_auto_update.{h,cc}`:

- WinHTTP for both the GitHub API call and the `.msi` download -- not the
  `curl.exe`-subprocess pattern `base/marina_curl_fetch.cc` uses on macOS.
  A TIP-hosted design would have needed subprocess invocation with untrusted
  data (a GitHub-served URL) in the arguments, and Windows command-line
  quoting is a real bug class if done by hand (unlike POSIX `popen`, where
  no shell is invoked once arguments are pre-quoted); avoided entirely by
  not shelling out. (This turned out to be moot once the design moved to
  the renderer, but WinHTTP remains the more robust choice there too: no
  dependency on `curl.exe` being on PATH, no subprocess overhead.)
- `TaskDialogIndirect` for the update offer (Download & Install… / Open
  release page / Later), mirroring mac's three-button `NSAlert`.
- Launch via `WinUtil::ShellExecuteInSystemDir(L"open", path, nullptr)`
  (already used by `Process::OpenBrowser`) -- triggers the UAC elevation
  prompt for the MSI's custom actions, the same consent step as macOS
  Installer.app's password prompt. Since the MSI is unsigned for now (see
  above), SmartScreen may also warn first; the dialog copy says so.
- Trigger: mac hooks `activateServer:`, which fires on every focus change
  into any app using the IME -- there is no safe Windows analogue given the
  TIP constraints above. Instead, a background thread starts once at
  `WindowManager::Initialize()` (renderer startup) and re-tests the 24h
  throttle every 2h for as long as the renderer stays alive, so a process
  that never restarts still notices the next day's window. The throttle
  makes both triggers cheap no-ops outside their window; this is an
  adaptation of *when* to check to Windows' process model, not "more
  checking."
- `AutoUpdateChecker::Stop()` (called from `WindowManager::DestroyAllWindows()`)
  signals and detaches rather than joins: `RunCheckOnce()` can be mid-flight
  in network I/O with timeouts up to several tens of seconds, and process
  shutdown must not wait on that. (Header comment matches this — it does not
  claim to join.)

Also closed the matching gap in the Qt Settings dialog, which is shared
cross-platform code and had partially rotted on Windows: the manual "Check
for updates…" button already detected newer releases on Windows (the
check itself, `gui/base/github_update_checker.cc`'s `CheckForUpdates()`,
already ran `curl` via `QProcess`'s safe argv-list form, cross-platform,
with no code change needed), but `DownloadAndOpenInstaller()` was `#if
defined(__APPLE__)`-only, and `onUpdateAvailable`'s auto-check-on-dialog-open
was `#ifdef __APPLE__`-only despite the check beneath it being generic --
so opening Settings on Windows never auto-checked, and a manual check that
found an update could only fall back to opening the browser. Added the
Windows branches: `QProcess` + `curl` (the same safe argv pattern already
used for the check) to download, then `QDesktopServices::openUrl` on a
`file://` URL to launch. The "Automatically check for updates (once a day)"
checkbox is now shown on Windows as well as macOS (it was previously
`#ifdef __APPLE__`-only while the renderer checker already read
`auto_check_for_updates`, which defaults to true — so Windows users would
have been prompted with no UI to turn the check off).

**Review fix (2026-08-07):** `OpenSuccessfulGet` originally returned only the
request `HINTERNET` while destroying the session and connection handles at
function exit. In WinHTTP, closing a parent handle indirectly invalidates
its children (`ERROR_INVALID_HANDLE` on later reads), so the GitHub poll and
`.msi` download would have failed silently after every successful HTTP
handshake. It now returns a `WinHttpGet` that owns the full
session → connect → request chain for the lifetime of the body read /
file write; destructors close in reverse declaration order (request,
connect, session).

Files changed: `base/marina_github_releases.{h,cc}` (additive; existing mac
functions untouched), `gui/base/github_update_checker.{h,cc}`,
`gui/config_dialog/config_dialog.cc`, `protocol/config.proto` (doc comment
only -- `auto_check_for_updates` was already a shared field, just
undocumented for Windows), `renderer/win32/window_manager.{h,cc}`, and the
`BUILD.bazel` files for `base`, `bazel/win32` (new `winhttp` target), and
`renderer/win32`.

New files: `renderer/win32/marina_auto_update.{h,cc}`,
`base/marina_github_releases_test.cc`.

macOS and Linux are untouched -- `marina_curl_fetch.cc`'s `__APPLE__` branch
and `marina_auto_update.mm` were not modified.

**Verification:** `base/marina_github_releases_test.cc` passes on macOS
(new coverage for previously-untested parsing/lookup logic).
`config_cc_proto`, `marina_semver`, and `mac:mozc_toolbar` still build
clean, confirming the proto comment edit and the additive
`marina_github_releases.cc` changes didn't disturb the mac build.
**`gui/` targets (`github_update_checker`, `config_dialog`) could not be
built even for macOS in this environment** -- the local Qt SDK checkout is
incomplete (confirmed pre-existing and unrelated to this change: the
identical failure reproduces on the pre-change tree). The
`renderer/win32/marina_auto_update.cc` WinHTTP/TaskDialog code is new API
surface for this codebase and could not be compiled at all (no MSVC
toolchain, as throughout this session) -- checked via Bazel graph queries,
a `somepath` confirmation that it links into `marinamoji_renderer.exe`, and
a declaration/definition cross-check. **Awaiting Windows CI build +
on-laptop smoke test** (update prompt, download/install, Settings
auto-check checkbox, shutdown while a check is in flight).

### Windows: uppercase macron vowels (ĀĒĪŌŪ) were unreachable

`Ctrl+Alt+Shift+`vowel produced nothing on Windows, at **every** keyboard
layout — not just the non-QWERTY layouts this was originally suspected to
affect. Lowercase `ā ē ī ō ū` worked; the uppercase forms did not.

Cause: the macron rules in the keymap TSVs are written `Ctrl Alt Shift A` ..
`Ctrl Alt Shift U`, and `KeyParser::ParseKeyVector` stores a single-glyph
token verbatim, so those rules carry an *uppercase* `key_code`. But the
alphabet path in [`keyevent_handler.cc`](src/win32/base/keyevent_handler.cc)
reports `lower_char` whenever Ctrl is held and — unlike the
Shift-without-Ctrl branch — leaves `SHIFT` in the modifier set. The resulting
event (`key_code='a'`, `{CTRL, ALT, SHIFT}`) matches no rule at all. There are
no lowercase-plus-Shift macron rows, so the chord was inert rather than firing
some other command.

The bug only bites with CapsLock **off**: with CapsLock on, the CAPS branch
emits `lower_char` and `KeyEventUtil::NormalizeModifiers` flips the case back,
which happens to produce a match.

Fixed by reporting `upper_char` for `Ctrl+Alt+Shift+`vowel, mirroring the
`macron_shift` fixup in `mac/KeyCodeMap.mm` and restricted to the same five
vowels so no other `Ctrl+Alt+Shift+`letter binding changes shape. The test is
on the produced character rather than the virtual key, because under a fixed
romaji layout (`MarinaKeyboardLayout`) the character comes from that layout's
table and it is the character, not the physical key, that the keymap matches.

No Linux-style workaround is needed: `key_translator.cc:341` remaps a Hiragana
*keysym* that X11 can produce for these chords, and Windows has no keysym
layer — it delivers virtual-key codes directly.

Two tests added:

- `session/keymap_test.cc` — `MarinaMacronVowelsRequireUppercaseKeyCode` pins
  the contract cross-platform: uppercase matches, lowercase-plus-SHIFT (the
  buggy shape) does not, lowercase-without-Shift still matches. **This one
  runs on macOS and passes**, so the premise of the fix is verified by
  execution rather than inference.
- `win32/base/keyevent_handler_test.cc` — `MarinaMacronVowelUppercaseKeyCode`
  covers the Windows handler itself across all five vowels, the no-Shift case,
  and two non-vowels (to prove the fixup is scoped). Windows CI only.

macOS and Linux are untouched.

**Verification:** the cross-platform test passes locally, along with
`session_test` and `key_parser_test`. The Windows handler and its test could
not be compiled here (no MSVC toolchain) — Bazel graph queries pass, but the
Windows test is unrun until CI.

### Windows: keystrokes no longer leak into documents during a sync

**This is a data-integrity fix, not a cosmetic one.** While marinaMojiSync
rewrites user data it takes a global lock on the session handler
(`sync/sync_runner.cc` → `Client::BeginSyncLock`), after which
`SessionHandler::SendKey`/`TestSendKey`/`SendCommand` all fail with
`Output::SYNC_LOCKED` and **no `consumed` field**
([`session_handler.cc:441`](src/session/session_handler.cc)). macOS and Linux
guard against this — macOS with a `sync.status.json` poller
(`mac/sync_overlay.mm`), Linux with both that poller *and* an explicit
`SYNC_LOCKED` check ([`mozc_engine.cc:758`](src/unix/ibus/mozc_engine.cc)).

Windows had neither. The missing `consumed` field made
[`keyevent_handler.cc:1114`](src/win32/base/keyevent_handler.cc) set
`result.succeeded = false`, which made
[`tip_keyevent_handler.cc`](src/win32/tip/tip_keyevent_handler.cc) set
`*eaten = FALSE` — handing the keystroke to the application unprocessed. The
user-visible effect: during any sync run, typing Japanese inserted raw romaji
(`nihongo` instead of にほんご) into the document, with no beep, no overlay,
and no indication why. Sync is scheduled as a logon task on Windows
(`win32/base/task_scheduler_util.cc`), so this was reachable in normal use.

Fixed with the same two-layer defence Linux uses:

- **Proactive** — new `win32/base/sync_lock_util.{h,cc}` exposes
  `IsSyncLockActive()`, wrapping `sync::IsSyncRunning()` with a 250 ms TTL
  cache (matching the mac/Linux poll interval). The cache matters: this is
  called per key event, on the TSF thread, inside the host application's
  process, so an uncached read would put disk I/O in the input hot path.
  Guards added to **both** `OnTestKey` and `OnKey` in
  `win32/tip/tip_keyevent_handler.cc` — TSF consults `OnTestKey` first and
  routes the key to the application itself if the IME declines it, so a guard
  in `OnKey` alone would never see the keystroke.
- **Reactive** — both `result.succeeded == false` paths in
  `tip_keyevent_handler.cc` now check for `Output::SYNC_LOCKED` and claim the
  key, covering a lock taken in between polls. That path also invalidates the
  cache so the proactive guard takes over from the very next key.
- **Audible feedback** — `NotifySyncBlockedInput()` issues a rate-limited
  `MessageBeep` (1 s throttle, mirroring `SyncOverlayFlashBlockedInput()` on
  mac and Linux), on key-down only.

Also added the missing visible overlay: new
`renderer/win32/sync_overlay_window.{h,cc}`, a borderless, click-through,
always-on-top window centred on the work area showing the sync message.
Hosted in the renderer rather than the TIP because the TIP DLL loads into
every application process (same reasoning as the floating toolbar), and it
polls `sync.status.json` on a 250 ms `WM_TIMER` exactly as the mac/Linux
watchers do — so it needs no IPC and stays visible for the whole sync rather
than only when a key is pressed. Deliberately excluded from
`WindowManager::HideAllWindows()`: it reports that input is being held, which
is precisely the situation where the IME has no focus. It uses a uniform
`LWA_ALPHA` translucency plus a rounded region rather than mac/Linux's opaque
label over a 55 %-alpha backdrop, since matching that exactly would require
compositing text into a premultiplied DIB and GDI text drawing does not
maintain the alpha channel.

macOS and Linux are untouched. (Noted while investigating, not changed:
macOS relies solely on its proactive poll, so it has a narrow race if a sync
begins between the poll and a keystroke — microseconds of exposure versus the
whole sync duration on Windows.)

New files: `win32/base/sync_lock_util.{h,cc}`,
`renderer/win32/sync_overlay_window.{h,cc}`. Modified:
`win32/tip/tip_keyevent_handler.cc`, `renderer/win32/window_manager.{h,cc}`,
`base/const.h`, and the `BUILD.bazel` files for `sync` (visibility),
`win32/base`, `win32/tip`, and `renderer/win32`.

**Verification:** same constraint as below — the Windows files could not be
compiled here (no MSVC toolchain). Checked via Bazel graph queries on all new
targets, `somepath` confirmation that the overlay links into
`marinamoji_renderer.exe` and that `sync_lock_util` reaches the TIP,
declaration-vs-definition cross-checks, sync API signature checks, and
dep-ordering checks. **Needs a real Windows build and a manual test against a
running sync before it is trusted.**

### Windows floating toolbar: parity pass with macOS, plus fixes

The Windows toolbar (`renderer/win32/toolbar_window.cc`) and Symbols Palette
(`renderer/win32/symbols_palette_window.cc`) were compared against the macOS
implementation (`mac/mozc_toolbar.mm`) and brought up to parity, with a
number of pre-existing bugs fixed along the way. **macOS and Linux/GTK
toolbar code is untouched** — the shared pieces below are new, Windows-only
modules, not refactors of the mac/Linux implementations.

Bugs fixed:

- Symbols Palette: window size no longer omits the non-client area
  (caption/borders), which was clipping the bottom row of symbols.
- Symbols Palette: content taller than the window now scrolls instead of
  being silently cut off (e.g. a long `user_symbols.txt`).
- Symbols Palette: child controls now get an explicit `WM_SETFONT` (shell UI
  font, enlarged for symbol glyphs) instead of falling back to the legacy
  bitmap `SYSTEM_FONT`, which rendered CJK/symbol glyphs poorly.
- Symbols Palette: full per-monitor DPI scaling (previously all metrics were
  raw, unscaled pixels).
- Symbols Palette: dark mode is now respected (background, text, and common
  controls via `DarkMode_Explorer` theming); previously always light.
- Toolbar: DPI is now re-read after a drag and on `WM_DPICHANGED`, so moving
  the toolbar to a monitor with a different scale factor rescales it.
  Previously only `WM_DISPLAYCHANGE` (a resolution change) triggered a
  refresh.
- Toolbar: icon-tier selection now rounds ties up (was rounding down),
  fixing under-scaled icons at 125% and 175% Windows scaling.
- Toolbar: `SavePosition()` no longer truncates `toolbar.conf` down to just
  `x=`/`y=`, which was silently dropping the `toolbar_visible` key written
  by the TIP from a different process.
- Toolbar: a position restored from `toolbar.conf` is now clamped back onto
  a currently-attached monitor, so unplugging the monitor the toolbar was on
  can no longer strand it off-screen with no way to drag it back.
- Toolbar: a `Hide()` suppressed while the mode menu is open is now
  re-applied once the menu closes, instead of being dropped — previously the
  toolbar could stay on screen indefinitely after a focus change during menu
  use.
- Toolbar: renamed the `LoadIcon` helper to `LoadToolbarIcon` — it was
  silently colliding with the `<winuser.h>` `LoadIcon`/`LoadIconW` macro.
- `win32/base/toolbar_config.cc`: `LoadToolbarVisiblePreference()` is now
  cached with a 1-second TTL instead of hitting disk on every renderer
  update (i.e. every keystroke, on the TSF thread, inside the host
  application's process). The TTL keeps cross-process propagation working
  (the TIP loads separately into every application).

UX/feature parity added:

- Toolbar: hover and pressed-state highlighting on all six buttons (custom
  GDI-composited, since the toolbar has no child HWNDs).
- Toolbar: tooltips on every button, via a `TOOLTIPS_CLASS` control.
- Toolbar: an `IAccessible` (MSAA) implementation
  (`renderer/win32/toolbar_accessible.{h,cc}`) exposing the toolbar as a
  `ROLE_SYSTEM_TOOLBAR` with six `ROLE_SYSTEM_PUSHBUTTON` children — the
  toolbar was previously invisible to screen readers/UI Automation, since it
  draws everything into one composited bitmap. `accDoDefaultAction` posts
  back to the toolbar's own thread (MSAA calls arrive on an RPC thread).
- Toolbar: right-click context menu (Dictionary Tool, Keyboard Shortcuts,
  Settings, Hide toolbar) — previously the Dictionary Tool and "hide
  toolbar" were unreachable from the toolbar itself.
- Toolbar and Symbols Palette: localized strings (EN/FR/JA), via new
  `renderer/win32/marina_localized_string.{h,cc}` — previously all UI text
  was hardcoded English literals. Mirrors the `MM.*` keys used by
  `mac/Resources/{en,fr,ja}.lproj/Localizable.strings` so wording stays in
  step across platforms, without sharing code (this file is Windows-only).
- **New: Keyboard Shortcuts viewer for Windows**
  (`renderer/win32/shortcuts_window.{h,cc}`), reachable from the toolbar's
  Shortcuts button and its context menu. Mirrors macOS's
  `MozcShortcutsWindowController` (Script / Composition / Kaeriten tabs).
  Supporting plumbing:
  - New `session/marina_shortcut_list_util.{h,cc}` (Windows-only,
    `visibility = ["//win32/tip:__pkg__"]`): builds the three shortcut
    lists from the effective keymap table, marina number-row bindings, and
    kaeriten table. Modelled on macOS's private helpers in
    `mac/mozc_toolbar.mm` but implemented separately — reads keymap tables
    via `ConfigFileStream`'s `system://` scheme rather than loose files,
    and de-duplicates `(command, key)` pairs. Has its own test suite
    (`marina_shortcut_list_util_test.cc`, 9 cases).
  - New `RendererCommand::ApplicationInfo::ShortcutsInfo` proto message
    (`protocol/renderer_command.proto`), populated TIP-side
    (`win32/tip/tip_ui_handler_conventional.cc`'s `FillShortcutsInfo`) since
    the renderer deliberately doesn't link config/keymap parsing.
  - New `SessionCommand` types `SHOW_SHORTCUTS_WINDOW` /
    `HIDE_SHORTCUTS_WINDOW` (local UI-visibility signals, intercepted in
    `win32/tip/tip_edit_session.cc`, same pattern as the existing
    `SHOW_SYMBOLS_PALETTE`/`HIDE_SYMBOLS_PALETTE`) and a new
    `TipPrivateContext::shortcuts_window_visible()` flag.
- New `SessionCommand::LAUNCH_DICTIONARY_TOOL` (wired into
  `session/session.cc`'s `Session::LaunchDictionaryTool`) and
  `SessionCommand::HIDE_TOOLBAR` (writes the `toolbar_visible` preference
  from the TIP side), both reachable from the toolbar's new context menu.
- `renderer/renderer_server.cc`: allowlisted and routed the four new
  `SessionCommand` types above through the existing renderer→TIP channel.

Files changed: `renderer/win32/toolbar_window.{h,cc}`,
`renderer/win32/symbols_palette_window.{h,cc}`,
`renderer/win32/window_manager.{h,cc}`, `renderer/renderer_server.cc`,
`session/session.cc`, `win32/base/toolbar_config.cc`,
`win32/tip/tip_edit_session.cc`, `win32/tip/tip_private_context.{h,cc}`,
`win32/tip/tip_ui_handler_conventional.cc`, `protocol/commands.proto`,
`protocol/renderer_command.proto`, `base/const.h`, plus the `BUILD.bazel`
files for `bazel/win32`, `composer`, `renderer/win32`, `session`, and
`win32/tip`.

New files: `renderer/win32/marina_localized_string.{h,cc}`,
`renderer/win32/toolbar_accessible.{h,cc}`,
`renderer/win32/shortcuts_window.{h,cc}`,
`session/marina_shortcut_list_util.{h,cc}`,
`session/marina_shortcut_list_util_test.cc`.

**Verification:** the cross-platform pieces (both edited protos, `session`
and `session_test`, `renderer_server`, `mac:mozc_toolbar`, and
`marina_shortcut_list_util` with its new 9-test suite) were built and
tested on macOS. The Windows-only files could not be compiled in this
environment (no MSVC toolchain available to Bazel here) — they were instead
checked via Bazel `BUILD`-graph queries on every touched target, a
`somepath` check confirming the new shortcuts window links into
`marinamoji_renderer.exe`, a declaration-vs-definition cross-check across
all three window classes, a localization-key audit, a generated-proto
accessor check against the built `.pb.h`, and a dep-ordering check across
all `BUILD.bazel` lists touched. **This is not a substitute for an actual
Windows build**, which should happen before this is trusted.
