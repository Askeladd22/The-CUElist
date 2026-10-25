# Registro sviluppo

## Versione 0.4 portable — 25 ottobre 2026

- Uscite audio spostate dall'inspector alle utility di colonna, con aggiornamento dei canali attivi. Le colonne MIDI/OSC mostrano solo un selettore non operativo; controlli audio nascosti.
- Icone vettoriali altoparlante barrato e cerchio barrato per mute e polarita, con evidenza dello stato e tooltip.
- Cella audio con tempo trascorso/durata, barra di progresso e indicatori di playback/pausa/attesa. Telemetria atomica separata dallo stato mutabile del renderer, senza accessi concorrenti alle voci. A dispositivo disconnesso la GUI nasconde gli stati attivi e conserva l'ultima posizione nota.
- Auto Stop per riga, attivo per default: GO di qualsiasi altra cue ferma tutte le voci delle righe abilitate, anche dirette/escluse dal GO. Anche una cue vuota attiva l'arresto; Play singolo e Stop non interferiscono con altre righe. Disattivando il toggle si mantiene la sovrapposizione.
- JSON v4 conserva stopOnNext; v1/v2/v3 vengono importati con Auto Stop attivo. Salvataggio e media restano portable.
- Build Release completa: **68 casi, 20.472 asserzioni, zero fallimenti**. Il conteggio include verifiche di 10.000 righe nel test di grandezza. Verificati routing PCM live, editor draft conservati, hitbox del toggle, stop selettivo, telemetria e migrazione.
- Revisione indipendente: eliminato il costo quadratico dell'Auto Stop nel callback; ora scansioni O(righe + voci). Seconda revisione senza ulteriori rilievi.
- Anteprime native verificate con Audio/MIDI/OSC affiancate e clip a 2 secondi su 5; controllata anche la dimensione minima. ZIP `dist/The-Cuelist-0.4.0-Windows-x64.zip` verificato per estrazione, hash, launcher non compilante, avvio offline e chiusura normale.
- Nessuna prova di ascolto o cambio dispositivo su hardware esterno; test PCM con uscite simulate. Nessun push o rilascio pubblico.

## Versione 0.3 portable — 25 ottobre 2026

- Rinomina: focus immediato nel campo e testo selezionato, verificato su dialogo JUCE reale.
- Pausa/Restart sostituisce Restart: prima mette in pausa, successiva attivazione riparte dall'inizio; Pausa/Resume conserva la posizione. Stop rimosso dai menu, mantenuto come comando esplicito.
- Stop a destra del titolo cue ferma anche le clip escluse dal GO. Pulsanti Play/Stop nelle celle indipendenti dalla sequenza; Play applica il comportamento scelto.
- Timeline con zoom 1x–32x, meno/piu/Fit, Ctrl+rotella attorno al puntatore, barra orizzontale. Seek e drag testati anche con zoom e resize.
- Utility sotto ogni colonna Audio: fader dB, pan/bilanciamento stereo, mute e inversione polarita. Aggiornamenti live senza azzerare il cursore; valori non modificati conservati senza quantizzazione indiretta.
- Progetti v3 con utility persistenti; lettura v1/v2 e migrazione dei vecchi Restart/Stop a Retrigger. Il salvataggio resta portable con media raccolti.
- Build completa Release: **58 casi, 389 asserzioni, zero fallimenti**. Revisione indipendente senza problemi bloccanti; corretta la precisione dei parametri non toccati dai toggle.
- Rendering GUI verificato a 1440x900 e 1180x820. ZIP `dist/The-Cuelist-0.3.0-Windows-x64.zip` estratto e verificato: hash uguale alla build, launcher non compilante, avvio offline e chiusura normale.
- Nessun test di ascolto o lunga durata su hardware esterno, nessun push o rilascio pubblico. Motori MIDI/OSC ancora previsti.

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
