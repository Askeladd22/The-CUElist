# The Cuelist

**The Cuelist** è un'applicazione desktop per Windows dedicata al **controllo delle cue audio e MIDI durante spettacoli teatrali, concerti ed eventi dal vivo**. Permette di organizzare una scaletta di interventi sonori e avviarli, interromperli o sovrapporli da un'unica interfaccia.

È pensata per chi deve gestire la riproduzione durante uno show senza utilizzare un'intera DAW.

## Funzionalità principali

- **Scaletta di cue:** griglia di righe e colonne con comandi GO, Stop, Panic e Auto Stop.
- **Audio:** importazione e riproduzione di WAV e MP3, loop, livelli, pan e routing per colonna.
- **MIDI:** importazione e creazione di clip di note, con Piano Roll e selezione dell'uscita MIDI esterna.
- **Timeline:** posizionamento delle clip, zoom, griglia musicale sincronizzata ai BPM e snap disattivabile.
- **Progetti portabili:** salvataggio degli show in formato `.cuelist`, con cartella dei media e backup.

**Stato attuale:** versione **0.5.0** in sviluppo, per **Windows x64**. Il controllo OSC, il video e il timecode non sono ancora disponibili. Prima dell'uso in uno spettacolo reale sono necessarie prove sui dispositivi hardware interessati.

## Scaricare e avviare il programma

> **Importante:** il pulsante GitHub **Code → Download ZIP** scarica i *sorgenti*, **non** l'applicazione compilata.

1. Apri [GitHub Actions — Build Windows portable](https://github.com/Askeladd22/The-Cuelist/actions/workflows/windows-portable.yml).
2. Seleziona una compilazione conclusa con successo e scarica l'artifact Windows portable da **Artifacts** (potrebbe essere richiesto l'accesso a GitHub).
3. Estrai l'archivio. Se contiene un secondo file `.zip`, estrai anche quello.
4. Avvia **The Cuelist.exe**. Non è necessaria un'installazione né Visual Studio.

Per cominciare, importa un WAV/MP3 in una cella. **Spazio** avvia la cue successiva; **Esc / Panic** interrompe tutte le riproduzioni. Per il MIDI serve una porta o un dispositivo di uscita esterno.

Per spostare uno show su un altro computer, copia il file `.cuelist` insieme alla relativa cartella `_media`.

## Documentazione e sviluppo

- [Guida completa all'utilizzo](docs/guida-completa.md)
- [Note di progettazione](docs/design.md) e [registro di sviluppo](docs/progress.md)
- Compilazione da sorgenti su Windows: `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1`

## Licenza

The Cuelist è **software proprietario**. La formula commerciale prevista è una **licenza perpetua con pagamento una tantum**, senza abbonamento obbligatorio per continuare a usare la versione acquistata. La disponibilità dei sorgenti su GitHub non li rende open source.

Leggi [LICENSE](LICENSE) e [NOTICE.md](NOTICE.md). JUCE e le altre dipendenze hanno licenze separate: **la copertura JUCE per la distribuzione commerciale deve essere verificata prima della vendita o distribuzione**.
