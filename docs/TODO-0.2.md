# The Cuelist 0.2 — checklist

L'utente ha autorizzato lo sviluppo e delegato le decisioni tecniche. Nessun video, solo applicazione portatile Windows x64.

- [x] WAV e MP3, anche trascinati dalla cartella.
- [x] Show vuoto, nomi cue/colonne vuoti senza scene dimostrative precompilate.
- [x] Rinomina con doppio click e conferma Invio.
- [x] GO sequenziale con Spazio: avvia una cue, prepara la successiva, nessun wrap automatico.
- [x] Spostamento clip nella griglia mediante trascinamento; scambio se destinazione occupata, solo colonne compatibili.
- [x] Controlli visibili per riordinare/eliminare cue e colonne; niente menu contestuale per queste operazioni.
- [x] Livello in dB, variazioni live senza fermare o riavviare il trasporto.
- [x] Comportamenti Retrigger, Restart, Pause/Resume, Continue e Stop verificati anche attraverso l'inspector.
- [x] Offset clip modificabile trascinandola sulla timeline, espresso in secondi.
- [x] Playhead trascinabile e seek reale sulla cue selezionata.
- [x] Parametri globali cue: livello, mute, loop e override del comportamento delle celle.
- [x] Salvataggio portatile con copia media e percorsi relativi; lettura progetti v1.
- [x] Pacchetto 0.2 Windows x64 completo senza compilatori richiesti.
- [x] Test automatici, build, rendering GUI e revisione finale.

## Contratto tecnico

Cell.startSeconds (double >=0): posizione della clip in secondi nella cue. Cell.gain e Row.gain restano ampiezze lineari serializzate; GUI -60..+12 dB, estremo minimo equivale a silenzio. Row.muted e Row.loop sono override globali; Row.overrideBehaviour abilita Row.behaviour per il trigger di riga, mantenendo quello della cella sul trigger diretto.

AudioEngine.publish(state, preservePlayback=true): trasferisce lo stato delle voci con identità/media invariati; aggiornare parametri non riavvia la riproduzione. Modifiche strutturali e apertura show possono fermare esplicitamente.

AudioEngine.seekRow(rowId, seconds), monitoredRowId/playheadSeconds/rowPlaying atomici per controllo e visualizzazione. Nessuna allocazione o I/O nel callback.

loadShow(file, destination) risolve i percorsi relativi dopo validazione. saveShow raccoglie i media in una cartella a fianco del progetto, non modifica il modello live, preserva originale e backup in caso di errore. moveCell scambia contenuto e identità, limitandosi a colonne dello stesso tipo.

## Verifiche

Renderer: ritardo, seek, loop, gain e override riga; migrazione snapshot non azzera Pause/Continue. Persistenza: roundtrip v2 e migrazione v1, copia della cartella su un'altra posizione senza file originali. GUI: Invio, GO sequenziale, slider in dB, drag griglia e timeline. Packaging: eseguibile completo nel pacchetto, launcher mai compilante.

