# THE CUELIST — Redesign nativo V2, fase N3.4: BPM / Auto Stop

**Base verificata:** main 0.9.0-beta.16 (PR #100 N3.3 integrata)  
**Proposta:** 0.9.0-beta.17  
**Branch:** `feat/native-cue-metrics-n34-beta17`

## Ambito

Questo incremento riguarda esclusivamente il componente nativo `CueMetricsOverlay` della Cue Grid e i relativi test. Nessun mockup HTML. Il Master Audio, fader, pan, meter, mute, Quick Learn, routing e tutti gli altri controlli del mixer rimangono fuori ambito.

## Verifica architetturale

- `WorkspaceLayout` colloca `geometry.metrics` immediatamente a destra del viewport, senza un nuovo scroll orizzontale. Larghezza nativa: **184 px**.
- L'unico controller di scorrimento è ancora `MainComponent::viewport`. Le righe del componente fisso chiamano `CueGridGeometry::overlayRowTop(row, viewport.getViewPositionY())` e l'hit test usa `overlayRowAt` con la stessa coordinata Y.
- La fascia è separata dal contenuto scrollabile, mentre la testata resta alta **72 px** e la riga **62 px**; nessuna ricomputazione del Master Audio, della Timeline o delle geometrie dell'Inspector.
- Le operazioni reali restano `editRowBpm`, `setAutoStop` e `grid.toggleAutoStopDetails`; il livello N3.4 modifica soltanto la loro rappresentazione e il riconoscimento del target.
- La fascia mantiene il bordo continuo di 3 px controllato dai test preesistenti. Il testo Auto Stop ON/OFF è indipendente dal colore.

## Intervento

1. **Aspetto:** superfici e selezione sincronizzate con la N3.3, alternanza Midnight Slate, sottili righe divisorie e accentazioni selezione/NEXT/playback utilizzabili anche contemporaneamente. Il colore custom delle cue permane.
2. **Intestazione:** base `palette::panel`, leggero rule teal, `CUE METRICS`, `BPM` e `AUTO STOP`; disegnata *dopo* le righe e protetta con `reduceClipRegion`.
3. **Valore BPM:** testo reale di `row.tempo.front().bpm`, editor diretto con hitbox preesistente; stato secondario `NEXT`, `LIVE`, `PAUSED` o `EDIT BPM` (nessuno stato aggiunto al runtime).
4. **Hitbox:** API statiche di `CueMetricsOverlay`: `bpmEditBounds` 91×62, `autoStopToggleBounds` 78×26, `cellDetailsBounds` 68×20. Nessuna sovrapposizione; la stessa geometria alimenta paint, hover e mouseDown.
5. **Hover:** modifica soltanto la grafica e il cursore, senza invocare callback del modello né cambiare selezione/NEXT. Disclosure CELLS conserva lo stato reale esistente.

## Test

`tests/CueGridN3VisualTests.h` ora verifica:
- non sovrapposizione dei target e mantenimento delle dimensioni;
- identità del colore di sfondo tra riga selezionata nella griglia scrollabile e fascia fissa, con NEXT attivo;
- target della riga 5 con viewport scrollato in X e Y, senza interferenze sul toggle della riga successiva;
- hover ripristinato all'uscita, senza modificare show, NEXT o selezione.

Restano i test di N3.1–N3.3 su intestazioni pinned, ritagli, scrollbar a sinistra, 13×11, 32:9 e interazioni. La verifica manuale su Windows, diverse scale DPI, dati reali e dispositivi audio/MIDI/OSC resta un gate di N3.5.

## Componenti esplicitamente esclusi

`src/ColumnUtilities.cpp`, Master Audio, mixer/strip, Inspector, Timeline, menu, strumenti di mapping, motori, schema JSON v10, distribuzione portable. Nessun nuovo editor né alterazione del trasporto.

**Criterio di integrazione:** CI Windows/CTest verde, nessuna regressione a UX del BPM o dell'Auto Stop, revisione dei pixel e del layout sul PC Windows prima dell'accettazione finale.
