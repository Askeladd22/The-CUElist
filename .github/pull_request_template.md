## Versione e changelog

- [ ] Ho incrementato `VERSION.txt` (SemVer maggiore di `main`).
- [ ] Ho aggiunto **in testa** la sezione corrispondente a `CHANGELOG.md`.
- [ ] Ho documentato aggiunte, modifiche, correzioni/rimozioni, test realmente eseguiti e limiti.
- [ ] Ho aggiornato CMake `project VERSION` **solo** se cambia major/minor/patch.
- [ ] Confermo che lo schema `.cuelist` v10 è invariato oppure documento espressamente la migrazione.

### Cosa cambia

### Verifiche eseguite (Windows CI / CTest / hardware reale)

### Problemi noti e verifiche ancora aperte

> Il controllo automatico `version-changelog.yml` rifiuta PR senza versione e registro aggiornati. Per obbligare anche il **merge**, impostare il job `Validate VERSION.txt and CHANGELOG.md` come Required status check nella branch protection di `main`.
