# Macron vowels

## Right Shift tap (Direct input)

This is the default way to type ā ē ī ō ū on **Windows, macOS, and Linux**.

In **Direct input**, tap **Right Shift** on its own (press and release without
typing anything else) and then type a vowel:

| Keys | Output |
|------|--------|
| Right Shift, then a | ā |
| Right Shift, then Shift+A | Ā |

While the dead key is armed, a **¯** placeholder (U+00AF MACRON) appears on
screen so you can see that a macron is pending. It was previously **◌̄**
(dotted circle + combining macron), the usual way to show a diacritic with no
base letter, but Windows renders the dotted circle at full size beside the bar
rather than under it. Because the vowel's case comes from ordinary
Shift, you can tap Right Shift and then simply *hold* Right Shift for the vowel
to get the capital form (Ō).

- **Esc** or **Backspace** cancels the pending macron: the placeholder
  disappears and nothing is inserted. Backspace here only clears the
  placeholder — it never deletes the character before it.
- Any other non-vowel key cancels the pending state and is handled normally.
- This is Direct input only. In Hiragana/Katakana/Manyōshū modes, Right Shift
  alone keeps its usual Hiragana ↔ Manyōshū toggle; in ASCII composition modes
  it is still passed through to the application (use **AltGr+¨** then a vowel
  there, or switch to Direct input).
- **Windows StickyKeys:** tapping Shift five times in a row is a Windows
  accessibility gesture that normally pops up a "turn on StickyKeys?" dialog
  -- a pattern our own Left/Right Shift tap shortcuts can trigger by
  accident. marinaMoji's renderer process turns off that specific popup for
  as long as it's running (see `win32/base/sticky_keys_util.h`) and restores
  the previous setting on exit; it never touches whether StickyKeys itself is
  on. If you use StickyKeys deliberately, a plain Shift tap latches Shift for
  the next key, so Right Shift, a can land as Ā instead of ā -- the Settings
  → Shortcuts tab shows a warning with a link to Ease of Access settings when
  it detects this.

`Ctrl+Alt`+vowel is **not** bound. That chord used to insert macrons in Direct
input and ASCII composition; it collided with desktop shortcuts and with
AZERTY `AltGr` (which Windows reports as Ctrl+Alt). The Right Shift dead key
replaced it.

`Ctrl+Alt+Right Shift` on Windows is unrelated: it toggles Left Shift mode
lock, the same as a double-tap of Left Shift.

## Macron dead key (AltGr+umlaut or AltGr+circumflex)

On layouts where **AltGr+umlaut** (¨, U+00A8) is easy to type (e.g. French AZERTY: key next to P), you can use it as a **dead key** for macrons:

1. Press **AltGr+¨** (nothing is inserted).
2. Press **a**, **e**, **i**, **o**, or **u** (with or without Shift) → the corresponding macron vowel is inserted (ā ē ī ō ū or Ā Ē Ī Ō Ū).

On some layouts (e.g. Dvorak or others where the macron dead key is on **^**), use **AltGr+^** (circumflex) the same way: then type a vowel to get the macron form.

This works in Composition, Conversion, Precomposition, and Direct input. If the next key is not a vowel, the dead state is cancelled and the key is handled normally.

- **AltGr+deadkey still not working:** If **AltGr+¨** or **AltGr+^** does nothing, your layout may send a different keysym for the dead key or AltGr may not set the modifiers we use. Check the debug log (see development docs) for the keyval and modifiers received; we support diaeresis keyval 0xA8 and 0xFE20, and circumflex 0x5E and 0xFE22.
