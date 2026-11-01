# The Cuelist — V2 nativo, N3.2B: nome cue e trasporto su due fasce

**Branch:** `feat/native-cue-title-two-tier-beta15`  
**Versione:** `0.9.0-beta.15`  
**Baseline:** `main` `0.9.0-beta.14`, che integra già le intestazioni N3.2.

## Ambito delle modifiche native

La PR amplia il titolo della cue da 94 a 196 px **senza aumentare la larghezza originale di 318 px della lane Cue/Master** e senza ridurre la riga da 62 px. Il titolo si trova sulla fascia superiore; il numero progressivo e lo stato restano leggibili nella fascia inferiore a sinistra. Play/Pause, Stop e menu contestuale si spostano nella fascia inferiore a destra; GO mantiene la propria area distinta a sinistra.

I nuovi rettangoli condivisi in `src/MainComponent.h` (`rowGoBounds`, `rowNameBounds`, `rowStatusBounds`, `rowPlayBounds`, `rowStopBounds`, `rowMenuBounds`, `rowTitleHit`) sono utilizzati dal disegno JUCE e dall'input per la riga scorrevole e la lane fissata. `src/CueOperations.cpp` posiziona l'editor inline usando esattamente `rowNameBounds` e l'offset verticale di `CueGridGeometry`. `PinnedCueHeaders` conserva il tooltip dei nomi completi solo quando la nuova larghezza non basta. I tooltip nativi delle colonne della beta.14 restano invariati.

## Sicurezza e funzionalità preesistenti

- La semplice selezione della fascia titolo non attiva mai GO, Play o Stop; le hitbox sono disgiunte anche durante il drag e il doppio clic.
- Non vengono cambiati la geometria di colonne e scroll, il mixer, l'Inspector, la Timeline, le celle e i loro editor, i motori Audio/MIDI/OSC/Control, la banda fissa BPM/Auto Stop, il routing/mapping o il formato `.cuelist` schema JSON v10.
- **Master Audio assolutamente invariato**: non vengono modificati `src/ColumnUtilities.cpp`, dimensioni o callback del Master.
- Non si produce alcuna build portable.

## Test e gate

`tests/CueGridN3VisualTests.h` aggiorna i controlli dei rettangoli dopo la nuova disposizione e aggiunge una prova GUI di selezione nell'area ampliata del titolo, assenza di GO e rinomina inline salvata. `tests/UiRefinementTests.h` verifica separazione spaziale e hitbox dentro la riga. Restano le suite N3.1 e N3.2 originali per scroll, colonne miste, BPM fisso e geometrie 1280×720, 1647×895, 1920×1080, 3840×1080.

**Prima del merge:** Windows GitHub Actions, CTest, nessuna regressione, controllo su PC Windows in diverse risoluzioni/DPI e verifica dell'uso operativo. Questa modifica è una rifinitura di N3.2, non completa ancora N3.3–N3.5: non comprende il redesign delle celle, il nuovo schema degli stati o la densità finale 36–40 px.
