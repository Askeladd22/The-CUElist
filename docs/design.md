# The Cuelist — primo incremento

Specifica funzionale: message.txt fornito dall'utente. L'immagine definisce esclusivamente lo stile: fondo blu notte, griglia di cue, sidebar, inspector e timeline. Nessuna gestione video.

## Decisioni
- Applicazione Windows standalone C++17/JUCE 8.0.12, dipendenza fissata; CMake e MSVC. Scelta C++/JUCE confermata dall'utente.
- Primo incremento: editor di righe/colonne Audio, MIDI, OSC; audio WAV mono/stereo simultaneo, trigger di cella/riga, esclusione dal trigger di riga, comportamenti Retrigger/Restart/PauseResume/Continue/Stop, loop audio, Panic, selezione dispositivo e coppia di uscite; salvataggio JSON versionato e backup.
- MIDI/OSC sono tipi del modello, chiaramente non operativi in questa versione. Nessun indicatore di connessione fittizio.
- Mappa tempo per riga in quarter-note beats, BPM e metrica. Audio a velocità originale; nessun time-stretch. La timeline del primo incremento visualizza la durata dei file della riga selezionata, non è ancora un editor di eventi.
- UI, modello/serializzazione e motore audio separati. Nessuna lettura file o allocazione nel nostro callback audio. PCM decodificato prima della riproduzione, limite di memoria dichiarato; snapshot audio pubblicati atomicamente e liberati dal thread UI.
- Importazione, apertura progetto e modifiche strutturali fermano il trasporto: scelta conservativa del primo incremento, segnalata nell'interfaccia.
- Progetti con riferimenti assoluti ai WAV, rilevamento file mancanti e ricollegamento tramite import. Raccolta portatile, autosave, MIDI/OSC operativi, LTC/MTC, gruppi e mapping sono incrementi successivi.
- Nessuna promessa di precisione professionale o uso live certificato senza prove hardware, carico e lunga durata.

## Alternative considerate
Qt + motore separato aumenta il numero di integrazioni. JUCE offre già GUI, dispositivi audio e MIDI e OSC nello stesso stack. Prima della distribuzione definire licenza del prodotto e licenza JUCE; ASIO resta disabilitato in questo incremento. WASAPI è disponibile.

## Verifiche
Test automatici di mappa tempo, validazione file progetto e roundtrip, trasporto audio offline (campioni, loop, stop, esclusione, conversione sample rate). Build Release, avvio desktop e ispezione GUI. L'ascolto e la verifica di hardware professionale richiedono una sessione con l'utente.
