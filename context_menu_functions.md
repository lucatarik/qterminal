# Menu contestuale del tasto destro: descrizione delle funzionalità

## 1. Quando si apre

- Si apre con il click destro sul testo del terminale, nel punto del click.
- Il click destro porta anche il focus sul pannello cliccato e, se la finestra non aveva il focus, **lo dà e apre il
  menu con lo stesso click**. Il click destro non va mai scartato come "click che serve solo a dare il focus"
- Si chiude con Esc, con un click fuori dal menu o dopo aver scelto una voce.
- Quando si apre, il programma memorizza cosa c'era sotto il puntatore (la cella, un eventuale link, un eventuale
  percorso, l'eventuale selezione) e le voci agiscono su quello, non su dove si trova il mouse dopo.

## 2. Cosa mostra

Le voci di testo stanno in cima, poi una linea di separazione, poi le voci già presenti nel menu del terminale (dividi
pannello, zoom, chiudi pannello e così via). Ogni voce compare solo se ha senso:

| Voce | Compare quando |
|---|---|
| Apri link nel browser Brave | il click è sopra un indirizzo web |
| Incolla senza a capo | sempre |
| Copia percorso | il click è sopra un percorso di file o di cartella |
| Apri percorso nel file manager | il click è sopra un percorso di file o di cartella |
| Apri questo percorso nel terminale | il click è sopra un percorso che esiste |
| Seleziona parola e copia testo | il click è su una cella del terminale |
| Seleziona riga e copia testo | il click è su una cella del terminale |
| Seleziona comando e copia testo | il click è su una cella del terminale |

## 3. Cosa fa ogni voce

**Apri link nel browser Brave.** Apre l'indirizzo con il browser Brave (comando `brave` con l'indirizzo come argomento),
non con il browser predefinito di sistema. Si accettano solo schemi sicuri (web e posta; i file locali solo se
l'utente li ha abilitati), perché un programma remoto può stampare collegamenti con schemi arbitrari.

**Incolla senza a capo.** Come Incolla, ma prima toglie dal testo tutti i ritorni a capo: a capo, carriage return e
anche i separatori di riga Unicode (U+2028 e U+2029). Le righe vengono unite senza aggiungere spazi. Serve per
incollare comandi copiati su più righe senza che la shell li esegua a metà.

**Copia percorso.** Mette negli appunti il percorso **assoluto**. Un percorso che comincia con `~/` viene espanso con la
cartella home; un percorso relativo viene unito alla cartella corrente della shell (se il terminale la conosce).

**Apri percorso nel file manager.** Apre il file manager del sistema: se il percorso è una cartella apre quella, se è un
file apre la cartella che lo contiene. Il file manager va avviato "staccato", senza aspettare che termini: se il
terminale resta in attesa del processo figlio, si blocca finché il file manager non esce.

**Apri questo percorso nel terminale.** Apre una nuova scheda il cui terminale parte nella cartella del percorso: se è
una cartella, quella; se è un file, la cartella che lo contiene. Compare solo se il percorso esiste. Il preset dei
terminali (split) non si applica: la scheda ha un solo terminale.

**Seleziona parola e copia testo.** Seleziona la parola sotto il puntatore (con le stesse regole del doppio click del
terminale, compresi indirizzi, email e simili) e la copia. La parola resta evidenziata.

**Seleziona riga e copia testo.** Seleziona l'intera riga cliccata (come il triplo click) e la copia. La riga resta
evidenziata.

**Seleziona comando e copia testo.** Copia il comando che si trova sulla riga cliccata, **dall'inizio del comando e non
dall'inizio della riga**. Regole:

- Si parte dalla riga cliccata e si toglie tutto ciò che precede il comando: spazi iniziali, un eventuale numero di
  riga (per esempio quello di un elenco numerato), un eventuale marcatore di differenza (`+` o `-`) e un eventuale
  simbolo di prompt (`$`, `#`, `>`, `❯`). Esempio: `      40 +sudo sv status cloudflared` diventa
  `sudo sv status cloudflared`.
- Si tolgono gli spazi in fondo e tutti i ritorni a capo e ritorni carrello.
- Se il terminale ha l'integrazione con la shell (segnala dove inizia ogni prompt e quale comando è stato scritto) e la
  riga cliccata fa parte di quel comando che è andato a capo su più righe, si usa il comando intero riportato dalla
  shell, unito in una sola riga. Attenzione: le righe di **output** sotto un prompt non devono essere scambiate per il
  comando di quel prompt; il comando intero si usa solo se la riga cliccata ne è davvero una parte.
- Senza integrazione con la shell basta la sola riga cliccata, ripulita come sopra.
- La riga resta evidenziata come feedback; negli appunti va solo il comando pulito.

## 4. Come si riconoscono link e percorsi sotto il puntatore

- Gli indirizzi web e i percorsi vengono riconosciuti nel testo visibile (e i link dichiarati dal programma sono già
  associati alle celle). Al click destro si controlla se la cella cliccata cade dentro uno di questi elementi, tenendo
  conto dell'eventuale scorrimento nello storico.
- Un percorso è un testo che comincia con `/`, `./`, `../` o `~/` (anche con suffisso `:riga` o `:riga:colonna`, che
  non fa parte del percorso).
- **Il simbolo di prompt attaccato a un percorso non fa parte del percorso.** Esempio: in un prompt come
  `~/progetti/app$ ` il percorso riconosciuto non deve includere il `$` finale; si tolgono dal fondo `$`, `#` e `>`.
- Il percorso e l'indirizzo vanno determinati al momento del click destro, non dopo.

## 5. Comportamenti da rispettare

- Tutte le voci che copiano usano gli appunti di sistema (Ctrl+V negli altri programmi). Le voci "Seleziona … e copia"
  lasciano anche la selezione evidenziata nel terminale.
- Nessuna voce deve bloccare il terminale: aprire il browser o il file manager è un'operazione staccata.
- Le voci già esistenti del menu del terminale non cambiano.
- Il menu non deve mostrare voci che non hanno senso nel punto cliccato (per esempio "Copia link" fuori dai link).

## 6. Dettagli utili da non dimenticare

- Il primo click destro dopo che la finestra aveva perso il focus deve funzionare (non ignorarlo come click di focus).
- Il click destro non deve dipendere da un'informazione "l'interfaccia grafica sta usando il mouse" che può essere
  rimasta vecchia dopo un cambio di focus: se non è visibile nessuna interfaccia sopra il terminale, il click destro va
  gestito e basta.
- Le etichette del menu sono in italiano e vanno mantenute così: Copia testo, Copia link, Apri link nel browser Brave,
  Incolla, Incolla senza a capo, Copia percorso, Apri percorso nel file manager, Apri questo percorso nel terminale, Seleziona parola e copia testo,
  Seleziona riga e copia testo, Seleziona comando e copia testo.
