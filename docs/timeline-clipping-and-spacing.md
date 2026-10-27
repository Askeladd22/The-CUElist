# Timeline resizing / remaining black-space repair

This focused follow-up repairs the Timeline when the operator reduces its height. It does not affect cue playback, routing, the audio callback, the project schema or MIDI/OSC editor functionality.

## Container geometry

- The right edge of the **upper** fixed BPM/Auto Stop band and the **lower** Timeline and docked editors now meets the Inspector boundary. The scrolling cue headers, column viewport and Utility strips still use independent fixed/scrolling regions.
- The utility panel's bottom edge exactly meets the dock/Timeline top edge. The previously empty 14 px strip above the resize handle is recovered for the Timeline rather than relocated to the footer.
- The original maximum dock height at 1180×820 and 1440×900 is maintained with its own explicit budget, while the Timeline uses its newly recovered 14 px in its resize calculations.
- The splitter remains centered on the shared border between components, drawn on top of contiguous panels, never across a permanently blank region.

## Timeline paint / child clipping

JUCE's local component clipping is explicitly applied before drawing the Timeline. Content uses three presentation states:

1. **75–119 px**: header/title only. Grid/Snap controls, ruler markings, track lanes and footer/horizontal scrollbar are hidden so none can overlap.
2. **120–169 px**: grid/snap toolbar and time ruler, without the footer, track lanes or horizontal scrollbar. Playback, zoom and project state remain unchanged.
3. **170+ px**: full header, ruler, track lanes, clip rendering, scrollbar and footer.

These thresholds correspond to actual header, ruler, track and footer heights; no negative screen offsets or offscreen hiding is involved. Repeated drag down/up recalculates visibility from the Timeline's *actual bounds*.

## Tests and acceptance

The existing Windows CTest regression suite and added geometry/real JUCE control tests cover compact/intermediate/expanded states, repeated drag reversals, parent-bound child visibility, sidebar/Inspector gap-free boundaries, standalone and docked MIDI/OSC editors, minimum cue-grid/Timeline heights, and preserved dock budgets.

Manual visual acceptance is still necessary on Windows, in particular across 1180×820, 1440×900 and 1647×895, with compact and expanded timelines, different active clip types and many scrolled columns. The Timeline remains integrated; detachable mode is deliberately outside scope.
