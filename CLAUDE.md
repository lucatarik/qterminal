# QTerminal + menu contestuale + tmux

Patch su QTerminal **2.4.0** (tag upstream, non master: master richiede un qtermwidget più nuovo di quello di sistema, 2.4.0).
Qui c'è solo la patch: `qterminal-tmux-contextmenu.patch` (formato `-p1`, relativa all'albero upstream).
Specifica del menu contestuale: `context_menu_functions.md` (la fonte di verità per etichette e regole).

## Ricostruire

```
git clone --depth 1 --branch 2.4.0 https://github.com/lxqt/qterminal.git src
patch -p1 -d src < qterminal-tmux-contextmenu.patch
cmake -S src -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   # serve lxqt-build-tools (pacman)
cmake --build build
QT_QPA_PLATFORM=offscreen build/test/qterminal_test          # 9 test
sudo cmake --install build                                    # -> /usr/local
```

Per aggiornare la patch: modifica `src/`, poi `diff -ruN` contro un clone pulito del tag 2.4.0.

## Cosa aggiunge

- `src/terminalcontext.{h,cpp}`: helper puri sul testo (togli a capo, pulisci comando, percorso sotto il cursore, percorso assoluto, URL sicuri, intervallo parola). Testati in `test/`.
- `src/termwidget.cpp`: menu contestuale. Cella, link (hotspot), riga e percorso sono letti all'apertura. Il tasto destro è intercettato in `TermWidget::eventFilter` (apre il menu anche col mouse tracking di tmux e col primo click che dà il focus), il menu si apre in modo sincrono.
- `src/tmuxmanager.{h,cpp}`: ogni terminale = una sessione tmux su socket privato `-L qterminal`, config generata in `~/.local/share/qterminal/tmux/qterminal.conf`. Salvataggio con tmux-resurrect ogni `TmuxSaveIntervalMinutes` (3) e all'uscita; restore all'avvio; layout dei tab in `layout.json` (`TabWidget::layout/restoreTmuxTabs`, `TermWidgetHolder::layout/applyLayout`).
- `src/tabbar.cpp`: pulsante "+" dopo l'ultima tab.
- Menu **Tmux** nella barra (attiva, salva ora, ripristina). Flag `-T/--no-tmux` (solo per quell'esecuzione).

## Invarianti da non rompere

- Nessun tmux con `-e` (`hasCommand()`): processo unico, senza detach. Chiudere tab/pannello fa `kill-session`; uscire dall'app (ultima finestra, `setQuitting`) conserva le sessioni.
- La directory corrente sotto tmux è `TermWidgetImpl::currentDirectory()` (non `workingDirectory()`, che è quella del client tmux). Il controllo "processo in esecuzione" è `hasForegroundJob()`.
- Le voci nuove del menu sono in italiano letterale (non `tr()`); le voci esistenti non cambiano.
- Il salvataggio è pilotato da QTerminal, non da tmux-continuum: il suo autosave passa dalla status line, che qui è `off`.
- tmux mouse è `off` di default (`TmuxMouse=false`) con `smcup@:rmcup@`: con `on` il trascinamento passa a tmux e al rilascio deseleziona.
- tmux non entra mai in copy mode per lo scrollback. Sotto tmux la scrollbar nativa è spenta e al suo posto c'è `TmuxScrollBar` (stesso lato di `ScrollbarPosition`). Scorrendo indietro (rotella con `TmuxMouse=false`, Shift+PagSu/Giù, trascinamento) legge la cronologia una volta con `capture-pane -p -e -S - -E -` (+ `display-message` con lo stato, nella stessa chiamata) e la scrive in una `QTermWidget` di sola lettura sopra il terminale vivo (`startTerminalTeletype` + scrittura non bloccante su `getPtySlaveFd`, mai bloccante: il terminale legge nello stesso thread). Griglia, font e margini uguali al terminale vivo, altrimenti la vista non si apre. Valore barra = riga in alto (0 = la più vecchia) = `history_size - scroll_position`; durante lo scorrimento non parte nessun comando tmux. In fondo, a un tasto, al resize, allo zoom e a `propertiesChanged` la vista si chiude. Ctrl+rotella = zoom (sempre, solo sul terminale vivo).
- Con alternate screen o mouse tracking (opencode, codex) la barra è disattivata (range 0) e la rotella va al programma (`TmuxManager::scrollProgram`: SGR `\e[<64;c;rM` se `mouse_any_flag`+`mouse_sgr_flag`, altrimenti Su/Giù, PagSu/Giù per Shift+Pag). Con `TmuxMouse=false` la config generata forza `mouse off` anche dopo `~/.tmux.conf`.
- Copia (`TermWidget::copySelection`) prende la selezione della vista se aperta e non vuota. La vista è un `TermWidgetImpl` (secondo costruttore, senza shell né tmux) e il suo tasto destro/centrale è intercettato in `TmuxScrollBar::eventFilter`: stesso menu del terminale vivo, ma incolla e directory corrente vanno al terminale vivo (`m_live`).
- La selezione fatta via `setSelectionStart/End` non ridisegna da sola: `selectCells` forza `scrollBarPositionChanged` + `updateImage` sul display.

## Limiti noti

- qtermwidget 2.4.0 non ha shell integration né OSC 52: "Seleziona comando" usa la sola riga cliccata; la copia di tmux passa da `copy-command` (wl-copy/xclip/xsel).
- Caratteri larghi (CJK) sfasano colonne e testo.
- Il plugin tmux-resurrect viene clonato da GitHub al primo avvio (serve rete e `git`).
- Vista di cronologia: l'apertura costa ~200 ms con 50000 righe (una lettura di tmux).
- Chiavi in `qterminal.ini`: `TmuxEnabled`, `TmuxMouse`, `TmuxSaveIntervalMinutes`, `OpenFileLinks` (link `file://`, default no).
