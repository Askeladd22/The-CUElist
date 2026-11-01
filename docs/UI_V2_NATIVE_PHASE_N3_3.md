# THE CUELIST — Native V2, fase N3.3: righe, celle e stati

**Baseline:** `main` `0.9.0-beta.15` (N3.1, N3.2 e N3.2B già integrati)  
**Versione proposta:** `0.9.0-beta.16`  
**Branch:** `feat/native-cue-grid-rows-cells-beta16`

## Audit delle responsabilità operative

La Cue Grid continua ad essere la `CueGrid` JUCE esistente, senza componenti HTML né riscrittura dei dati.

| Funzione esistente | Riferimento JUCE conservato |
| --- | --- |
| Colonna Cue e trasporto GO/Play/Stop/More | `CueGrid::paint`, `clickRowHeader`, `rowGoBounds/rowPlayBounds/rowStopBounds/rowMenuBounds` |
| Selezione di riga e di cella | `owner.select`, `selectedRow`, `selectedColumn` |
| Stato NEXT/ON DECK | `owner.nextCueIndex`, distinto da `selectedRow` |
| Playback reale | `owner.engine.getCellPlayback` (playing/paused/waiting) |
| Rinomina inline e tooltip | `PinnedCueHeaders`, `positionInlineNameEditor` |
| Menu contestuali, duplicazione, cut/copy/paste | `showRowMenu`, `showCellMenu` e callback correnti |
| Riordino cue/colonne e drag delle clip | `PinnedCueHeaders`, `clickColumnHeader`, `CueGrid::mouseDrag/mouseUp` |
| Editor Audio/MIDI/OSC/Control | `mouseDoubleClick`, `owner.editMidiClip`, `owner.editOscClip`, `owner.editControlSet` e editor attuali |
| Celle e transport indipendenti | `cellPlayBounds`, `cellStopBounds`, `triggerCellAt`, `stopCellAt` |
| Auto Stop di riga e cella | `CueMetricsOverlay`, `autoStopVisibleRows`, `setCellAutoStop` |
| Scroll e mixer | `CueGridGeometry`, `WorkspaceLayout`, unico `juce::Viewport`, `utilities.syncScroll` |

## Modifiche reali

Intervento mirato su `src/CueViews.cpp` e `src/MainComponent.h`.

1. **Riga continua:** fondo Midnight Slate con alternanza minima e separatori verticali/orizzontali sottili. La selezione si estende a tutta la riga, comprese le zone senza contenuti. NEXT e playback si sovrappongono come segnali distinti anziché sostituire la selezione.
2. **Slot vuoti:** non disegnano più pannelli scuri con bordo per ogni cella. Conservano l'indicazione testuale di import/creazione. Una piccola superficie compare solo con hover, che ridisegna la cella interessata e non muta dati, selezione o trasporto.
3. **Contenuti veri:** clip e Control Map mantengono il nome e le informazioni esistenti; l'accento cromatico viene applicato a una piccola barra laterale e al contenuto. Progress, loop, play/stop, auto stop e indicazioni di errore sono preservati.
4. **Stato operativo:** il pallino di attività e il testo WAITING/PAUSED provengono esclusivamente da `engine.getCellPlayback`. Il testo dell'intera cue mantiene PLAYING/PAUSED/NEXT CUE/MUTED/STANDBY. Il design non finge nuovi stati READY/STOPPED/ERROR quando il runtime non li distingue.
5. **Focus:** la cella selezionata mostra un contorno sottile, rafforzato quando il contenitore operatore ha focus. La selezione non esegue clip/GO.

## Contratti rispettati

- Geometria invariata: riga 62 px, header 72 px, lane Cue 318 px, colonne 152 px / compresse 48 px, BPM/Auto Stop 184 px. Nessuna compattazione 36–40 px finché non siano adeguati i target di trasporto.
- **Master Audio totalmente intoccato:** `src/ColumnUtilities.cpp` e ogni dimensione, fader, meter, pan, limiter, mute e Quick Learn rimangono invariati.
- Nessun cambiamento ai dati `.cuelist` v10, al mixer, all'Inspector, Timeline, sidebar, menu, motori, routing, mapping e transport.
- Nessuna build portable.

## Regressioni automatizzate e gate

Aggiunti in `tests/CueGridN3VisualTests.h`: snapshot native JUCE di selezione sull'intera larghezza, hover reversibile senza mutare show/next/selection, marker di file mancante e clip audio caricata. Restano attivi i test precedenti su scroll, BPM, menu, editing, drag/drop e geometria nelle risoluzioni 1280×720, 1647×895, 1920×1080 e 3840×1080 (32:9).

La CI Windows + CTest deve risultare verde **prima** di considerare integrabile la PR; la verifica visiva su PC Windows e i test DPI/accessibilità/performance esaustivi restano necessari per N3.5 e la successiva accettazione operativa.

**N3.4** rifinirà la fascia BPM/Auto Stop fissa per armonizzarla alla matrice; **N3.5** eseguirà la verifica integrata sulle configurazioni dense e dispositivi reali.
