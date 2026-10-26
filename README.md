# The Cuelist

**The Cuelist** è un'applicazione desktop per Windows dedicata al **controllo delle cue audio e MIDI durante spettacoli teatrali, concerti ed eventi dal vivo**. Permette di organizzare una scaletta di interventi sonori e avviarli, interromperli o sovrapporli da un'unica interfaccia.

È pensata per chi deve gestire la riproduzione durante uno show senza utilizzare un'intera DAW.

## Funzionalità principali

- **Scaletta di cue:** griglia di righe e colonne con GO, Stop e Panic; copia/incolla celle (Ctrl+C/V), menu riga per copia/incolla/duplicazione/eliminazione, BPM modificabile sulla riga e Auto Stop selettivo espandibile sotto il toggle.
- **Audio:** WAV/MP3, loop, channel strip compatti con un unico GainMeterFader stereo (barre L/R in dBFS, cursore in dB e scala verticale condivisa), pan rotativo di dimensioni intermedie e routing; Master audio nella zona utility a sinistra delle colonne, indipendente da MIDI/OSC.
- **MIDI:** importazione e creazione di clip di note, con Piano Roll e selezione dell'uscita MIDI esterna.
- **OSC:** Clip Editor visivo con eventi OSC e automazioni di argomenti int/float (rampe e curve multipunto), parametri int/float/string/bool, timeline in secondi o beat, Undo/Redo e routing UDP per colonna.
- **Timeline:** posizionamento delle clip, zoom, ridimensionamento delle clip MIDI/OSC dal bordo destro, griglia BPM visibile/nascosta e magnete indipendente.
- **Control Mapping (prima integrazione):** ingresso MIDI fisico selezionato manualmente, MIDI Learn per CC, note e Pitch Bend, preset di scena, priorita cue/preset/colonna/globale, Pickup/Jump e destinazioni Master, gain Audio, GO, Panic, MIDI CC e OSC Float.
- **Branding:** icona trasparente approvata inclusa in `assets/thecuelist_icon.png` e incorporata nella GUI/eseguibile.
- **Progetti portabili:** salvataggio degli show in formato `.cuelist`, con cartella dei media e backup.

**Stato attuale:** sviluppo **0.6 (Control & Mapping v10 iniziale)**, eseguibile ancora identificato come **0.5.0**. Il formato degli show e `v10`, compatibile con v1-v9. Le modifiche sono una PR non ancora compilata o verificata su hardware Windows. Video, timecode e feedback MIDI verso controller non sono ancora disponibili. Il sistema Control Mapping resta sperimentale: non utilizzarlo durante uno spettacolo senza prove complete.
