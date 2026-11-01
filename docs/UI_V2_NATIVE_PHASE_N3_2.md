# UI V2 — Fase N3.2: intestazioni e Cue/Transport nativi

**Versione:** `0.9.0-beta.14` · **Baseline:** `main` `0.9.0-beta.13` · **Branch:** `feat/native-cue-grid-headings-beta14`.

## Implementazione JUCE

La fase N3.1 ha unificato le geometrie. Questa PR migliora il **paint nativo** in `src/CueViews.cpp` senza spostare neppure una hitbox.

- **Header Cue:** fondo Midnight Slate continuo, accento teal di soli 3px, titolo Cuelist più leggibile, conteggio cue/clip preservato e indicazione `CUE / TRANSPORT`. Il nome Cuelist rimane rinominabile come prima.
- **Header colonne:** sostituite le grandi superfici colorate con regole superiori da 3px, riempimento neutro, separatori verticali più sottili e testi distinti dai colori Audio/teal, MIDI/amber, OSC/viola, Control/azzurro. I titoli sono leggibili nella stessa geometria originale; le colonne compresse conservano simbolo e iniziale.
- **Controlli secondari:** frecce di spostamento, pulsanti di rimozione, di collasso e il menu `...` hanno superfici cromatiche più leggere. Si conservano i glifi Noto Sans Symbols 2 con fallback vettoriali già verificati. GO, Play/Pause e Stop non cambiano posizione, superficie interattiva o callback.
- **Cue Name e stato:** nome in Hind più leggibile, stato esplicito `PLAYING / PAUSED / NEXT CUE / MUTED / STANDBY` e contrasto coerente anche in assenza di colore. I pulsanti operativi rimangono completamente indipendenti dall'area selezionabile.
- **Tooltip di nomi completi:** le intestazioni cue e colonna *fissate* ricevono tooltip dinamici per i titoli troncati. La trasformazione per il nome delle cue usa l'offset verticale del viewport; le colonne usano la posizione X già condivisa dalla griglia. Nessun click o comportamento di trascinamento aggiuntivo.

## Hitbox e funzioni conservate

I calcoli usati da `CueGrid::clickRowHeader` e `CueGrid::clickColumnHeader` non cambiano. Dimensioni: 62px/riga, 72px/header, 318px per Cue/Transport, 152px per colonna espansa, 48px compressa. Restano inalterati: GO, Play/Pause, Stop, menu contestuali riga/cella, sposta sopra/sotto, aggiungi/elimina, duplicazione, rinomina inline, collasso, scroll e drag & drop; le celle Audio/MIDI/OSC/Control sono identiche a N3.1.

**Master Audio ESCLUSO dal redesign**: nessuna modifica a `src/ColumnUtilities.cpp` e al routing/fader/meter/pan/mute/limiter/Quick Learn; mixer, Inspector, Timeline, sidebar, menu superiori e motori invariati.

## Verifiche

- `tests/CueGridN3VisualTests.h`: snapshot JUCE reale della barra superiore con accenti distinti, hitbox GO/Play/Stop/Menu immutate, tooltip completi nelle overlay fisse e selezione di Cue Name senza attivare GO.
- Invariate ed eseguite le suite native dei casi speciali di drag/drop, riordino, BPM/Auto Stop, colonne collassate, scrollbar e dimensioni 1280×720, 1647×895, 1920×1080, 3840×1080.
- Compilazione Windows + CTest obbligatori prima del merge; per l'approvazione estetica servono comunque screenshot eseguiti sul PC Windows e controllo DPI.

## Prossima fase

**N3.3**: lavorare su righe e celle come superficie continua, focus/hover e indicatori di stato; solo allora valutare la densità di 36–40px, insieme ai click target, senza sacrificare funzioni.
