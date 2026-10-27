# Workspace layout repair — upper pinned region vs lower full-width region

This narrow UI follow-up to PR #34 keeps the approved behavior:
* Cue/row headers stay pinned on the left during horizontal column scrolling.
* The central utility strips are clipped at the left edge of the fixed BPM / Auto Stop band, and Master Audio stays visible.

## Lower-panel geometry

The **upper** region consists of pinned cue header (290 px), scrolling cue/utility columns, then pinned BPM/Auto Stop (184 px). These containers retain independent horizontal scroll/clipping bounds.

The **lower** region contains the Timeline and docked MIDI/OSC editor. It now uses the entire workspace width up to the Inspector, **including** the 184 px otherwise reserved for the upper BPM band. BPM/Auto Stop has no lower-panel ownership. MIDI/OSC window/undocked behaviors are unchanged.

## Timeline resizing

A discreet draggable horizontal handle lives at the top edge of the Timeline, across both docked and undocked states. A drag upward enlarges the Timeline; downward shrinks it. The geometry helper applies a bounded adjustment to the available upper-grid height, keeps the fixed cue header top unchanged and preserves the minimum 310 px upper grid allocation and 75 px Timeline. The existing docked-editor height preference and its separate resize handle are maintained.

The Timeline detaching feature is **not** part of this change (optional in the request).

## Column and Master restoration

All expanded Audio/MIDI/OSC/Control columns return to **152 px**, from the incorrect 136 px compact variant. The original 92 px stereo fader, 49 px panpot, output and row hitbox geometry are reinstated. Collapsed columns remain 48 px and the previously corrected MIDI/OSC mute vertical baseline is preserved.

The Master pinned strip retains a compact width of 290 px, without squeezing the operational columns. Master Mute is centered beneath Pan, following its value label; redundant “PRE-LIMIT L/R” and “MIDI and OSC unaffected” text is removed.

Workspace gutters are just 2 px beyond the 180 px sidebar and 2 px below the top toolbar. The Inspector starts at y=76, directly beneath that top toolbar. No artificial negative offsets or global clipping masks were added.

## Regression gates

The geometry regression suite checks restored wide columns, full-width Timeline/docked editors on multiple screen widths and independent upper clipped regions, the Timeline vertical resize constraints, actual mouse drag behavior, pinned headers and Master/column control bounds.

Release acceptance remains: CI Windows Release plus CTest, and manual visual/audio/device validation at 1180×820, 1440×900, 1647×895 and wider desktop resolutions, including many collapsed and expanded Audio/MIDI/OSC columns. The audio engine callback, show version, cue transport/GO/PANIC, MIDI/OSC queues and routing are untouched. No portable build was requested.
