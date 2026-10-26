# The Cuelist

**The Cuelist** è un'applicazione desktop per Windows dedicata al **controllo delle cue audio e MIDI durante spettacoli teatrali, concerti ed eventi dal vivo**. Permette di organizzare una scaletta di interventi sonori e avviarli, interromperli o sovrapporli da un'unica interfaccia.

È pensata per chi deve gestire la riproduzione durante uno show senza utilizzare un'intera DAW.

## Funzionalità principali

- **Scaletta di cue:** griglia di righe e colonne con comandi GO, Stop, Panic e Auto Stop.
- **Audio:** importazione e riproduzione di WAV e MP3, loop, livelli, pan e routing per colonna.
- **MIDI:** importazione e creazione di clip di note, con Piano Roll e selezione dell'uscita MIDI esterna.
- **OSC:** clip di messaggi UDP con parametri interi, float o stringa, tempi in secondi o beat e destinazione IP/porta per colonna.
- **Timeline:** posizionamento delle clip, zoom, griglia BPM visibile/nascosta, risoluzione musicale configurabile e magnete indipendente per lo snap.
- **Progetti portabili:** salvataggio degli show in formato `.cuelist`, con cartella dei media e backup.

**Stato attuale:** sviluppo **0.6 (OSC)**, eseguibile ancora identificato come **0.5.0** nel pacchetto Windows x64. OSC su UDP include editor di messaggi tipizzati e tempo in secondi o beat; video e timecode non sono ancora disponibili. Prima dell'uso in uno spettacolo reale sono necessarie prove sui dispositivi hardware interessati.
