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
- **0.7 stability preview:** periodic validated crash-recovery checkpoints (prompt on next launch); explicit Preflight report for media and device readiness; Diagnostics for local audio/MIDI/OSC counts; show preparation before interrupting currently loaded playback; a CI portable startup/shutdown smoke test.
- **0.8 architecture/integration preview:** a dedicated project media loader with immutable PCM deduplication and transactional memory checks, a bounded background recovery checkpoint writer, shared block timing for MIDI/OSC, timestamp-delay counters, exact-ID reconnection for both MIDI input and output, and real loopback UDP integration tests. An optional Windows hardware inventory tool is supplied for on-site checks; real hardware soak and latency tests are still required.

**Development status:** **0.8.0-alpha.1 (architecture and integration preview)**; product, Windows package and release identifiers use the single `VERSION.txt` source. The 0.8 architecture/timing work is partially implemented and still undergoing validation, not a stable release. Show format **v10** with optional Control Column fields can load projects from v1–v9 and earlier v10 documents. Windows CI builds and automated tests have succeeded, but a complete test on live production hardware has not been carried out. Video, timecode, and MIDI controller feedback are not yet implemented. Control Mapping remains experimental: do not use it in a live show without thorough rehearsal and validation.

For license terms, see the [English reference translation](LICENSE.en.md) and the authoritative [Italian original](LICENSE).

See [the complete user guide](docs/guida-completa.md) for operating instructions, build details, and limits.

## Recovery, safety and release maturity

This is a **0.8.0-alpha.1 preview**, not a certified production release. Click **Preflight** before rehearsals to inspect missing files and configured device availability; click **Diagnostics** to examine local counters. The app creates an atomic recovery checkpoint approximately every 20 seconds while the project is marked unsaved. If the process crashes, the next launch offers **Restore**, **Discard checkpoint**, or **Later**. Restoring never starts playback or overwrites the original file. Save the recovered show explicitly to keep it. Clean shutdown clears the checkpoint; a deferred checkpoint can be retained using **Later**.

For background, limitations and hardware validation procedures, read [Stabilization & MIDI/OSC Timing](docs/stability-and-timing.md). The checkpoint records model edits that have been applied to the project; text still typed into an uncommitted Inspector field is not independently captured. Local OSC “sent” does not guarantee reception by another program.

**Version rule:** the human-facing prerelease version is stored in `VERSION.txt`, synchronized with CMake's numeric version and consumed by the Windows packaging scripts. Keep those files consistent for future prereleases. The serialized project schema remains v10, independent of the application version.

## Local 0.8 hardware checks

On the actual show PC, run `powershell -ExecutionPolicy Bypass -File scripts/hardware-check.ps1` to compile and run a read-only inventory of audio/MIDI devices. Add `-OscLoopback` to test UDP transmission to the local machine (never to external targets). Reports are stored under `build/hardware-reports/`. Follow the [0.8 integration checklist](docs/0.8-implementation.md) for manual GO/PANIC, MIDI unplug/reconnect and OSC target tests. No test suite on a GitHub runner can certify the physical interfaces used at your venue.
