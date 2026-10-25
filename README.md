# The Cuelist

Applicazione desktop Windows per cue multimediali. Primo incremento C++/JUCE: audio WAV reale e interfaccia ispirata al riferimento fornito. Non gestisce video.

## Avvio

Per **usare il programma non servono Visual Studio, Build Tools, Git o CMake**.

1. Copia sul computer di destinazione il pacchetto **The-Cuelist-0.1.0-Windows-x64.zip** (prodotto nella cartella `dist`).
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

## Primo utilizzo

1. Seleziona una cella Audio e premi **Importa WAV**, oppure trascina un WAV sulla cella.
2. Doppio click sulla cella o **Avvia cella** per riprodurla. La freccia a sinistra della cue, **GO** o **Spazio** avviano tutte le celle Audio partecipanti della riga. Spazio non attiva cue mentre si scrive in un campo testo.
3. **ESC** o **PANIC** fermano l'audio; **Ferma** agisce sulla cella selezionata.
4. Nell'inspector modifica nome, BPM iniziali, metrica, comportamento, loop, livello e uscite. **Applica modifiche**, **Salva** e il cambio di selezione applicano i parametri. L'applicazione dei parametri ferma il trasporto.
5. **Salva** crea un progetto JSON `.cuelist`; sovrascrivendo un progetto viene mantenuta la versione precedente in `.bak`. **Apri** carica il progetto e segnala i file mancanti, da ricollegare con **Importa WAV**.

**+ Cue** e **+ Colonna** estendono la griglia. Click sul titolo della colonna per rinominare, spostare o eliminare; tasto destro sul nome della cue per spostarla o eliminarla. Eliminare celle o cue non cancella i WAV originali.

**Dispositivi audio** seleziona dispositivo, canali attivi, sample rate e buffer. Il routing delle colonne usa la numerazione delle uscite *attive* del dispositivo; ricontrollarlo dopo un cambio di dispositivo. Non viene effettuato un ripiego automatico su altre uscite.

## Semantica audio di questo incremento

| Comportamento | Attivazione |
|---|---|
| Retrigger | Riparte dall'inizio, anche se già in esecuzione |
| Restart | Ferma e riparte al confine dello stesso blocco audio; nessuna pausa artificiale |
| Pause / Resume | Alterna pausa e ripresa mantenendo la posizione |
| Continue | Continua se già in esecuzione, altrimenti riprende dalla posizione conservata |
| Stop | Ferma e azzera la posizione |

**Loop** è indipendente dal comportamento di attivazione. **Partecipa al GO della riga** esclude o include la cella nel trigger collettivo; l'attivazione diretta rimane possibile. Le cue non si escludono a vicenda: più righe possono suonare contemporaneamente.

Ogni cue ha una mappa tempo separata, modificabile con **Mappa tempo**. Formato: `beat BPM numeratore/denominatore`, un punto per riga, primo beat `0`; beat espresso in quarti. Esempio:

```text
0 120 4/4
8 90 3/4
```

I BPM non cambiano la velocità dei file. Il renderer adatta il sample rate del file a quello del dispositivo con interpolazione lineare. La timeline mostra le durate e un'anteprima del segnale delle prime tracce visibili della cue selezionata: non è ancora un editor di eventi né un riferimento timecode.

## Implementato e limiti

- Riproduzione simultanea WAV mono/stereo, una traccia per cella; mono duplicato sulla coppia di uscite.
- PCM precaricato: massimo 128 MB per file e 512 MB per show. Lo streaming da disco e più tracce interne per cella sono previsti.
- Callback del nostro renderer senza letture file, allocazioni o mutex. Importazione/decodifica avvengono sul thread UI: l'interfaccia può attendere durante file grandi. Le modifiche strutturali, importazione e apertura fermano l'audio.
- Panic prioritario rispetto ai comandi precedenti; coda trasporto limitata e errore esplicito se piena.
- Salvataggio versionato con file temporaneo e backup; riferimenti assoluti ai media. Raccolta portatile, autosave e recupero dopo crash non ancora implementati.
- Colonne MIDI/OSC modificabili come struttura, ma **nessun invio/ricezione MIDI o OSC**. Nessun LTC, MTC, mappatura ingressi o gruppo di attivazione in questa versione.
- WASAPI disponibile tramite JUCE; ASIO disabilitato in questa build.
- Nessuna certificazione live: test di durata, jitter, disconnessioni hardware e carico professionale restano da eseguire. L'interpolazione lineare e il taglio ai limiti +/-1 non sostituiscono un ricampionatore professionale o un limiter.

## Struttura

- `src/ShowModel.*`: modello, mappa tempo, validazione e persistenza.
- `src/AudioEngine.*`: decoding WAV, renderer, trasporto e snapshot atomici.
- `src/MainComponent.*`, `src/Theme.h`, `src/Main.cpp`: editor desktop e avvio.
- `tests/`: test reali del modello, renderer, coda trasporto e inspector.
- `docs/design.md`, `docs/progress.md`: decisioni e stato verificato.

I test generano un WAV stereo di cinque secondi a basso livello e uno show di verifica in `build/Release/test-artifacts/verification.cuelist`. Si possono aprire con **Apri** per provare l'app senza cercare un file audio.

## Dipendenze e distribuzione

JUCE è fissato a **8.0.12**, commit `29396c22c93392d6738e021b83196283d6e4d850`, scaricato sotto `vendor/JUCE` e non incorporato nel repository del progetto. JUCE offre licenza propria e AGPLv3: prima di distribuire il prodotto definire e rispettare la licenza appropriata. Nessuna licenza commerciale è stata acquistata o attivata da questo lavoro. Consultare `vendor/JUCE/LICENSE.md` e <https://juce.com/juce-legal/>. La licenza del codice dell'applicazione non è ancora stata scelta dal titolare del progetto.
