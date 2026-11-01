# The CUElist — Redesign V2.3: parità funzionale obbligatoria

> **Eccezione vincolante: il Master Audio non va ridisegnato.**
> Conservare il componente JUCE originale di `ColumnUtilities` con
> aspetto, posizione, dimensioni, disegno e tutti i controlli/callback
> attuali. Nessun componente HTML alternativo deve essere tradotto
> nel Master nativo. La preview mostra solo un'indicazione statica
> del Master originale; Quick Learn conserva i propri hotspot
> solo nella modalità esistente dell'applicazione.
>

> **Rifinitura beta.9:** i canali MIDI/OSC/Control nella
> preview sono rappresentati con dati pertinenti
> (porta, host/porta, mute TX, priorità, Edit set)
> senza inventare fader audio. Audio rappresenta
> meter/fader, routing, PRE/POST, pan, mute,
> polarità, send Master L/R. Il codice JUCE reale
> rimane l'autorità per ogni callback e dispositivo.
> Tutte le funzioni già presenti devono restare
> raggiungibili dopo il futuro redesign nativo.
>


**Decisione:** il redesign cambia soltanto look, densità, disposizione e navigazione. **Nessuna funzione può essere eliminata, nascosta senza un percorso equivalente o trasformata in una simulazione nella GUI JUCE finale.** La prova HTML rimane non operativa e non modifica l'applicazione.

## Duplicazioni di navigazione

L'area superiore ospita **un solo accesso** a ciascun comando di configurazione. La sidebar non ripete le voci Cuelists, Devices, Control Mapping, Settings e Project info; mostra solamente lo show corrente, lo stato On Deck e GO/PANIC, sempre visibili.

Il concept V2.2 espone menu dimostrativi con percorsi espliciti:

| Funzioni attuali | Percorso visivo proposto |
|---|---|
| Nuovo, Apri, Salva, Salva con nome, informazioni progetto | File |
| Undo, Redo, modifica, copia, incolla, duplicazione/eliminazione cue | Edit |
| Settings e Preferences (senza fonderne i comportamenti diversi) | Show |
| Nuova cue list, rinomina inline, duplicazione, chiusura e navigazione tab | Cuelists |
| Configurazione audio, MIDI, OSC, Control Mapping e relativo Learn | Devices |
| Mixer, Inspector, Timeline, monitor messaggi, layout e scaling | View |
| Guida in-app e About con versione/licenza | Guide |

Questa tabella riguarda la collocazione, non una modifica del codice di esecuzione. L'elenco non sostituisce un audit completo delle azioni dell'app: prima di spostare i menu nativi bisogna confrontare **tutte** le action ID effettive e le scorciatoie da tastiera.

## Master Audio: inventario nativo vincolante (nessun redesign)

Verificato in `src/MainComponent.cpp`, `src/ColumnUtilities.cpp` e nei componenti JUCE del mixer:

| Funzione nativa | Origine codice | Concept V2.2 |
|---|---|---|
| Master gain in dB (fader e reset) | `masterSlider` / `GainMeterFader` | Fader verticale e lettura dB |
| Meter stereo e peak hold L/R | `masterSlider.setStereoPeaks` | Due barre L/R (valori illustrativi) |
| Master stereo pan con centro e reset | `masterPan` | Pan e lettura L/C/R |
| Master audio mute, senza arrestare MIDI/OSC | `masterMute` | Mute separato, con stato |
| Selezione coppia uscita fisica Master Bus | `masterOutput` | Main Out, menu coppie |
| Limiter di sicurezza -1 dBFS, solo Master Bus | `safetyLimiter` | Limit con toggle dedicato |
| Meter di gain reduction del limiter | `safetyLimiter.setGainReduction` | **Da rendere graficamente nella GUI nativa**; non eliminare il meter del widget esistente |
| Quick Learn Master Gain | `masterGainLearnHotspot` | Target Gain, visibile in Learn |
| Quick Learn Master Pan | `masterPanLearnHotspot` | Target Pan, visibile in Learn |
| Quick Learn limiter toggle | `masterLimiterLearnHotspot` | Target Limiter, visibile in Learn |

Il Master è **solo audio**; il suo mute e limiter non devono influire su MIDI e OSC. `Main Out` non deve alterare le uscite audio Direct. In produzione le opzioni del menu Main Out devono provenire **dal dispositivo audio reale** e segnalare coppie non disponibili; i quattro valori fissi del prototipo sono dati fittizi. La modalità Quick Learn deve rimanere integrata con il mapping effettivo, non deve essere sostituita dai pulsanti fittizi.

Il Master del prototipo è **soltanto una sagoma statica illustrativa**. Non si deve creare un nuovo layout nativo da 280 × 190 px, né ridimensionare o reinterpretare il Master: riutilizzare le geometrie, il componente e tutti i controlli JUCE originali.

## Altri canali: parità di comportamento

- **Audio:** gain/fader con meter, pan, mute, phase invert, send Master L/R, PRE/POST, assegnazione uscita, Direct, Quick Learn per gain/pan/mute/phase/send.
- **MIDI:** routing del dispositivo, trasmissione, mute, clip MIDI, editor e controlli di temporizzazione.
- **OSC:** indirizzo/porta, messaggi, invio, mute, editor OSC e unità di tempo supportate.
- **Control:** priorità, set di mapping, editor set, learn e overrides di colonna/cue.
- **Cue list:** GO/STOP, auto stop, BPM, next, selezione, righe, celle, trascinamento, ordinamento, rinomina inline, Undo/Redo, tab multiple e scroll.
- **Inspector e Timeline:** tutte le proprietà contestuali, note, import, loop, snap, zoom e strumenti di editing esistenti.
- **Diagnostica e protezione:** Panic, stato dispositivi, logging, monitor messaggi, recupero show, avvisi e scorciatoie.

La demo presenta alcuni di questi comandi con testi descrittivi o controlli non operativi: **non è autorizzazione a rimuoverli dalla UI nativa**. Ogni controllo può diventare contestuale o essere spostato in una sezione espandibile, ma deve rimanere raggiungibile, leggibile e utilizzabile via mouse e tastiera.

## Gate prima di qualsiasi modifica `src/`

1. Inventario machine-readable di componenti, callback, scorciatoie e tooltips del layout precedente.
2. Matrice di copertura: `funzione esistente → posizione V2 → controllo JUCE → test`; nessuna riga senza destinazione.
3. Test di equivalenza su modello show JSON v10, motori Audio/MIDI/OSC/Control, mapping, GO/Panic, routing, fader e limiter.
4. Verifica visuale a 1280×720, 1647×895, 1920×1080 e ultrawide 32:9 (DPI/scaling Windows), inclusi focus e dimensioni dei target.
5. Validazione hardware reale audio/MIDI/OSC per accettare l'implementazione nativa. **Una CI verde non prova l'idoneità all'uso live.**

**Stato:** solo prototipo V2.2 e specifica. Il codice JUCE e tutti i runtime rimangono invariati.
