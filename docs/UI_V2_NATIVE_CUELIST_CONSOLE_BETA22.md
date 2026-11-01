# THE CUELIST — Native V2 / beta.22: Cuelist console and fixed Master Audio

Baseline: `main` 0.9.0-beta.21. This revision follows the visual reference of the operator's left Cuelist column, whose Master Audio panel is permanently pinned at the bottom, with horizontally scrolling media strips only to its right.

## Baseline CI finding

Windows Actions run **41341498221** compiled the program successfully but **1 of 406648 assertions** failed in the CTest suite: `Compact Master Mute is centred below Pan and not over its label`. The beta.21 layout positioned Mute at y142–148 and Pan at y58–101, making the separation less than the proven minimum of 55px.

## Actual modifications

**Master:** in `MasterAudioLayout::atHeight`, Pan is now at y43 with height43. Mute and limiter start at `min(148, height-38)` and end before the card bottom, even with a 180px utility panel. Pan label is y25–41, knob y43–86, pan numeric readout y91–108, mute caption y121/127–138/144 and Mute button y142/148–173/179. None of these interactable areas or annotations overlap. Master L/R signal processing, gain/meter, limiter reduction, mute, output selection and Quick Learn handlers are untouched. The Master stays fixed at the left 318px with `scrollableStrips` x=318; no reparenting or new scrolling container.

**Cuelist heading:** preserve height72 and all existing edit, move, collapse and transport hitboxes. Slightly refine the title line, cue/clip count, and identity rail; remove the redundant third text line `CUE / TRANSPORT`, replacing it with a subtle 1px lower rule. No new controls or popup windows.

## Validation contract

`tests/UtilityPolishTests.h` asserts the exact Pan-to-Mute minimum clearance, correct vertical order of the Pan caption, knob, value, Mute caption and button, clipping inside the fixed card, and no output overlap at 1280×768, 1440×820, 1440×900 and 1920×1080. Existing `UiRefinementTests` assert Master pinning and all five real controls up to 3840×1080. The full Windows CI/CTest suite must succeed before integration.

No portable build, engine modification, master bus routing change, MIDI/OSC/control schema edit or change to the per-cue JSON data. Visual checks at normal/150% DPI and ultrawide remain required for release.
