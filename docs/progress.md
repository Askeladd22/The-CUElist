# Registro sviluppo

## Versione 0.2 portable — 25 ottobre 2026

Questa sezione aggiorna e sostituisce i limiti della versione 0.1 descritti sotto.

- Implementate tutte le richieste elencate in `docs/TODO-0.2.md`: MP3, nomi cue/colonne vuoti, Invio e doppio click per rinomina, GO sequenziale, controlli visibili di riordino/eliminazione, drag nella griglia e timeline, seek reale, livello dB live e parametri globali di cue.
- I parametri audio preservano posizione e pausa; importazioni, riordino, eliminazione e apertura possono fermare il trasporto. La selezione scorre verticalmente fino alla cue.
- Persistenza v2: offset e controlli globali, raccolta media con percorsi relativi, migrazione v1, backup e test di trasferimento senza file originali. Le generazioni media precedenti restano disponibili per i backup e occupano spazio.
- Build Release completa: 46 casi, 234 asserzioni, zero fallimenti. Inclusi PCM MP3 reali, controlli effettivi dell'inspector, eventi mouse di riordino/drag/seek, pausa, replay dopo fine e comandi accodati durante aggiornamenti dei parametri.
- Revisione indipendente: corretti perdita di comandi in coda durante pubblicazione parametri e replay delle cue concluse che saltava gli offset. Regressioni riprodotte prima delle correzioni; seconda revisione senza problemi gravi residui.
- Anteprime native verificate a 1440x900 e 1180x820, schede Cella e Intera cue. Corretta sovrapposizione sidebar alla dimensione minima.
- Pacchetto `dist/The-Cuelist-0.2.0-Windows-x64.zip` creato e verificato: estrazione, hash, launcher senza compilazione, avvio offline e chiusura normale dell'eseguibile estratto. Dipendenze verificate: solo DLL Windows, runtime C/C++ statico.
- Nessun test sul PC `C:\Users\admin`, nessun test di ascolto o lunga durata. MIDI/OSC, ASIO, streaming e timecode restano fuori da questo incremento. Nessun push o rilascio pubblico effettuato.

Per usare il programma estrarre tutto lo ZIP e aprire `The Cuelist.exe`. Per trasferire un progetto copiare insieme `.cuelist` e relativa cartella `_media`.

- Autorizzazione utente: C++/JUCE; inizio implementazione e autonomia tecnica.
- Baseline: repository vuoto, nessun codice o test precedente.
- Ambiente trovato: MSVC 14.51, CMake integrato in Visual Studio Build Tools 2026.
- Ruling: lavoro nella cartella vuota sul ramo feat/desktop-foundation; nessun worktree necessario a proteggere lavoro esistente.
- Ruling: implementazione diretta nella sessione e una revisione finale; nessuna ulteriore approvazione progettuale richiesta dopo l'invito esplicito a iniziare.
- Interfacce: il modello identifica celle con ID persistenti; il renderer usa gli stessi ID; la GUI pubblica snapshot completi prima di inviare comandi di trasporto.
- Stato al 25 ottobre 2026: primo incremento implementato, build Release compilata e avviata.

## Risultati verificati

- Comando: `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1`.
- MSVC 19.51 / CMake 4.3, x64 Release. Ultima compilazione senza errori o warning.
- CTest: 1 suite, **18 casi e 79 asserzioni**, zero fallimenti. Log: `build/Testing/Temporary/LastTest.log`.
- Modello: mappe tempo separate, griglia rettangolare, ID unici, roundtrip, rifiuto di documenti malformati senza sostituire lo show corrente.
- Renderer: PCM effettivi, coppie di routing, conversione sample rate, loop, Pause/Resume, Continue, Stop, esclusione dal GO e Panic.
- Engine: GO mantenuto in coda finché il suo snapshot diventa attivo; Panic invalida i GO precedenti.
- Persistenza: WAV stereo decodificato; salvataggi reali, riapertura, backup della versione precedente e rifiuto di destinazione non scrivibile.
- Editor: modifiche in attesa conteggiate come lavoro non salvato, cambio selezione le applica; Escape nei campi testo raggiunge Panic; dialogo dispositivi distrutto prima del manager audio.
- GUI Release aperta su Windows e stato Audio disponibile osservato. Comando Apri verificato fino al dialogo nativo; apertura completa via UI e ascolto non completati a causa di interferenze con input utente. Il caricamento progetto e WAV è coperto dai test di integrazione su disco.
- Anteprima nativa JUCE renderizzata e ispezionata: `build/Release/test-artifacts/interface-preview.png`.

## Revisione e correzioni

Revisione indipendente eseguita sull'intero nuovo codice. Quattro problemi importanti individuati e corretti: perdita di GO durante aggiornamento snapshot, Escape consumato dai campi testo, perdita di modifiche dell'inspector durante Save/Quit, vita del dialogo dispositivi oltre quella del manager audio.

I primi tre casi sono stati riprodotti con test fallenti prima della correzione; il quarto è coperto da un test di distruzione del dialogo. Corretti anche caratteri non ASCII resi male e sovrapposizione dei controlli sidebar alla dimensione minima (altezza minima 820).

## Decisioni tecniche del responsabile

- Un WAV mono/stereo per cella e precaricamento PCM (128 MB/file, 512 MB/show). Il costo è un limite sui media lunghi; lo streaming e il multitraccia interno sono incrementi futuri.
- Import, apertura e modifica dei parametri fermano il trasporto. Il costo è l'impossibilità di editare durante una riproduzione continua in questo incremento.
- Ricampionamento lineare e saturazione di sicurezza +/-1: sufficienti al primo nucleo verificabile, non equivalenti a DSP professionale certificato.
- Colonne MIDI/OSC presenti nel modello, motori non ancora operativi; ASIO, LTC/MTC e mapping rinviati e dichiarati nella GUI/documentazione.
- Codice lasciato nella cartella richiesta sul ramo di sviluppo; nessun push, merge o pubblicazione.

## Rifiniture rinviate

- Le scelte di routing nell'inspector si aggiornano cambiando selezione dopo una modifica dei dispositivi; aggiornamento automatico ancora da aggiungere.
- La selezione con frecce non scorre automaticamente fino alla cue se questa esce dalla parte visibile della griglia.
- Nessun test di lunga durata, latenza/jitter, ascolto, disconnessioni reali o distribuzione su Windows pulito.

## Prossimo incremento

Streaming WAV e routing hardware verificato, poi invio MIDI/OSC con test su loopback; mantenere separati tempo relativo, musicale e timecode.

## Correzione distribuzione — 25 ottobre 2026

- Risolto il launcher che, in assenza di `build`, tentava di compilare e richiedeva Visual Studio sul computer dell'utilizzatore.
- `Avvia.cmd` ora avvia esclusivamente un eseguibile esistente; se manca, spiega come ottenere il pacchetto portatile. La compilazione resta un'azione separata dello sviluppatore.
- Creato `dist/The-Cuelist-0.1.0-Windows-x64.zip`: eseguibile Release, launcher, guida, informazioni JUCE e hash SHA256.
- Verifica `dumpbin /dependents`: solo DLL di Windows; nessuna DLL runtime Visual C++ esterna. Runtime C/C++ collegato staticamente.
- `scripts/check-package.ps1` eseguito: estrazione ZIP riuscita, eseguibile identico alla build verificata, avvio senza eseguibile restituisce istruzioni senza compilare.
- Nessuna installazione o prova eseguita sul computer `C:\Users\admin`: il pacchetto va copiato ed estratto su quel computer. Nessun upload o rilascio pubblico effettuato.
