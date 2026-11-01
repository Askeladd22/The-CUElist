# UI V2 — Fase N2: sidebar senza duplicazioni

**Repository/branch:** `Askeladd22/The_Cuelist`, `feat/native-operator-header-beta10`.  
**Versione:** `0.9.0-beta.11`. Continuazione del branch N1 dopo il merge verde della PR #93.

## Modifica nativa

La UI dei sette menu `File / Edit / Show / Cuelists / Devices / View / Guide` è già implementata. La fase N2 elimina soltanto le ridondanze ancora mostrate nella sidebar:

- `New / Open / Save` non sono più disegnati a sinistra: le funzioni originali sono raggiungibili da `File`, tramite lo **stesso callback** che gestisce conferma di abbandono, recupero e salvataggi.
- `Settings / Preferences` non sono più un secondo gruppo di pulsanti: `Show → Settings` e `Devices → MIDI / OSC Settings` continuano a usare il popup di configurazione JUCE già esistente con tutte le opzioni (fader ceiling, clock, MIDI input/output, OSC e audio). Poiché l'originario pulsante è ora nascosto, il popup viene ancorato al **menu superiore visibile Show**, evitando target di menu invisibili.
- `Control Mapping` è raggiungibile da `Devices → Control Mapping`, con la finestra nativa e le azioni di Learn che già funzionavano.
- L'header `PROJECT` e il riquadro con `N cues / M columns` sono rimossi dalla sidebar perché ripetono la matrice cue. **La rinomina show non sparisce:** il singolo nome di show nel riquadro WORKSPACE rimane il medesimo `projectNameLabel` JUCE editabile, spostato più in alto, e `Show → Rename show` lo apre.
- L'indicatore **UNSAVED CHANGES** resta visibile quando necessario, senza duplicare numeri o un altro riquadro.
- Il pannello `ON DECK`, `GO / Space`, `PANIC / Esc` e gli hotspot MIDI/OSC di Quick Learn restano in posizione e con gli stessi callback.
- I sette menu e il responsive `More` non cambiano.

## Compatibilità e limiti

`newButton`, `openButton`, `saveButton`, `preferencesButton`, `deviceButton` e `controlButton` restano componenti JUCE con i callback originali, ma sono **non visibili**; File/Show/Devices usano gli stessi handler, senza ridefinire alcuna funzione. Nessun cambio ai motori, ai callback di trasporto, ai salvataggi `.cuelist` schema v10, alle cue list o alle colonne.

**Master Audio escluso dal redesign:** `src/ColumnUtilities.cpp`, il `Master Bus`, i fader, il meter stereo, pan, mute, limiter e Quick Learn non vengono modificati.

## Gate di test

- `tests/UiRefinementTests.h` ora controlla l'unico show editabile, sidebar senza duplicazioni, menu File/Show/Devices oppure More raggiungibili su finestre da 1180 a 1920 px, e persistenza dello show rinominato.
- `tests/OperatorNavigationTests.h` controlla che nessun comando presente nella UI N1 sia sparito, incluso il Master.
- Compilazione Windows e suite CTest restano obbligatorie prima del merge.

**Fase N3 (separata):** modernizzare la matrice cue e le strip non-Master con un modello di geometria condiviso, senza perdere funzionalità e senza toccare il Master Audio.
