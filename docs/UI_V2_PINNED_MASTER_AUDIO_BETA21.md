# THE CUELIST — V2 Native / beta.21: pinned Master Audio

**Baseline:** main 0.9.0-beta.20.  
**User reference:** screenshot of the Cuelist left lane with Master Audio at its base. The Master may be redesigned provided it **stays permanently locked into that lane** while cue and column content scroll.

## Real architecture verified

- `WorkspaceLayout` places `utilities` directly underneath the cue viewport at the same fixed X. The layout chooses panel height 180 or 208px, depending on window size.
- `ColumnUtilities` is not a child of the scrolling `CueViewport`. Its first child, `scrollableStrips`, starts at **X=318** and clips horizontally scrolling audio/MIDI/OSC/Control strips. Master controls are direct children of `ColumnUtilities`, outside `scrollableStrips`.
- `CueGrid::nameWidth` and `MasterAudioLayout::laneWidth` are statically asserted equal: **318px**. The position must remain fixed when `utilities.syncScroll(viewport.getViewPositionX())` is called; vertical cue scrolling must not move the Master.
- Master Bus callback and data contracts remain `MainComponent::setMasterAudio`, `setMasterPan`, `setMasterMute`, `setMasterOutput`, `setSafetyLimiter`, plus `refreshRouting`, `refreshMeters`, Quick Learn and native GainMeterFader.

## Visual refresh

The `MasterAudioLayout` struct now owns layout rectangles for the real controls and their visual captions, instead of unrelated hardcoded coordinates in `ColumnUtilities::resized` and `paint`. The fixed card uses a restrained Midnight Slate raised surface, thin teal rule, header and L/R marker, and coherent caption alignment. The fader and stereo peaks remain prominent at left; output and pan form an upper group; limiter and mute form a lower group. No control may extend beyond 318px or escape the utility panel.

## Regression from beta.20

Windows CI run 41340313310 compiled successfully but CTest failed **one snapshot test** (N3.4B CONT. badge): it compared an idle-cell pixel with a cell still hovered. The test now calls `CueGrid::mouseExit` before matching the restored default pixel. The implementation of Auto Stop and persisted exceptions is unchanged.

## Testing

`tests/UiRefinementTests.h` checks the five original controls remain direct children of the pinned utility panel, their bounds equal the shared geometry and never exceed 318px, the scrollable strip container always begins at 318px, and horizontal/vertical cue scrolls never move the Master. Run at 1280×820, 1440×900, 1647×895, 1920×1080 and 3840×1080.

Retain all existing fader/limiter/routing/mapping tests. **No portable build**. Require green Windows CI/CTest and manual Windows/DPI visual review before integration.

## Explicitly unchanged

Cue Grid logic, fixed BPM/Auto Stop and CONT markers, Inspector, Timeline, popups, audio/MIDI/OSC engines, pan laws, master meter math, limiter signal path, mapping/routing and `.cuelist` JSON v10.
