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

## Phase 2 — cue editing boundaries

**Phase 2a (merged into feat/desktop-foundation):** split existing UI-owned editing commands by their responsibility, without changing logic, IDs, event ordering or ownership.

- `src/CueOperations.cpp`: row colour/Auto Stop, add/rename/reorder/copy/duplicate/paste/delete cue, column add/reorder/delete/collapse, tempo/BPM edits and the transient editing dialogs.
- `src/CellOperations.cpp`: cell swap/copy/paste/cut/delete, loop/follows-row toggles, clip timing and timeline seek.
- `src/MainComponent.cpp`: retains transport, mixer/routing, event coordination, device handling and visual layout; moving method definitions into separate translation units keeps all existing caller contracts.
- `CMakeLists.txt`: compile both modules in the application and tests.
- Extend automated tests with row/column identity, typed-cell associations and cached PCM ownership while preserving existing copy/paste and row duplication test coverage.

**Phase 2b (merged into feat/desktop-foundation):** introduce `ShowEditOperations.h/.cpp` in `CuelistCore` with three tested, JUCE-GUI-free model operations:

- `swapCueRows`: bounded cue reordering that preserves stable row IDs.
- `swapCueColumns`: bounded column reordering with corresponding typed cell slots swapped across every row; rejects malformed rectangular layouts.
- `prepareInsertedCueCopy`: transactional cue insertion into a separate output Show, reassigning row, cell and private Control Set binding IDs. The destination is only replaced after full `validateShow` succeeds; failed copies leave output unmodified.
- `CueOperations.cpp` keeps existing operator/transport sequencing but delegates these three model edits; the GUI still owns media-cache association and checks PCM memory limits **before** adopting a newly prepared show.
- New model-only regression tests cover stable IDs, clip/type association, correct Control Set identity cloning, range failures and project nonmutation.

**Phase 2c (merged into feat/desktop-foundation):**

- `prepareDeletedCue` and `prepareDeletedColumn` build a replacement Show and gather the deleted cell IDs without touching the current model, PCM cache, GUI or devices. Neither output changes on failure.
- Cue deletion removes legacy mappings scoped to that cue ID and clears stale locked-Control default-source cue IDs without discarding their settings; column deletion removes references to its ID from legacy mappings, locked Control Column defaults, cue-specific sets, and reusable templates.
- Cleanup includes column gain, **column pan**, MIDI CC, OSC output and explicit column-scoped mappings (previous UI deletion omitted column pan).
- After model validation succeeds, the UI removes only those cell IDs from its PCM cache, installs the prepared model, refreshes mapping state and invokes the existing stopped-transport change flow.
- Deleting the final cue or column leaves a valid blank cue or Audio column just as before; no live audio or file on disk is deleted.
- New model-only tests cover cross-layer mapping cleanup, locked-Control source metadata, stable IDs, last-item fallback and invalid-input atomicity; an additional UI test verifies that remaining decoded PCM survives column deletion.

The next stage is Phase 3: extracting testable workspace layout geometry from painting, while preserving existing screen positions.

No existing audio callback, playback state inheritance, project schema or media-cache ownership changes are made in phase 2a.

## Phase 3 — workspace layout boundaries (current PR)

**Phase 3a (implemented; CI verification pending):** centralize the screenshot-approved workspace geometry in the deterministic `WorkspaceLayout.h/.cpp` calculator.

- `calculateWorkspaceLayout` owns rectangular bounds for the cue viewport, mixer utilities, timeline, docked clip editor and drag/close/collapse handles, pinned BPM / Auto Stop area, inspector outer panel and both section cards, plus virtual cue grid dimensions.
- `WorkspaceLayout::inspectorTransform` and `scaleInspectorControl` share the identical vertical scaling formula previously duplicated between painting and `resized()`. The Inspector card background and CELL heading remain aligned across window sizes.
- `MainComponent::workspaceLayout()` adapts current window size, cue/column dimensions and active dock height into a geometry input. `paint()` and `resized()` consume the same result, while `MainComponent` continues to own component lifetime, visibility and event routing.
- Regression tests verify 1180×820, 1440×900, 1647×895 and 1920×1080, scrolling/pinned controls, collapsed/expanded clip dock, the shared Inspector transform and actual JUCE component bounds.
- No visual reflow or spacing change in this phase; it is a structural extraction with the exact existing baseline coordinates and control sizes.

**Phase 3b (later, separate PR):** once the geometry helper is proven, address narrower display constraints or improve layout responsiveness with deliberate visual change review, rather than silently changing the approved layout.

## Phase 4 — Control Mapping boundaries (separate PR)

- Separate mapping resolution/dispatch from the Control Panel component.
- Cover MIDI Learn, device reconnection, GO/PANIC physical-edge debouncing, Control Set priority and OSC/MIDI output by regression tests.
- Hardware acceptance remains mandatory before release.

## Release gate

Green CI establishes that source compilation and automated regression tests pass. It does not establish that actual connected audio interfaces and MIDI/OSC peripherals behave correctly during a live show.
