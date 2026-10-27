# The Cuelist — incremental refactor plan (post-0.9 GUI fixes)

## Non-negotiable invariants

- No implicit GO, PANIC, transport reset or MIDI/OSC transmission when editing Inspector data.
- Preserve project JSON format v10 and the existing v1–v10 compatibility and backup/recovery behavior.
- Preserve the asynchronous project-load transaction: media is prepared and checked before the current show is replaced.
- Do not allocate, block, read files or introduce GUI locks in the audio callback.
- Keep fixed BPM / Auto Stop controls, cue row and column hit targets, editable Gain dB and docked editors intact.
- Build and run the Windows CTest suite before merging each phase. Physical interface rehearsal remains a separate acceptance gate.
- The portable release/package workflow remains manual.

## Phase 1 — merged into feat/desktop-foundation

**Goal:** isolate responsibilities without changing runtime behavior or data ownership.

- `src/InspectorController.cpp`: refresh Inspector controls, compare committed state to drafts, validate and commit edits. Owns the private Gain dB text parser.
- `src/ProjectLifecycle.cpp`: confirmation before discard, save/open, project activation, preflight/diagnostics, crash recovery and safe handoff to prepared media.
- `src/MainComponent.cpp`: retains visual composition, window sizing, event routing and coordination.
- `CMakeLists.txt`: compile the two new translation units into both the application and unit-test binaries.

**Important limit:** phase 1 separates source boundaries, **not** model ownership. These methods are still `MainComponent` members. This is intentional: moving ownership or changing event ordering before regression tests would make this first step substantially riskier.

## Phase 2 — cue editing boundaries (current PR)

**Phase 2a (implemented; awaiting CI):** split existing UI-owned editing commands by their responsibility, without changing logic, IDs, event ordering or ownership.

- `src/CueOperations.cpp`: row colour/Auto Stop, add/rename/reorder/copy/duplicate/paste/delete cue, column add/reorder/delete/collapse, tempo/BPM edits and the transient editing dialogs.
- `src/CellOperations.cpp`: cell swap/copy/paste/cut/delete, loop/follows-row toggles, clip timing and timeline seek.
- `src/MainComponent.cpp`: retains transport, mixer/routing, event coordination, device handling and visual layout; moving method definitions into separate translation units keeps all existing caller contracts.
- `CMakeLists.txt`: compile both modules in the application and tests.
- Extend automated tests with row/column identity, typed-cell associations and cached PCM ownership while preserving existing copy/paste and row duplication test coverage.

**Phase 2b (future separate PR):** after characterization and CI, factor selected *pure* show-model editing operations behind explicit inputs and outputs, preserving stable IDs and atomic failure semantics. This is deliberately not folded into the code-motion step.

No existing audio callback, playback state inheritance, project schema or media-cache ownership changes are made in phase 2a.

## Phase 3 — workspace layout boundaries (separate PR)

- Unify geometry for Cue Grid, Inspector cards, pinned BPM / Auto Stop, mixer strips and Timeline.
- Extract a layout calculation separate from JUCE painting, so 1180x820, 1440x900 and wider desktop sizes can be tested without screenshot-only assumptions.
- Keep user-visible positions unchanged initially; future spacing adjustments require independent UI review.

## Phase 4 — Control Mapping boundaries (separate PR)

- Separate mapping resolution/dispatch from the Control Panel component.
- Cover MIDI Learn, device reconnection, GO/PANIC physical-edge debouncing, Control Set priority and OSC/MIDI output by regression tests.
- Hardware acceptance remains mandatory before release.

## Release gate

Green CI establishes that source compilation and automated regression tests pass. It does not establish that actual connected audio interfaces and MIDI/OSC peripherals behave correctly during a live show.
