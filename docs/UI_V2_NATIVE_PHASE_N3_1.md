# Native UI V2 — Fase N3.1: audit e geometria Cue Grid

**Versione:** `0.9.0-beta.13` · **Baseline:** `main` `0.9.0-beta.12` · **Branch:** `feat/native-cue-grid-geometry-beta13`.

## Stato e perimetro

Questo incremento **modifica davvero il codice JUCE**, non il prototipo HTML. Introduce il contratto puro `src/CueGridGeometry.h` e lo utilizza nei metodi esistenti di `CueGrid`, delle intestazioni fisse e dell'overlay BPM/Auto Stop. I valori rimangono deliberatamente quelli della GUI originale: 62 px/riga, 72 px/intestazione, 318 px/lane Cue/trasporto, 152 px/colonna espansa, 48 px/collassata, 184 px per BPM/Auto Stop, 4 px di inset, 44 px di margine del contenuto scorrevole. **Non sono ancora state applicate le righe 36–40 px del mockup**; farlo prima di adattare tutte le hitbox sarebbe rischioso.

La colonna Master audio esistente in `ColumnUtilities.cpp` segue la larghezza originale di 318 px ed è **esclusa** da ogni cambiamento di rendering, dimensioni o callback. Il mixer legge già `owner.grid.columnX(i)` e `columnWidthAt(i)`; queste ora passano per il contratto condiviso senza cambiare risultati.

## Audit dei componenti e responsabilità

| Componente / file | Funzioni esistenti da preservare | Dipendenze geometriche N3.1 |
|---|---|---|
| `CueGrid` in `src/CueViews.cpp` | Paint righe e celle, colonna Name/transport, stato cue, play/stop/GO, doppio click, context menu, selezione, import WAV/MP3/MIDI, clip drag/drop, collasso/riordino colonne | `widthAt`, `xAt`, `columnAt`, `rowAt`, `rowTop` |
| `PinnedCueHeaders` | Titoli, bottoni riga, GO/stop, rinomina, riordino con drag, rotella con shift | `rowAt`, `overlayRowTop`; usa la stessa larghezza del Cue Grid |
| `PinnedColumnHeaders` | Header sempre visibile, modifica titolo, reorder, collapse, tasti di azione | `pinnedColumnGridX`: stesso offset dello scroller delle celle |
| `CueMetricsOverlay` | BPM editabile, Auto Stop e dettaglio per cella, menu contestuale, rotella | `overlayRowAt`, `rowLocalY`, `overlayRowTop`; resta fuori dallo scroll orizzontale |
| `WorkspaceLayout` | Assegna viewport e bande separate, calcola scroll orizzontale/verticale e l'ingombro dell'Inspector/Timeline | I default derivano da `CueGridGeometry`; la coda di scorrimento è condivisa |
| `MainComponent` | Possiede il viewport, chiama `utilities.syncScroll(viewport.getViewPositionX())`, posiziona overlays e allinea editor inline | Usa valori esposti da `CueGrid` (ora alias della sorgente comune) |
| `ColumnUtilities` | Strip audio/MIDI/OSC/Control, routing, mute, fase, fader, Quick Learn e Master | Legge coordinate da `owner.grid.columnX`/ `columnWidthAt`; **non modificato** |
| `Theme.h` | Midnight Slate, font Hind/Exo2, icone, focus e scrollbar | **Non modificato**: le rifiniture grafiche appartengono alle fasi N3.2/N3.3 |

### Contratto conservativo delle coordinate

- `columnX(index)` e `columnWidthAt(index)` rimangono le API native già utilizzate da paint, hit test, header e mixer; internamente ora delegano entrambe alla stessa sorgente.
- `columnAt(x)` usa la medesima successione di larghezze (compresi i 48 px delle colonne collassate). Coordinate prima della lane Cue o dopo l'ultima colonna restituiscono -1.
- `rowAt(gridY)`, `rowLocalY(gridY)`, `overlayRowAt(localY,scrollY)` e `overlayRowTop(row,scrollY)` centralizzano le conversioni su cui dipendono disegno e input.
- Il punto di una colonna nella header fissa è `nameWidth + scrollX + localX`. Lo scroll rimane proprietà del **medesimo `juce::Viewport`**, con `utilities.syncScroll` già connesso al callback: nessun secondo scroller è stato introdotto.
- La banda BPM/Auto Stop rimane a destra del viewport, non dentro la parte scorrevole. Il bordo col mixer non cambia.

## Test e accettazione

`tests/CueGridGeometryTests.h` copre: parità esatta con le dimensioni e le hitbox attuali, 13 cue/11 colonne con larghezze miste e colonne collassate, hit test ai bordi, offset X orizzontali e Y verticali, allineamento BPM fisso/viewport/mixer e geometrie a **1280×720, 1647×895, 1920×1080 e 3840×1080 (32:9)**. Rimangono in esecuzione tutte le suite preesistenti dedicate a GO/Stop, celle, menu, drag/drop, editor, Cuelist, Master, mixer, Inspector e serializzazione schema v10.

**Vincolo di questa PR:** nessuna modifica a paint visivo, hitbox pixel, comportamento di trasporto, device routing, editor, mixer o al Master Audio. Nessuna build portable. Il mockup V2.3 resta il riferimento per **N3.2 → N3.5**, da implementare in PR separate dopo CI verde e senza eliminare funzionalità.

## Seguito pianificato

- **N3.2:** ridisegno nativo di intestazioni e Cue Name/Transport sul contratto geometrico, preservando i click effettivi.
- **N3.3:** rifinitura delle righe/celle e stati, con test di focus, hover e layout denso; riduzione delle altezze solo quando gli input restano coerenti.
- **N3.4:** banda BPM/Auto Stop fissa, con test anche sotto scroll estremo e Inspector collassato.
- **N3.5:** stress test 12 Cuelist/13 righe/11 colonne, diverse risoluzioni e DPI; verifica manuale GPU Windows e hardware MIDI/OSC prima della certificazione d'uso live.
