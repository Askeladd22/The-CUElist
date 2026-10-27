# The Cuelist

**The Cuelist** is a Windows desktop application for **controlling audio and MIDI cues in theatre productions, concerts, and live events**. It lets operators arrange, start, stop, and overlap sound cues from a single interface.

It is designed for show playback control without requiring a full digital audio workstation (DAW).

## Main features

- **Cue list:** row-and-column cue grid with GO, Stop, and Panic; cell copy/paste (Ctrl+C/V); row menus for copying, pasting, duplicating, and deleting cues; directly editable cue BPM; and selective Auto Stop options beneath each row's toggle.
- **Audio:** WAV/MP3 playback, loops, compact channel strips with an integrated stereo GainMeterFader (L/R dBFS meters, dB gain slider, shared vertical scale), rotary pan and output routing. The Master in the utilities area has the same integrated fader, plus Master Pan and Mute, and does not affect MIDI/OSC.
- **MIDI:** import and create note clips using the built-in Piano Roll, with external MIDI output selection.
- **OSC (0.7 UX revision):** unified vertical scrolling across EVENTS and automation tracks; shared horizontal zoom/pan for ruler, events and ramps; persistent address/argument tracks containing multiple nonoverlapping segments; one-second 0→1 ramps on double-click; point editing/deletion, independent endpoints and non-stretch resize; typed OSC arguments (int/float/string/bool), Undo/Redo and per-column UDP destinations.
- **Timeline:** clip placement and zoom, right-edge resizing of MIDI/OSC clips, independent BPM grid visibility and beat snapping.
- **Control Columns (Road to 0.7):** create multiple Control columns in the cue grid, each with one or more cue-specific Control Sets. **Lock** a column to promote the current cue's set to a low-priority default for every cue; unlock to restore cue-specific sets. Unlocked columns support **Medium**, **High**, and **Maximum** priorities; mappings override only the same physical MIDI input, leaving unrelated faders inherited from lower layers. Equal-priority conflicts resolve to the rightmost Control Column. Edits are activated by confirmed **GO**, not merely selecting a cue. Control Sets support audio/master gain and pan, MIDI CC, OSC float, GO and Panic through MIDI Learn. The floating editor provides range limits, Linear/Ease In/Ease Out curves, Pickup/Jump takeover, mapping updates, inherited/override previews and a reusable Save Set / Apply Set library.
- **Control Mapping (initial implementation):** manually selected physical MIDI input, MIDI Learn for CC, notes and Pitch Bend, cue presets, cue/preset/column/global mapping priorities, Pickup/Jump takeover, and targets for Master, Audio column gain, GO, Panic, MIDI CC, and OSC Float.
- **Branding:** approved transparent icon included as `assets/thecuelist_icon.png` and embedded in the GUI/executable.
- **Portable projects:** save shows as `.cuelist` files, alongside their media folder and backups.

**Development status:** **0.7.0-alpha.1 (stabilization preview)**; product, Windows package and release identifiers use the single `VERSION.txt` source. The 0.8 architecture/timing work is preliminary, not a complete 0.8 release. Show format **v10** with optional Control Column fields can load projects from v1–v9 and earlier v10 documents. Windows CI builds and automated tests have succeeded, but a complete test on live production hardware has not been carried out. Video, timecode, and MIDI controller feedback are not yet implemented. Control Mapping remains experimental: do not use it in a live show without thorough rehearsal and validation.

For license terms, see the [English reference translation](LICENSE.en.md) and the authoritative [Italian original](LICENSE).

See [the complete user guide](docs/guida-completa.md) for operating instructions, build details, and limits.
