# The Cuelist — Complete User Guide

**Current prerelease:** `0.9.0-beta.5` · **Windows:** x64 · **Project schema:** v10 · **JUCE:** 8.0.12 · **License:** proprietary

This English guide covers the software currently implemented in this repository. It replaces the legacy 0.8-alpha instructions. For an executive overview, see the [README](../README.md).

> **Important:** The Cuelist is beta software, not certified for show-critical use. Always rehearse with the actual audio interface, controllers and OSC receivers, run Preflight, check backups and maintain a tested emergency procedure. A green CI build is not proof of hardware reliability or end-to-end timing accuracy.

## 1. Concepts and project structure

A **Show** contains shared output columns, device and mixer configuration, Control Mapping settings, and one to twelve **Cuelists**. Every Cuelist has its own ordered cue rows, clip contents and technical notes. Switching Cuelists deliberately stops playback so that a cue from the previous sequence cannot continue unintentionally.

Each cue is a row; each column represents a kind of material:
- **Audio:** mono/stereo WAV or MP3 files with per-cell, cue and strip gain and playback settings.
- **MIDI:** note clips played via a selected hardware/software MIDI output, synchronized to the cue tempo map.
- **OSC:** timed UDP messages and numeric automation curves sent to a configured OSC destination.
- **Control:** MIDI input mappings associated with a cue or promoted to a column default; these are control rules, not media playback clips.

An individual **cell** can be played independently. **Row GO** triggers the participating cells together. Multiple rows may overlap until stopped manually or by an Auto Stop rule. BPM and Auto Stop remain in a pinned area of the cue grid.

## 2. Starting and configuring The Cuelist

### 2.1 Running an existing build

Extract the entire Windows x64 package. Open `The Cuelist.exe` or `Avvia.cmd`; the launcher does not compile or install software. A GitHub source-code archive does not contain a ready-to-run EXE.

A properly packaged build does not require Visual Studio, Git or CMake to run. Keep the included `LICENSE`, `NOTICE.md` and JUCE license notices with the package. The command-line option `--offline` starts the interface without opening the audio device and is intended for diagnostics and smoke tests.

### 2.2 First rehearsal

1. Open **Audio devices** in the left sidebar and choose the intended audio interface and output configuration.
2. Open an existing `.cuelist` or create a new show. Verify the media referenced by each cue.
3. Configure audio routing, MIDI ports and OSC host/port under the individual Column Utilities.
4. Connect any MIDI controller and select its actual input device under **Control Mapping**; verify MIDI Learn only after physical input is present.
5. Select **Preferences → Pre-flight**, read the warnings and resolve missing files, routes and devices.
6. Test cell Play/Stop, **GO**, **Panic**, loop and Auto Stop with the connected equipment before relying on the show sequence.
7. Save the show and make a separate external backup.

## 3. Workspace overview

### 3.1 Left sidebar

The sidebar provides **New**, **Open**, **Save**, the editable show title, **Audio devices**, **Preferences**, **Control Mapping** and a project overview. Near the bottom, **On Deck** identifies the next cue; **GO / SPACE** and **PANIC / ESC** remain accessible at all times.

- **New** starts a fresh show and warns if current changes would be discarded. The compact **Unsaved changes** dialog keeps **Cancel** as the safe choice; **Discard and continue** must be selected explicitly to proceed without saving. Closing the dialog or pressing Escape also cancels the operation. To save instead, select Cancel and then Save.
- **Open** loads an existing project, subject to preparation and validation before activation.
- **Save** writes the project and collects referenced media.
- **Preferences** contains Pre-flight, Diagnostics, Fader Range and Rename active Cuelist.
- **Control Mapping** opens the controller configuration view. Depending on selection, this can expose the Control Set editor or legacy MIDI mapping interface.

The shortcut hints formerly shown at the bottom of the sidebar have been removed; the keyboard shortcuts still work.

### 3.2 Top bar, Help, and operational telemetry

The top row contains named **Cuelist tabs**, the **+** action, **Undo/Redo**, a 24-hour clock, Windows CPU/RAM usage and a compact MIDI RX/TX / OSC RX/TX / audio-availability monitor.

The **Help** button in the top toolbar opens a menu with **Guide** and **About The Cuelist**. The Guide entry opens the content of the local `docs/complete-user-guide.md` **inside The Cuelist**, in a separate non-modal, resizable, scrollable window with headings and a built-in search. No external browser or Markdown editor is launched. The reader uses the bundled Markdown file, which is copied beside the compiled EXE and into portable packages, and reloads it each time the Guide is opened. About opens a compact, grouped native dialog with the version derived from `VERSION.txt`, the two credited co-authors **Davide Salvatore** and **Riccardo D'Ambrosio**, and concise copyright/licence information. It links to LICENSE and NOTICE.md without repeating the co-authors in multiple sections.

- **Undo/Redo** use the existing MIDI/OSC clip-edit history; they are not general-purpose undo for show deletion, tab management or every mixer action.
- **CPU/RAM** report system-level usage, not per-plugin or per-clip estimates. The two usage values and their horizontal meters have separate aligned positions; unsupported telemetry displays N/A.
- **MIDI RX/TX** indicate local traffic; **OSC TX** indicates local transmission. OSC RX remains inactive because a native OSC input endpoint is not implemented.
- **Audio Available** reports local device availability, not an assertion that speakers or downstream equipment are functioning.

### 3.3 Cue grid, Inspector, mixer and Timeline

The central cue grid has row controls on the left and columns for Audio/MIDI/OSC/Control. Its Cuelist title and all Audio/MIDI/OSC/Control column headings stay **fixed during vertical scrolling**. Column headings track their cells during horizontal scrolling. The **BPM** and **Auto Stop** strip remains fixed at the right, including during horizontal scrolling. During vertical scrolling, row values, toggles and action targets remain **below** their fixed headers: controls hidden behind the header are neither visible nor clickable. Row Play/Pause, Stop and the action menu are physically separated, as are the larger Play/Stop targets inside each cell. Column Utilities and the Master Audio strip occupy the lower working area; a Timeline with zoom/grid/snap controls is underneath.

Small icon buttons across the workspace use a restrained shared visual style: the glyph carries the semantic colour while the border stays subtle. Hover, pressed, toggled, disabled and keyboard-focus states remain distinguishable. This affects the appearance, **not** the Play/Stop/GO/PANIC actions, selection, saved shows or clicking areas. Destructive actions still use red/pink symbols. A visible focus outline indicates keyboard focus; the critical GO and PANIC actions retain their distinct treatment.

The **Inspector** on the right edits the selected cue and cell separately. Use its narrow edge tab (one framed surface with a directional chevron) to collapse the Inspector toward the right, increasing the width of the workspace; click the tab again to reopen. The same area contains a cue-specific multiline **Notes** field.

Columns can be collapsed individually or together with **Collapse all** in the cue grid header. Collapsing is a layout choice; it does not stop running media or change audio routing. The pinned BPM / Auto Stop area has a continuous visual boundary with the scrollable grid and the mixer below, including at row separators. The Cue List's **vertical scrollbar sits at its left edge**, rather than between the last cell and BPM / Auto Stop. It appears only when there are more rows than can fit on screen. The pinned row header follows that scrollbar's visible inset, keeping Play, Stop and row names aligned with the scrolling content. When no vertical scrollbar is necessary, the grid expands all the way to the BPM boundary instead of retaining an empty gutter.

### Control Mapping: project functions and cue overrides (incremental implementation)

The mapping editor can create a **named project function** with an independent
output target, and bind an incoming hardware control to it. The destination
is stored in the function rather than in the controller input: rebinding
**Resolume Brightness** from Fader A to Fader B can still send MIDI CC 74
on the configured MIDI output. These function names are project-specific.

The `Default Map` consists of the project's global function inputs.
A Control Column cell can supply a local input for the **same function ID**.
When that cue becomes active after GO, the local input replaces the default
input; when moving to another cue, the default input is automatically restored
unless the next cue also overrides it. The old input is not kept alive alongside
its replacement. Copy/paste Control Set cells to repeat an override deliberately.
The Control Set editor's effective preview distinguishes inherited and locally
overridden functions. Existing direct bindings remain readable for legacy
projects.

**MIDI Learn / OSC Learn.** Two distinct **M Learn** and **O Learn** controls
are positioned next to the upper-right MIDI/OSC activity monitor; they open
the global mapping panel and arm acquisition for the **Default Map only**.
The Control Set editor has its own MIDI Learn and OSC Learn controls for the
selected cue. An armed Learn state is marked amber; the first learned
message is consumed rather than dispatched to GO/PANIC. The monitor's
OSC RX indication counts genuine incoming datagrams, independent of Learn.

**OSC input configuration.** OSC input is **disabled by default**, including
when loading older projects. The operator must explicitly configure a UDP
input port (suggested 9011) in the mapping editor and activate OSC Learn.
Only single OSC 1.0 float, integer or boolean messages with short printable
ASCII addresses are supported; bundles and other argument types are ignored.
The listener has a bounded queue and transfers mapping decisions to the
GUI thread. It binds a UDP port on the host: use only a trusted LAN, restrict
inbound access using the Windows firewall and never expose show-control
inputs directly to the public Internet. OSC/UDP has no authentication or
guaranteed delivery.

**Portable Default Map presets.** Use **Export Default Map** / **Import Default
Map** in the global mapping editor. The exported JSON contains the physical
assignments and function destinations, but **not** project-specific function
names. Import matches functions by their order in the project, retains existing
project names/IDs and local cue overrides, and rejects incompatible target
columns without partially overwriting the show. Review the output hardware
and all routed targets after import; device IDs may differ on another PC.

**Legacy Control Column Lock.** The old per-column lock is hidden for newly
created control columns. Existing locked projects retain an explicit
**Unlock** path for safe migration before named functions can be added.
The old lock schema/compatibility code is still retained for loading such
projects; it is *not* a recommended part of the new mapping workflow.
A separate global operator edit lock is not active yet because the
actions it will forbid have not been specified.

## 4. Managing multiple Cuelists

### 4.1 Creating, selecting and removing

- Click **+** next to the Cuelist tabs to create a new list. A newly created list starts with an independent blank cue.
- The application uses the first available `Cuelist N` name, including numbers freed by deletion, rather than skipping numbers.
- Click a tab to select a list. The current transport is stopped before switching, and the Cue List, Timeline and Inspector reflect the new list.
- Tabs continue side by side while there is space; a separate overflow menu provides access to hidden tabs and their rename/delete actions when the bar is too narrow.
- Use the **×** beside a visible tab to delete a Cuelist. A confirmation dialog explains that its cues are removed from the show. Source audio files are not deleted from disk.
- The last remaining Cuelist cannot be deleted.

Deleting a Cuelist is a structural operation and is not an Undo/Redo action. Save a backup before reorganizing a live show.

### 4.2 Renaming — synchronized in both directions

**Double-click either the Cuelist tab or the Cuelist name in the pinned cue-grid header** to rename it. The alternative **Preferences → Rename active Cuelist** remains available. The Rename Cuelist, Rename cue, Rename column, Cue BPM and Cue tempo map windows now share a consistent dark editor: a title, short left-aligned instruction, labelled text field and Apply/Rename plus Cancel controls. Single-line editors accept Enter; Escape or Cancel closes without changing the model. Names still use the existing uniqueness/length constraints.

The tab and the grid header are two views of the **same saved Cuelist name**. Renaming either updates both immediately, and the name persists when you save the `.cuelist` file. Names must be unique, nonempty and no longer than 64 characters.

The grid heading displays the current name only; it does not need an additional `CUE LIST /` prefix.

### 4.3 Shared and independent settings

Cuelists share column definitions, routing and the overall show settings. They do **not** share their cue ordering, cell contents or per-cue technical notes. Adding, moving or removing a column updates the corresponding cell structure across all lists; removing a Cuelist never removes a shared column.

## 5. Playing and editing cues

| Action | Behavior |
| --- | --- |
| **GO / Space** | Launches the armed next cue; it does not wrap to the beginning at the end of a sequence. |
| **Row GO** | Starts cells included in that row's GO. |
| **Cell Play** | Starts or resumes the individual cell according to its playback behavior. |
| **Cell Stop** | Stops and resets that cell. |
| **Row Stop** | Stops the cue's cells, including cells excluded from Row GO. |
| **Row Play/Pause** | Pauses or resumes row playback without restarting the cue where supported. |
| **PANIC / Esc** | Stops active audio, MIDI and OSC playback. |
| **Ctrl+C / Ctrl+V** | Copies/pastes compatible selected cells; row operations are also available through cue context menus. |
| **Ctrl+X / Backspace** | Cuts or removes the selected cell where allowed, without intercepting normal text editing. |

Space does not launch a cue while typing into a text input. Escape is intended to remain available for Panic even from the Inspector note editor; check the behavior with your connected devices.

Click a cue to make it the next armed cue. Use the row menu to **Copy**, **Paste below**, **Duplicate** or **Delete** a whole cue. The cell context menu covers cell-specific copy, paste, cut, delete, loop and inclusion in Row GO. Duplicated rows and cells receive appropriate new identities; imported audio can share decoded PCM in memory when safe.

Use the **▲ / ▼** controls to reorder cue rows. These glyphs, the **◀ / ▶** column controls, **▶ / ⏸ / ⏹** transport icons and **⋯** row actions use **Noto Sans Symbols 2** explicitly where the embedded typeface provides them. A vector symbol is used when the exact glyph is unavailable; no operating-system emoji substitution occurs. The Delete cross remains vector geometry. See the [Unicode symbol audit](unicode-symbol-audit.md). Changes that affect the structure or timing may stop transport to preserve deterministic cue state.

### 5.1 Playback behavior and looping

The clip playback behavior determines how a second trigger acts: **Retrigger**, **Pause/Restart**, **Pause/Resume** or **Continue** as configured. **Loop playback** is independent of that behavior. The Inspector separately exposes the cue's **Loop all clips**, the cell's **Loop playback** and **Include in row GO** settings.

### 5.2 Auto Stop

Each row has a pinned **Auto Stop** toggle, normally enabled initially. Select the separate **CELLS +** control beneath the switch to reveal the cell-level Auto Stop options. It changes to **CELLS −** while expanded. The switch and disclosure have distinct clickable regions and remain below the fixed header when you scroll.

A previously active cell is stopped by a later cue's GO only when both its row-level Auto Stop and its cell-level Auto Stop are enabled. Disabling row Auto Stop allows its clips to continue. A direct Stop, Row Stop or Panic still stops them. The next GO can also stop protected clips independently of whether the new cue itself has media.

## 6. Inspector: cue settings, cell settings and notes

The right-side Inspector distinguishes **Cue properties** from **Cell properties**.

**Cue:** name, initial BPM and tempo-map editor, cue-wide loop selection. **Cell:** name, compatible options for Audio/MIDI/OSC/Control, gain in dB for audio, inclusion in GO, playback loop, clip import or editor launch.

The **Notes / Cue NN** multiline editor holds technical stage directions, reminders and other operational notes. They belong to the selected cue and are included in the project save; each Cuelist maintains its own notes. Moving to another cue or collapsing the Inspector commits valid pending changes. An invalid Inspector value can prevent the panel from closing until corrected.

Text input not yet applied to the model is not independently guaranteed to exist in a periodic recovery checkpoint. Save explicitly after editing cue notes and titles.

## 7. Audio, mixer and routing

### 7.1 Audio cells

Audio cells support WAV and MP3, imported with the cell's import action. One mono or stereo file may be associated with a cell. The show decodes audio to PCM for playback and enforces memory limits; media import and saving large collections can still take time.

Individual cell gain is expressed in **dB**. A peak meter reports **dBFS**: 0 dB gain is unity, while 0 dBFS is the digital ceiling. The two units are not interchangeable. A muted fader reads **−∞ dB**; its mathematical minus/infinity symbols are rendered directly with embedded Noto Sans Math, while the unit remains in Exo 2. The meter's L/R labels and the gain readout occupy separate vertical lanes.

### 7.2 Column Utilities

Audio column strips provide gain, stereo meters, pan, Mute, phase inversion and routing. MIDI and OSC strips expose the appropriate output controls rather than audio faders. Collapsed Audio columns retain compact meter/gain and Mute controls. **Mute** uses the embedded **Noto Sans Symbols 2** directly: **🔊** means audio/output enabled; **🔇** means muted. Both are centred by their visible glyph outlines inside the compact button, rather than by the font's advance width. The same two-state icon is used for Master, Audio columns and MIDI/OSC data-output mute, while phase inversion retains its own symbol.

An Audio column can send signal to the **Master** on its left/right paths and/or to a physical stereo **Direct Out** pair. Direct can be **PRE** or **POST** relative to the strip fader; post-strip Master Send is independent. Mute, pan and other per-cell/per-cue processing follow their implemented signal path.

### 7.3 Master Audio

The **Master Audio** strip provides Main Out routing, stereo gain/meter, pan, mute and an optional safety peak limiter. Its meter is pre-limiter. The limiter indicates current gain reduction and does not affect independent Direct Out paths.

**Routing warning:** sending a Direct Out to the same physical channels as Master Main Out can cause post-limiter summation and clipping. Preflight warns about the conflict, but an operator must verify the actual routing.

Master gain/mute/pan act on PCM audio, not MIDI notes or OSC commands. The output stage clamps device PCM samples, but that is not a substitute for physical gain staging and rehearsal.

## 8. MIDI clips and Piano Roll

Add a **MIDI** column and choose the correct external output in its Column Utilities. Import an existing MIDI file or create a note clip in the built-in **Piano Roll**. The clip participates in cue GO, stopping, looping and timeline operations as supported.

The importer supports standard MIDI type 0/1 files using PPQ, subject to size and note-count limits. File tempo and non-note events such as controllers, program changes, SysEx and sustain may not be imported; the cue's tempo map controls playback of imported notes. SMPTE and type-2 MIDI files are unsupported.

MIDI ports are tracked by their device identifiers. A missing configured output is **not** silently replaced by another interface. Reconnecting the original exact-ID port invalidates outdated queued messages. Physical output latency remains dependent on hardware, driver and OS scheduling.

Use **Undo/Redo** in the supported clip editor operations; the buttons are disabled when no relevant history exists.

## 9. OSC messages and automation

Add an **OSC** column and enter the destination **host/IP address and UDP port** in Column Utilities. Create an OSC clip, then edit timed events and automation tracks using the OSC editor.

- Event messages contain an OSC address, event time and ordered typed arguments.
- Arguments use `i:` (int32), `f:` (float32), `s:` (string), and `b:` (boolean), for example `i:7; f:0.5; s:scene; b:true`.
- Automation lanes hold a shared OSC address/argument and one or more non-overlapping ramp segments. Add and move control points, resize permitted segments and adjust start/end values.
- Events and automation share the editor's horizontal timeline; vertical scrolling follows the same track alignment.
- Clips may use Seconds or Beats; conversion uses the selected cue's tempo map.
- MIDI/OSC events are timestamped against the audio callback's common monotonic origin, but actual delivery can be delayed by device workers and the operating system.

**UDP has no built-in delivery acknowledgement.** A transmitted packet count or illuminated OSC TX indicator does not prove that the receiver applied a command. Use receiver-side monitoring during rehearsal.

## 10. Control Mapping and Control Columns

### 10.1 MIDI Learn and destinations

Open **Control Mapping**, select the actual MIDI input port, start **MIDI Learn**, and move a physical fader, encoder or button. The mapping subsystem recognizes CC, note events and Pitch Bend as supported and exposes destinations such as Master/Audio gain and pan, GO, Panic, MIDI CC and OSC float.

Mapping settings can include a value range, inversion and response curves. **Jump** applies the physical value immediately; **Pickup** waits for the physical input to cross the current destination value to reduce sudden parameter changes. The configuration window groups **MIDI Input**, **Mapping Configuration**, **Cue Presets / Reusable Control Sets**, and **Saved Mappings**. The Destination column selector has a visible label; MIDI Output CC and OSC address fields are enabled only for their respective target types. When no mappings exist, the saved list explains how to create one, and Update/Remove remain disabled until a mapping is selected.

### 10.2 Scope and priority

Mappings can be Global, Column, Preset or cue-specific. Cue-specific mapping becomes active through a **successful GO**, not merely selecting a row. Panic clears active cue-specific mapping state as designed while preserving the configured global rules.

A **Control Column** can contain per-cue Control Sets. Locking a Control Column promotes its chosen set to a low-priority default across cues; unlocking restores cue-specific sets. Unlocked columns can have Medium, High or Maximum priority. More specific/higher-priority mappings override the same physical input while unrelated inputs inherit lower-level mappings. Equal-priority conflicts resolve by the rightmost Control Column.

Reusable Control Sets can be saved and applied to cells or defaults. All mapping data is included in the project schema. **Control Mapping is experimental** and requires physical-device testing, especially hot unplug/reconnect, Pickup/Jump, key repeat and conflicting targets.

## 11. Tempo maps and Timeline

Each cue contains a tempo map in quarter-note beats, with its first event at beat zero. The shared tempo editor has a multiline field labelled **Tempo changes** and a syntax reminder (**beat BPM**). A typical entry list is:

```text
0 120
8 90
```

These entries set 120 BPM at beat 0 and 90 BPM from beat 8. MIDI/OSC beat timing follows the tempo map; changing BPM does **not** change the natural playback rate of imported WAV/MP3 audio. Older projects may retain legacy time-signature fields.

The Timeline represents the selected cue's clip timing, provides zoom, grid visibility, snap and a horizontal scrollbar. Dragging/range edits are available for supported MIDI/OSC clips. Displaying grid lines and enabling snap are separate choices. The **Snap** button uses a compact, centred vector magnet: teal magnet for ON, grey magnet crossed by a red slash for OFF. The Unicode magnet emoji is not included in the bundled Noto Sans Symbols 2, so it is intentionally not rendered through an operating-system emoji font. The pinned row BPM and Auto Stop controls remain visible even when scrolling columns.

## 12. Files, backups and recovery

### 12.1 Saving and transferring a show

The project document is `<name>.cuelist`, a versioned JSON file with **schema v10**. The application version `0.9.0-beta.5` is a separate identifier. Audio assets are collected into an adjacent `<name>_media` folder; **transfer the project file and this folder together**. MIDI/OSC clip data and Cuelist metadata are saved within the project document.

Overwriting may retain a `.cuelist.bak` backup and older media generations. Check disk usage. Previously created schema v1–v9 and compatible v10 projects can be opened by the supported migration logic; the optional multi-Cuelist and cue-note fields do not require a schema-number change. Newer feature data may not be understood by older application binaries.

### 12.2 Project load safety

Opening a project parses and validates the model and prepares its media on a background worker. The active show is not replaced until the new project passes validation, media decoding and PCM budget checks. Newer load requests supersede stale ones. Switching Cuelists inside an already open project stops playback before changing the active cue list.

### 12.3 Crash recovery

A bounded background checkpoint writer periodically saves applied unsaved model changes (approximately every 20 seconds) using an atomic file replacement. After an abnormal exit, the application can offer **Restore**, **Discard checkpoint** or **Later**. Recovery does not automatically start playback and does not overwrite the saved show.

A checkpoint is not a substitute for `Save` and off-device copies. It may not reflect keystrokes that were still in an uncommitted Inspector field, and it references media files rather than making additional copies.

## 13. Preflight, Diagnostics and physical validation

Use **Preferences → Pre-flight** to inspect model validity, missing or empty audio, configured device availability, potential routing collisions and OSC endpoint configuration. An OSC destination being configured is not proof that any remote server is online.

Use **Preferences → Diagnostics** for a grouped report covering **Audio**, **MIDI**, **OSC** and **Recovery**, with left-aligned metrics, right-aligned values and distinct colours for connected, disconnected, late and failure states. Counters are a snapshot of the current process, not a continuously updating hardware readout. These measurements describe The Cuelist's workers; external cable, device and receiver latency is not measured.

**Recommended acceptance rehearsal:** run simultaneous audio/MIDI/OSC cues; exercise GO, Row Stop, Panic, selective Auto Stop, Direct/MAIN outputs, Master limiter, Cuelist switching, note saving, MIDI controller connection changes, and OSC receiver failure/restart. Perform a prolonged load test and record real device behavior.

See [Stability and timing](stability-and-timing.md) and the [0.9 release acceptance checklist](0.9-release-checklist.md) for further engineering context.

## 14. Build, test and optional packaging

Install Visual Studio 2022 or Build Tools with **Desktop development with C++**, a Windows SDK and CMake. The build script downloads the pinned JUCE **8.0.12** dependency when `vendor/JUCE` is missing (Git required only for that acquisition).

```powershell
# Release application, test targets and CTest
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1

# Optional: test-only target, or compile then launch
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 -TestOnly
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 -Run

# Explicit package creation and extraction verification (not a regular build)
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/package.ps1 -SkipBuild
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/check-package.ps1 -SmokeLaunch

# Read-only hardware inventory, optionally followed by localhost OSC test
powershell -ExecutionPolicy Bypass -File scripts/hardware-check.ps1
powershell -ExecutionPolicy Bypass -File scripts/hardware-check.ps1 -OscLoopback
```

The executable is written to `build/TheCuelist_artefacts/Release/The Cuelist.exe`. Standard pull-request CI builds and runs CTest; creating a portable ZIP is an explicitly requested separate operation. Packaged archives include a SHA-256 checksum and the required license notices.

### Source layout

| Path | Responsibility |
| --- | --- |
| `src/ShowModel.*` | Show/Cuelist data, validation, migrations, JSON and media paths |
| `src/AudioEngine.*` | PCM playback, transport, DSP, output routing and telemetry |
| `src/MainComponent.*`, `src/CueViews.cpp` | Main workspace, tabs, cue grid, sidebar, Inspector coordination |
| `src/Timeline.cpp`, `src/ColumnUtilities.cpp` | Timing views and media/output utility strips |
| `src/MidiClip.cpp`, `src/MidiUI.cpp`, `src/PianoRoll.cpp` | MIDI clips and note editor |
| `src/OscClip.cpp`, `src/OscUI.cpp`, `src/OscEditor.cpp` | OSC clips, automation and editor |
| `src/ControlMapping.h`, `src/ControlRuntime.cpp` | MIDI input mappings, layers, controller dispatch |
| `tests/`, `scripts/` | Automated tests, Windows build, hardware probe and packaging |

## 15. Known limitations and legal terms

This prerelease does not provide native video playback, LTC/MTC, motorized MIDI controller feedback or guaranteed sample-accurate external MIDI/OSC delivery. The application still has UI-thread work in some import/save operations and needs hardware stress testing before a public production release. Prefer independent backups and a tested fallback.

The Cuelist is **proprietary software; all rights reserved, not open source**. The [LICENSE](../LICENSE) is an English-only conventional copyright reservation, **not a complete commercial end-user licence agreement**. Individual licences already lawfully granted continue under their own terms; any new paid distribution requires separately prepared licence and sales conditions. See [NOTICE.md](../NOTICE.md) for independent JUCE and other third-party obligations. Do not distribute a proprietary binary without securing all applicable third-party rights.
