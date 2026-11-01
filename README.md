# The Cuelist

**Native Windows show control for audio, MIDI and OSC.**

The Cuelist is a desktop application for preparing and executing cue-based live productions, theatre performances, concerts and installations. It brings audio playback, MIDI sequences, OSC events and MIDI-controller mappings into one operator-focused workspace. It is not a digital audio workstation and does not include native video or timecode playback.

**Current development version:** `versione indicata in [VERSION.txt](VERSION.txt)` · **Platform:** Windows x64 · **Project format:** `.cuelist` JSON schema v10 · **Technology:** C++17, JUCE 8.0.12 · **License:** proprietary.

> **Beta software — not production certified.** Run Preflight, verify each actual audio/MIDI/OSC device and rehearse every transition on the intended show computer. A successful automated test suite does not establish physical-device reliability or end-to-end timing guarantees.

## Versione e storico aggiornamenti

La versione dell'applicazione è definita **esclusivamente**
in [`VERSION.txt`](VERSION.txt), già utilizzato da CMake, About e
packaging. Ogni aggiornamento — funzionalità, correzione, UI, refactor,
test, documentazione o pulizia — incrementa la versione e aggiunge una
sezione in testa al [`CHANGELOG.md`](CHANGELOG.md) con modifiche
e verifiche eseguite. La CI controlla obbligatoriamente la corrispondenza
fra i due file e il bump rispetto a `main`; vedere
[`docs/VERSIONING.md`](docs/VERSIONING.md).

Il changelog include anche una **ricostruzione retrospettiva** delle
versioni 0.7/0.8/0.9 e delle 86 PR precedentemente integrate (con
link alle PR), distinguendo la cronologia di sviluppo dalle release
pubbliche. La versione del progetto salvato `.cuelist` (**schema v10**)
è indipendente dal numero del software.

## UI V2 — Operator workspace design study

**V2.3 / `0.9.0-beta.9`:** rifinito il prototipo di
[Operator Workspace](design/operator-workspace-v2.html):
strip Audio, MIDI, OSC e Control chiaramente differenziate,
testo più leggibile e sidebar senza note provvisorie.
La cue matrix, il Master completo e lo scroll condiviso
del mixer restano invariati. Tutte le funzioni operative
devono continuare a essere disponibili nella GUI nativa:
vedi [parità funzionale](docs/UI_V2_FEATURE_PARITY.md).
Il prototipo resta offline e non operativo.


**V2.2 / `0.9.0-beta.8`:** il Master nel prototipo di
[Operator Workspace](design/operator-workspace-v2.html)
rappresenta tutte le funzioni che il programma ha già in
JUCE (gain/meter stereo, pan, mute, Main Out, limiter,
gain reduction, Quick Learn); i comandi di configurazione
duplicati nella sidebar sono stati raccolti in un
unico menu superiore. **Nessuna funzionalità nativa viene
rimossa nel redesign**: vedi il
[gate di parità funzionale](docs/UI_V2_FEATURE_PARITY.md).
La preview è dimostrativa, non invia audio, MIDI o OSC.


La `0.9.0-beta.7` avvia uno studio di redesign **solo visuale** della
cue matrix, prendendo a riferimento un uso ricco (12 Cuelist, 13 cue,
11 colonne). La [specifica UI V2](docs/UI_V2_OPERATOR_REDESIGN.md)
descrive i vincoli del futuro intervento JUCE e l'[anteprima HTML
offline](design/operator-workspace-v2.html) permette di valutare
gerarchia, densità e collasso dei pannelli. La GUI di produzione,
l'esecuzione di cue e il formato show v10 sono **invariati**.
La preview non invia comandi audio, MIDI o OSC e non costituisce
una nuova build del programma. **V2.1:** dopo il primo feedback,
la parte superiore della demo rimane invariata e il mixer è
stato integrato sotto le stesse 11 colonne della cue matrix,
con scroll orizzontale condiviso, Master allineato alla zona cue
e BPM/Auto Stop ancora fissi. Vedere la
[specifica del mixer integrato](docs/UI_V2_INTEGRATED_MIXER_SPEC.md).

## Capabilities

| Area | Current functionality |
| --- | --- |
| **Cuelists** | Up to 12 named Cuelists within a show, independent cue sequences, tabs with rename/delete, overflow navigation and safe transport stop when switching. |
| **Cue control** | Row and cell GO, Stop, Pause/Resume, Panic, overlapping playback, loop, inclusion in Row GO and per-row/per-cell Auto Stop. |
| **Audio** | WAV/MP3; independent audio columns, gain, pan, mute, phase inversion, output routing, Master Send, Direct Out with PRE/POST selection and an optional Master peak limiter. |
| **MIDI** | MIDI note-clip creation/import, built-in Piano Roll, named output selection, cue tempo mapping, editing and transport. |
| **OSC** | Timed UDP events, typed arguments and automation ramps with editable control points; per-column destination. |
| **Control Mapping** | MIDI Learn, global/column/preset/cue mappings and layered Control Sets with priorities, takeover modes, curves and selected audio/MIDI/OSC targets. |
| **Workspace** | Zoomable Timeline; fixed BPM/Auto Stop; collapse/expand of the Inspector and columns; cue-specific notepad; configurable editor dock; Undo/Redo for supported MIDI/OSC edits. |
| **Operations** | Preflight, Diagnostics, crash-recovery checkpoints, transactional project loading, offline tests and an optional hardware inventory utility. |

### Typography

The native interface uses **Exo 2** for compact operational labels (no smaller than **10 pt**) and **Hind** for larger type, headings and titles. **Noto Sans Math** is selected explicitly for mathematical symbols, including the `−∞` fader readout and the **Preferences → Fader Range** menu; **Noto Sans Symbols 2** is selected explicitly for row/column arrows, cue/cell Play–Pause–Stop, the ⋯ actions menu, compact information/caution icons, and the two-state speaker glyphs **🔊 / 🔇** on Master/column/data-output Mute buttons (where supported by the embedded font, with vector alternatives). These Noto fonts are **not general-purpose text fallback fonts**. Operational glyphs such as Delete use vector geometry, and text remains Exo 2/Hind. All four typefaces are embedded from pinned Google Fonts sources under SIL OFL 1.1, independent of fonts installed on the operator's Windows computer. See [NOTICE.md](NOTICE.md) for copyrights and licenses, and the [Unicode symbol audit](docs/unicode-symbol-audit.md) for every supported UI symbol and the deliberate vector exclusions.

### Icon-button visual language

Compact icon controls now share a subdued **Midnight Slate** surface: neutral low-contrast outlines at rest, slightly brighter on hover, light semantic tint when toggled, muted disabled ink and a distinct keyboard-focus indicator. The symbol remains the visual priority; Play, Stop, Mute, Snap, row/column controls, Inspector grip and control lock retain their operational meanings, tooltip text and hitboxes. Dedicated **GO / PANIC** and ordinary labelled buttons deliberately keep their own treatment. Noto Sans Symbols 2 and Noto Sans Math remain *explicitly selected* for their corresponding glyphs, not font fallbacks. See [Unicode symbol audit](docs/unicode-symbol-audit.md).

### Abstract Control Mapping and Default Map

The controller system supports named project functions with stable IDs and
independent routing: **physical MIDI/OSC input → function → output target**.
A global **Default Map** assigns hardware inputs; a cue's Control Column cell
may override a function's *input* without modifying its destination (for
example Resolume brightness remains MIDI CC 74 on its configured channel).
Overrides do not carry forward between cues unless copied into their Control
Sets. The effective preview identifies inherited vs local assignments.

Global **M Learn / O Learn** controls sit beside the top-right activity
monitors; the Control Set editor provides its own scoped MIDI/OSC Learn.
The OSC receiver starts only on a configured UDP port, accepts bounded
numeric/boolean OSC 1.0 input and updates genuine OSC RX activity. It is
**unauthenticated**: use on a trusted LAN with appropriate firewall rules.
Name-free controller routing presets can be exported and imported without
overwriting project-specific function labels or unrelated local Control Sets.
The old per-column Control Lock is removed from the operator interface.
On project activation, its global bindings migrate transactionally into
named Default Map functions; dormant cue-specific sets are archived as
reusable Control Sets rather than activated unexpectedly. Review and save
the migrated project explicitly: the original file is not overwritten
automatically. Legacy fields remain decoder-compatible, and a separate
global edit lock is reserved for a later explicitly defined policy.

### The workspace

A shared dark editor component is used for cue/column/Cuelist renames, BPM and multiline tempo maps. Control Mapping uses labelled sections, contextual MIDI/OSC target fields and an explicit empty-list state. The left sidebar contains New, Open, Save, Audio devices, Preferences, Control Mapping, the next-cue indicator and GO/Panic. The upper bar displays the current Cuelist tabs, Undo/Redo, a 24-hour clock, CPU/RAM telemetry and MIDI/OSC activity indicators.

The cue grid keeps **Cuelist and Audio/MIDI/OSC/Control column headings pinned while scrolling vertically**. Column headings follow the same horizontal scroll position as their cells, including collapse/reorder controls. BPM and Auto Stop remain fixed at the right. The vertical cue scrollbar now appears on the **left edge** of the Cue List when needed, so the final cells meet the BPM / Auto Stop divider without a dark 9px gutter. Short Cuelists reserve no unnecessary scrollbar space. The Cue List / BPM divider remains continuous across the scrolling grid and mixer. Row values and switches are clipped beneath fixed headers while scrolling vertically, and the row's Play/Pause, Stop and menu actions have separate hit targets. Cell Play/Stop buttons likewise use distinct, larger targets. The row-level Auto Stop switch has a separate **CELLS +/−** disclosure for cell-specific stop settings. **Double-click a Cuelist tab or the Cuelist name in the cue-grid header to rename it**; both always show the same saved name. The `×` action deletes a Cuelist after confirmation, but the last remaining Cuelist cannot be deleted. A separate overflow menu appears only when tabs no longer fit. The Inspector can collapse into a narrow right-side tab and reopen without losing committed edits. Cue notes are saved with their cue.

Undo/Redo use the existing MIDI/OSC edit history; they are **not** a universal undo system for every action in the application.

## Getting started

1. Obtain a compiled Windows x64 build or build from source using the instructions below.
2. Launch **The Cuelist.exe** (or `Avvia.cmd` from a properly extracted package).
3. Open **Audio devices**, select the intended device and confirm its output configuration.
4. Import a WAV/MP3 clip into an Audio cell, or add MIDI/OSC columns and configure their outputs.
5. Use **GO** (Space) to trigger the next cue, and **Panic** (Esc) to stop playback. Test both before connecting to production equipment.
6. Save the show to a `.cuelist` file; keep its accompanying `_media` folder when transferring the show.

For detailed operation, device behavior, MIDI/OSC editors and the project format, read the **[Complete User Guide](docs/complete-user-guide.md)**.

## Build from source

**Requirements:** Windows x64, Visual Studio 2022 / Build Tools with *Desktop development with C++*, Windows SDK and CMake tools. Git is used when the pinned JUCE dependency is not yet present in `vendor/JUCE`. The build script retrieves **JUCE 8.0.12** if required.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1
```

Or run `Compila.cmd`. The Release executable is written to:

```text
build/TheCuelist_artefacts/Release/The Cuelist.exe
```

The script configures CMake, compiles the application and test targets, then runs CTest. Use `scripts/build.ps1 -TestOnly` for the test binary only, `-Run` to launch after a successful build, and `--offline` when manually starting the executable without opening an audio device.

### Portable Windows package

Package creation is a **separate, explicit release operation**, not part of a standard local build or pull-request CI check:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/package.ps1 -SkipBuild
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/check-package.ps1 -SmokeLaunch
```

The `Windows CI (portable package on demand)` workflow can also be started manually from GitHub Actions. The ordinary `Windows Release and tests` workflow only builds and tests. No GitHub Release is implied by the presence of a source branch or passing CI.

The ZIP contains the executable, launcher, README, **one application `LICENSE`**, `NOTICE.md` and the separate **JUCE third-party license notice**. Packaging does **not** establish that the rights holder has secured a commercial JUCE distribution license.

## Projects, backups and recovery

Projects use JSON schema **v10**, independent of the application's `versione indicata in [VERSION.txt](VERSION.txt)` number. The loader retains compatibility with earlier project versions. Cuelist names, per-cue notes and additional mixer/control data are saved as optional v10 fields.

Save collects referenced audio into the project's sibling `<show-name>_media` directory; keep the project and media together. Overwrites preserve a backup. Project opening prepares and validates media before replacing the running show. Recovery checkpoints periodically protect applied unsaved model changes, but **are not a substitute for manual saves** and may not contain text still being edited.

See the [release acceptance checklist](docs/0.9-release-checklist.md) and [stability and timing notes](docs/stability-and-timing.md) for test scope and remaining limitations.

## Development status and known limitations

- This is **versione indicata in [VERSION.txt](VERSION.txt)**, not a stable release or certified live-show system.
- MIDI/OSC dispatch counters describe local software activity, **not confirmed delivery** or end-to-end latency. OSC UDP offers no acknowledgement by default.
- Hardware MIDI controller feedback, native video playback, LTC and MTC are not implemented.
- File import and project save can still involve blocking work; long-duration, real-device stress tests remain necessary.
- Control Mapping should be rehearsed carefully with the actual controller, especially Pickup/Jump, connection changes and layered priorities.
- JUCE's licensing terms apply independently of the application's proprietary license.

## License

The Cuelist is **proprietary software; all rights reserved**. The single [LICENSE](LICENSE) contains a conventional copyright reservation rather than an improvised end-user licence agreement. No public licence to reproduce, modify, build derivatives from or redistribute the original source is granted by publishing this repository. Any already-granted individual licence agreement remains governed by its own terms. A future commercial end-user licence must be prepared separately and professionally reviewed.

GitHub confirms that **without an open-source licence**, copyright remains with the rights holder; public repository visibility nevertheless permits other users to view and fork through GitHub's functionality. See [GitHub's licensing guidance](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/licensing-a-repository) and [GitHub's Terms of Service](https://docs.github.com/en/site-policy/github-terms/github-terms-of-service). To restrict *future* access to the source, the owner should consider making the repository private. Existing forks and local copies may remain accessible.

Third-party rights, particularly JUCE, remain subject to [NOTICE.md](NOTICE.md) and the respective vendor licences. Commercial binary distribution requires verification of those separate rights.
