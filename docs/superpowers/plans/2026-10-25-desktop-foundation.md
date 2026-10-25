# Desktop Foundation Implementation Plan

> **For agentic workers:** use superpowers:executing-plans, task by task.

**Goal:** primo programma desktop compilabile con editor e riproduzione WAV reale.
**Architecture:** modello versionato separato dalla GUI; render audio offline testabile e callback senza allocazioni; pubblicazione snapshot sul confine di blocco.
**Tech Stack:** C++17, JUCE 8.0.12, CMake, MSVC.
**Spec:** ../../design.md (specifica di incremento; origine: TXT utente).

## Global Constraints
- Windows standalone, nessun video.
- Colonne Audio/MIDI/OSC; tempo per riga, velocità audio indipendente dai BPM.
- Panic e gestione esplicita di errori/file mancanti.
- Nessuna rete MIDI/OSC o timecode presentata come funzionante.

## Review Focus
- Progetto malformato non deve sostituire quello corrente.
- Panic deve rendere silenzioso il blocco successivo, anche con loop attivi.
- File mancanti e destinazioni non scrivibili non devono perdere dati.
- Sample rate diverso non deve cambiare durata/intonazione.
- Cambio snapshot e chiusura non devono liberare memoria usata dal callback.

### Task 1: modello e persistenza
Files: src/ShowModel.h, src/ShowModel.cpp, tests/Tests.cpp, CMakeLists.txt.
Interfaces: Show, Row, Column, Cell; validateShow; encodeShow/decodeShow; secondsAtBeat.
- [x] Scrivere test per mappe tempo indipendenti, input malformato e roundtrip.
- [x] Osservare test fallire, implementare modello e conversioni, rieseguire suite.

### Task 2: motore audio
Files: src/AudioEngine.h, src/AudioEngine.cpp, tests/Tests.cpp.
Interfaces: RenderState::render/trigger/panic; AudioEngine::publish/send/deviceManager.
- [x] Testare campioni prodotti, loop, esclusione dal trigger, Panic e sample rate.
- [x] Implementare decoding WAV e snapshot con reclamazione sul thread UI.
- [x] Build e suite offline completa.

### Task 3: desktop
Files: src/Main.cpp, src/MainComponent.h, src/MainComponent.cpp, src/Theme.h, scripts/build.ps1, Avvia.cmd, README.md.
- [x] Griglia scrollabile, sidebar, inspector, timeline di durata, import e trasporto.
- [x] File chooser per aprire/salvare/importare; errori leggibili e protezione modifiche non salvate.
- [x] Compilare Release, smoke test di avvio e controllo grafico. Prove di ascolto/hardware da completare.
- [x] Revisione indipendente del codice e aggiornamento stato finale.
