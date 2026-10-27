# The Cuelist — Stabilization (0.7) and transport foundations (0.8)

**Current development version:** 0.7.0-alpha.1, **show format:** v10. A successful build is not certification for unattended or safety-critical show operation.

## Implemented in the stabilization preview

- Crash-recovery snapshots: validated, atomic, reference original media rather than duplicating it; after an interrupted process the operator chooses whether to restore. The current saved show and its backup are never overwritten by recovery.
- Application-level Preflight: checks model validity, media existence/emptiness, audio device status, configured MIDI output IDs, MIDI controller presence and OSC endpoint configuration. It intentionally does not claim OSC delivery.
- Preparation before activation: when opening a show, decode, load media and check the memory budget before interrupting playback. MIDI output handles are selected after those checks.
- Transport telemetry: counters for local audio command overflow, MIDI input drops, MIDI output sent/dropped messages and OSC UDP sent/dropped packets; a visible Diagnostics report.
- Compiler cleanliness: shadowed `cursor` and `dockHeight` locals renamed.
- Tests: recovery atomicity/corruption, preflight failures and no false positive OSC reachability, offline audio callback stress, sample timestamp consistency at common rates. Windows CI also extracts and smoke-launches the portable program.

## Operator recovery procedure

1. Save the working show normally: this keeps its media generation and `.bak` backup.
2. After a power loss or crash, reopen The Cuelist; if a checkpoint is available, choose **Restore**.
3. Review the recovered cue matrix and run **Preflight**. Missing media may need reconnecting; no cue will start automatically.
4. Select **Save** and save the recovered project to a suitable location. Do not rely on recovery snapshots as your normal archival system.
5. If recovery is not wanted choose **Discard checkpoint**. **Later** defers the decision for the next launch.

**Limitations:** checkpoint updates are periodic (~20 seconds) and contain applied in-memory model edits, not every keystroke in an Inspector field. Long file writes can briefly occupy the UI thread; moving serialization/writes to a dedicated worker remains 0.8 work. Checkpoints do not make audio media themselves redundant. They are not a substitute for show backups.

## Timing and devices — 0.8 foundations only

One monotonic origin is captured at each audio callback and passed to both outbound MIDI and OSC workers. Sample offsets are converted using that same origin, preventing two independent clock reads from introducing systematic inter-protocol skew. Each output worker still has OS scheduling and transport jitter: this change **does not** promise sample-accurate delivery on a physical MIDI interface or UDP network.

The periodic MIDI output refresh closes missing configured devices and attempts reconnect only to the **exact same device identifier**. Reconnection resets route time and invalidates old MIDI streams, so queued notes from before a disconnect cannot be replayed on the recovered port. No automatic substitution of another device is allowed.

## Manual hardware integration checklist

- On the intended Windows computer, preflight a real show with WAV/MP3, imported MIDI clips, OSC events and Control Sets.
- Repeatedly issue GO, STOP, PANIC while audio output is live. Verify exact routing and that stopping one cue does not stop unrelated cues.
- Disconnect and reconnect the configured MIDI output and MIDI input during rehearsal. Inspect diagnostics and check note-offs, reconnect behavior and the absence of stuck notes.
- Verify OSC/UDP bytes on loopback (automated), then with the real target software, including wrong destination address/port and target shutdown/restart. UDP has no built-in delivery acknowledgement.
- Measure scheduled MIDI/OSC event times relative to the audio playback start with external timestamp-capable equipment. Record worst-case and percentile jitter over long tests; offline tests alone do not characterize device latency.
- Leave a representative show running for several hours and monitor memory, CPU, device disconnects and drop counters. Repeat on multiple audio interfaces and Windows versions.
- Test recovery after terminating the process without a clean exit and after power interruption using **disposable show copies**, not production originals.

## Release policy

`VERSION.txt` is the human-facing version authority. Keep its numeric major/minor/patch identical to `project(TheCuelist VERSION ...)` in CMake. Packages read `VERSION.txt`, and the executable embeds that same version as a quoted compile definition. Alpha/beta suffixes are allowed. Versioning of the serialized `.cuelist` show schema is independent and should change only with incompatible model changes; additive optional fields can remain within v10.

The standard Windows workflow compiles and runs CTest, generates a package, validates its content/hashes, smoke-launches the extracted executable and uploads the archive. Hardware tests remain a separate release gate. Do not promote a preview to 1.0 based only on green CI.
