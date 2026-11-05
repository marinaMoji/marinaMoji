# marinaMoji privacy notice

Source text for the network-features section of the website privacy policy
(`user/pages/10.privacy` in the site repo, EN/FR/JA) and for the in-app
"Prefer GlyphWiki drawings" explanation. Keep the three in step.

marinaMoji runs on your computer. What you type, your dictionaries and your
conversion history stay on your computer. marinaMoji has no accounts, no
analytics and no telemetry.

There are two features that can contact the internet. Both are described
here, along with exactly what they send.

## 1. GlyphWiki drawings (optional, off by default)

**Preferences → Writing → "Prefer GlyphWiki drawings"**

When this is on, the character composer can show the glyph that
[GlyphWiki](https://glyphwiki.org) has registered for a character, instead of
drawing it from the structure you built.

- **When a request is made.** Only when the setting is on, the structure you
  built matches a Unicode character, and neither the bundled data nor the
  local cache already has that glyph. Structures with no Unicode encoding are
  always drawn on your computer and never sent anywhere.
- **What is sent.** A request to `https://glyphwiki.org/api/glyph` containing
  the glyph name (for example `u6728`, or a component name such as
  `u6728-01`). The request identifies itself as
  `marinaMoji-character-composer`. Like any web request, it also reveals your
  IP address to GlyphWiki. Nothing else is sent: not the text you are typing,
  your dictionaries, or any identifier of you or your device.
- **What this can reveal.** The glyph names reveal which characters you looked
  up in the composer.
- **What is kept.** The glyph data GlyphWiki returns is stored in the
  `glyphwiki_cache` folder of your marinaMoji profile, so each glyph is
  requested at most once. You can delete that folder at any time.
- **Who sees it.** GlyphWiki is run by a third party and handles requests
  under its own policies, which marinaMoji does not control.
- **Turning it off.** Untick the setting. No further requests are made.
  Already-cached glyphs stay on your computer until you delete them.

## 2. Update checks (on by default)

**Preferences → Application → "Check for updates automatically"**

marinaMoji asks GitHub's public API
(`https://api.github.com/repos/marinaMoji/marinaMoji/releases`) whether a
newer release exists. The request carries no information about your text or
settings; GitHub sees your IP address, as with any web request. You can turn
automatic checks off; "Check for updates…" then only runs when you choose it.

## What marinaMoji never sends

Your typed text, your user dictionary, your conversion history, your sync key
or your sync file. Encrypted sync, if you enable it, writes one encrypted file
to a folder you choose and does not use a marinaMoji server.

## Changes

If a future version adds another network feature, it will be off by default
unless it is needed for updates, and will be added to this notice before it
ships.

---
