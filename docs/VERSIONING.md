# The Cuelist — versione e changelog obbligatori

## Fonti ufficiali

- **`VERSION.txt` nella radice:** unica fonte della versione dell'applicazione.
  CMake la legge, la valida rispetto al `project(... VERSION 0.9.0)`, la
  passa a `THE_CUELIST_VERSION` e quindi ad About/eseguibile e
  agli script di packaging. **Non creare un secondo file di versione.**
- **`CHANGELOG.md` nella radice:** documento **cumulativo, non sovrascritto**
  che registra ogni modifica tecnica, di UI, operativa, documentale o di
  packaging, dalla versione più recente alla più vecchia.
- Lo **schema JSON show v10** riguarda il formato `.cuelist`, non la
  versione dell'applicazione e non va incrementato automaticamente.
- Il numero CMake `project VERSION` descrive la base numerica
  `0.9.0`; il suffisso prerelease deriva da `VERSION.txt`.
  Aggiornare anche CMake soltanto quando cambia la terna major/minor/patch.

## Procedura per OGNI update

1. Partire dall'ultimo `main` e leggere `VERSION.txt`.
2. Incrementare la versione in senso **strettamente crescente** secondo
   SemVer (es. `0.9.0-beta.6` → `0.9.0-beta.7`). Non riusare
   lo stesso numero dopo una modifica, anche se piccola.
3. Aggiungere **in testa** a `CHANGELOG.md` la sezione
   `## [0.9.0-beta.7] - YYYY-MM-DD`, con le modifiche effettive:
   Aggiunto / Modificato / Corretto / Rimosso, test e rischi aperti.
   Omettere le categorie vuote, ma non il risultato delle verifiche.
4. Indicare numero/link della PR e problemi rilevati; non attribuire
   alla CI verifiche manuali con apparecchi reali che non sono state eseguite.
5. Confrontare la versione del progetto precedente e quella finale.
   Se due PR parallele scelgono lo stesso numero, ribasare la seconda
   su `main` e assegnare una versione nuova prima del merge.
6. Verificare CMake, Windows CI e CTest. Per versioni stabili è
   necessaria l'approvazione dei gate hardware e delle licenze; il bump
   di versione non attesta una release certificata.

### Controllo automatico

```powershell
python scripts/check_versioning.py
python scripts/test_versioning.py
```

GitHub Actions esegue il controllo di entrambi i file su ogni PR, e
sui push a `main`: presenza di `VERSION.txt` e `CHANGELOG.md` nel
diff, versione strettamente maggiore di quella base, prima sezione
del changelog identica alla nuova versione, formato e ordinamento
validi. La PR non supera la verifica se si dimentica il registro.
Un job CI non è un vincolo di branch protection: per bloccare il merge
anche tramite interfaccia GitHub, impostare il controllo come
**Required status check** nelle regole di protezione di `main`.

### Versione iniziale del changelog

La baseline `0.9.0-beta.6` del 31 ottobre 2026 è un aggiornamento
**documentale e procedurale** sopra `0.9.0-beta.5`, effettivamente
presente in `VERSION.txt`. Le versioni 0.7/0.8/0.9 precedenti
sono attestate dai commit che modificavano `VERSION.txt` e dal titolo
delle PR; la fase **0.6** è ricostruita dai PR integrati ma non si
finge una release pubblica autonoma. Tutte le 86 PR integrate (su 88
rilevate prima della baseline) sono elencate nell'appendice del
changelog; le 2 non integrate sono escluse.

I test Windows richiedono JUCE/Visual Studio su Windows x64. Il
controllo del changelog non esegue build, ma non la sostituisce.

## Template per update

```markdown
## [0.9.0-beta.7] - YYYY-MM-DD

**Stato:** aggiornamento beta, non release stabile.

### Aggiunto
- Funzionalità realmente introdotta (PR #...).

### Modificato
- ...

### Corretto
- ...

### Rimosso
- ...

### Test e limitazioni
- Eseguiti: ...
- Da verificare su hardware reale: ...
```

**Non riscrivere e non cancellare le voci precedenti** per
presentare a posteriori una storia artificiale; eventuali rettifiche
vanno dichiarate nella nuova voce. La cronologia Git resta
il riferimento per i dettagli di ogni PR.
