# THE CUELIST — Native V2 / beta.19: global header and inline cue BPM

Baseline: `main` 0.9.0-beta.18. The user explicitly identifies **two different headers**: the global top application header and the fixed BPM/Auto Stop lane. Both are included here, without changing the audio/mixer architecture.

## Native global header

- Two visual levels within the **original 72 px**: seven action menus with their existing overflow, above the Cuelist tabs.
- The menu text uses embedded Exo 2 and a gentle hover underline, not raised cards for every action.
- Active Cuelist has an explicit teal underline, subtle flat fill; the existing edit-on-double-click, drag/drop, confirmation of deletion and overflow remain.
- Time, CPU, RAM, MIDI RX/TX, OSC RX/TX and audio-availability are the original telemetry sources at their original coordinates and refreshing rates. Their old three separate card borders are replaced with **one shared Midnight Slate monitor strip and narrow dividers**.
- Do not move or resize the workspace, Master Audio, mixer, Inspector, Timeline, or 72px cue header.

## BPM / Auto Stop

- The fixed **184 px** lane retains only `BPM` and `AUTO STOP` at the top. No `CUE METRICS`, `NEXT`, `CELLS` or new controls.
- A single BPM numeric label is vertically centred in each 62px row; the binary Auto Stop switch retains its action.
- Clicking the BPM uses a **native child `juce::TextEditor`** under `CueMetricsOverlay`, not a `DialogWindow`.
- Editor name: `inlineBpmEditor`; existing `MainComponent::editRowBpm` now delegates to it.
- Return: try to commit; invalid values keep focus and allow correction. Escape: cancel with no mutation. Focus loss: commit if valid, otherwise cancel. Scroll past the fixed heading: commit valid value or discard invalid value and hide the editor. The editor is positioned in the fixed lane from `CueGridGeometry::overlayRowTop` and follows viewport scroll.
- Validation: nonempty decimal with at most one dot; finite numeric value 20–400 BPM, plus `validateShow` on a candidate copy. The ID of the cue is captured at edit start to avoid committing to a different cue if indices change. Subsequent tempo changes remain unchanged. `changed("Cue initial BPM updated.",true)` preserves the prior publish/transport policy.

## Auto Stop defaults

The beta.18 behavior remains unchanged: only the cue toggle is present in the fixed lane; per-cell exceptions live in context menus and Inspector and show `CONT.` in cells. The data remains in the existing `.cuelist` JSON v10 fields.

## Regression gates

- `tests/UiRefinementTests.h`: native editor displayed, with no modal, correct Esc.
- `tests/CueGridN3VisualTests.h`: valid Enter, invalid values, Escape, scroll alignment and stable row identity.
- `tests/OperatorNavigationTests.h`: two-tier header, active tab accent, no menu/monitor overlap from 1280 to 3840px; existing menu actions untouched.
- GitHub Windows build + full CTest required. Final visual inspection on Windows at different DPI and hardware engine testing still required before a release sign-off.

**Protected:** `src/ColumnUtilities.cpp`, Master Audio and all its fader/pan/mute/limiter/meter/learn geometry, Audio/MIDI/OSC engines, Inspector, Timeline, routing/mapping, schema JSON, portable build.
