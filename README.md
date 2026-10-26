# The Cuelist

**The Cuelist** is a Windows desktop application for **controlling audio and MIDI cues in theatre productions, concerts, and live events**. It lets operators arrange, start, stop, and overlap sound cues from a single interface.

It is designed for show playback control without requiring a full digital audio workstation (DAW).

## Main features

- **Cue list:** row-and-column cue grid with GO, Stop, and Panic; cell copy/paste (Ctrl+C/V); row menus for copying, pasting, duplicating, and deleting cues; directly editable cue BPM; and selective Auto Stop options beneath each row's toggle.
- **Audio:** WAV/MP3 playback, loops, compact channel strips with an integrated stereo GainMeterFader (L/R dBFS meters, dB gain slider, shared vertical scale), rotary pan and output routing. The Master in the utilities area has the same integrated fader, plus Master Pan and Mute, and does not affect MIDI/OSC.
- **MIDI:** import and create note clips using the built-in Piano Roll, with external MIDI output selection.
- **OSC:** visual Clip Editor with instant OSC events and integer/float argument automation (ramps and multipoint curves), int/float/string/bool arguments, a seconds-or-beats timeline, Undo/Redo, and per-column UDP destinations.
- **Timeline:** clip placement and zoom, right-edge resizing of MIDI/OSC clips, independent BPM grid visibility and beat snapping.
- **Control Mapping (initial implementation):** manually selected physical MIDI input, MIDI Learn for CC, notes and Pitch Bend, cue presets, cue/preset/column/global mapping priorities, Pickup/Jump takeover, and targets for Master, Audio column gain, GO, Panic, MIDI CC, and OSC Float.
- **Branding:** approved transparent icon included as `assets/thecuelist_icon.png` and embedded in the GUI/executable.
- **Portable projects:** save shows as `.cuelist` files, alongside their media folder and backups.

**Development status:** **0.6 (initial Control & Mapping v10)**; the executable is still identified as **0.5.0**. Show format **v10** can load projects from v1–v9. Windows CI builds and automated tests have succeeded, but a complete test on live production hardware has not been carried out. Video, timecode, and MIDI controller feedback are not yet implemented. Control Mapping remains experimental: do not use it in a live show without thorough rehearsal and validation.

For license terms, see the [English reference translation](LICENSE.en.md) and the authoritative [Italian original](LICENSE).

See [the complete user guide](docs/guida-completa.md) for operating instructions, build details, and limits.
