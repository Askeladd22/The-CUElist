# The Cuelist — compact UI and selectable audio fader range

## Scope

Implements the operator-requested UI/layout/fader adjustments without changing
cue transport, stored Show v10 schema, MIDI/OSC network routing or audio callback.

- Workspace outer margins: cue list starts at x=184, y=78; Inspector panel
  starts at y=84 and reaches the same bottom limit as before. Existing Inspector
  contents and the two section cards move upward by 66 px.
- Cue row headers are painted and operated in a pinned JUCE component.
  The **same CueGrid rendering** provides row names, GO, Play/Pause, Stop,
  reorder, delete and context menu; one hit-target helper is shared by grid
  and pinned header. Vertical scrolling remains synchronized.
- Cue columns use 136 px (previously 152; -10.5%). Master Audio uses the
  narrower 290 px pinned left area (previously 315). The MIDI, OSC and Audio
  strip controls have separate inner bounds; no global indiscriminate scale.
- BPM / Auto Stop takes an exclusive 184 px right-side band **outside** the
  horizontally scrolling cue viewport. The Utility/mixer also stops before
  this band. A clipped child container keeps scrolled strips from painting
  over the pinned Master Audio area at any vertical position.
- MIDI and OSC collapsed mute controls use the same vertical position as the
  collapsed Audio Mute. The Pan value label moves upward by 5 px; L/R does
  not move.
- Row Play/Pause uses filled JUCE vector paths, independent of installed fonts.

## Fader Range — application setting

The **Preferences → Fader Range** submenu contains two mutually exclusive
choices: `-inf dB / +6 dB` and `-inf dB / 0 dB`.

This value persists in JUCE's per-user `PropertiesFile` storage under the
`audioFaderCeilingDb` key. It does *not* modify a Show's saved gain, media
or any existing routing. Changing the value redraws the Master and column
faders without causing `onGainChanged` callbacks.

- Top position is exactly +6 dB or 0 dB (linear gain 1.995262 / 1.0).
- Bottom position is exact digital silence (linear gain 0; displayed -inf).
- Existing out-of-range project values are preserved on loading and on
  preference changes; when an operator moves the fader they select a level
  within the newly selected range.
- MIDI control mappings aimed at the Master or a column **gain** also respect
  the active fader ceiling, including custom mappings above unity. Pan,
  mute, sends, routing and output control mappings are not capped.
- Both stereo meter channels remain independent of one another.

## Automated acceptance and remaining operator QA

Added/updated regression coverage for pure geometry, pinned row-header
interactions during horizontal scroll, fixed right-band bounds, compact mute
alignment, app-preference persistence and nonmutation, MIDI fader maxima,
and silence/unity/+6 fader scale mapping. Existing cue, Master, MIDI/OSC,
project, playback and safety tests remain included.

Still verify visually on a Windows desktop at **1180×820**, **1440×900**,
**1647×895** and a larger monitor. In a show containing many columns and
rows, scroll fully right and down, check right BPM, left cue controls, utility
Master and bottom panels. Confirm real MIDI controllers at low/high CC
values, no unexpected gain after switching presets, and external audio/MIDI/OSC
interfaces. Automated CI does not substitute for these physical checks.
