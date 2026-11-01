# CHANGELOG — The Cuelist

Tutte le modifiche future devono avere **versione incrementata in
[`VERSION.txt`](VERSION.txt)** e una nuova sezione in testa a questo file.
La versione del progetto `.cuelist` (**schema JSON v10**) è **indipendente**
da quella dell'applicazione. Modalità di aggiornamento:
[`docs/VERSIONING.md`](docs/VERSIONING.md).

Questo changelog è stato introdotto il **31 ottobre 2026** e ricostruito
retrospettivamente da `VERSION.txt`, dalla cronologia Git e dalle PR
effettivamente integrate. **Non esistono GitHub Releases o tag che
certifichino tutte le milestone riportate**; le etichette storiche
indicano versioni di sviluppo presenti nei commit, non distribuzioni
pubbliche. I particolari di ogni intervento sono nell'appendice.

## [0.9.0-beta.16] - 2026-11-01

**Tipo:** redesign nativo V2 / N3.3 — righe, celle e stati della Cue Grid, senza riscrivere la logica operativa.

### Interfaccia JUCE
- Le righe cue formano una matrice continua con separatori molto sottili, lieve alternanza Midnight Slate e selezione estesa all'intera larghezza, anche oltre l'ultima colonna.
- Selezione, NEXT e playback reale hanno indicatori indipendenti anche quando coesistono; i testi operativi PLAYING, PAUSED, NEXT CUE, MUTED e STANDBY restano presenti. Non sono introdotti stati READY/ERROR/STOPPED artificiosi nel runtime.
- Celle vuote senza riempimenti e bordi ripetuti; hover discreto senza alcun effetto sullo show o sul trasporto. Focus/selezione della cella delineati separatamente.
- Clip e mappe caricate mantengono accenti per tipo, nome, durata, progress bar, play/stop, loop e Auto Stop per cella. Indicazione puntuale di attività soltanto da `getCellPlayback`; attese/pausa e media missing restano segnalate con testo.
- Le colonne collassate distinguono il contenuto reale da una cella vuota; gli indicatori Auto Stop per cella sono portati a 10pt.

### Regressioni e vincoli
- Test JUCE di snapshot della selezione a tutta larghezza, hover reversibile senza mutazione del modello, media mancante e clip caricata; rimangono i test di N3.1/N3.2 e i test dell'esecuzione.
- Invariati gli hitbox di GO, Play/Stop, import, menu, duplicazione, cancellazione, drag/drop ed editing Audio/MIDI/OSC/Control.
- Conservate le metriche originarie (62px riga, 72px header, 318px lane cue, 152/48px colonne, 184px BPM) e la sincronizzazione mixer/scroll.
- **Master Audio intatto**: nessuna modifica a `ColumnUtilities.cpp`, Inspector, Timeline, barre di menu, motori/routing/mapping o formato `.cuelist` JSON v10. Nessuna build portable.
- CI Windows/CTest e revisione visiva su PC con DPI differenziati restano criteri necessari prima del merge. N3.4/N3.5 rimangono incrementi separati.

## [0.9.0-beta.15] - 2026-11-01

**Tipo:** complemento incrementale della N3.2 — layout su due fasce della colonna Cue Name/Transport.

### Modificato
- Titolo della cue ampliato da 94 a **196 px**, preservando la larghezza complessiva di 318 px della lane Cue/Master e la riga da 62 px.
- Play, Stop e menu operativo disposti sulla seconda fascia accanto al numero/stato; GO resta separato e non viene attivato dalla semplice selezione.
- Stessa geometria per paint, hit testing, drag della cue, doppio click e posizione dell'editor inline. Gli offset dello scroll verticale continuano a essere condivisi con la lane fissa.
- Tooltip della lane Cue mostrato soltanto se il testo eccede la larghezza visibile, mantenendo i tooltip delle colonne e lo stile delle intestazioni già integrati nella beta.14.

### Regressioni e limiti
- Test JUCE aggiornati per le nuove hitbox e l'assenza di intersezioni, incluso click sulla zona aggiuntiva del titolo e rinomina inline.
- Invariati mixer, strip, **Master Audio**, Inspector, Timeline, sidebar, motori, formati `.cuelist` JSON v10, gestione delle celle e sincronizzazione BPM/Auto Stop.
- Mantiene N3.1 e N3.2 originali. Non include ancora le nuove superfici delle celle e gli stati della N3.3 né la compattazione definitiva 36–40 px.
- Verifiche Windows Actions/CTest e screenshot su Windows sono gate prima del merge. Nessuna build portable.

## [0.9.0-beta.14] - 2026-11-01

**Tipo:** fase N3.2 del redesign nativo Cue Grid: intestazioni e Cue/Transport.

### Grafica JUCE
- Intestazioni della matrice in Midnight Slate continuo:
  superfici meno stratificate, regole di tipo sottili da 3px,
  separatori verticali e etichette Audio/MIDI/OSC/Control
  identificate anche testualmente.
- Gerarchia migliorata per nome Cuelist, contatore cue/clip,
  titoli delle colonne e nomi/stati Cue; gli stati PLAYING,
  PAUSED, NEXT CUE e MUTED sono distinti senza affidarsi
  soltanto al colore.
- Frecce, pulsanti di gestione, collasso e menu contestuale
  visualmente più leggeri, senza restringere la relativa
  superficie interattiva.
- Tooltip con titolo completo sulle intestazioni cue e
  colonna pinned, che seguono gli offset reali del viewport.

### Regressioni e perimetro
- Test nativi in `tests/CueGridN3VisualTests.h`: paint
  dello header tramite screenshot JUCE, accenti cromatici,
  hitbox Play/Stop/Menu, tooltip di nomi lunghi e selezione
  senza GO accidentale. Restano le suite precedenti.
- `docs/UI_V2_NATIVE_PHASE_N3_2.md` documenta scope e
  verifiche della fase.
- Non è ancora una riduzione delle righe a 36–40px:
  dimensioni originali, celle, motori, layout Inspector,
  Timeline, menu, mix e formato show JSON v10 invariati.
- **Master Audio non modificato**: nessun intervento al
  componente JUCE originale o alle sue funzionalità.
- Nessuna build portable.

## [0.9.0-beta.13] - 2026-11-01

**Tipo:** prima implementazione JUCE della fase N3 (N3.1 — fondazioni geometriche della Cue Grid), senza ancora cambiare la grafica.

### Modifiche native
- Nuovo contratto puro `src/CueGridGeometry.h` con le **dimensioni originali**, larghezze di colonne espanse/collassate, coordinate e hit test delle celle, conversioni per le righe e gli overlay fissi, scroll e banda BPM/Auto Stop.
- `CueGrid` ora delega alla stessa fonte le posizioni e larghezze delle colonne e il relativo hit testing; anche `PinnedCueHeaders`, `PinnedColumnHeaders` e `CueMetricsOverlay` usano le conversioni comuni.
- Default `WorkspaceLayoutInput` e margine di scorrimento derivati dalla medesima sorgente.
- Nessuna diminuzione prematura della riga da 62 px o delle hitbox: la compattezza 36–40 px richiederà la fase N3.2/N3.3 con test dedicati.

### Verifiche
- `tests/CueGridGeometryTests.h`: 13 cue, 11 colonne, collasso misto, scroll X/Y, limiti dei click e geometria a 1280×720, 1647×895, 1920×1080 e 3840×1080.
- Inventario dei componenti, responsabilità e limiti della fase:
  [`docs/UI_V2_NATIVE_PHASE_N3_1.md`](docs/UI_V2_NATIVE_PHASE_N3_1.md).

### Funzionalità preservate
- Il mixer continua a sincronizzarsi con l'unico viewport esistente.
- Banda BPM/Auto Stop, GO/Stop, riga/cella, editor, DnD, mapping, tema, Inspector e Timeline invariati.
- **Master Audio rigorosamente intatto**: nessuna modifica al paint, dimensioni o callback.
- Nessuna modifica al formato dei salvataggi `.cuelist` JSON v10, audio/MIDI/OSC o al motore. Nessuna build portable.

## [0.9.0-beta.12] - 2026-11-01

**Tipo:** correzione dell'architettura dei menu JUCE N2.1; parità funzionale con le opzioni del vecchio Settings.

### Corretti
- Rimossi i duplicati **Pre-flight** e **Diagnostics** da File/View:
  compaiono ora **una sola volta**, nel menu Show.
- Nessun generico "Settings" o "MIDI / OSC Settings" che ripresenti
  il vecchio menu in un altro punto: Devices include ora
  direttamente **Audio Devices**, **MIDI Inputs**,
  **MIDI Outputs**, **OSC Input** e **Control Mapping**.
- Ripristinate e rese visibili le impostazioni del menu originale:
  input MIDI principale e aggiuntivi, attivazione/disattivazione
  delle uscite MIDI, configurazione/ascolto UDP OSC e IP locale,
  intervallo fader 0/+6 dB (**View → Fader Range**),
  formato orario 24/12 h (**View → Clock**).
- Le scorciatoie MIDI/OSC Learn rimangono nella telemetria
  superiore; eliminati i doppioni nel menu Devices. Collapse all
  rimane nel cue header anziché apparire anche in View.
- I menu MIDI usano i device ID rilevati all'apertura, evitando
  associazioni errate se l'enumerazione cambia durante il popup.
- Il menu Cuelists mostra spostamento/eliminazione soltanto
  quando l'operazione è effettivamente possibile, evitando
  voci disabilitate inutili con un solo elenco.

### Verifiche e garanzie
- Nuovo test `test_native_operator_menu_parity.py` eseguito
  nelle CI di versione e Windows; documentato l'inventario
  originale nel
  [menu audit N2.1](docs/UI_V2_NATIVE_MENU_AUDIT.md).
- **Master Audio immutato**; nessuna modifica a
  `ColumnUtilities.cpp`, codec, motori, progetto JSON v10,
  GO/Panic, cue grid e strip.

## [0.9.0-beta.11] - 2026-11-01

**Tipo:** fase N2 del redesign JUCE — rimozione delle duplicazioni dalla sidebar; continuazione del branch `feat/native-operator-header-beta10`.

### Modificato
- Nascosti i pulsanti duplicati **New, Open, Save, Settings,
  Preferences e Control Mapping** nella sidebar, poiché sono ora
  richiamabili dai menu nativi File, Show e Devices della fase N1.
- Gli stessi callback JUCE originali sono conservati; File
  continua a gestire apertura/salvataggio e conferme di abbandono.
- Settings conserva ogni voce e impostazione esistente:
  il popup è ora ancorato al menu Show visibile, anziché
  al vecchio pulsante Preferences nascosto.
- Eliminato il riquadro PROJECT con numero cue/colonne duplicato.
  Rimane un solo nome show editabile, spostato nella zona
  WORKSPACE; l'indicatore modifiche non salvate appare
  soltanto quando serve.
- **ON DECK, GO/PANIC, shortcut, cue grid, inspector,
  mixer, Master Audio e diagnostica restano invariati.**
- Aggiornati i test nativi per verificare sidebar pulita,
  show editabile, accessibilità File/Show/Devices/More
  e il formato show v10 senza regressioni.

### Documentazione
- [UI V2 N2 — Sidebar](docs/UI_V2_NATIVE_PHASE_N2.md).
- Nessuna nuova dipendenza, nessuna build portable.

## [0.9.0-beta.10] - 2026-11-01

**Tipo:** prima integrazione reale JUCE del redesign V2 (fase N1: navigazione superiore); nessun intervento sul Master Audio.

### Implementato
- Barra superiore nativa con icona compatta, identità The CUElist e
  menu **File, Edit, Show, Cuelists, Devices, View, Guide**.
- Popup JUCE funzionali collegati ai metodi già esistenti
  dell'applicazione: Nuovo/Apri/Salva, Undo/Redo,
  cue e colonne, Settings, Cuelist, dispositivi,
  Control Mapping, MIDI/OSC Learn, Inspector, Guide/About.
- Overflow **More** a larghezze ridotte, che raggruppa
  le categorie non visibili senza perdere i comandi
  (1024/1180/1280 px testati).
- Sostituiti nella vecchia barra superiore i pulsanti diretti
  Help, Undo e Redo con i nuovi menu; mantenuti i callback
  e le scorciatoie originali.
- Conservati temporaneamente i comandi di sidebar
  per evitare regressioni prima della sua fase nativa separata.
- Inseriti test CTest JUCE in `OperatorNavigationTests.h`
  su geometria, overflow, presenza pulsanti veri, collegamenti
  originali e permanenza del Master.

### Vincoli
- **Master Audio originale intoccato**: nessuna modifica a
  `ColumnUtilities.cpp`, routing, fader/meter stereo,
  pan, mute, Main Out, limiter e Quick Learn.
- Nessuna modifica a motori, modello Show v10, cue,
  timeline, Inspector, MIDI/OSC/Control, mixer o
  comportamenti di salvataggio.
- Fase N1 documentata in
  [UI_V2_NATIVE_PHASE_N1.md](docs/UI_V2_NATIVE_PHASE_N1.md).
  L'integrazione del resto della GUI è successiva.

## [0.9.0-beta.9] - 2026-11-01

**Decisione di design aggiuntiva:** MASTER AUDIO ESCLUSO DAL REDESIGN.
Il componente JUCE originale, il suo disegno e la sua geometria, tutti
i controlli e i callback devono essere preservati senza modifiche:
fader/meter stereo, Pan, Mute, Main Out, limiter/gain reduction e Quick Learn.
La preview HTML sostituisce il Master sperimentale con una semplice
rappresentazione statica indicativa del componente originale; non è un
nuovo Master né una specifica da implementare in JUCE. Le altre
strip Audio/MIDI/OSC/Control possono essere rifinite.

**Tipo:** rifinitura mirata del concept Operator Workspace V2.3; nessun cambiamento ai sorgenti e ai motori JUCE di produzione.

### Migliorato
- **Mixer integrato più leggibile:** mantenuta la continuità geometrica
  delle 11 colonne con la matrice e lo stesso scroll orizzontale.
  Audio conserva fader, meter e segnali visivi di routing, pan,
  mute, inversione di fase, invio Master L/R e PRE/POST;
  MIDI mostra porta, stato TX e mute della trasmissione;
  OSC mostra endpoint UDP, TX e mute della trasmissione;
  Control mostra priorità, stato del mapping e accesso a Edit set.
- **Gerarchia tipografica:** valori di gain, cue name, legenda
  colonne e stati delle strip più leggibili, con contrasto dei
  testi secondari leggermente incrementato senza introdurre
  nuove cornici o cambiare l'identità Midnight Slate.
- **Sidebar ripulita:** eliminata la nota temporanea sulla
  collocazione dei menu, sostituita con un breve riepilogo
  di show (12 cue list, 13 cue, 11 colonne).
  GO/Panic e on-deck rimangono immediatamente visibili.
- I menu superiori non scompaiono più nei layout stretti
  (fino a 1280px), evitando che funzioni spostate
  dalla sidebar diventino irraggiungibili.
- **Master V2.2 invariato:** conservati gain/meter stereo,
  Main Out, limiter e gain reduction, pan/mute e Quick Learn;
  nessuna riduzione funzionale del prototipo.

### Verifica e limitazioni
- Test esteso di parità del concept a confronto con i nomi
  dei controlli JUCE realmente esistenti in
  `src/ColumnUtilities.cpp`, più test per la sidebar.
- La preview HTML non attiva transport, dispositivi,
  fader, mute, routing o mapping: i dati sono dimostrativi.
  Nessun file `src/` modificato, nessun intervento a
  AudioEngine, MIDI, OSC, Control o schema dello show.
- Misure e leggibilità native su Windows ancora da
  validare prima del trasferimento in JUCE.

## [0.9.0-beta.8] - 2026-11-01

**Tipo:** concept UI V2.2 e gate di parità funzionale — soltanto design, documentazione e test, nessuna modifica ai sorgenti JUCE.

### Corretto
- Rimossa la doppia navigazione dalla sidebar della preview: lo show
  attivo, NEXT, GO e PANIC restano visibili a sinistra; File,
  Edit, Show, Cuelists, Devices, View e Guide espongono le voci
  organizzate in menu superiori dimostrativi, inclusi Settings,
  Preferences, Project info e Control Mapping.
- Sostituito il Master generico a due controlli con la rappresentazione
  delle funzioni verificate nel codice nativo: gain e peak L/R,
  pan, mute audio, selezione Main Out, limiter di sicurezza
  -1 dBFS, gain reduction e tre target Quick Learn
  (gain, pan, limiter).
- Master mantenuto sotto la cue label nella fascia mixer integrata;
  la parte superiore della cue matrix resta invariata.

### Parità funzionale obbligatoria
- `docs/UI_V2_FEATURE_PARITY.md`: inventario Master proveniente da
  `MainComponent.cpp` e `ColumnUtilities.cpp`, vincoli per Audio,
  MIDI, OSC, Control, inspector, timeline, trasporto, mapping,
  tab, salvataggi e dispositivi. Il redesign nativo richiederà
  una corrispondenza verificabile per ogni funzione preesistente.
- Estesi i test della preview per rilevare omissioni nel Master
  o accessi duplicati e per confrontarli con il codice JUCE reale.

### Limiti
- Nessuna modifica a `src/`, eventi, audio/MIDI/OSC/Control,
  routing, mapping, motori, cue, progetto `.cuelist`
  (schema v10) o GUI attualmente utilizzata.
- I controlli HTML rimangono **preview non operativa**.
  Il limiter, i meter e le opzioni di routing reali saranno
  ancora quelli dei componenti JUCE nativi; la preview
  serve solo a definire la futura posizione dei controlli.
- QA visuale Windows e implementazione JUCE native successive.

## [0.9.0-beta.7] - 2026-11-01

**Tipo:** fase P0 del redesign strutturale UI V2 — studio e prototipo autonomo, senza cambiamenti alla GUI operativa.

### Aggiunto
- `design/operator-workspace-v2.html`: prototipo offline ad alta densità, con 12 Cuelist, 13 cue, 11 colonne miste Audio/MIDI/OSC/Control, cue fissi, banda BPM/Auto Stop fissa, selezione evidente, stato on-deck, Inspector, mixer dock e Timeline. Supporta solo interazioni visive reversibili: cambio tab, selezione cue, densità e collasso pannelli.
- `docs/UI_V2_OPERATOR_REDESIGN.md`: audit JUCE, specifica di product design, geometrie candidate e piano di migrazione graduale senza toccare il runtime.
- `scripts/test_operator_workspace_v2.py`: test standard library su densità, regioni fisse, interazioni accessibili e isolamento hardware del prototipo.

### Iterazione V2.1 — feedback sulla demo
- Parte superiore del concept confermata; modificata solo la regione inferiore.
- Mixer non più disegnato come dock indipendente pieno di card:
  Master allineato alla zona cue, ogni canale nella stessa colonna
  Audio/MIDI/OSC/Control della matrice, scroll orizzontale derivato
  dall'unico viewport della cue list.
- Colonne non-audio senza meter/fader fittizi, ma con destinazione,
  stato e controllo semantico; placeholder grafici, non comandi hardware.
- Aggiunta [specifica geometrica V2.1](docs/UI_V2_INTEGRATED_MIXER_SPEC.md)
  e test di allineamento della demo.

### Verifiche e limitazioni
- La nuova preview non usa AudioEngine, MIDI, OSC o il modello show e non può eseguire GO/Panic: i comandi sono dimostrativi.
- Nessuna modifica a `src/`, routing, persistenza v10 o build portable. Il mockup impiega un font browser surrogato; la futura UI nativa conserva Hind/Exo 2.
- La direzione grafica è proposta, non approvata: servono feedback su 1647×895, 720p, 1080p e l'applicazione successiva con test CTest e hardware.

## [0.9.0-beta.6] - 2026-10-31

**Tipo:** aggiornamento di manutenzione e tracciabilità; non è una release pubblica o una certificazione per spettacoli live.

### Aggiunto
- Registro versionato `CHANGELOG.md`, con ricostruzione retrospettiva delle fasi documentate e inventario delle pull request effettivamente integrate.
- Procedura obbligatoria per ogni modifica in `docs/VERSIONING.md`: versione, descrizione, prove e limiti.
- Verifica CI che confronta `VERSION.txt` e la sezione più recente del changelog e richiede incremento di versione per ogni PR successiva.

### Modificato
- Versione sorgente da `0.9.0-beta.5` a `0.9.0-beta.6`, mantenendo l'unica fonte esistente `VERSION.txt`. CMake, versione in-app About, eseguibili e packaging continuano a derivarla da quel file. I pacchetti portable manuali includono ora sia `VERSION.txt` sia `CHANGELOG.md`, verificati dal controllo di integrità.
- README e checklist beta rimandano alla versione corrente in `VERSION.txt`, evitando numeri obsoleti copiati a mano.

### Verifiche e limitazioni
- Validatore SemVer e ricostruzione storica verificati sui sorgenti GitHub; i test Windows JUCE/CTest restano di competenza della CI e dell'ambiente locale.
- Nessuna modifica a routing audio, MIDI, OSC, modello show, interfaccia operativa, API o formato progetto v10.
- La raccolta delle PR precedenti è un **registro retrospettivo**, non implica che quelle versioni fossero pubblicate come release GitHub.

## [0.9.0-beta.5] - 2026-10-28

**Versione attestata:** `VERSION.txt` portato a beta.5 nel commit `3b3a9db`.
**Evoluzione successiva sulla stessa beta:** fino al 31 ottobre sono state integrate ulteriori modifiche senza bump del file VERSION; sono tutte elencate nell'appendice PR, non retroattivamente attribuite a nuove versioni.

### Aggiunto / modificato
- Aggiornamento della documentazione beta, licenza applicativa unica, guida completa e percorsi di packaging (PR #48 e interventi correlati).
- Redesign Midnight Slate; Cuelist multiple/tab, note e Inspector a scomparsa; rinomina inline, Undo/Redo su editor supportati e compact monitor (PR #39–#47).
- Finestra Guide in-app, About con crediti, caratteri Hind/Exo 2 e simboli Noto espliciti, controlli per ultrawide, icone e dialoghi uniformati (PR #50–#72).
- Funzioni astratte e Default Map, MIDI/OSC Learn, override locali per cue, feedback lampeggiante, diagnostica e correzioni di compilazione (PR #73–#80).
- Riordino e rinomina inline di Cuelist e cue; pannello Control Mapping e archivi mappe indipendenti, sincronizzazione MIDI/OSC dei controlli fader/pan e correzioni Windows (PR #81–#88).
- **Limite:** alcune funzioni dell'editor e del mapping sono cambiate dopo il commit che assegnava beta.5; per lo stato preciso consultare il codice e i test dell'ultimo `main`, non una fotografia congelata al 28 ottobre.

### Test / rischio
- Modifiche verificate tramite le PR e le rispettive CI; compatibilità reale con hardware e tempi MIDI/OSC resta da collaudare su postazioni di spettacolo.

## [0.9.0-beta.4] - 2026-10-27

**Versione attestata:** commit `f2334c3`, dichiarato beta.4.
- Test e note di regressione per gain audio, retrigger e Inspector, con comportamento precedente preservato.
- La numerazione del **formato show v10** è distinta dalla versione dell'applicazione.

## [0.9.0-beta.3] - 2026-10-27

**Versione attestata:** commit `48cfa291`, dichiarato beta.3.
- Mixer più compatto, mute MIDI/OSC, attività LED e test del limiter condiviso, con copertura di regressione.

## [0.9.0-beta.2] - 2026-10-27

**Versione attestata:** commit `4efca83`, dichiarato beta.2.
- Routing Master/Direct indipendente e checklist di accettazione hardware esplicita.

## [0.9.0-beta.1] - 2026-10-27

**Versione attestata:** commit `ea1d52e`, dichiarato beta.1.
- Suite di regressione, gate di release e workflow manuale per il packaging portable separato dalla normale CI.
- Il superamento della CI non equivale a certificazione per un utilizzo live.

## [0.8.0-alpha.1] - 2026-10-27

**Versione attestata:** commit `129a22a`.
- Fase architettura/integrazioni: separazione di ciclo progetto, Inspector, operazioni su cue/celle, modello transazionale, layout e routing Control Mapping.
- Controllo più rigoroso del dispatch MIDI e politica di riconnessione/One-shot Learn, con test (PR #25–#33).
- Piano di accettazione hardware esplicitamente non completato.

## [0.7.0-alpha.1] - 2026-10-27

**Versione attestata:** commit `023bab77`.
- Inizio della fase di stabilizzazione: checkpoint di recupero, preflight, diagnostica, caricamento progetti più sicuro e correzione di warning/build, secondo cronologia e note beta.
- Versionamento centralizzato in `VERSION.txt`, senza confondere la versione del programma con lo schema JSON.

## Cronologia precedente (0.5 → sviluppo 0.6) - 2026-10-25 / 2026-10-26

**Nota:** `0.6` compare nelle denominazioni delle PR, ma non risulta un tag di release GitHub o una serie completa di versioni `VERSION.txt` da cui ricostruire patch/alpha numerici con certezza. Per questo **non vengono inventate versioni di patch**.

### Funzioni ricostruite dalle PR integrate
- Prime pipeline Windows e pacchetto portable, controlli di build e checksum (PR #1–#2); documentazione/licenza iniziale (PR #5).
- Editor clip integrato e flottante, modifica MIDI immediata, griglia beat e snap indipendenti (PR #6, #8).
- Editor OSC, messaggi UDP, automazioni tempo/beat, per-cella Auto Stop e mixer Master con gain/pan/meter (PR #9–#16).
- Control Mapping v10, MIDI Learn, preset e routing; test/fix compilazione e miglioramenti della UI (PR #17–#20).
- Le PR #4 e #7 **non furono integrate**; non si presentano come funzionalità consegnate.

## Appendice A — cronologia completa delle PR fino al 31 ottobre 2026

Questo indice registra **ogni PR integrata** visibile nella cronologia GitHub
al momento del bootstrap (PR #1–#88); una riga non indica necessariamente
una versione nuova o una release. Data = data del merge UTC, titolo = titolo
della PR originale. Le PR non integrate sono riportate separatamente.

- **2026-10-31 — [PR #88](https://github.com/Askeladd22/The-CUElist/pull/88):** Fix Control Mappings: in-place map selection, function names and Save/Cancel
- **2026-10-31 — [PR #87](https://github.com/Askeladd22/The-CUElist/pull/87):** Control Mappings: separate map archive, compact mapping list and contextual editor
- **2026-10-30 — [PR #86](https://github.com/Askeladd22/The-CUElist/pull/86):** Fix Windows C2248 compilation error in ControlRuntimeTests
- **2026-10-30 — [PR #85](https://github.com/Askeladd22/The-CUElist/pull/85):** Fix MIDI/OSC real-time fader and pan UI synchronization
- **2026-10-30 — [PR #84](https://github.com/Askeladd22/The-CUElist/pull/84):** Fix independent Control Maps and empty Control Column display
- **2026-10-30 — [PR #83](https://github.com/Askeladd22/The-CUElist/pull/83):** Document mapping rework and validate Windows regression suite
- **2026-10-30 — [PR #82](https://github.com/Askeladd22/The-CUElist/pull/82):** Inline rename and three-way reordering of Cuelists and cue rows
- **2026-10-29 — [PR #81](https://github.com/Askeladd22/The-CUElist/pull/81):** Polish header clock/CPU telemetry and compact cue action buttons
- **2026-10-29 — [PR #80](https://github.com/Askeladd22/The-CUElist/pull/80):** Fix MIDI Quick Learn yellow blinking when no MIDI input is connected
- **2026-10-29 — [PR #79](https://github.com/Askeladd22/The-CUElist/pull/79):** Fix OSC Timeline layout and add MIDI/OSC Learn blinking feedback
- **2026-10-29 — [PR #78](https://github.com/Askeladd22/The-CUElist/pull/78):** Fix MIDI/OSC Learn: clickable GUI parameters and persistent multi-binding mode
- **2026-10-29 — [PR #77](https://github.com/Askeladd22/The-CUElist/pull/77):** Rework Control Mapping: direct MIDI/OSC Learn, advanced functions and cue overrides
- **2026-10-29 — [PR #76](https://github.com/Askeladd22/The-CUElist/pull/76):** Fix Windows compilation: remove duplicate Control Mapping implementation from PR #74
- **2026-10-29 — [PR #75](https://github.com/Askeladd22/The-CUElist/pull/75):** Fix OSC Control Learn float saturation for extreme finite values
- **2026-10-29 — [PR #74](https://github.com/Askeladd22/The-CUElist/pull/74):** Control Mapping: stable functions and cue-local physical overrides (work in progress)
- **2026-10-29 — [PR #73](https://github.com/Askeladd22/The-CUElist/pull/73):** Introduce abstract control functions, Default Map inheritance and cue-local input overrides
- **2026-10-29 — [PR #72](https://github.com/Askeladd22/The-CUElist/pull/72):** Fix false blank Noto glyph failure by finalizing JUCE Graphics before pixel inspection
- **2026-10-29 — [PR #71](https://github.com/Askeladd22/The-CUElist/pull/71):** Fix MSVC C2664 in Noto UI regression tests
- **2026-10-29 — [PR #70](https://github.com/Askeladd22/The-CUElist/pull/70):** Fix blank Noto transport glyphs and failing 0.9 UI regression
- **2026-10-29 — [PR #69](https://github.com/Askeladd22/The-CUElist/pull/69):** Unify and soften icon-button frames across the operator UI
- **2026-10-29 — [PR #68](https://github.com/Askeladd22/The-CUElist/pull/68):** Correct ultrawide test for intentionally off-screen collapsed Inspector
- **2026-10-29 — [PR #67](https://github.com/Askeladd22/The-CUElist/pull/67):** Diagnose and resolve 32:9 ultrawide workspace test regression
- **2026-10-29 — [PR #66](https://github.com/Askeladd22/The-CUElist/pull/66):** Pin all Cue List column headers and centre Noto Mute glyphs
- **2026-10-29 — [PR #65](https://github.com/Askeladd22/The-CUElist/pull/65):** Fix disappearing Audio/MIDI/OSC headers during cue scrolling
- **2026-10-29 — [PR #64](https://github.com/Askeladd22/The-CUElist/pull/64):** Audit Unicode UI icons and use explicit Noto Sans Symbols 2 glyphs
- **2026-10-29 — [PR #63](https://github.com/Askeladd22/The-CUElist/pull/63):** Eliminate the 9px Cue List / BPM gutter at its source
- **2026-10-29 — [PR #62](https://github.com/Askeladd22/The-CUElist/pull/62):** Fix Control Mapping test race from asynchronous ComboBox notifications
- **2026-10-29 — [PR #61](https://github.com/Askeladd22/The-CUElist/pull/61):** Fix ControlsV3 test compilation after shared editor refactor
- **2026-10-29 — [PR #60](https://github.com/Askeladd22/The-CUElist/pull/60):** Unify cue editor dialogs and Control Mapping; polish reports and BPM seam
- **2026-10-29 — [PR #59](https://github.com/Askeladd22/The-CUElist/pull/59):** Polish Diagnostics/About dialogs, Noto fader menu, Inspector grip and cue divider
- **2026-10-29 — [PR #58](https://github.com/Askeladd22/The-CUElist/pull/58):** Fix Auto Stop and Play/Stop regression tests after visual polish
- **2026-10-29 — [PR #57](https://github.com/Askeladd22/The-CUElist/pull/57):** Polish cue actions, telemetry, faders and explicit Noto font rendering
- **2026-10-29 — [PR #56](https://github.com/Askeladd22/The-CUElist/pull/56):** Fix stale Collapse-all layout assertion after cue header resize
- **2026-10-29 — [PR #55](https://github.com/Askeladd22/The-CUElist/pull/55):** Fix pinned cue headers, transport spacing and Expand all tooltip
- **2026-10-29 — [PR #54](https://github.com/Askeladd22/The-CUElist/pull/54):** Fix Unicode symbols using embedded Noto font fallback
- **2026-10-28 — [PR #53](https://github.com/Askeladd22/The-CUElist/pull/53):** feat(help): native in-app Guide viewer and consistent menu labels
- **2026-10-28 — [PR #52](https://github.com/Askeladd22/The-CUElist/pull/52):** fix(build): restore JUCE 8 Windows compilation for Exo 2 / Hind typography
- **2026-10-28 — [PR #51](https://github.com/Askeladd22/The-CUElist/pull/51):** design(type): Exo 2 for small text (min 10pt), Hind for headings
- **2026-10-28 — [PR #50](https://github.com/Askeladd22/The-CUElist/pull/50):** feat(ui): Help/About, co-author credits and compact Inspector panel icons
- **2026-10-28 — [PR #49](https://github.com/Askeladd22/The-CUElist/pull/49):** fix(tests): make Midnight Slate popup checks renderer-independent
- **2026-10-28 — [PR #48](https://github.com/Askeladd22/The-CUElist/pull/48):** Docs/refresh beta5 and license
- **2026-10-28 — [PR #47](https://github.com/Askeladd22/The-CUElist/pull/47):** design(ui): clean Cuelist heading and improve left sidebar spacing
- **2026-10-28 — [PR #46](https://github.com/Askeladd22/The-CUElist/pull/46):** feat(ui): bidirectional Cuelist renaming, Undo/Redo and compact signal monitor
- **2026-10-28 — [PR #45](https://github.com/Askeladd22/The-CUElist/pull/45):** fix(cuelists): inline responsive tabs, delete action, numbering and popup test
- **2026-10-28 — [PR #44](https://github.com/Askeladd22/The-CUElist/pull/44):** design(ui): consistent Midnight Slate dropdown and context menus
- **2026-10-28 — [PR #43](https://github.com/Askeladd22/The-CUElist/pull/43):** fix(inspector): Escape from cue notes reaches global Panic
- **2026-10-28 — [PR #42](https://github.com/Askeladd22/The-CUElist/pull/42):** design(theme): balanced Midnight Slate palette between both UI references
- **2026-10-28 — [PR #41](https://github.com/Askeladd22/The-CUElist/pull/41):** fix(windows): Inspector collapse test build and unused variable
- **2026-10-28 — [PR #40](https://github.com/Askeladd22/The-CUElist/pull/40):** feat(ui): Inspector collapsible to the right via persistent side tab
- **2026-10-28 — [PR #39](https://github.com/Askeladd22/The-CUElist/pull/39):** feat(gui): Cuelist tabs, cue notepad, collapsible columns and live system panels
- **2026-10-27 — [PR #38](https://github.com/Askeladd22/The-CUElist/pull/38):** Fix stale expanded-dock regression after gap-free Timeline layout
- **2026-10-27 — [PR #37](https://github.com/Askeladd22/The-CUElist/pull/37):** Paint collapsed MIDI OSC dock region as panel to avoid a black Timeli…
- **2026-10-27 — [PR #36](https://github.com/Askeladd22/The-CUElist/pull/36):** Fix Timeline minimum-height clipping and remaining workspace gutters
- **2026-10-27 — [PR #35](https://github.com/Askeladd22/The-CUElist/pull/35):** Fix layout regressions: full-width lower editors, restored columns, resizable Timeline
- **2026-10-27 — [PR #34](https://github.com/Askeladd22/The-CUElist/pull/34):** UI density, pinned cue headers/metrics, selectable audio fader range
- **2026-10-27 — [PR #33](https://github.com/Askeladd22/The-CUElist/pull/33):** Refactor phase 4c: deterministic MIDI reconnect and one-shot Learn session policy
- **2026-10-27 — [PR #32](https://github.com/Askeladd22/The-CUElist/pull/32):** Refactor phase 4b: isolate physical MIDI dispatch decisions and output target lookup
- **2026-10-27 — [PR #31](https://github.com/Askeladd22/The-CUElist/pull/31):** Refactor phase 4a: separate Control Mapping runtime from Control Panel UI
- **2026-10-27 — [PR #30](https://github.com/Askeladd22/The-CUElist/pull/30):** Refactor phase 3b: responsive docked editors on compact windows
- **2026-10-27 — [PR #29](https://github.com/Askeladd22/The-CUElist/pull/29):** Refactor phase 3a: centralize deterministic workspace layout geometry
- **2026-10-27 — [PR #28](https://github.com/Askeladd22/The-CUElist/pull/28):** Refactor phase 2c: transactional cue/column deletion and mapping cleanup
- **2026-10-27 — [PR #27](https://github.com/Askeladd22/The-CUElist/pull/27):** Refactor phase 2b: pure transactional show-model operations
- **2026-10-27 — [PR #26](https://github.com/Askeladd22/The-CUElist/pull/26):** Refactor phase 2a: extract cue and cell editing operations
- **2026-10-27 — [PR #25](https://github.com/Askeladd22/The-CUElist/pull/25):** Refactor phase 1: separate Inspector and project lifecycle from MainComponent
- **2026-10-27 — [PR #24](https://github.com/Askeladd22/The-CUElist/pull/24):** Tighten cue grid margins and Inspector alignment; edit cell Gain as dB text
- **2026-10-27 — [PR #23](https://github.com/Askeladd22/The-CUElist/pull/23):** Remove redundant Cue List heading
- **2026-10-27 — [PR #22](https://github.com/Askeladd22/The-CUElist/pull/22):** Fix crooked Loop Playback glyph alignment
- **2026-10-27 — [PR #21](https://github.com/Askeladd22/The-CUElist/pull/21):** UI/usability polish: project title, contextual actions, activity LEDs and lock icon
- **2026-10-26 — [PR #20](https://github.com/Askeladd22/The-CUElist/pull/20):** Feat/master fader osc timeline 20261026
- **2026-10-26 — [PR #19](https://github.com/Askeladd22/The-CUElist/pull/19):** Control Mapping v10 — conferma GO e icona approvata The Cuelist
- **2026-10-26 — [PR #18](https://github.com/Askeladd22/The-CUElist/pull/18):** Fix Windows compilation: invalid break in ControlUI.cpp
- **2026-10-26 — [PR #17](https://github.com/Askeladd22/The-CUElist/pull/17):** v0.6 — Control & Mapping v10: MIDI Learn, preset cue e routing + icona
- **2026-10-26 — [PR #16](https://github.com/Askeladd22/The-CUElist/pull/16):** Feat/v06 osc automation editor
- **2026-10-26 — [PR #15](https://github.com/Askeladd22/The-CUElist/pull/15):** v0.6 OSC Clip Editor v0.2 — timeline, automazioni e Stop/Restart
- **2026-10-26 — [PR #14](https://github.com/Askeladd22/The-CUElist/pull/14):** v0.6 — GainMeterFader stereo integrato con scala dB comune
- **2026-10-26 — [PR #13](https://github.com/Askeladd22/The-CUElist/pull/13):** v0.6 UX polish — Auto Stop compatto, azioni cue, pan e nomi automatici
- **2026-10-26 — [PR #12](https://github.com/Askeladd22/The-CUElist/pull/12):** v0.6 Fase 6 — revisione UX: celle, Auto Stop, timeline, OSC e mixer
- **2026-10-26 — [PR #11](https://github.com/Askeladd22/The-CUElist/pull/11):** v0.6 Fase 5 — mixer compatto, pan rotativo, meter e Master audio
- **2026-10-26 — [PR #10](https://github.com/Askeladd22/The-CUElist/pull/10):** v0.6 Fase 4 — Auto Stop 2.0 per cella audio, MIDI e OSC
- **2026-10-26 — [PR #9](https://github.com/Askeladd22/The-CUElist/pull/9):** v0.6 Fase 3 — OSC UDP, editor secondi/beat e formato show v6
- **2026-10-26 — [PR #8](https://github.com/Askeladd22/The-CUElist/pull/8):** v0.6 Fase 2: griglia musicale e snap indipendenti nella timeline
- **2026-10-26 — [PR #6](https://github.com/Askeladd22/The-CUElist/pull/6):** v0.6 — Fase 1: Clip Editor integrato, flottante e editing MIDI immediato
- **2026-10-26 — [PR #5](https://github.com/Askeladd22/The-CUElist/pull/5):** Documentazione essenziale e licenza proprietaria perpetua
- **2026-10-26 — [PR #3](https://github.com/Askeladd22/The-CUElist/pull/3):** Correzioni immediate: griglia BPM, snap e selezione multipla MIDI
- **2026-10-25 — [PR #2](https://github.com/Askeladd22/The-CUElist/pull/2):** ci: stabilizzare il pacchetto portable Windows x64
- **2026-10-25 — [PR #1](https://github.com/Askeladd22/The-CUElist/pull/1):** ci: build and verify distributable Windows portable ZIP

### PR non integrate, escluse dalle funzionalità consegnate

- [PR #7](https://github.com/Askeladd22/The-CUElist/pull/7) — The Cuelist 0.6 Fase 1 — Clip Editor integrato e modifiche MIDI immediate (**non integrata**)
- [PR #4](https://github.com/Askeladd22/The-CUElist/pull/4) — Fix Windows portable CI and provide one-extraction download (**non integrata**)
