# The CUElist — Operator Workspace V2

> **Decisione sul Master Audio:** escluso completamente dal redesign.
> Riutilizzare tale e quale il componente nativo in `ColumnUtilities`,
> mantenendo le sue dimensioni, i controlli, il paint e i callback.
> Adattare il resto del nuovo workspace attorno al Master,
> non il Master alla nuova matrice. La preview include solo
> un riferimento visivo statico al componente originale.
>

> **Aggiornamento V2.3 / 0.9.0-beta.9:** affinamento
> delle strip Audio/MIDI/OSC/Control, leggibilità e
> sidebar. Matrice approvata, Master completo,
> scroll sincronizzato e BPM/Auto Stop fissi
> rimangono inalterati. [Gate di parità](UI_V2_FEATURE_PARITY.md).
>


> **Aggiornamento V2.2 / 0.9.0-beta.8:** Master integrale secondo
> il codice JUCE esistente e navigazione senza duplicazioni.
> Ogni funzione deve restare disponibile: vedere
> [inventario e gate di parità funzionale](UI_V2_FEATURE_PARITY.md).
> La cue matrix superiore già approvata non viene ridisegnata.
>

**Stato:** concept V2.1 esplorativo, non ancora interfaccia di produzione.  
**Feedback confermato:** parte superiore approvata; mixer integrato nella stessa geometria delle colonne cue.  
**Baseline:** `main` alla versione `0.9.0-beta.6`, screenshot operatore a 1647×895: 12 cue list, 13 cue, 11 colonne Audio/MIDI/OSC/Control, Inspector aperto.  
**Prototipo testabile:** [`design/operator-workspace-v2.html`](../design/operator-workspace-v2.html).

## Perché ripartiamo dalla matrice affollata

La prima schermata vuota nascondeva il vero problema. Con 11 colonne, le cornici di ogni singola cella, gli 11 channel strip, la timeline e l'Inspector sembrano componenti equivalenti in competizione. Il ridisegno deve mantenere **tutta la densità utile** e rimuovere solo rumore, contorni e comandi secondari sempre visibili. Le righe cue restano l'unità di lettura orizzontale; GO, STOP, stato NEXT/READY, BPM e Auto Stop sono operativamente prioritari.

### Principi verificabili

1. **Un solo centro visivo:** cue list. Righe alternate e divisori lievi, selezione full-width, NEXT/READY distinguibile da selezione e playback. Non dedurre playback dal solo colore.
2. **Strumenti gerarchizzati:** Sidebar = navigazione + trasporto di emergenza; Inspector = editor della selezione; Mixer = dock; Timeline = vista opzionale. GO e PANIC **non vengono nascosti**.
3. **Meno bordi, più informazione:** niente card arrotondata attorno a ogni cella vuota. Tipo colonna identificato da una fascia tinta poco satura; dati reali visibili anche senza hover, azioni secondarie su hover **e focus keyboard**.
4. **Matrice stabile:** cue header congelato a sinistra, BPM/Auto Stop congelati a destra, righe perfettamente allineate fra le due aree, scroll orizzontale solo per le 11 colonne.
5. **Densità controllata:** 37 px/row come riferimento, modalità compatta 31 px/row per il mockup; entrambe da verificare su Windows e con scaling DPI. Non modificare hitbox di produzione finché non passano i test reali.
6. **Tipografia:** Hind per titoli, Exo 2 per etichette operative, ≥10 pt tipografici come nel sistema attuale. Il prototipo web usa Segoe UI solo come *surrogato di test*: non è una decisione di sostituire i font embedded JUCE.
7. **Semantica distinta:** Audio = teal, MIDI = amber, OSC = viola, Control = azzurro; errore = rosso, GO = teal forte, on-deck = segno amber. Non codificare lo stato solo con colore.
8. **Sicurezza show:** selezione non deve attivare un cue; cambiare tab conserva l'attuale sicurezza transport-stop; GO, Panic, mute, clip, timing e dati di progetto devono rimanere sul runtime esistente.

## Audit del codice reale

| Area | Attualmente | Intervento V2 |
|---|---|---|
| `src/MainComponent.h` | `CueGrid::rowHeight=62`, `headerHeight=72`, `nameWidth=318`, `columnWidth=152` | Nuove metriche isolate e condivise fra paint, hit testing, pinned headers, scroll e inspector; niente sostituzioni numeriche sparse |
| `src/CueViews.cpp` | CueGrid, row/cell actions, heading e metriche dipinte con bordi e surface per cella | Layer di presentazione: righe continue e celle leggere; mantenere cue identità, cell actions, right click e tastiera |
| `src/WorkspaceLayout.cpp` | Inspector 288 px fisso, Sidebar/viewport e dock derivati da offset numerici storici | Layout adaptivo con budget minimo cue grid, dock e timeline realmente collassabili; nessun oscuramento dei cue |
| `src/ColumnUtilities.cpp` | Strip per Audio/MIDI/OSC/Control con trattamento visivo uniforme | Fader e meter prominenti solo su Audio; telemetria e output non-audio semplificati, senza togliere mute/routing |
| `src/InspectorController.cpp` | Due card e controlli scalati verticalmente fino a 0,72 | Gerarchia sezionale, campi leggibili, proprietà contestuali; mantenere bind ai dati reali e target di input |
| `src/Theme.h` | Midnight Slate, Hind ed Exo 2 già embedded; look-and-feel condiviso | Token V2 semantici + metriche visive per surface/selection/hover/focus; non perdere i fallback vettoriali |
| `tests/CuelistGuiMockupTests.h` e `tests/WorkspaceLayoutTests.h` | Regressioni tabs, cue, layout, scroll e controlli | Aggiungere test V2 per 12 tab, 13 cue, 11 colonne, Inspector, viewport, hitbox, narrow/ultrawide e regressione del trasporto |

### Vincoli funzionali non negoziabili
- Non toccare `AudioEngine`, `MidiOutputRouter`, `OscOutputRouter`, `ControlRuntime`, modello show e formato progetto JSON v10.
- Non convertire un mockup in uno switch di backend. Preview mai connessa ai dispositivi.
- Mantenere 12 cue list selezionabili e gestione overflow, rinomina inline e DnD esistenti.
- Il pannello BPM/Auto Stop deve restare sempre osservabile anche scorrendo la matrice.
- GO/Panic accessibili con Space/Esc in produzione; i click del prototipo web **non attivano nulla**.
- Lo screenshot ricco è un test di stress visivo, non un invito a nascondere colonne, controlli o righe della sessione.

## Specifica geometrica candidata — da non imporre ancora al codice JUCE

| Misura | Baseline codice | Proposta nel mockup |
|---|---:|---:|
| Riga cue | 62 px | 37 px (opzione 31 px) |
| Header della matrice | 72 px | 52 px |
| Larghezza contenuto cue | 318 px | 280 px |
| Colonna espansa | 152 px | 109 px |
| Banda BPM/Auto Stop fissa | 184 px | 128 px |
| Inspector | 288 px | 272 px |
| Mixer integrato sotto le stesse colonne | variabile e nel layout | 150 px, richiudibile a 30 px; stesso scroll della matrice, nessuna card separata |
| Timeline | ridimensionabile, minimo 75 px | 75 px, richiudibile a 29 px |

Questi valori sono **ipotesi del prototipo**. La riduzione della riga non autorizza a ridurre i target cliccabili sotto dimensioni sicure, né a comprimere campi di clip complessi in produzione. Le colonne JUCE dovranno offrire almeno un'espansione contestuale, e differenziare dati persistenti da comandi.

## Piano di integrazione successivo

**V2.1 · Fascia inferiore (questa PR):** dopo la verifica dello screenshot, la parte superiore rimane invariata; mixer direttamente allineato alle colonne, Master sotto cue header e BPM/Auto Stop ancora fisso. Si veda [specifica mixer integrato](UI_V2_INTEGRATED_MIXER_SPEC.md).

**P0 · Confronto UX (questa PR).** Prototipo HTML offline autocontenuto, 12/13/11, interazioni visive reversibili per scelta cue/tab e collasso pannelli. Acquisire feedback a 1647×895, 1920×1080 e 1280×720. Nessuna modifica JUCE attiva.

**P1 · Fondazioni native.** Creare `OperatorDesignTokens` e nuove funzioni pure di layout JUCE sotto flag di sviluppo esplicito. Testare cue selection, row/column hover/focus, pinned BPM/Auto Stop, scroll sincronizzato, dock e Inspector. Non migrare motori.

**P2 · Matrice operativa.** Migrarvi prima la cue list con input invariati, header pinning, celle clip e controllo GO/STOP. Verificare 11 colonne, DnD tab/row, click destro e controlli MIDI/OSC con test automatici e screenshot Windows.

**P3 · Contesto e strumenti.** Inspector e timeline in componenti adattivi; mixer nativo come prosecuzione delle stesse colonne della cue matrix (senza secondo sistema di geometria/scroll), quindi menu/toolbar e settings unificati. Test regressione legati a dispositivi; nessun cambiamento ai modelli o al routing.

**P4 · Accettazione.** CI Windows/CTest, prove DPI 100–200%, 16:9 e ultrawide, cue grid 12/13/11, operator keyboard, hardware check Audio/MIDI/OSC e controllo FPS/CPU. Non certificare uso live dalla sola CI.

## Confini della demo

`design/operator-workspace-v2.html` è un documento HTML standalone con CSS e un dataset artificiale; non richiede rete, non legge né salva file e non può mandare comandi hardware. Le tab, la selezione e i toggle di densità, Inspector, dock e Timeline sono **solo presentazione**. I pulsanti GO/Panic sono deliberatamente simulati; non devono essere scambiati per un transport di produzione.

**Gate aperto:** design direction non ancora approvata; PR design-only, nessuna riscrittura dell'applicazione.
