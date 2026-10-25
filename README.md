# The Cuelist

Applicazione desktop Windows per cue multimediali. Versione 0.4 C++/JUCE: audio WAV/MP3 reale e interfaccia ispirata al riferimento fornito. Non gestisce video.

## Avvio

Per **usare il programma non servono Visual Studio, Build Tools, Git o CMake**.

1. Copia sul computer di destinazione il pacchetto **The-Cuelist-0.4.0-Windows-x64.zip** (prodotto nella cartella `dist`).
2. Estrai tutto lo ZIP in una cartella a scelta.
3. Fai doppio click su **The Cuelist.exe** oppure **Avvia.cmd** nella cartella estratta.

L'archivio ZIP dei **sorgenti** scaricato dal repository non include automaticamente l'eseguibile. In quel caso occorre anche il pacchetto portatile, non installare strumenti di sviluppo. Il launcher cerca un eseguibile accanto a se stesso, in `portable`, oppure nella build locale; se manca, mostra queste istruzioni e **non compila**.

Eseguibile: `build/TheCuelist_artefacts/Release/The Cuelist.exe`.

Il runtime C/C++ viene collegato staticamente. La verifica delle dipendenze dell'eseguibile mostra solo DLL di Windows, senza DLL di Visual C++ da distribuire. Il pacchetto e destinato a Windows x64; una prova sul computer di destinazione rimane necessaria.

## Solo per sviluppare o ricompilare

**Compila.cmd** ricompila sorgenti e test dopo una modifica. Il computer di sviluppo dispone di Visual Studio Build Tools 2026 con C++ e CMake. Su un altro computer di sviluppo installare Visual Studio Build Tools con il carico **Sviluppo desktop con C++**, Windows SDK e strumenti CMake, e Git. Il primo build scarica JUCE 8.0.12 dal repository ufficiale.

Da PowerShell, nella cartella del progetto:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1
```

Risultato atteso: compilazione Release, test CTest con `100% tests passed`, eseguibile nel percorso indicato. L'opzione `-Run` lo apre; `-TestOnly` compila soltanto i test. Avviando l'eseguibile con `--offline` l'editor non apre il dispositivo audio.

Per creare il pacchetto portatile, eseguire `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/package.ps1`. Questo compila, esegue i test e prepara lo ZIP in `dist`. L'opzione `-SkipBuild` riutilizza una build Release gia verificata. Lo script di packaging e destinato allo sviluppatore, non all'utilizzatore del programma.

## Uso della versione 0.4

Si parte da una cue e una colonna Audio senza nomi precompilati. **+ Cue** e **+ Colonna** estendono la griglia.

- Importa **WAV o MP3** con il pulsante o trascinandoli sulla cella. Doppio click sulla cella avvia l'audio.
- **Spazio / GO** avvia la prossima cue, indicata in giallo, poi avanza. Alla fine si ferma senza ripartire dalla prima. Seleziona una riga per riarmare la sequenza da li. Il GO sulla riga avvia quella cue. Il toggle **Auto Stop** a destra della riga e attivo per default: al GO di un’altra cue ferma tutte le clip di quella riga, anche quelle escluse dal GO. Disattivalo per lasciare la riga in sovrapposizione. Anche il GO di una cue vuota ferma le altre righe abilitate; Play delle celle e Stop espliciti non fermano altre righe.
- **ESC / PANIC** ferma tutto. **Ferma** arresta la cella selezionata. Ogni cella ha Play e Stop separati: Play usa il comportamento scelto nel menu, Stop ferma e azzera. Il quadrato rosso a destra del titolo cue ferma tutte le clip della riga, anche quelle escluse dal GO. Questi pulsanti non cambiano la prossima cue della sequenza.
- Doppio click sul titolo della cue o della colonna per rinominarla: il campo riceve subito il focus e il testo viene selezionato; **Invio** conferma anche i nomi nell'inspector.
- Le frecce visibili riordinano righe e colonne, **X** elimina. Eliminando l'ultima rimane una riga/colonna vuota. I file originali restano sul disco.
- Trascina una clip nella griglia per spostarla: la destinazione occupata viene scambiata; serve una colonna dello stesso tipo.
- Trascina una clip nella **timeline** per impostarne l'inizio nella cue. Trascina il cursore sul righello per cambiare posizione di riproduzione. La rotella scorre le tracce. **− / +** regolano lo zoom; **Fit** mostra tutta la cue. **Ctrl+rotella** fa zoom attorno al puntatore e la barra orizzontale permette di spostare la vista ingrandita.
- Il fader **dB** cambia il livello immediatamente, da silenzio (-60) a +12 dB; doppio click riporta a 0 dB. La scheda **Intera cue** offre gain, mute, loop di tutte le clip e comportamento del GO. Il gain della cue moltiplica quello delle singole celle.
- La riga **Utility colonne** sotto la griglia controlla tutte le clip di ciascuna colonna: fader in dB, pan, mute e inversione di polarita (**cerchio barrato**). I parametri restano salvati nello show. Il pan e un bilanciamento stereo: centro invariato, estremi con canale opposto silenziato; doppio click centra il pan o riporta il fader a 0 dB. Mute (altoparlante barrato) non ferma la riproduzione. I simboli sono evidenziati quando attivi. Nelle colonne MIDI/OSC questi controlli sono nascosti e resta solo il selettore uscita non ancora operativo.
- Il menu **Out** nelle utility sceglie la coppia di uscite audio di ogni colonna; le voci si aggiornano al cambio dei canali attivi. Il routing non e piu nell’inspector. Le uscite memorizzate ma assenti sono segnalate senza sostituzione automatica.
- Ogni cella caricata mostra **tempo trascorso / durata** e una barra legata al cursore audio effettivo. Il punto si illumina durante playback; pausa e attesa sono evidenziate in giallo. Il progresso segue seek e loop e si azzera allo Stop.
- I parametri audio non fermano il trasporto. Invio, **Applica modifiche**, Salva o un cambio selezione confermano i campi testo.

**Salva** raccoglie i media in una cartella `<nome>_media` accanto al `.cuelist`, con percorsi relativi. Per trasferire lo show copia **entrambi**, mantenendoli affiancati. Ad ogni salvataggio la versione precedente del progetto resta in `.cuelist.bak`; le generazioni precedenti dei media sono conservate per rendere utilizzabile il backup. Questo puo aumentare lo spazio occupato. **Apri** legge anche progetti v1/v2/v3 e backup; il formato corrente e v4. I progetti precedenti ricevono Auto Stop attivo su tutte le righe. Nei vecchi progetti Restart e Stop vengono convertiti in Retrigger e le utility di colonna partono neutre; salva di nuovo un progetto vecchio per raccoglierne i media. Nessun installatore o compilatore serve sul PC di utilizzo.

**Dispositivi audio** seleziona dispositivo e uscite attive. Il routing usa la numerazione delle uscite attive: ricontrollarlo dopo un cambio di dispositivo. Nessun ripiego automatico su altre uscite.

## Semantica audio di questo incremento

| Comportamento | Attivazione |
|---|---|
| Retrigger | Riparte dall'inizio, anche se già in esecuzione |
| Pausa / Restart | Durante playback mette in pausa; la successiva attivazione riparte dall’inizio |
| Pause / Resume | Alterna pausa e ripresa mantenendo la posizione |
| Continue | Continua se già in esecuzione, altrimenti riprende dalla posizione conservata |

**Loop** è indipendente dal comportamento di attivazione. **Partecipa al GO della riga** esclude o include la cella nel trigger collettivo; l'attivazione diretta rimane possibile. Le righe con Auto Stop disattivato possono continuare a suonare quando viene attivata un’altra cue.

Ogni cue ha una mappa tempo separata, modificabile con **Mappa tempo**. Formato: `beat BPM numeratore/denominatore`, un punto per riga, primo beat `0`; beat espresso in quarti. Esempio:

```text
0 120 4/4
8 90 3/4
```

I BPM non cambiano la velocità dei file. Il renderer adatta il sample rate del file a quello del dispositivo con interpolazione lineare. La timeline modifica gli offset in secondi e la posizione della cue. Il trigger diretto di una cella parte subito, senza il suo offset. Il loop globale ripete ciascuna clip indipendentemente; non riavvolge il clock della riga.

## Implementato e limiti

- Riproduzione simultanea WAV/MP3 mono/stereo, una traccia per cella; mono duplicato sulla coppia di uscite.
- PCM precaricato: massimo 128 MB per file e 512 MB per show. Lo streaming da disco e più tracce interne per cella sono previsti.
- Callback del nostro renderer senza letture file, allocazioni o mutex. Importazione/decodifica avvengono sul thread UI: l'interfaccia può attendere durante file grandi. Le modifiche strutturali, importazione e apertura fermano l'audio.
- Panic prioritario rispetto ai comandi precedenti; coda trasporto limitata e errore esplicito se piena.
- Salvataggio versionato con file temporaneo e backup; media raccolti con riferimenti relativi. Autosave e recupero dopo crash non ancora implementati.
- Colonne MIDI/OSC modificabili come struttura, ma **nessun invio/ricezione MIDI o OSC**. Nessun LTC, MTC, mappatura ingressi o gruppo di attivazione in questa versione.
- WASAPI disponibile tramite JUCE; ASIO disabilitato in questa build.
- Nessuna certificazione live: test di durata, jitter, disconnessioni hardware e carico professionale restano da eseguire. L'interpolazione lineare e il taglio ai limiti +/-1 non sostituiscono un ricampionatore professionale o un limiter.

## Struttura

- `src/ShowModel.*`: modello, mappa tempo, validazione e persistenza.
- `src/AudioEngine.*`: decoding WAV/MP3, renderer, trasporto e snapshot atomici.
- `src/MainComponent.*`, `src/Theme.h`, `src/Main.cpp`: editor desktop e avvio.
- `tests/`: test reali del modello, renderer, coda trasporto e inspector.
- `docs/design.md`, `docs/progress.md`: decisioni e stato verificato.

I test generano un WAV stereo di cinque secondi a basso livello e uno show di verifica in `build/Release/test-artifacts/verification.cuelist`. Si possono aprire con **Apri** per provare l'app senza cercare un file audio.

## Dipendenze e distribuzione

JUCE è fissato a **8.0.12**, commit `29396c22c93392d6738e021b83196283d6e4d850`, scaricato sotto `vendor/JUCE` e non incorporato nel repository del progetto. JUCE offre licenza propria e AGPLv3: prima di distribuire il prodotto definire e rispettare la licenza appropriata. Nessuna licenza commerciale è stata acquistata o attivata da questo lavoro. Consultare `vendor/JUCE/LICENSE.md` e <https://juce.com/juce-legal/>. La licenza del codice dell'applicazione non è ancora stata scelta dal titolare del progetto.
