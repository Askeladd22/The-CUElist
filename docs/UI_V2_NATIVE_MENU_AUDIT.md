# UI V2 — Audit e correzione dei menu nativi (fase N2.1)

**Versione:** `0.9.0-beta.12`  
**Segnalazione:** i sette menu della barra superiore introdotti in beta.10 e la sidebar de-duplicata in beta.11 ripetevano Pre-flight e Diagnostics, mentre il vecchio pannello Preferences nascondeva opzioni importanti dietro una voce generica. Questo documento mappa ogni funzione originale a un **unico percorso principale**.

| Funzione originale / comportamento | Posizione dopo la correzione | Callback originale |
|---|---|---|
| New show / Open show / Save show | **File** | `newButton.onClick` (conferma abbandono), `openButton.onClick`, `saveButton.onClick` |
| Undo / Redo | **Edit** | `undoEdit` / `redoEdit` |
| Aggiungi cue / colonna | **Edit** e scorciatoie contestuali della matrice (accessi complementari) | `addCue`, `showColumnMenu` |
| Rinomina show | **Show** | `projectNameLabel.showEditor` |
| Pre-flight | **Show** e nessun altro menu | `showPreflight` |
| Diagnostics | **Show** e nessun altro menu | `showDiagnostics` |
| Crea / rinomina / sposta / elimina Cuelist | **Cuelists** e tabs reali | Metodi `createCuelist`, `renameActiveCuelist`, `moveCuelist`, `requestDeleteCuelist` |
| Scelta dispositivo Audio | **Devices → Audio Devices** | `showDevices` |
| Input MIDI principale / input aggiuntivi, selezione e disattivazione | **Devices → MIDI Inputs** | `selectControlInput`, `toggleAdditionalMidiInput` |
| Abilitazione e disabilitazione uscite MIDI | **Devices → MIDI Outputs** | `engine.setMidiOutputEnabled` e `appPreferences` |
| Porte UDP OSC in ingresso e disattivazione ascolto | **Devices → OSC Input** | `configureOscInputPorts`; mostra IP locale e porte attive |
| Mapping dei controlli | **Devices → Control Mapping** | `openControlPanel(false)` |
| MIDI Learn / OSC Learn | **Pulsanti permanenti in alto** | `globalMidiLearn`, `globalOscLearn`, `beginQuickLearn`; nessun doppione nel menu |
| Inspector aperto/chiuso | **View → Collapse/Expand Inspector**, più linguetta a destra | `inspectorSideTab.triggerClick` |
| Collassa/espandi tutte le colonne | **Pulsante fisso nella cue matrix** | `collapseAllButton`; nessuna seconda voce nel menu View |
| Intervallo fader 0 dB / +6 dB | **View → Fader Range** | `setAudioFaderCeilingDb` |
| Formato orologio 24h / 12h | **View → Clock** | `appPreferences.clock12Hour` |
| Guida utente / About | **Guide** | `openUserGuide`, `showAbout` |

## Garanzie

- Il vecchio `preferencesButton` rimane nascosto e i suoi callback di compatibilità restano nel codice per non spezzare integrazioni storiche, ma **non è più una via necessaria** per raggiungere le opzioni. Le voci di cui sopra aprono direttamente i controlli effettivi.
- Per input/output MIDI si conservano gli **identificatori reali dei dispositivi rilevati all'apertura del menu** in modo che un riordino dell'enumerazione non indirizzi un comando verso un altro dispositivo mentre il popup è aperto.
- I controlli MIDI/OSC con stato on/off sono impostazioni reali, non mockup. Il formato dei salvataggi `.cuelist` schema v10, il motore audio/MIDI/OSC e l'ordine GO/Panic restano immutati.
- Il **Master Audio nativo è escluso da tutte le modifiche**: disegno, ingombri, controlli e callback originali.
- Le colonne di cue e il mixer sono invariati. Alcune azioni dell'editor esistono anche nella griglia e nelle tabs perché sono **accessi alternativi utili**, non azioni duplicate tra più menu.

## Test e QA

- `scripts/test_native_operator_menu_parity.py` analizza **la costruzione reale dei menu JUCE** e il dispatch dei comandi, non il concept HTML, per intercettare opzioni perse, duplicazioni e rimozioni del Master.
- Il test viene eseguito dal controllo versione e nella CI Windows, in aggiunta ai test JUCE e alle prove di persistenza esistenti.
- Restano da verificare in-app su Windows tutti i sottomenu con dispositivi fisici MIDI/OSC collegati, le voci con stato spuntato, il popup More su schermi stretti e la navigazione da tastiera. La CI non sostituisce il test hardware.
