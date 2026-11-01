# THE CUELIST — Redesign nativo V2 / N3.4B: Auto Stop senza «CELLS»

**Baseline:** `main` 0.9.0-beta.17 (PR #101, N3.4 integrata).  
**Versione proposta:** `0.9.0-beta.18`.  
**Branch:** `feat/native-autostop-exceptions-beta18`.

## Decisione di UX approvata

La fascia destra rimane fissa, alta 72 px di header e larga 184 px; ogni riga resta alta 62 px. Vi rimangono **soltanto BPM e Auto Stop della cue**. Il pulsante `CELLS`, il relativo hitbox e lo stato legacy `autoStopVisibleRows` vengono eliminati, senza un secondo toggle sostitutivo.

Le celle che seguono il comportamento standard non hanno alcun badge. Solo una cella con `stopOnNext=false` riceve un'indicazione `CONT.` ambra, che spiega che la cella è esclusa dall'Auto Stop e può continuare dopo il GO successivo quando l'Auto Stop della cue è attivo. Il tooltip spiega l'eccezione e l'accesso al menu contestuale. Nelle colonne collassate l'indicazione diventa una tacca ambra con tooltip per preservare la dimensione 48 px.

## Comportamenti preservati

- `row.stopOnNext` continua a essere gestito dal toggle fisso e da `MainComponent::setAutoStop`.
- `cell.stopOnNext` continua a essere persistito nello **stesso** JSON v10 e gestito da `MainComponent::setCellAutoStop`; tutte le celle rimangono inizialmente incluse.
- Per Audio/MIDI/OSC e per colonne Control bloccate: menu contestuale esistente `CueGrid::showCellMenu`, nuova voce con spunta `Includi nell'Auto Stop`; per le Control Map non bloccate la medesima voce è inserita in `MainComponent::showControlCellMenu`, senza sacrificare le funzioni di mapping.
- L'Inspector conserva il toggle di Auto Stop per la cella selezionata; lo stato e gli aggiornamenti runtime non cambiano.
- I badge sono **soltanto informativi**. Click sulla cella, GO, Play/Stop, import, DnD, loop e menu non acquisiscono nuovi comandi nascosti.
- Lo spazio per CONT. viene sottratto ai soli titoli della cella dove necessario; il loop si sposta a sinistra, mantenendo le dimensioni e gli hit target del trasporto.
- Nessun nuovo hover o layer sulla fascia fissa che assorba il click sul BPM o Auto Stop.

## Codice

Interventi circoscritti a `src/CueViews.cpp`, `src/MainComponent.h`, `src/ControlUI.cpp`, test e documentazione. La vecchia modalità transitoria non rimane nascosta nel codice (rimossi nome del metodo, Set di row IDs, click e pittura); `cellContinueBadgeBounds` governa il marker nella griglia espansa.

## Test e gate

In `tests/CueGridN3VisualTests.h` i test legacy N3.4 di CELLS sono sostituiti da test per:
- due soli hitbox non sovrapposti nella fascia fissa;
- toggle cue con scorrimento X/Y e click nell'ex spazio CELLS privo di effetti;
- badge esclusivo per Audio/Control esclusi, reversibilità, tooltip localizzato e persistenza JSON;
- marker e spiegazione visibili nella colonna collassata.

I test generali N3.1–N3.4, 13×11, 32:9, scorrimento, editor e motori devono continuare a passare. **CI Windows + CTest obbligatori**, senza dichiarare riusciti i test hardware o DPI finché non sono stati svolti.

## Esclusioni

Non modificare `src/ColumnUtilities.cpp`, Master Audio, fader, pan, meter, mute, limiter, Quick Learn, Inspector, Timeline, barra menu, motori, mapping/routing né creare una build portable. Questo incremento corregge la UX del design N3.4 e non ne modifica il contratto strutturale.

> Questo documento **sostituisce** soltanto la descrizione del tasto CELLS presente nel documento storico `UI_V2_NATIVE_PHASE_N3_4.md` della beta.17.
