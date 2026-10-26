# The Cuelist — Guida completa

Questa pagina conserva le istruzioni operative e tecniche dettagliate della versione 0.5.0.


**0.5.0 · Windows x64 · Portable · C++17 / JUCE 8 · Licenza proprietaria**

Controllo di spettacoli mediante una griglia di cue, clip audio e timeline. Riproduce WAV e MP3; non gestisce video. Supporta clip MIDI di note e un piano roll integrato. Le colonne OSC inviano messaggi UDP a un destinatario IP/host e porta configurati nelle utility. Le clip OSC possono contenere eventi in secondi o beat e argomenti tipizzati (interi, float e stringhe).

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

## Control & Mapping (prima implementazione)

Il pulsante **CONTROL MAPPING** nella barra superiore apre il pannello di gestione dei controlli fisici. Il sistema e separato dalle clip MIDI e OSC e opera sugli eventi ricevuti da un ingresso MIDI scelto espressamente. Al momento la UI comprende solo la prima versione: per costruire un mapping bisogna disporre di un controller collegato e usare MIDI Learn.

1. Collega un controller MIDI e apri **CONTROL MAPPING**. Seleziona esplicitamente la porta nel menu *Ingresso MIDI*. La lista *Aggiorna porte* ricarica i dispositivi disponibili; non vengono scelti automaticamente controller sostitutivi in caso di scollegamento.
2. Premi **MIDI Learn** e muovi un fader, un encoder, un pad o un pulsante. Il sistema legge **CC**, **Note On/Off** o **Pitch Bend**, indicando canale e numero. Il callback hardware utilizza una coda a capacita fissa e non avvia operazioni GUI o rete.
3. Scegli l'**ambito**: Globale, Colonna, Preset oppure Singola cue. In **Preset cue** crea un preset condiviso (ad esempio `Luci sala`) e assegnalo alla cue selezionata. Cambiare selezione nella griglia non attiva i suoi mapping; e necessario un GO riuscito.
4. Scegli la **destinazione**: Master Audio, Gain di una colonna Audio, GO, PANIC, MIDI CC oppure OSC Float. Per le destinazioni di colonna scegli una colonna compatibile dal menu. Per MIDI CC imposta il numero del CC di uscita; per OSC il percorso address. I controlli verso MIDI e OSC usano le uscite gia configurate nelle Utility Colonne.
5. Seleziona **Pickup** per evitare salti di volume quando il cursore fisico e in una posizione diversa dal valore corrente, oppure **Jump** per applicare immediatamente il valore. Il pannello permette l'inversione dell'asse. **Aggiungi mapping** registra la regola nello show; selezionala dall'elenco per eliminarla.

**Priorita:** override della singola cue, poi preset della cue, mapping della colonna e infine mapping globale. Un pulsante GO o PANIC non deve ripetersi fino a quando non viene rilasciato; il cambio di cue non azzera il debounce fisico.

**Cue attiva:** la cue diventa attiva solo dopo che GO ha accettato il comando; selezionare una riga o avviare una singola cella non cambia il preset attivo. Panic disattiva i mapping specifici della cue ma lascia disponibili quelli globali. I comandi vengono inseriti nel trasporto audio e vengono elaborati dal suo callback: tra il GO e la sua esecuzione puo esserci la latenza di un blocco audio. Un allineamento a livello di campione restera un miglioramento successivo.

**Uscite generiche:** l'invio MIDI CC viene effettuato dal worker MIDI esistente e **non apre una seconda istanza della stessa porta**. Gli OSC di mapping vengono inviati in UDP tramite un worker indipendente con coda limitata; non vi e garanzia di consegna. Panic annulla gli invii che non sono ancora stati elaborati, non quelli gia ricevuti dai dispositivi.

**Limiti attuali:** per i livelli Audio il mapping utilizza ancora la ricostruzione dello stato di rendering con l'aggiornamento GUI a 25 Hz; prima di un impiego live con fader ad alta frequenza occorrera usare parametri audio atomici e test di carico. Mancano inoltre richiamo automatico dei valori al cambio scena, feedback LED/motorizzato e un'interfaccia dedicata per i mapping multipli. Non sono state eseguite build o prove reali nell'ambito di questa PR.

**Persistenza:** i progetti sono salvati in formato `.cuelist` **v10**. Preset, binding, ingresso MIDI prescelto e associazione delle cue vengono serializzati insieme agli altri dati. Aprire file v1-v9 inizializza il sistema di mapping vuoto; i nomi e gli ID dei dispositivi dipendono dal computer utilizzato.

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

### Mixer e Master audio

Nelle **Utility Colonne** ogni colonna Audio presenta una striscia compatta: **GainMeterFader integrato** con cursore del gain in dB e due barre dei picchi L/R in dBFS, tutti riferiti alla medesima scala verticale da −60 a +12 dB, **pan rotativo compatto, di dimensioni intermedie** con indicatore L/C/R e pulsanti **Mute** e **inversione di fase**. Le uscite Audio, MIDI e OSC mantengono il proprio routing: le colonne MIDI mostrano la porta MIDI e quelle OSC la destinazione UDP, senza controlli del volume audio.

Il meter stereo di colonna misura separatamente **sinistra e destra**, sulla somma delle clip audio che stanno suonando nella stessa colonna: dopo gain di cella/riga/colonna, pan e mute, **prima del Master**. Il gain regolato dal cursore è un'amplificazione in **dB**, mentre le barre sono livelli digitali in **dBFS**: 0 dB del cursore significa gain neutro, 0 dBFS del meter significa piena scala digitale. Le due grandezze condividono la stessa posizione delle tacche, senza rappresentare lo stesso significato. Ogni barra ha un indicatore di picco con breve mantenimento; quando il segnale scompare, indicatore e barra scendono. Non è un misuratore RMS né un limiter. La doppia pressione sul controllo riporta il gain a 0 dB; frecce su/giu modificano il gain a passi di 0,1 dB (1 dB con Shift).

**Limite tecnico:** il mix stereo per la lettura del meter utilizza buffer preparati prima del callback, fino a 8192 frame per blocco. Per blocchi audio più lunghi, il meter mantiene un valore di sicurezza calcolato sui singoli contributi nel tratto eccedente; il rendering audio resta invariato.

Nell'area sinistra delle Utility Colonne, prima dei canali, trovi **MASTER AUDIO**: 0 dB è neutro, valori negativi attenuano tutto l'audio in uscita e valori positivi amplificano il mix. Il Master agisce soltanto sul PCM finale, **non** su messaggi MIDI o OSC e **non cambia** i livelli salvati nelle colonne. È applicato prima del taglio di sicurezza a ±1; non sostituisce un limiter per eventi dal vivo. Il meter globale OUTPUT sul lato sinistro indica il livello dopo il Master.

La regolazione Master è salvata nello show (formato v8). Gli show v1–v7 si aprono con Master **0 dB**.

### Copia/incolla e BPM direttamente nella griglia

Seleziona una cella e usa **Ctrl+C** per copiarne contenuto e impostazioni; seleziona una cella della **stessa tipologia di colonna** (Audio, MIDI oppure OSC) e usa **Ctrl+V** per incollare. Puoi utilizzare anche il **clic destro sulla cella** e il menu Copia/Incolla. L'ID della cella di destinazione rimane invariato, e per l'audio viene riutilizzato il media gia caricato nella sessione. Le colonne di tipo differente non possono ricevere l'incolla.

Le nuove colonne senza nome vengono chiamate automaticamente **Audio 1**, **Audio 2**, **MIDI 1**, **OSC 1** e cosi via, contando separatamente i tipi. Un doppio clic sul titolo della colonna permette di rinominarla; i nomi personali restano invariati. I vecchi progetti con nomi vuoti visualizzano comunque un'etichetta numerata senza riscrivere il file salvato.

Per le **righe complete**, usa il pulsante **…** accanto al nome della cue oppure il **clic destro sull'area del nome**: **Copia cue**, **Incolla cue sotto questa**, **Duplica cue**, **Elimina cue**. L'incolla crea una nuova riga sotto quella scelta, con i suoi tempi, tutte le clip e le impostazioni. La duplicazione crea subito una copia della riga con suffisso "(copia)". Entrambe assegnano nuovi identificativi alla riga e alle celle, senza cambiare gli ID originali. Il media audio precaricato puo essere condiviso in memoria; l'operazione interrompe il trasporto, analogamente al riordino o all'eliminazione delle righe. La copia conservata puo essere incollata anche dopo l'eliminazione della riga sorgente, purché la struttura delle colonne sia rimasta compatibile.

La **scritta BPM sulla riga** e cliccabile: permette di cambiare direttamente il tempo iniziale della cue, lasciando intatti gli eventuali cambi BPM successivi. Il cambiamento ricostruisce la temporizzazione MIDI/OSC e arresta il trasporto durante l'operazione. La metrica non e piu mostrata nell'interfaccia della griglia.

### Auto Stop 2.0

**Auto Stop della riga** resta l'interruttore generale, a destra delle cue, ed e attivo per impostazione predefinita. Il comando **AS ON / AS OFF** delle singole celle Audio, MIDI e OSC rimane nascosto per non affollare la griglia: premi **+** posto **sotto il toggle Auto Stop** della stessa riga per mostrarlo o nasconderlo; poi clicca il comando della cella che vuoi includere/escludere. La stessa opzione e disponibile nell'inspector, scheda **Cella**, come **Arresta al GO della prossima cue**.

**Una cella si ferma al GO di un'altra cue soltanto se sono ON sia l'Auto Stop della riga sia quello della cella.** Se la riga e OFF, tutte le sue clip continuano indipendentemente dalla scelta della cella. Se la riga e ON, le celle AS OFF continuano, mentre le altre si fermano. Le clip protette conservano il loro trasporto e la timeline della cue. La regola vale anche per i lanci diretti delle celle e quando la nuova cue e vuota. Il GO della stessa cue non applica Auto Stop a se stessa.

**Stop diretto, Stop della cue e Panic** interrompono sempre la riproduzione, anche per le celle AS OFF. L'opzione Auto Stop e distinta da **Partecipa al GO della riga**: quest'ultima stabilisce solo se una cella parte quando si lancia il GO collettivo.

I nuovi show hanno tutte le celle AS ON. All'apertura degli show precedenti (v1-v6), tutte le celle mantengono il valore predefinito ON, preservando il comportamento precedente del controllo Auto Stop di riga.

### Comportamenti di attivazione

| Comportamento | Effetto |
| --- | --- |
| Retrigger | Riparte dall'inizio, anche durante la riproduzione |
| Pausa / Restart | Mette in pausa; alla successiva attivazione riparte dall'inizio |
| Pausa / Resume | Alterna pausa e ripresa dalla posizione raggiunta |
| Continue | Continua se sta suonando; altrimenti riprende dalla posizione conservata |

**Loop** è indipendente dall'attivazione. **Partecipa al GO della riga** include o esclude la cella dal lancio collettivo. Nella scheda **Intera cue** puoi impostare livello, mute, loop e un comportamento comune per il GO; il lancio diretto mantiene il comportamento della cella.

## Clip OSC / UDP e automazioni

1. Aggiungi una colonna **OSC** e configura **host/IP e porta UDP** dalle Utility Colonne: la destinazione appartiene alla colonna, non ai singoli messaggi.
2. Seleziona una cella e premi **Crea clip OSC**. L'editor usa la **stessa timeline orizzontale** per eventi istantanei e corsie di automazione. Puoi staccarlo e riagganciarlo: è lo stesso editor e conserva gli stessi dati. Quando è integrato, allargane l'altezza trascinando la maniglia.
3. **Clic singolo nella corsia EVENTI**: crea un messaggio all'istante indicato. **Doppio clic sul marcatore**: modifica tempo, address e argomenti. Il pulsante **+** crea un messaggio e apre la modifica. Trascina il marcatore per spostarlo; usa **Duplica**, **Elimina**, il menu contestuale o Canc da tastiera.
4. **+ Rampa** crea una semplice automazione con due punti iniziale/finale; trascina i punti sulla curva o fai doppio clic sulla corsia per aprire l'editor avanzato. Nell'editor avanzato inserisci più punti, in formato `0:0; 0.5:0.75; 1:1`, riferiti all'inizio dell'automazione. Ogni curva indica un *indice argomento* numerico (1-based nell'interfaccia); gli altri argomenti restano al valore configurato. È possibile automatizzare **int32** e **float32**, non stringhe e booleani.
5. Gli argomenti OSC sono ordinati e si scrivono come `i:7; f:0.5; s:scena; b:true`. `i` = int32, `f` = float32, `s` = stringa, `b:true/false` = booleano OSC (type tag `T`/`F`, senza payload). La clip può essere in **Secondi** o in **Beat**: cambiando modalità la mappa BPM della cue converte gli istanti e i punti delle curve.
6. **Griglia** e **Snap** nell'editor sono interruttori **indipendenti**. Il menu delle suddivisioni regola lo snap. Tutte le modifiche valide sono applicate immediatamente e possono essere annullate/ripetute tramite Undo/Redo condivisi. Non esistono pulsanti Apply/Cancel. La durata complessiva della clip rimane separata e si può allungare dalla timeline principale.

**Riproduzione:** gli eventi istantanei partono una sola volta al rispettivo tempo. Le curve producono messaggi periodici, fino a 30 aggiornamenti/s (un budget massimo di circa 8.192 tick compilati per clip riduce la frequenza in automazioni molto lunghe o numerose). I messaggi, comprese le modifiche ai soli argomenti desiderati, sono **preparati sul thread non realtime**; il clock audio invia i pacchetti a un thread UDP dedicato.

**Stop:** interrompe l'automazione e annulla i pacchetti ancora in coda, **senza inviare reset**. Il ricevitore mantiene l'ultimo valore ricevuto, se non applica politiche proprie. **Retrigger:** riparte dall'inizio e invia di nuovo **il valore iniziale esatto** di ogni automazione al suo tempo di avvio, anche se uguale al precedente. **Loop:** ripete gli stessi messaggi. **Seek:** salta le posizioni già passate e prosegue dai prossimi aggiornamenti coerenti; non provoca reset.

**Più automazioni:** se più curve sullo stesso address controllano argomenti differenti, il compilatore compone insieme i loro valori numerici nei pacchetti successivi, per evitare che un aggiornamento riporti gli altri argomenti ai valori base. Due curve sovrapposte sullo stesso argomento vengono rifiutate.

L'invio utilizza **OSC 1.0 su UDP**, che non garantisce la ricezione. Il clock del trasporto richiede un dispositivo audio inizializzato anche per una scaletta di soli messaggi OSC. Prima di uno spettacolo testa host, porta, latenza e comportamento reale del ricevitore.

## Modifica e timeline

- Doppio click sul titolo di cue o colonna per rinominarlo: il campo riceve subito il focus. **Invio** conferma anche i nomi nell'inspector.
- Le frecce riordinano righe e colonne; **X** elimina. Eliminando l'ultima rimane una riga/colonna vuota. I file originali non vengono cancellati.
- Trascina una clip nella griglia per spostarla; una destinazione occupata viene scambiata. Le colonne devono essere dello stesso tipo.
- Trascina una clip nella timeline per modificarne l'inizio in secondi; sulle clip **MIDI/OSC** trascina la **maniglia sul lato destro** per modificarne la durata, mantenendo gli eventi già presenti. La sezione **GRIGLIA** mostra/nasconde la griglia BPM e ne seleziona la risoluzione (1/4, 1/8, 1/16, 1/32). La sezione **SNAP** ha un pulsante con **icona a magnete** per attivare/disattivare l'aggancio e una suddivisione separata. **La griglia può essere nascosta lasciando attivo lo snap, oppure mostrata con snap disattivato.** Le linee seguono la mappa BPM della cue, anche quando il tempo cambia. L'avvio diretto della cella parte subito, senza il ritardo di posizionamento.
- Trascina il cursore sul righello per cambiare la posizione della cue. Ogni cella mostra tempo trascorso/durata, barra di avanzamento e stato di riproduzione, pausa o attesa.
- **− / +** regolano lo zoom, **Fit** mostra tutta la cue e **Ctrl+rotella** ingrandisce attorno al puntatore. La barra orizzontale sposta la vista; la rotella scorre le tracce. **Clic su una clip MIDI nella timeline** apre il Clip Editor integrato sopra la timeline; **Stacca/Riaggancia** lo trasferisce fra pannello e finestra flottante.

I parametri audio si aggiornano senza azzerare il trasporto. Invio, **Applica modifiche**, Salva o il cambio selezione confermano i campi testo. Importazione, apertura e modifiche strutturali possono fermare l'audio.

## Utility di colonna e uscite

| Controllo | Funzione |
| --- | --- |
| Out | Coppia di uscite attive; una destinazione salvata ma assente viene segnalata senza sostituzione automatica |
| Fader dB | Da silenzio (−60 dB) a +12 dB; doppio click per 0 dB |
| Pan | Bilanciamento stereo: centro invariato, estremi con canale opposto silenziato; doppio click per centrare |
| Altoparlante barrato | Mute, senza fermare la clip |
| Cerchio barrato | Inversione della polarità di entrambi i canali |

I simboli si evidenziano quando attivi. I livelli di cella, cue e colonna si moltiplicano. Le uscite si aggiornano cambiando dispositivo o canali attivi. Sotto MIDI i controlli audio sono nascosti e il selettore permette di scegliere un dispositivo MIDI hardware o virtuale. Sotto OSC e disponibile il pulsante destinazione UDP: specifica il nome host o indirizzo IP e la porta del ricevitore. L'invio non fornisce conferma di ricezione (UDP).

## Clip MIDI e piano roll

1. Aggiungi una colonna **MIDI** con **+ Colonna**.
2. Seleziona la cella e usa **Importa MIDI** per un `.mid`/`.midi`, oppure **Crea clip MIDI** per partire da una clip vuota di 4 beat. Puoi anche trascinare un file sulla cella.
3. Seleziona una cella MIDI e scegli **Crea clip MIDI / Modifica piano roll** oppure clicca sulla clip nella timeline per aprire il **Clip Editor**; il pulsante Play della cella avvia la riproduzione.
4. Nelle utility seleziona l'**uscita MIDI**. Serve un sintetizzatore esterno, un'applicazione o uno strumento collegato a quella porta: The Cuelist non contiene un sintetizzatore.

Nel piano roll: clic sinistro nella griglia vuota per inserire una nota; trascina la nota per spostare tempo e altezza, oppure il bordo destro per cambiare durata. **Tasto destro + trascinamento** seleziona più note in un rettangolo; **Shift+clic** aggiunge o rimuove note dalla selezione. Le note selezionate possono essere spostate o eliminate insieme, e ricevono collettivamente le modifiche a **Velocity** (1–127) e **Canale** (1–16). **Canc** o **Elimina nota** le rimuovono. **Snap** sceglie la griglia da 1/4 a 1/32 oppure **Off** per spostamenti liberi; **+ / −** cambiano lo zoom e le barre scorrono la vista. **Beat clip** regola la durata senza tagliare note esistenti. I beat sono quarti; C4 corrisponde alla nota MIDI 60.

Le modifiche MIDI vengono **applicate subito al modello della clip**, senza un pulsante Applica. **Undo/Redo** annullano o ripetono le modifiche alle note. Il pannello può essere ridotto, chiuso oppure staccato e riagganciato; chiuderlo non annulla i cambiamenti. Premi **Salva** per conservarli nel file dello show.

Le clip MIDI partecipano a GO, Auto Stop, loop, pausa e seek della timeline; si possono spostare tra celle MIDI e trascinare nella timeline. I livelli in dB e il pan agiscono sull'audio; per le note si usa la velocity. Il trasporto MIDI usa il clock del dispositivo audio, che deve quindi essere aperto anche per uno show solo MIDI.

Importazione: MIDI standard tipo 0/1 con divisione PPQ, massimo 8 MB, 10.000 note e 4096 beat per clip. Le tracce vengono unite preservando i canali. Questa versione importa **solo note**: controller, sustain, program change, pitch bend, SysEx e tempo del file non vengono importati; un avviso segnala gli eventi ignorati. SMPTE e MIDI tipo 2 non sono supportati. La riproduzione segue la mappa tempo della cue.

Le uscite sono memorizzate per identificativo: un dispositivo assente non viene sostituito automaticamente con un'altra porta. Il GO con note MIDI richiede un'uscita disponibile. La precisione su hardware dipende da driver, buffer audio e pianificazione del sistema; le prove fisiche di latenza e disconnessione restano da eseguire.

## Progetti portabili e backup

**Salva** crea un `.cuelist` e raccoglie gli audio in una cartella `<nome>_media` accanto al progetto. Per trasferire uno show **copia entrambi**, mantenendoli affiancati. I percorsi salvati sono relativi.

Sovrascrivendo uno show, la versione precedente resta in `.cuelist.bak`. Le generazioni precedenti dei media vengono conservate per rendere utilizzabile il backup: salvataggi ripetuti possono aumentare lo spazio occupato.

Il formato corrente è **v10**. **Apri** legge anche v1/v2/v3/v4/v5/v6/v7/v8/v9 e backup. La v9 aggiunge automazioni OSC, punti di controllo e argomenti booleani. Il flag Auto Stop per cella viene letto dagli show v7 e inizializzato su ON negli show precedenti. Le clip MIDI sono salvate direttamente nello show, senza dipendere dal file MIDI importato:

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
- OSC UDP con messaggi int32/float/string/boolean e automazioni lineari implementato; LTC/MTC, mapping ingressi, autosave e recupero dopo crash non implementati. ASIO disabilitato in questa build; WASAPI disponibile tramite JUCE.
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
