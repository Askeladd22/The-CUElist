# The Cuelist — Complete Guide

This page describes current 0.8.0-alpha.1 behavior and retains historical context for earlier features. See the [0.8 architecture/integration notes](0.8-implementation.md) for new reliability features and hardware testing.

**0.8.0-alpha.1 · Windows x64 · Portable · C++17 / JUCE 8 · Proprietary license**

The Cuelist controls live performances through a cue grid, audio clips, and a timeline. It plays WAV and MP3 files but does not support video. It also supports MIDI note clips with a built-in piano roll. OSC columns send UDP messages to a host/IP address and port configured in the column utilities. OSC clips may contain timed events in seconds or beats, with typed integer, float, string, and boolean arguments.

## Quick Start

1. Extract **all** files from `The-Cuelist-0.8.0-alpha.1-Windows-x64.zip` into a writable folder.
2. Open **The Cuelist.exe** or **Avvia.cmd**.
3. Choose a device in **Audio devices**, import a WAV/MP3 file, and select the column's **Out** destination in its utilities.

You do **not** need Visual Studio, Build Tools, CMake, Git, or Python to *run* the program, and there is no installer. A source-only archive does not automatically contain a compiled executable. Use the portable package from the `dist` folder.

The C/C++ runtime is statically linked; the verified executable depends only on Windows DLLs. `Avvia.cmd` looks for the executable next to the launcher, in `portable`, or in a local build folder. It never compiles the program.

## Downloading the Portable Executable from GitHub Actions

The **Build Windows portable** workflow builds on Windows x64, runs automated tests, creates the ZIP, and verifies the archive contents and checksums. The pipeline performs an offline GUI startup/shutdown check (`--offline`) with `scripts/check-package.ps1 -SmokeLaunch`. Visual Studio is not required to *use* the package.

1. Open the repository's **Actions** tab and select **Build Windows portable**.
2. Open a successful run (green check mark). Under **Artifacts**, download **The-Cuelist-Windows-x64-portable**.
3. Extract the downloaded artifact. It contains **The-Cuelist-0.8.0-alpha.1-Windows-x64.zip** and a `.sha256` checksum.
4. Extract that second ZIP to any folder, including a USB drive, and open **The Cuelist.exe**. No installation is required.

The workflow runs on commits to `feat/desktop-foundation`, pull requests targeting that branch, or manual dispatches from Actions. GitHub artifacts expire after 30 days; download the ZIP for long-term storage.

**Data portability:** the program does not require a user profile to store shows. Save to an accessible location, such as a folder near the executable. Always copy the `.cuelist` file *together with* its `_media` directory. Media paths within the show are relative. Audio/MIDI device configuration may depend on the PC hardware, while Windows and its drivers may store separate system settings.

## Control Columns and Control Sets (Road to 0.7)

A **Control Column** is a fourth column type, alongside Audio, MIDI and OSC. Its cells contain **Control Sets** (collections of physical MIDI-input mappings), not playback clips. Control Columns do not introduce another transport: they select mappings for the cue currently confirmed by GO, using the existing input worker, control mapping engine and audio/MIDI/OSC output paths.

1. Click **+ Column → Control**; create more than one Control Column if you need layers. Click a Control cell, double-click it, or use **Edit Q set** under its Column Utilities to open the floating editor. The toolbar **CONTROL MAPPING** also edits the selected set while a Control Column is selected; from other columns, it opens the legacy mapping view.
2. Select the physical MIDI device and choose **MIDI Learn**. Move a fader or press a button to capture its ID, message kind, channel and CC/note number. The status line identifies the selected mapping, or indicates that you are learning a new mapping. Choose the target (Audio/Master gain or pan, GO, Panic, MIDI CC, OSC Float), range, response curve (**Linear, Ease in, Ease out**), inversion and **Pickup / Jump**. **Add mapping** adds to this cell's set without removing other mappings. Select a rule in the list to change its parameters and use **Update selected**, or **Remove selected** to delete it.
3. To create a default, select the cue cell containing the desired set, then click **Lock** under that column. A copy becomes the column's default and takes effect across **all cues**; the priority automatically becomes **Low**. Cue-specific sets are kept as dormant data for future unlocking. Unlock re-enables cue-specific editing. Locked cells display **DEFAULT / LOCKED** and their set is edited through **Edit default**.
4. Other, unlocked Control Columns have **Medium, High, or Maximum** priority, selected in the column utilities. A higher-priority set overrides **only the same physical input** (device, MIDI kind, channel and number). Unmentioned inputs inherit mappings from lower-priority Control Columns. A second Control Column can therefore remap one fader for Q5 while the other three continue to control their original channels.
5. **Equal-priority rule:** if two columns map the same physical input with the same priority, the **rightmost Control Column in the visible cue grid wins**. Reordering the columns changes this tie-breaker. Priority always wins before column order.
6. The floating editor's **effective mapping preview** shows the selected cue (marked **PREVIEW** until GO actually activates it), each winning input, its source Control Column, priority and whether it is **LOCAL** or **INHERITED**. This preview does **not** activate a cue. Actual mapping changes occur only after GO has been acknowledged by the audio transport.
7. Enter a name under **Reusable Set name** and click **Save Set** to store a reusable configuration in the project's Control Set library, independently of columns and cue positioning. Choose it from **Choose saved set** and click **Apply Set** to copy that content into the selected cell (or the locked column default). Reused bindings receive fresh IDs. Ctrl+C / Ctrl+V also supports copying Control cells into other Control cells when unlocked.

**Takeover:** **Jump** immediately applies the physical control's current value. **Pickup** waits for the physical value to meet/cross the current destination's value; this protects against abrupt fader jumps when changing cues. Curves and inversion are applied to the mapped value, independently of the captured physical input. A row click alone never changes live mapping.

**OSC example:** a MIDI fader can send an OSC float to `/composition/opacity` using the OSC column's configured UDP host and port. This does **not** mean that native video playback or native video opacity is implemented inside The Cuelist; the actual receiver (for example, show-control software) interprets the OSC message.

**Compatibility and limits:** the show remains JSON `.cuelist` v10. The new column, optional cell/control-template fields and optional response-curve parameter are additive; older v10 projects load with empty Control Columns and a linear curve by default. Files containing new Control Column types require a version of The Cuelist that understands them. MIDI input selection and output routing still depend on the computer's hardware. The system is experimental: test pickup, OSC destination, physical fader latency and controller disconnects before live use. No automatic motorized-controller feedback or sample-accurate parameter automation is promised.

## Control & Mapping (Initial Implementation)

**CONTROL MAPPING** in the top bar opens the physical-controller mapping panel. This subsystem is separate from MIDI and OSC clips. It responds to events received through an explicitly selected MIDI input. The current UI is an initial implementation: creating a mapping requires a connected controller and MIDI Learn.

1. Connect a MIDI controller, open **CONTROL MAPPING**, and explicitly choose its port under *MIDI input*. **Refresh ports** reloads the available devices. Disconnecting a device does not automatically select a substitute.
2. Click **MIDI Learn** and move a fader, turn an encoder, or press a pad/button. The system recognizes **CC**, **Note On/Off**, and **Pitch Bend**, displaying the channel and number. The hardware callback uses a fixed-capacity queue and does not perform GUI or network operations.
3. Set the **scope** to Global, Column, Preset, or Single cue. Under **Cue preset**, create a shared preset (for example, `House Lights`) and assign it to the selected cue. Selecting a different grid row does not activate its mapping; a successful GO is required.
4. Choose a **destination**: Master Audio, Audio column gain, GO, PANIC, MIDI CC, or OSC Float. Select a compatible column for column-specific targets. For MIDI CC, set the outgoing CC number; for OSC, enter the target address. MIDI and OSC mapping targets use outputs already configured in the column utilities.
5. Use **Pickup** to prevent gain jumps when a physical fader's position differs from the current gain, or **Jump** to apply its value immediately. The panel also supports inversion. Click **Add mapping** to save a rule; select an existing rule to remove it.

**Priority:** single-cue override, then cue preset, column mapping, and finally global mapping. GO/PANIC button actions do not repeat until the button is released; switching cues does not reset physical-button debounce.

**Active cue:** a cue becomes active only when GO accepts its command. Selecting a row or starting an individual cell does not change the active preset. Panic disables cue-specific mappings but leaves global mappings available. Commands enter the audio transport and are processed in its callback: GO execution may be delayed by one audio block. Sample-accurate control alignment remains future work.

**Generic outputs:** outgoing MIDI CC is sent by the existing MIDI worker, without opening a second instance of the port. Mapping OSC is sent over UDP by a separate worker with a bounded queue; delivery is not guaranteed. Panic cancels pending sends, but cannot retract messages already received by hardware.

**Current limitations:** audio-level mapping still rebuilds render state through 25 Hz GUI updates. Atomic audio parameters and load tests will be needed before using high-frequency physical faders live. Automatic value recall on scene changes, LED/motorized feedback, and advanced multi-mapping editing are not yet implemented. No comprehensive live-hardware acceptance test has been completed.

**Persistence:** mappings, presets, the selected MIDI input, and cue-to-preset associations are stored in `.cuelist` **v10**. Opening v1–v9 shows initializes empty mapping settings. Device names and IDs depend on the computer.

## Cues and Playback

A new session starts with one cue and one Audio column, without user-defined names. **+ Cue** and **+ Column** expand the grid.

| Command | Effect |
| --- | --- |
| Import WAV / MP3 or drop a file onto a cell | Load mono or stereo audio |
| Cell Play or double-click a clip | Start the cell according to its selected playback behavior |
| Cell Stop | Stop and reset that clip |
| Row GO | Trigger participating cells and select the following cue |
| Space / side GO | Trigger the indicated next cue; do not wrap around at the end |
| Stop next to the cue title | Stop every clip in the row, including cells excluded from GO |
| Esc / PANIC | Stop all playback |

Selecting a row re-arms the sequence from that cue. Cell Play/Stop and row Stop do not change the next-cue selection. Space does not trigger a cue while you are typing in a text field.

### Mixer and Audio Master

Under **Column Utilities**, each Audio column has a compact strip with an **integrated GainMeterFader**: a dB gain slider and two L/R peak meters in dBFS sharing a vertical −60 to +12 dB scale, a mid-sized rotary pan control with L/C/R markings, and **Mute** and **Phase Invert** buttons. Audio, MIDI, and OSC destinations are configured separately; MIDI columns show the MIDI port and OSC columns show their UDP destination, not audio gain controls.

Each column's stereo meter measures left and right channels separately, using the sum of clips currently playing in that column, after cell/row/column gains, pan, and mute, but **before the Master**. Slider gain is expressed in **dB** while peak levels are in **dBFS**: 0 dB gain is neutral, but 0 dBFS is digital full scale. Their tick positions align without equating these different quantities. Each channel has a peak indicator with short hold and release. This is not an RMS meter or a limiter. Double-click resets gain to 0 dB; arrow keys adjust by 0.1 dB (1 dB with Shift). The fader's position-to-dB mapping is logarithmic, while saved gain values remain unchanged.

**Technical limit:** stereo meter calculations use buffers prepared before the audio callback, up to 8,192 frames per block. For longer blocks, a conservative value based on individual contributions is used for the overflow; actual audio rendering is unaffected.

The **MASTER AUDIO** area is on the left of the column utilities. It uses the same integrated stereo GainMeterFader and also includes **Master Pan** and **Master Mute**. At 0 dB gain is neutral; negative values attenuate the output and positive values boost the mix. Master controls affect final PCM audio only, **not** MIDI or OSC messages, and do not change stored column levels. Master processing occurs before safety clipping at ±1; it does not replace a live-show limiter. The global OUTPUT meter shows the level after the Master.

Master gain is stored in the show (added in v8). Legacy v1–v7 projects open at **0 dB Master gain**. Master Pan and Mute default to centered/unmuted if missing from older projects.

### Copy/Paste and BPM in the Grid

Select a cell and press **Ctrl+C** to copy its contents and settings. Select a cell in the **same column type** (Audio, MIDI, or OSC) and press **Ctrl+V** to paste. The cell's right-click menu offers the same actions. The destination cell ID remains unchanged and any already-loaded audio media can be shared in memory. Pasting between different column types is not allowed.

Unnamed new columns are automatically named **Audio 1**, **Audio 2**, **MIDI 1**, **OSC 1**, and so forth, with separate numbering by type. Double-click a column title to rename it; custom names are preserved. Older projects with empty names still display a numbered label without rewriting the saved file.

For **whole rows**, use the **…** menu next to the cue name or right-click the name area. Options are **Copy cue**, **Paste cue below**, **Duplicate cue**, and **Delete cue**. Pasting creates a new row below the selected one, including its tempo map, clips, and settings. Duplicating immediately creates a row with the suffix "(copy)". Both operations assign new row/cell IDs without changing the originals. Preloaded audio may be shared in memory. These structural actions stop playback, as does reordering/deleting rows. Copied rows can still be pasted after their source has been deleted, provided the column layout is compatible.

Click the cue's **BPM label** in the row to change its initial tempo directly, retaining later tempo changes. The operation rebuilds MIDI/OSC timing and stops playback. The time signature is no longer shown in the cue-grid interface.

### Auto Stop 2.0

The row's **Auto Stop** switch is the master gate, displayed at the right of each cue and enabled by default. Individual Audio, MIDI, and OSC cell **AS ON / AS OFF** controls are initially hidden to reduce clutter. Click the **+ below the row's Auto Stop toggle** to show or hide them, then toggle individual cells. The equivalent Inspector setting, on the **Cell** tab, is **Stop on next cue GO**.

**A cell stops on another cue's GO only when both the row Auto Stop and that cell's Auto Stop are ON.** If the row switch is OFF, all its clips continue regardless of individual settings. If the row is ON, AS OFF cells continue while other cells stop. Protected clips retain their transport and cue timeline. The rule also applies to directly triggered cells and when the incoming cue is empty. Re-triggering the same cue does not apply Auto Stop to itself.

**Direct Stop, cue Stop, and Panic** always stop playback, including AS OFF cells. Auto Stop is independent of **Include in row GO**, which only controls whether the cell is started by a collective row GO.

New shows default all cells to AS ON. When opening older v1–v6 shows, their cells also default to ON, preserving the previous row-level behavior.

### Playback Behaviors

| Behavior | Effect |
| --- | --- |
| Retrigger | Restart from the beginning, even if already playing |
| Pause / Restart | Pause; the next activation restarts from the beginning |
| Pause / Resume | Toggle pause/resume from the saved position |
| Continue | Keep playing if active; otherwise resume from the saved position |

**Loop** is independent of playback behavior. **Include in row GO** determines whether a cell participates in collective launch. On the **Entire cue** tab, you can set cue-wide gain, mute, loop, and a common GO behavior; direct cell launch still uses each cell's own behavior.

## OSC Clips / UDP and Automation

1. Add an **OSC** column and configure its UDP **host/IP and port** in the column utilities. Routing belongs to the column, not to individual messages.
2. Select a cell and click **Create OSC clip**. Instantaneous events and automation lanes share the **same horizontal timeline**. You may detach and reattach the editor without creating a separate copy of its data. When docked, drag its height handle to resize it.
3. **Click the EVENTS lane** to add a message at the chosen time. An event marker displays its **OSC address and arguments**, when present. **Double-click a marker** to edit its time, address, and ordered arguments. The **+** button creates a message and opens its editor. Drag a marker to move it; Duplicate, Delete, the context menu, and the Delete key are available.
4. Click the **+** button to add a persistent **empty automation track** (it does not create a ramp). Each track has one OSC address and automated argument, and can hold multiple nonoverlapping segments. **Double-click empty space in a track** to create a **0 → 1 ramp lasting one true second** without a setup dialog. Drag the ramp body to move it, or its right handle to resize. **Existing intermediate points retain their original times during resizing**; shortening is rejected if it would silently discard a point. **Shift+click** adds a control point, drag points to change their values or times, and **right-click a point → Delete point** to remove an intermediate point (endpoints cannot be deleted). A **0 → 1 → 0** curve is supported: the final value is not forced to equal the parameter maximum. **Double-click the ramp** to edit independent start/end values, the parameter min/max range, the argument list/type and the automated argument index. **Double-click the track header** to edit the shared OSC address. The typed OSC message template, other constant arguments and saved parameter range are held with the track while each segment keeps its independent times and control points.
5. Arguments are ordered and entered as `i:7; f:0.5; s:scene; b:true`. `i` is int32, `f` is float32, `s` is string, and `b:true/false` is an OSC boolean (type tag `T`/`F`, with no payload). A clip can use **Seconds** or **Beats**; switching units uses the cue's BPM map to convert times and curve points. Only numeric int32 and float32 arguments may be automated.
6. The **EVENTS lane, all track headers, segments and control points use one shared vertical scroll position**. Use the mouse wheel to scroll vertically; use **− / + / Fit** for horizontal zoom, **Ctrl+wheel** to zoom around the cursor and **Shift+wheel** to pan horizontally. The ruler, events and all automation segments share the exact same time scale at every zoom level. **Grid** and **Snap** remain independent; the division selector sets snap resolution. Valid edits are immediately applied and use the shared Undo/Redo history. Overall clip length can also be resized in the main timeline.

**Playback:** instant events fire once at their specified time. Curves generate periodic messages at up to 30 updates/s (a compiled limit of about 8,192 ticks per clip can reduce the rate for long or numerous automations). Messages, including changes to specific arguments, are **prepared off the real-time thread**. The audio clock schedules packets through a dedicated UDP thread.

**Stop:** stops automation and cancels queued messages without sending a reset. Receivers normally keep their most recently received value unless configured otherwise. **Retrigger:** restarts and sends each automation's precise initial value at its scheduled start, even if unchanged. **Loop:** repeats its messages. **Seek:** skips past positions and continues from subsequent updates without resetting the target.

**Multiple automations:** if curves at one address control different arguments, their current numeric values are combined into outgoing packets. This avoids resetting other arguments to defaults. Overlapping curves targeting the same argument are rejected.

Sending uses **OSC 1.0 over UDP** and is not delivery-guaranteed. The transport clock requires an initialized audio device even for OSC-only shows. Test the target host, port, latency, and receiver behavior before a performance.

## Editing and Timeline

- Double-click a cue or column title to rename it; the input receives keyboard focus immediately. **Enter** also confirms text fields in the Inspector.
- Use arrows to reorder rows/columns and **X** to delete. Deleting the last item leaves a usable empty row/column. Original imported files are never deleted.
- Drag a clip within the cue grid to move it; an occupied destination is swapped. Columns must have matching types.
- Drag a clip on the timeline to move its start time in seconds. Drag the **right-edge handle** on MIDI/OSC clips to resize their duration, keeping existing content. Under **GRID**, show/hide BPM lines and select divisions of 1/4, 1/8, 1/16, or 1/32. **SNAP** has a separate magnet toggle and division selector. The grid can be hidden with snapping enabled, or visible while snapping is disabled. Grid lines follow tempo-map changes. Direct cell launch begins immediately instead of waiting for the timeline offset.
- Drag the timeline ruler cursor to seek within the cue. Each cell displays elapsed/total time, progress, and whether it is playing, paused, or waiting.
- **− / +** change zoom; **Fit** displays the full cue, and **Ctrl+mouse wheel** zooms around the pointer. Use the horizontal scrollbar to pan and the mouse wheel to scroll tracks. **Click a MIDI clip in the timeline** to open its docked Clip Editor; **Detach/Reattach** moves the same editor between a panel and a floating window.

Audio parameters update without resetting playback. Enter, **Apply changes**, Save, or changing selection commits text fields. Importing, opening, or making structural changes can stop audio playback.

## Column Utilities and Outputs

| Control | Function |
| --- | --- |
| Out | Pair of active outputs; a saved but missing output is reported without automatic replacement |
| dB fader | From silence (−60 dB) to +12 dB; double-click for 0 dB |
| Pan | Stereo balance: center unchanged; extremes mute the opposite channel; double-click to center |
| Crossed-out speaker | Mute, without stopping clips |
| Slashed circle | Invert polarity of both channels |

Active icons are highlighted. Cell, cue, and column gains are multiplied. Output lists update when the device or active channels change. MIDI strips hide audio controls and allow a hardware or virtual MIDI output to be selected. OSC strips expose UDP destination configuration: enter the receiver's host/IP and port. UDP delivery is not acknowledged.

## MIDI Clips and Piano Roll

1. Add a **MIDI** column with **+ Column**.
2. Choose **Import MIDI** to load a `.mid`/`.midi`, or **Create MIDI clip** to start with an empty four-beat clip. MIDI files can also be dropped onto cells.
3. Select a MIDI cell and choose **Create MIDI clip / Edit piano roll**, or click its timeline clip to open the docked editor. The cell Play button starts playback.
4. Choose a **MIDI output** in column utilities. An external synthesizer, app, or device must listen on that port: The Cuelist does not contain a synthesizer.

In the piano roll, left-click an empty grid position to add a note. Drag a note to change its timing and pitch, or drag its right edge to change duration. **Right-drag** selects multiple notes in a rectangle. **Shift+click** adds/removes notes from the selection. Selected notes can be moved, deleted, or edited together for **Velocity** (1–127) and **Channel** (1–16). **Delete** removes them. **Snap** selects subdivisions from 1/4 to 1/32, or **Off** for free movement. **+ / −** adjust zoom and scrollbars move the view. **Clip beats** changes the overall length without trimming existing notes. Beats are quarter notes; C4 is MIDI note 60.

MIDI changes are **immediately applied to the clip model**. **Undo/Redo** reverses or reapplies note edits. The panel can be collapsed, closed, detached, or reattached. Closing it does not discard changes. Click **Save** to keep the updates in the show file.

MIDI clips participate in GO, Auto Stop, loop, pause, and timeline seek. They can be moved between MIDI cells and on the timeline. Audio dB gains and pan do not affect MIDI; note velocity is used instead. MIDI transport follows the audio device's clock, which therefore needs to be open even for MIDI-only shows.

**Import limits:** standard MIDI type 0/1 with PPQ division, at most 8 MB, 10,000 notes, and 4,096 beats per clip. Tracks are merged without losing their MIDI channels. Only **notes** are imported: file tempo, CC, sustain, program changes, pitch bend, and SysEx are ignored, with a warning for discarded events. SMPTE timing and type 2 MIDI files are unsupported. Playback uses the cue tempo map.

Outputs are saved by device ID. A missing output is not automatically replaced. GO with MIDI notes requires an available output. Accuracy depends on drivers, audio buffers, and OS scheduling; external hardware latency and disconnect tests are still required.

## Portable Projects and Backups

**Save** creates a `.cuelist` file and collects audio in a sibling `<name>_media` folder. Transfer **both** together, keeping them adjacent. Stored paths are relative.

Overwriting a show retains the previous version as `.cuelist.bak`. Earlier media generations may be retained so backups remain valid; repeated saves can therefore increase disk usage.

The current show format is **v10**. **Open** can also load v1–v9 and backup files. v9 introduced OSC automation curves and boolean arguments. Cell Auto Stop flags were introduced in v7 and default to ON for older shows. MIDI clips are stored inside the show, so imported MIDI source files are not needed afterwards.

- Auto Stop is enabled by default for pre-v4 projects.
- For v1/v2, former Restart and Stop options migrate to Retrigger, and column utilities start in neutral states.
- For v3, Pause/Restart behavior and saved column settings are retained.

Save a legacy project again to collect its media. Missing files are reported and can be relinked by importing them into the affected cells.

## Musical Tempo

Each cue has its own **Tempo map**, with one change per line in the format `beat BPM`. The first beat is `0`, with quarter-note units. New maps use a **4/4** time signature; the Inspector no longer edits it. Legacy time signatures are preserved until the tempo map is changed.

```text
0 120
8 90
```

BPM and the tempo map control MIDI timing, not WAV/MP3 playback speed. The renderer adapts audio sample rates using linear interpolation. Global looping repeats each clip independently without rewinding the row clock.

## Current Limitations

- One mono/stereo WAV/MP3 clip per cell, with preloaded PCM limited to **128 MB per file and 512 MB per show**. Streaming and internal multitrack processing are not implemented.
- Decoding is performed on the UI thread, so large files can cause delays. The render callback performs no I/O, allocation, or mutex locking.
- OSC UDP with int32/float/string/boolean messages and linear automation is implemented; LTC/MTC, autosave, and crash recovery are not. Basic physical MIDI input mapping is experimental. ASIO is disabled in this build; WASAPI is available through JUCE.
- Linear resampling and safety clipping at ±1 are used. Extended listening, jitter, and external-hardware disconnect testing remains to be done.

## Development and Verification

To **compile**, install Visual Studio Build Tools with **Desktop development with C++**, the Windows SDK, and CMake tools. Git is needed only if `vendor/JUCE` has not already been downloaded. The project uses JUCE **8.0.12**, commit `29396c22c93392d6738e021b83196283d6e4d850`.

```powershell
# Release build and tests; equivalent to Compila.cmd
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1

# Portable package with license notices (without source code)
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/package.ps1

# Verify archive, documentation, checksums, and launcher
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/check-package.ps1
```

`build.ps1 -TestOnly` builds only tests; `-Run` starts the application. `package.ps1 -SkipBuild` reuses a verified build. `check-package.ps1 -SmokeLaunch` checks offline startup and shutdown. The executable accepts `--offline` to avoid opening an audio device.

Executable path: `build/TheCuelist_artefacts/Release/The Cuelist.exe`. Version 0.4 was verified with **68 cases and 20,472 assertions**, including a show with 10,000 rows. Tests produce audio, project files, and previews under `build/Release/test-artifacts`. See the historical [development log](progress.md) for further details.

| Path | Contents |
| --- | --- |
| `src/ShowModel.*` | Model, validation, tempo, and persistence |
| `src/AudioEngine.*` | Decoding, renderer, transport, and telemetry |
| `src/MainComponent.*`, `src/CueViews.cpp` | Editor and cue grid |
| `src/Timeline.cpp`, `src/ColumnUtilities.cpp` | Timeline and mixer |
| `tests/`, `scripts/` | Tests, build, packaging, and verification |

## License

**The Cuelist is proprietary software; all rights reserved.** The original code is not open source. Third-party use, modification, or redistribution requires the copyright holder's permission, except where non-waivable legal rights apply. See [LICENSE](../LICENSE) and [NOTICE.md](../NOTICE.md).

The portable package does not include source code. User-created audio and show files retain their respective rights.

JUCE and the other dependencies retain their own licenses. **JUCE licensing for distribution of this proprietary application must be verified before sharing it with third parties**: no license has been declared or activated on behalf of the copyright holder. See [JUCE licensing](https://juce.com/get-juce/) and [JUCE 8.0.12 framework license](https://github.com/juce-framework/JUCE/blob/8.0.12/LICENSE.md).

## Recovery, Preflight and Diagnostics (0.7.0-alpha.1)

**Preflight** (upper-right session toolbar) reports model validation, missing or empty audio media, configured MIDI output IDs that are unavailable, MIDI input disconnection and configured OSC UDP endpoints. OSC endpoints are **not** declared online without an application-level acknowledgement. Preflight is advisory; no existing GO/PANIC keyboard shortcut is removed.

**Diagnostics** shows current local transport state: audio callback command overflow, MIDI input queue drops, MIDI output sent/dropped messages, OSC UDP sent/dropped messages, active voices and device status. Output counters restart when The Cuelist restarts, and an OSC send count does not prove the destination received it.

**Crash recovery**: the app writes an atomic checkpoint to the user's application-data recovery folder every ~20 seconds while unsaved changes exist. After an abnormal exit, choose **Restore**, **Discard checkpoint**, or **Later** at next startup. Restoring does not autoplay or overwrite the saved show; check the recovered show, reconnect unavailable media, and save to a chosen `.cuelist` file. Clean exits discard transient checkpoints (unless you explicitly defer recovery). Checkpoints reference source media by absolute paths; always retain the actual media files.

**Loading safety**: the application parses the new show and loads its media, including memory-limit checks, before stopping the previous show. A failed parse or exceeded PCM memory budget leaves the current show in place. Successfully switching to a new show does stop playback as before.

See [the stability and timing engineering notes](stability-and-timing.md) for operator tests, version policy and remaining 0.8 work.
