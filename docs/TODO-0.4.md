# The Cuelist 0.4

Richieste utente: uscite per colonna, simboli mute/fase, posizione di riproduzione nella cella e arresto automatico selettivo al GO successivo.

- [x] Utility audio nascoste nelle colonne MIDI/OSC; rimane solo il selettore uscita non operativo.
- [x] Selettore delle coppie di uscite audio nelle utility, aggiornato al cambio dispositivo; rimosso dall'inspector.
- [x] Icone vettoriali altoparlante barrato e cerchio barrato, con tooltip e stato acceso/spento.
- [x] Cella con tempo trascorso/durata, barra di avanzamento e stato playing/pausa/attesa dal renderer.
- [x] Auto Stop a destra della riga, attivo per default. GO di un'altra riga ferma tutte le voci delle righe abilitate; toggle spento permette sovrapposizioni. GO di riga vuota vale come attivazione; Play cella e Stop espliciti non fermano altre righe.
- [x] JSON v4: persistenza stopOnNext, importazione v1/v2/v3 con toggle attivo per default.
- [x] Test PCM, interazioni, revisione, rendering e pacchetto portable 0.4.

Telemetria: oggetti atomici per voce allocati sul thread GUI; callback audio senza mappe, allocazioni o mutex. GUI legge solo valori pubblicati, non lo stato mutabile delle voci. La progressione mostra secondi relativi al file, riparte nei loop e si azzera allo Stop; durante offset iniziale compare ATTESA.

