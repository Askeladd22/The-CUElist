# The Cuelist — Guida completa

Questa pagina conserva le istruzioni operative e tecniche dettagliate della versione 0.5.0.


**0.5.0 · Windows x64 · Portable · C++17 / JUCE 8 · Licenza proprietaria**

Controllo di spettacoli mediante una griglia di cue, clip audio e timeline. Riproduce WAV e MP3; non gestisce video. Supporta clip MIDI di note e un piano roll integrato. OSC rimane predisposto ma non operativo.

## Avvio rapido

1. Estrai **tutto** `The-Cuelist-0.5.0-Windows-x64.zip` in una cartella scrivibile.
2. Apri **The Cuelist.exe** oppure **Avvia.cmd**.
3. Seleziona il dispositivo in **Dispositivi audio**, importa un WAV/MP3 e scegli l'uscita della colonna nel menu **Out** delle utility.

Per usare il programma **non servono Visual Studio, Build Tools, CMake, Git o Python**. Non è richiesto un installer. L'archivio dei soli sorgenti non contiene automaticamente l'eseguibile: usa il pacchetto portable nella cartella `dist`.

Il runtime C/C++ è collegato staticamente; l'eseguibile verificato dipende solo da DLL di Windows. `Avvia.cmd` cerca l'eseguibile accanto a sé, in `portable` o nella build locale; non avvia mai una compilazione.

## Scaricare l'eseguibile portable da GitHub Actions

Il workflow **Build Windows portable** compila automaticamente su Windows x64, esegue i test, crea lo ZIP e ne verifica l'estrazione, il contenuto e gli hash. Un test grafico opzionale di avvio offline (`--offline`) e disponibile in locale con `scripts/check-package.ps1 -SmokeLaunch`. Non e necessario possedere Visual Studio per **utilizzare** il pacchetto.

1. Apri la scheda **Actions** del repository e scegli **Build Windows portable**.
2. Apri un'esecuzione riuscita (segno di spunta verde) e, nella sezione **Artifacts**, scarica **The-Cuelist-0.5.0-Windows-x64-portable**.
3. Estrai lo ZIP scaricato da Actions; al suo interno trovi **The-Cuelist-0.5.0-Windows-x64.zip** e il checksum `.sha256`.
4. Estrai anche il secondo ZIP in una cartella a tua scelta (anche su una chiavetta USB) e apri **The Cuelist.exe**. Non e richiesta un'installazione.

Il workflow viene eseguito sui commit al ramo `feat/desktop-foundation`, nelle pull request dirette a quel ramo oppure manualmente dalla scheda Actions. Gli artifact GitHub scadono dopo 30 giorni; per conservarli scarica lo ZIP.

**Portabilita dei dati:** il programma non richiede un profilo utente per memorizzare gli show. Quando salvi, scegli una posizione accessibile (per esempio una cartella vicino all'eseguibile): porta sempre con te il file `.cuelist` e la relativa cartella `_media`. I percorsi dei media all'interno dello show sono relativi. La configurazione delle interfacce audio e MIDI puo dipendere dall'hardware del PC su cui esegui il programma; Windows e i driver possono mantenere propri dati di sistema.

## Cue e riproduzione

La sessione iniziale contiene una cue e una colonna Audio senza nomi precompilati. **+ Cue** e **+ Colonna** estendono la griglia.

| Comando | Effetto |
| --- | --- |
| Importa WAV / MP3, oppure trascina un file sulla cella | Carica audio mono o stereo |
| Play della cella o doppio click sulla clip | Attiva la cella secondo il comportamento scelto |
| Stop della cella | Ferma e azzera quella clip |
| GO della riga | Attiva le celle partecipanti della cue e prepara la successiva |
| Spazio / GO laterale | Avvia la prossima cue indicata in giallo, senza riavvolgere la lista alla fine |
| Stop a destra del titolo della cue | Ferma tutte le clip della riga, incluse quelle escluse dal GO |
| ESC / PANIC | Ferma tutte le riproduzioni |

Selezionare una riga riarma la sequenza da quella cue. I pulsanti Play/Stop delle celle e lo Stop di riga non cambiano la prossima cue. Spazio non lancia cue mentre si scrive in un campo testo.

### Auto Stop

Il toggle **Auto Stop**, a destra di ogni riga, è **attivo per default**. Al GO di un'altra cue vengono fermate tutte le clip delle righe con questo toggle acceso, anche quelle lanciate singolarmente o escluse dal GO.

Disattivalo sulle righe che devono continuare in sovrapposizione. Anche il GO di una cue vuota attiva questo arresto; Play della singola cella e Stop espliciti non fermano altre righe. Riattivare la stessa cue non applica Auto Stop a sé stessa.

### Comportamenti di attivazione

| Comportamento | Effetto |
| --- | --- |
| Retrigger | Riparte dall'inizio, anche durante la riproduzione |
| Pausa / Restart | Mette in pausa; alla successiva attivazione riparte dall'inizio |
| Pausa / Resume | Alterna pausa e ripresa dalla posizione raggiunta |
| Continue | Continua se sta suonando; altrimenti riprende dalla posizione conservata |

**Loop** è indipendente dall'attivazione. **Partecipa al GO della riga** include o esclude la cella dal lancio collettivo. Nella scheda **Intera cue** puoi impostare livello, mute, loop e un comportamento comune per il GO; il lancio diretto mantiene il comportamento della cella.

## Modifica e timeline

- Doppio click sul titolo di cue o colonna per rinominarlo: il campo riceve subito il focus. **Invio** conferma anche i nomi nell'inspector.
- Le frecce riordinano righe e colonne; **X** elimina. Eliminando l'ultima rimane una riga/colonna vuota. I file originali non vengono cancellati.
- Trascina una clip nella griglia per spostarla; una destinazione occupata viene scambiata. Le colonne devono essere dello stesso tipo.
- Trascina una clip nella timeline per modificarne l'inizio in secondi. Il **magnete** consente di agganciarla alla griglia musicale (1/4, 1/8, 1/16 o 1/32); disattivalo con **Magnete OFF** per spostamenti liberi. La griglia segue la mappa BPM della cue, anche quando il tempo cambia. Il lancio diretto della cella parte subito, senza questo ritardo.
- Trascina il cursore sul righello per cambiare la posizione della cue. Ogni cella mostra tempo trascorso/durata, barra di avanzamento e stato di riproduzione, pausa o attesa.
- **− / +** regolano lo zoom, **Fit** mostra tutta la cue e **Ctrl+rotella** ingrandisce attorno al puntatore. La barra orizzontale sposta la vista; la rotella scorre le tracce. **Doppio clic su una clip MIDI nella timeline** apre il piano roll senza avviare la clip.

I parametri audio si aggiornano senza azzerare il trasporto. Invio, **Applica modifiche**, Salva o il cambio selezione confermano i campi testo. Importazione, apertura e modifiche strutturali possono fermare l'audio.

## Utility di colonna e uscite

| Controllo | Funzione |
| --- | --- |
| Out | Coppia di uscite attive; una destinazione salvata ma assente viene segnalata senza sostituzione automatica |
| Fader dB | Da silenzio (−60 dB) a +12 dB; doppio click per 0 dB |
| Pan | Bilanciamento stereo: centro invariato, estremi con canale opposto silenziato; doppio click per centrare |
| Altoparlante barrato | Mute, senza fermare la clip |
| Cerchio barrato | Inversione della polarità di entrambi i canali |

I simboli si evidenziano quando attivi. I livelli di cella, cue e colonna si moltiplicano. Le uscite si aggiornano cambiando dispositivo o canali attivi. Sotto MIDI i controlli audio sono nascosti e il selettore permette di scegliere un dispositivo MIDI hardware o virtuale. Sotto OSC il selettore rimane non operativo.

## Clip MIDI e piano roll

1. Aggiungi una colonna **MIDI** con **+ Colonna**.
2. Seleziona la cella e usa **Importa MIDI** per un `.mid`/`.midi`, oppure **Crea clip MIDI** per partire da una clip vuota di 4 beat. Puoi anche trascinare un file sulla cella.
3. Un doppio clic sulla cella apre il **piano roll**; il pulsante Play della cella avvia la riproduzione.
4. Nelle utility seleziona l'**uscita MIDI**. Serve un sintetizzatore esterno, un'applicazione o uno strumento collegato a quella porta: The Cuelist non contiene un sintetizzatore.

Nel piano roll: doppio clic nella griglia per inserire una nota; trascina la nota per spostare tempo e altezza, oppure il bordo destro per cambiare durata. **Tasto destro + trascinamento** seleziona più note in un rettangolo; **Shift+clic** aggiunge o rimuove note dalla selezione. Le note selezionate possono essere spostate o eliminate insieme, e ricevono collettivamente le modifiche a **Velocity** (1–127) e **Canale** (1–16). **Canc** o **Elimina nota** le rimuovono. **Snap** sceglie la griglia da 1/4 a 1/32 oppure **Off** per spostamenti liberi; **+ / −** cambiano lo zoom e le barre scorrono la vista. **Beat clip** regola la durata senza tagliare note esistenti. I beat sono quarti; C4 corrisponde alla nota MIDI 60.

**Applica** trasferisce le modifiche alla cella; **Annulla** o la chiusura della finestra le scartano. Salva poi lo show per conservarle su disco. Aprire l'editor ferma il trasporto.

Le clip MIDI partecipano a GO, Auto Stop, loop, pausa e seek della timeline; si possono spostare tra celle MIDI e trascinare nella timeline. I livelli in dB e il pan agiscono sull'audio; per le note si usa la velocity. Il trasporto MIDI usa il clock del dispositivo audio, che deve quindi essere aperto anche per uno show solo MIDI.

Importazione: MIDI standard tipo 0/1 con divisione PPQ, massimo 8 MB, 10.000 note e 4096 beat per clip. Le tracce vengono unite preservando i canali. Questa versione importa **solo note**: controller, sustain, program change, pitch bend, SysEx e tempo del file non vengono importati; un avviso segnala gli eventi ignorati. SMPTE e MIDI tipo 2 non sono supportati. La riproduzione segue la mappa tempo della cue.

Le uscite sono memorizzate per identificativo: un dispositivo assente non viene sostituito automaticamente con un'altra porta. Il GO con note MIDI richiede un'uscita disponibile. La precisione su hardware dipende da driver, buffer audio e pianificazione del sistema; le prove fisiche di latenza e disconnessione restano da eseguire.

## Progetti portabili e backup

**Salva** crea un `.cuelist` e raccoglie gli audio in una cartella `<nome>_media` accanto al progetto. Per trasferire uno show **copia entrambi**, mantenendoli affiancati. I percorsi salvati sono relativi.

Sovrascrivendo uno show, la versione precedente resta in `.cuelist.bak`. Le generazioni precedenti dei media vengono conservate per rendere utilizzabile il backup: salvataggi ripetuti possono aumentare lo spazio occupato.

Il formato corrente è **v5**. **Apri** legge anche v1/v2/v3/v4 e backup. Le clip MIDI sono salvate direttamente nello show, senza dipendere dal file MIDI importato:

- Auto Stop viene abilitato per default nei progetti precedenti alla v4.
- Nei progetti v1/v2, i vecchi Restart e Stop diventano Retrigger e le utility di colonna partono neutre.
- Nei progetti v3, Pausa/Restart e le utility salvate vengono conservati.

Salva nuovamente un progetto legacy per raccogliere i media. I file mancanti vengono segnalati e possono essere ricollegati importandoli nella cella.

## Tempo musicale

Ogni cue ha una **Mappa tempo** separata: una riga per punto, nel formato `beat BPM`. Il primo beat è `0`, espresso in quarti. La metrica è **4/4** per le nuove mappe e non è più modificabile nell'inspector. Le firme metriche precedenti vengono conservate nei vecchi show finché non si modifica la mappa tempo.

```text
0 120
8 90
```

I BPM e la mappa tempo regolano le note MIDI; non cambiano la velocità dei file audio. Il renderer adatta il sample rate mediante interpolazione lineare. Il loop globale ripete ciascuna clip indipendentemente, senza riavvolgere il clock della riga.

## Limiti attuali

- Una clip WAV/MP3 mono/stereo per cella; PCM precaricato fino a **128 MB per file e 512 MB per show**. Streaming e multitraccia interno non implementati.
- Decodifica sul thread dell'interfaccia: file grandi possono provocare attese. Il callback del renderer non esegue I/O, allocazioni o mutex.
- Motore OSC, LTC/MTC, mapping ingressi, autosave e recupero dopo crash non implementati. ASIO disabilitato in questa build; WASAPI disponibile tramite JUCE.
- Ricampionamento lineare e saturazione di sicurezza a ±1. Prove prolungate di ascolto, jitter e disconnessione su hardware esterno restano da eseguire.

## Sviluppo e verifiche

Per **ricompilare**, installa Visual Studio Build Tools con **Sviluppo desktop con C++**, Windows SDK e strumenti CMake. Git serve se `vendor/JUCE` non è già presente. Il progetto usa JUCE **8.0.12**, commit `29396c22c93392d6738e021b83196283d6e4d850`.

```powershell
# Build Release e test; equivalente a Compila.cmd
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1

# Pacchetto portable con avvisi di licenza (senza sorgenti)
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/package.ps1

# Verifica archivio, documentazione, hash e launcher
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/check-package.ps1
```

`build.ps1 -TestOnly` compila solo i test; `-Run` apre l'app. `package.ps1 -SkipBuild` riutilizza una build verificata. `check-package.ps1 -SmokeLaunch` prova avvio e chiusura offline. L'eseguibile accetta `--offline` per non aprire il dispositivo audio.

Output: `build/TheCuelist_artefacts/Release/The Cuelist.exe`. La versione 0.4 è stata verificata con **68 casi e 20.472 asserzioni**, compreso uno show di 10.000 righe. I test generano audio, progetto e anteprime in `build/Release/test-artifacts`. Stato dettagliato: [docs/progress.md](progress.md).

| Percorso | Contenuto |
| --- | --- |
| `src/ShowModel.*` | Modello, validazione, tempo e persistenza |
| `src/AudioEngine.*` | Decodifica, renderer, trasporto e telemetria |
| `src/MainComponent.*`, `src/CueViews.cpp` | Editor e griglia |
| `src/Timeline.cpp`, `src/ColumnUtilities.cpp` | Timeline e mixer |
| `tests/`, `scripts/` | Test, build, packaging e verifica |

## Licenza

**The Cuelist è software proprietario, con tutti i diritti riservati.** Il codice originale non è open source. Uso da parte di terzi, modifica e redistribuzione richiedono l'autorizzazione del titolare, salvo i diritti inderogabili previsti dalla legge. Vedere [LICENSE](../LICENSE) e [NOTICE.md](../NOTICE.md).

Il pacchetto portable non include i sorgenti. File audio e show degli utenti conservano i rispettivi diritti.

JUCE e le altre dipendenze mantengono le proprie licenze. **La copertura JUCE per distribuire questa applicazione proprietaria è da verificare prima della distribuzione a terzi**: non è stata dichiarata né attivata una licenza per conto del titolare. Vedere le [condizioni JUCE](https://juce.com/get-juce/) e la [licenza del framework 8.0.12](https://github.com/juce-framework/JUCE/blob/8.0.12/LICENSE.md).
