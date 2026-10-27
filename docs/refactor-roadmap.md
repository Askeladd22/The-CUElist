# The Cuelist — incremental refactor plan (post-0.9 GUI fixes)

## Non-negotiable invariants

- No implicit GO, PANIC, transport reset or MIDI/OSC transmission when editing Inspector data.
- Preserve project JSON format v10 and the existing v1–v10 compatibility and backup/recovery behavior.
- Preserve the asynchronous project-load transaction: media is prepared and checked before the current show is replaced.
- Do not allocate, block, read files or introduce GUI locks in the audio callback.
- Keep fixed BPM / Auto Stop controls, cue row and column hit targets, editable Gain dB and docked editors intact.
- Build and run the Windows CTest suite before merging each phase. Physical interface rehearsal remains a separate acceptance gate.
- The portable release/package workflow remains manual.

## Phase 1 — completed in the refactor branch

**Goal:** isolate responsibilities without changing runtime behavior or data ownership.

- `src/InspectorController.cpp`: refresh Inspector controls, compare committed state to drafts, validate and commit edits. Owns the private Gain dB text parser.
- `src/ProjectLifecycle.cpp`: confirmation before discard, save/open, project activation, preflight/diagnostics, crash recovery and safe handoff to prepared media.
- `src/MainComponent.cpp`: retains visual composition, window sizing, event routing and coordination.
- `CMakeLists.txt`: compile the two new translation units into both the application and unit-test binaries.

**Important limit:** phase 1 separates source boundaries, **not** model ownership. These methods are still `MainComponent` members. This is intentional: moving ownership or changing event ordering before regression tests would make this first step substantially riskier.

## Phase 2 — cue editing boundaries (separate PR)

- Characterize row/cell copy, paste, duplicate, delete, selection and dependent Control Set bindings with focused tests.
- Move cue and column editing operations into a dedicated compilation unit and, once characterized, a tested model-operation layer with explicit inputs/outputs.
- Do not touch playback state inheritance or IDs except where a pure, regression-covered helper is introduced.

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
