# V2.3 → JUCE: fase N1, barra superiore nativa

**Ambito:** prima fase operativa del redesign. Interessa soltanto la fascia alta di `MainComponent` e la navigazione. Il resto del layout è deliberatamente lasciato invariato; una conversione globale senza verifiche peggiorerebbe il controllo show in produzione.

## Implementazione effettiva

- Nuovo `src/OperatorNavigation.h`: calcola le posizioni reali di **File, Edit, Show, Cuelists, Devices, View, Guide**, e mostra il tasto **More** con categorie e sottomenù reali quando la larghezza è limitata. Le voci non spariscono a 1280/1180/1024 px.
- La prima riga dell'header ora contiene l'icona e il nome compatto **The CUElist**, come nel concept; la seconda fascia continua a ospitare le vere tab Cuelist. Non sono stati spostati scroll, righe, BPM/Auto Stop o timeline.
- Tutti i nuovi menu sono `juce::TextButton` nativi; i `juce::PopupMenu` riutilizzano **gli stessi metodi e callback di produzione**: New/Open/Save con conferma, Edit/Undo/Redo, aggiunta cue/colonne, Settings, rename show, preflight, diagnostica, gestione delle Cuelist, Devices, mapping, MIDI/OSC Learn, Inspector, Collapse all, Guide, About.
- Help/Undo/Redo diretti nella vecchia barra sono visivamente sostituiti dai corrispondenti menu. Restano validi l'Undo/Redo originali e le loro scorciatoie.
- I comandi New/Open/Save e Settings/Control Mapping della **sidebar restano temporaneamente visibili** in questa fase nativa: l'eliminazione delle duplicazioni attenderà la migrazione e i test della sidebar, non va anticipata sacrificando funzionalità.
- I pannelli CPU/RAM, MIDI RX/TX, OSC RX/TX e Audio Available originali sono conservati con i valori e le frequenze reali; si evitano badge fittizi o riduzioni di diagnostica non testate.

## Master Audio: ESCLUSO

Non modificare `src/ColumnUtilities.cpp` né la geometria/paint o i callback del Master in `src/MainComponent.cpp`. Fader/meter L/R, gain, pan, mute, Main Out, limiter/gain reduction, Quick Learn e routing sono invariati. Il Master non viene adattato al prototipo HTML.

## Gate di questa PR

- `tests/OperatorNavigationTests.h` verifica 7 pulsanti nativi e larghezze di riferimento, overflow senza perdita di categorie, accesso a comandi esistenti, Master originale visibile.
- CTest Windows completo, incluse tutte le regressioni show/mixer/audio/MIDI/OSC/Control.
- Nessun cambiamento a `ShowModel`, persistenza .cuelist JSON v10, audio engine, MIDI/OSC output, mapping, hardware, clip editor, cue transport o DnD.
- Nessuna build portable.

## Passi successivi, separati

**N2:** migrazione Sidebar/Show navigator, una voce per funzione e GO/Panic sempre visibili; test accessibilità senza duplicazioni e tastiera.

**N3:** migrazione CueGrid e strip **non-Master**, con un layout di colonna condiviso e parità funzionale. Non ridurre hitbox o proprietà reali in base alle dimensioni non verificate del mockup.

**N4:** Inspector/Timeline e adattamento DPI/ultrawide; QA manuale Windows e hardware. Queste fasi non sono comprese nella presente modifica.
