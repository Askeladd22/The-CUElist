# Control Mappings — focused archive redesign (2026-10-31)

This change implements the supplied **Redesign Control Mappings** brief
without replacing the playback, Control Column priority or Learn engine.

## Control Mappings (header-level window)

The global **Control Mappings** command opens an archive editor:

```text
CONTROL MAPPINGS
Map Selection: [ Default Map (fallback) v ] [ New Map ] [ Duplicate ] [ ... ]
[MIDI Learn] [OSC Learn] [Stop Learn] [+ Add Mapping] [Manage functions]

FUNCTION                    INPUT                      DESTINATION
Master Volume               MIDI CC 1                  Master Volume
Resolume Brightness         MIDI CC 21                 MIDI CC 74
Channel Mute                OSC /mute/1                Channel 1 Mute

[Editing one mapping? Show function, manual input and route editor here.]
```

- Default Map is **always** the first item and never deleteable or
  renamable. It is the lowest-priority fallback; contextual Control
  Columns override it according to the existing priority/activation rules.
- New Map creates a **reusable saved map** without adding a Control
  Column or assigning it to any cue.
- Duplicate clones mappings, assigning new stable binding IDs rather than
  sharing mutable list storage with the source map.
- The `...` menu groups **Rename, Delete unused, Export, Import**.
  Deletion refuses maps referenced by one or more cue cells.
- Each mapping row now shows *Function / Input / Destination*. Selecting
  it opens the details pane. Other rows keep their settings out of sight.
- `+ Add Mapping` opens the editor, without creating a new map or
  Control Column. Manual MIDI input uses the source configured in
  Settings → MIDI Devices; OSC inputs require an address beginning in
  `/`. Hardware Learn may alternatively capture it.
- The editor retains the existing target, output channel, MIDI CC,
  OSC address, takeover, response curve, minimum, maximum, inversion
  and legacy scope details when relevant. A project-wide abstract
  function owns its destination; assigning another physical input does
  **not** create another function.
- MIDI Learn and OSC Learn operate on the selected map and can be
  repeated until Stop Learn or the editor closes.
- Cue Presets, device selection, OSC input ports, and Audio Devices
  are **not** configured in this window. Legacy preset values remain
  readable from old project files; existing Control Column and Settings
  screens remain responsible for activation and hardware configuration.

## Separate Control Columns

Right-click a Control Column cue cell to **assign a saved map, remove its
assignment, or create/edit a local override**. This is unchanged. Named
maps can be assigned to more than one cue without copying their content.

The active mapping engine still resolves:

1. Default Map (global fallback).
2. Activated Control Column overrides in their established priority order.
3. Stable abstract function identity overrides the physical input; a
   function's output/destination is edited independently.

The new window does not change cue selection/activation or device
routing; editing a map is not equivalent to assigning it to a cue.

## Single-map import/export

`src/ControlMapArchive.h` defines a separate
`TheCuelistControlMap` JSON v1 format.

- Only one map's input assignments are exported, never cue rows,
  Control Column assignments, connected MIDI devices or OSC UDP settings.
- `...` → Export Map saves the selected map.
- `...` → Import Map creates a **new independently named** reusable
  map. It never overwrites Default Map.
- Each abstract function is looked up by **project function name** on
  import; the importing show's stable function ID and destination
  are retained. Missing names cause explicit errors instead of silently
  routing inputs to an unrelated destination.
- Import is transactional: all entries are schema-validated, have fresh
  binding IDs and are applied only if the whole project still validates.
- To reuse a map whose function names differ in another project, create
  or rename the intended abstract functions first.

**Legacy controller preset files** (`TheCuelistControllerPreset`) are
a different format; they are not silently treated as a saved map. Legacy
import/export behavior must remain available for backwards compatibility.

## Windows verification checklist

1. Open **Control Mappings**. The first selection must be
   `Default Map (fallback)`; no Cue Presets/device/port controls may
   occupy the map archive view.
2. New Map, Duplicate, Rename, Delete; ensure an assigned map cannot be
   deleted; confirm no new Control Column appears automatically.
3. Create a MIDI CC mapping manually, edit its input number, choose
   function, save; do the same for an OSC address and source/destination
   field visibility.
4. Choose a row, edit curve/invert/takeover, Save Mapping; no other
   mapping or project function's ID may change.
5. Trigger MIDI Learn and OSC Learn using real hardware/OSC input,
   perform consecutive captures, and Stop Learn.
6. Export a map, import into the same project with a new name,
   verify contents and stable destination; corrupt JSON import must
   leave the show untouched.
7. Assign the same saved map in multiple Control Column cue cells,
   activate GO, verify override precedence and Default Map fallback.
8. Verify all existing projects open and older Cue Preset references
   remain valid. Capture at 1280×720 and 1920×1080.

**Note:** headless/unit tests and compilation cannot substitute for
actual GUI/audio/MIDI/OSC device QA; do not claim hardware behavior was
tested when the CI only ran CTest.
