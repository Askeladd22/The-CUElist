# The Cuelist 0.3

Richieste utente del 25 ottobre 2026. Implementazione sul progetto JUCE esistente, distribuzione esclusivamente portable. Autonomia tecnica gia autorizzata.

- [x] Focus automatico e selezione testo nel dialogo di rinomina.
- [x] Pausa/Restart: attivazione durante playback mette in pausa; successiva riparte dall'inizio. Pausa/Resume conserva invece la posizione.
- [x] Stop rimosso da entrambi i menu attivazione; stop espliciti disponibili.
- [x] Stop a destra del titolo cue ferma tutte le clip della riga, incluse quelle escluse dal GO.
- [x] Play e Stop indipendenti nelle celle, senza cambiare la prossima cue sequenziale. Play usa il comportamento di attivazione scelto.
- [x] Timeline: meno/piu/Fit, Ctrl+rotella ancorato al puntatore e scorrimento orizzontale.
- [x] Riga utility allineata alle colonne: gain dB, pan, mute e polarita, con azione live e salvataggio nel progetto.
- [x] JSON v3; vecchi Restart/Stop migrati a Retrigger, nuovi parametri colonna neutri.
- [x] Verifica PCM, interazioni GUI, revisione, rendering e pacchetto portable.

Pan: bilanciamento stereo, centro a guadagno invariato; estremi silenziano il canale opposto. Inversione di fase: polarita invertita su entrambi i canali. I gain di cella, cue e colonna si moltiplicano. Utility disabilitate sulle colonne MIDI/OSC che non hanno ancora motore operativo.

