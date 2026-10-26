# The Cuelist

**The Cuelist** è un'applicazione desktop per Windows dedicata al **controllo delle cue audio e MIDI durante spettacoli teatrali, concerti ed eventi dal vivo**. Permette di organizzare una scaletta di interventi sonori e avviarli, interromperli o sovrapporli da un'unica interfaccia.

È pensata per chi deve gestire la riproduzione durante uno show senza utilizzare un'intera DAW.

## Funzionalità principali

- **Scaletta di cue:** griglia di righe e colonne con GO, Stop e Panic; copia/incolla celle (Ctrl+C/V), menu riga per copia/incolla/duplicazione/eliminazione, BPM modificabile sulla riga e Auto Stop selettivo espandibile sotto il toggle.
- **Audio:** WAV/MP3, loop, channel strip compatti con un unico GainMeterFader stereo (barre L/R in dBFS, cursore in dB e scala verticale condivisa), pan rotativo di dimensioni intermedie e routing; Master audio nella zona utility a sinistra delle colonne, indipendente da MIDI/OSC.
- **MIDI:** importazione e creazione di clip di note, con Piano Roll e selezione dell'uscita MIDI esterna.
- **OSC:** clip di messaggi UDP con parametri interi, float o stringa, tempi in secondi o beat e destinazione IP/porta per colonna.
- **Timeline:** posizionamento delle clip, zoom, ridimensionamento delle clip MIDI/OSC dal bordo destro, griglia BPM visibile/nascosta e magnete indipendente.
- **Progetti portabili:** salvataggio degli show in formato `.cuelist`, con cartella dei media e backup.

**Stato attuale:** sviluppo **0.6 (rifinitura interfaccia)**, eseguibile ancora identificato come **0.5.0** nel pacchetto Windows x64. OSC su UDP include editor di messaggi tipizzati e tempo in secondi o beat; video e timecode non sono ancora disponibili. Prima dell'uso in uno spettacolo reale sono necessarie prove sui dispositivi hardware interessati.
