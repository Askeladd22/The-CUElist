# The CUElist — V2.1: mixer integrato nella matrice

**Decisione di progetto (1 novembre 2026):** approvata la direzione della parte superiore della UI V2, da non alterare in questa iterazione. Il mixer **non deve costituire un pannello autonomo**. Deve essere il prolungamento inferiore delle stesse colonne Audio/MIDI/OSC/Control della cue matrix, senza un secondo scroller orizzontale visibile e senza singole card.

**Anteprima aggiornata:** [`design/operator-workspace-v2.html`](../design/operator-workspace-v2.html) sul branch di studio; i suoi controlli sono dimostrativi e non inviano comandi.

## Contratto geometrico di riferimento

| Regione | Larghezza nel prototipo | Movimento | Contenuto |
|---|---:|---|---|
| Cue header a sinistra | `--cue: 280px` | Fissa a sinistra | Nome/numero cue, selezione, trasporto per riga |
| 11 colonne | `--cell: 109px` ciascuna | Un solo scroll orizzontale sincronizzato | Audio/MIDI/OSC/Control; editor cue sopra, strumenti di colonna direttamente sotto |
| Banda BPM / Auto Stop | `--metric: 128px` | Fissa a destra | Stato per riga e continuità visiva nel tratto inferiore |
| Mixer integrato aperto | 150px di altezza | Stesse x delle colonne | Master nel blocco cue, una strip per ciascuna delle 11 colonne |
| Mixer richiuso | 30px di altezza | Stesse x delle colonne | Solo identificativi colonna; preserva lo spazio della matrice |
| Timeline | 75px, opzionale | Componente distinta, ma trattata come fascia tecnica | Riga di tempo con controlli; non trasforma il mixer in una card |

Le misure sono **di concept**, non target obbligatori JUCE. A 1647×895, 1280×720 e 1920×1080 occorre confrontare con i controlli reali e l'altezza operativa. Il master segue esattamente la larghezza del cue-header; non si inserisce una tredicesima strip sganciata dalle colonne.

### Regola principale di allineamento

Per ogni colonna `c` nell'ordine corrente (incluso il drag&drop), la posizione visibile della relativa strip deve essere

```text
x_strip(c) = width_cue_header
           + sum(width_colonna[i] per i < c)
           - scrollX_matrice
```

La larghezza della strip è **identica** alla larghezza della sua colonna. Eventuali colonne collassate mantengono lo stesso stato anche in basso. Master e banda BPM/Auto Stop restano fuori da `scrollX_matrice`.

Il mockup utilizza le stesse variabili CSS per matrice e fascia mixer e aggiorna `mixerScroll.scrollLeft` solo in risposta allo `scrollLeft` della matrice. Non è un secondo scorrimento indipendente. Per la GUI JUCE usare un **modello condiviso di geometrie di colonna**, non due layout che calcolano le x separatamente.

## Comportamento e differenze per tipo

**Audio.** Livello/meter verticale, fader, valore gain (dB), pan, mute e solo; direzione dell'output (Direct, Master L/R, PRE/POST) e controllo della fase rimangono accessibili nella UI operativa. La preview li riduce a pochi segni ma non autorizza a eliminarli dal software.

**MIDI.** Nome porta/output, stato TX e mute. Non disegnare un fader audio fittizio. L'editor MIDI e l'esecuzione MIDI rimangono invariati.

**OSC.** Destinazione host/porta, stato TX e mute. Non disegnare un fader audio fittizio. L'editor degli eventi, le rampe e l'invio OSC restano invariati.

**Control.** Priorità e Control Set con i comandi esistenti (`Edit set`, ecc.); non rappresentare come canale audio. La preview usa il testo del mapping quale placeholder.

**Master.** Fader/meter master e routing, dentro la stessa fascia visuale, allineati sotto la zona cue. Non esistono più un header “MIXER” sopra una collezione di card e uno spazio orizzontale indipendente dalle colonne.

## Continuità visiva

- Nessuna cornice arrotondata intorno alle singole strip e nessun box esterno “mixer”.
- I divisori sottili fra colonne proseguono nella fascia inferiore mantenendo un'identità verticale; un bordo singolo segna il passaggio tra righe cue e strumenti senza separare due applicazioni.
- Colori semantici già definiti (Audio teal, MIDI amber, OSC purple, Control blu) applicati ai microindicatori; non colorare l'intera superficie delle strip.
- Testo Exo 2, Hind per titoli secondo il sistema tipografico di produzione. Non ridurre i target cliccabili per raggiungere forzosamente i 109px: se necessario usare controlli espandibili/contestuali.

## Trasferimento graduale ai componenti JUCE (fase nativa successiva)

1. Introdurre una struttura `ColumnLayoutMetrics` condivisa dai painter della cue grid e dal calcolo delle strip di `ColumnUtilities`. Le larghezze e gli ID colonna devono provenire dal modello attivo, comprese colonne collassate e riordinate.
2. `CueGrid`, `PinnedColumnHeaders`, `PinnedCueHeaders` e `ColumnUtilities` devono leggere il **medesimo offset orizzontale**, attualmente gestito dal viewport. Non sincronizzare due scrollbar con conversioni approssimative.
3. In `WorkspaceLayout.cpp` mantenere la budget allocation (minimum cue grid e Timeline), ma creare una sola geometria coordinata per la regione cue e mixer. L'Inspector continua a riservare spazio.
4. Ridisegnare `ColumnUtilities::Strip` **prima nel paint**, poi nella disposizione dei child component, conservandone callback e valori effettivi: `GainMeterFader`, pan, mute, phase, master send, output, routing, MIDI/OSC activity, controllo priority/edit. Non sostituire i widget reali con i placeholder della demo.
5. Aggiungere test JUCE con 11 colonne e 12 tab, viewport con scroll, colonne riordinate/collassate, mixer aperto/chiuso, hitbox di ogni widget, BPM/Auto Stop fisso e Inspector visibile/nascosto.
6. Il transport `GO/Panic`, editing e il progetto `.cuelist` schema v10 restano invariati. Compilazione Windows, CTest e verifica hardware sono gate distinti dal concept HTML.

## Gate di design

**Approvata:** parte superiore del concept V2 precedente.  
**Modificata in questa iterazione:** solo fascia inferiore (mixer integrato e suo rapporto con la Timeline).  
**Da verificare:** leggibilità delle strip reali a 109px, continuità x durante lo scroll, densità con 11 colonne e DPI Windows, presenza dei comandi non-audio e resa a monitor 32:9.

La preview HTML è un contratto grafico esplorativo: nessun comportamento di trasporto, device routing o persistenza è effettivamente collegato.
