# Unicode UI symbols — implementation audit

This document describes the explicit glyph choices used by the desktop UI.
It is **not** a claim that every Unicode character is supported by one font.

## Typeface policy

- **Exo 2:** compact operational text (at least 10 pt).
- **Hind:** larger labels and headings.
- **Noto Sans Math:** mathematical operators (e.g. `−∞` in faders and Fader Range).
- **Noto Sans Symbols 2:** designated graphical symbols only; no global Unicode fallback.

The app embeds pinned OFL font binaries through CMake; see `CMakeLists.txt`
and `NOTICE.md`. `drawNotoSymbol()` checks
`getNominalGlyphForCodepoint()` before explicitly drawing from the
embedded Symbols 2 typeface. Each UI caller preserves a vector fallback,
rather than showing a missing-glyph box or relying on operating-system
emoji fonts. Availability is tested against the font **actually bundled**
with each build.

## Glyph assignments

| Interface | Unicode | Role | Fallback if missing |
| --- | --- | --- | --- |
| Row reorder | ▲ U+25B2 / ▼ U+25BC | Up / down | Vector triangles |
| Column reorder | ◀ U+25C0 / ▶ U+25B6 | Left / right | Vector triangles |
| Cue/Cell Play | ▶ U+25B6 | Start/resume | Vector triangle |
| Cue Pause | ⏸ U+23F8 | Pause | Vector pause bars |
| Cue/Cell Stop | ⏹ U+23F9 | Stop | Vector square |
| Row actions | ⋯ U+22EF | More actions | Three vector dots |
| Master/Audio/MIDI/OSC mute | 🔊 U+1F50A / 🔇 U+1F507 | Output active / muted | Vector speaker with waves / slash |
| About/Diagnostics | ℹ U+2139 | Information | Existing compact circled 'i' |
| Unsaved changes | ⚠ U+26A0 | Caution | Existing vector exclamation |

The mute controls use the same shared `UtilityIcon` class and switch
glyphs with their underlying toggle state. The words "Mute", "Snap",
"Play" and other text labels remain ordinary text, not Symbols 2.

## Deliberately retained vector assets

- **Snap magnet:** U+1F9F2 is not covered by the pinned Symbols 2 font;
  a high-contrast vector magnet, with a slash when disabled, is clearer
  at 41×25 logical pixels.
- **Loop indicator:** its custom arc and arrow are tuned for 13–15 px.
- **Delete cross, control lock, phase inversion and Inspector grip:** preserve
  predictable stroke width, semantics and hitbox alignment.
- **Add/remove arithmetic signs and text-only toolbar actions:** preserve
  the current accessible text and spacing; don't replace them with
  unrelated decorative Unicode.

## QA

1. Compile with the pinned fonts and run all native UI tests.
2. Check render at 100%, 125% and 150% Windows scaling.
3. Compare selected/unselected and enabled/disabled buttons; never
   mistake a colour-only change for a state change.
4. Verify no missing glyph boxes, colour emoji substitutions, clipped
   controls or changed click areas.
5. Check labels, native accessibility names and playback behavior remain
   unchanged.

See also `docs/0.9-release-checklist.md`.
