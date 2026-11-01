# THE CUELIST — Native V2 / N3.2: intestazioni e Cue Name / Transport

**Baseline:** `main` `0.9.0-beta.13` (N3.1 già integrata)  
**Versione di lavoro:** `0.9.0-beta.14`  
**Branch:** `feat/native-cue-grid-headers-beta14`

## Intervento reale nei componenti JUCE

- `src/CueViews.cpp`: header della matrice nativo più continuo e leggero, accenti funzionali in fascia superiore, titolo cue in Hind da 15, fascia stato distinta e comandi Play/Stop/More sulla riga inferiore. Rimane il numero progressivo, lo stato NEXT/PLAYING/PAUSED/MUTED/STANDBY, e il pulsante GO dedicato.
- `src/MainComponent.h`: `rowGoBounds`, `rowNameBounds`, `rowStatusBounds`, `rowPlayBounds`, `rowStopBounds`, `rowMenuBounds` e `rowTitleHit` sono le API comuni di paint e input nella lane fissa e nello scroller.
- `src/CueOperations.cpp`: `positionInlineNameEditor()` legge `CueGrid::rowNameBounds(0)` e `CueGridGeometry::overlayRowTop()` per posizionare l'editor con lo stesso scroll della lane.
- `CueGrid` e `PinnedCueHeaders` implementano `juce::TooltipClient` per mostrare il nome completo **solo** quando è più largo della fascia disegnata (calcolo con il font Hind utilizzato). Il già esistente `juce::TooltipWindow` rimane l'unico gestore.

## Geometria e protezioni

La riga rimane **62 px** (più inset 4 px in paint), intestazione **72 px**, Cue/Master **318 px**, colonne **152/48 px**, BPM **184 px**. Non sono state cambiate le misure originali dell'Audio Master né quelle usate per l'allineamento del mixer. La fascia titolo passa da 94 a **196 px**, con GO invariato a sinistra e Play/Stop/More su seconda fascia.

Le regioni di GO, Play, Stop e menu rimangono disgiunte fra loro e dalla fascia del titolo; la selezione e il doppio click nella nuova area del nome non eseguono il trasporto. I click effettivi sui comandi utilizzano gli stessi rettangoli disegnati. Le operazioni di colonna, drag dei clip e della cue, importazione, menu, rinomina e collasso restano nel codice nativo esistente.

Il tooltip non modifica le clip e non cambia l'editor, il colore manuale delle righe o il modello dello show.

## Verifiche automatiche

- `tests/UiRefinementTests.h`: larghezza della fascia titolo, assenza di intersezioni fra tutte le regioni operative, dimensioni e spaziatura dei comandi.
- `tests/CueGridGeometryTests.h`: geometria del titolo in 62 px, separazione dei comandi e test end-to-end JUCE con selezione della parte estesa del nome, NEXT non attivato e rinomina inline salvata.
- Restano le suite precedenti N3.1 per 13 cue, 11 colonne, 1280×720, 1647×895, 1920×1080 e 3840×1080.

## Esclusioni e prossimi gate

Questa PR **non** conclude tutto il redesign della Cue Grid. Le righe/celle continue e gli indicatori completi appartengono a N3.3, la banda BPM/Auto Stop a N3.4 e stress/scaling/focus/performance completi a N3.5. Le modifiche sono limitate alle intestazioni native e alla lane cue.

Nessuna modifica a `ColumnUtilities.cpp`, al Master Audio, al mixer, ai motori Audio/MIDI/OSC/Control, al formato `.cuelist` JSON v10, alla sidebar o alla Timeline. Non si produce un pacchetto portable. Compilazione Windows e test hardware/visivi sono gate separati: non dichiararli superati senza risultati reali.
