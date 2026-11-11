/***************************************************************************
 *   Copyright (C) 2010 by Petr Vanek                                      *
 *   petr@scribus.info                                                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include <QMenu>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QDesktopServices>
#include <QMessageBox>
#include <QAbstractButton>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QScrollBar>
#include <QStandardPaths>
#include <QSocketNotifier>
#include <QStyle>
#include <QTimer>
#include <QUrl>
#include <cassert>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

#ifdef HAVE_QDBUS
    #include <QtDBus/QtDBus>
    #include "termwidgetholder.h"
    #include "terminaladaptor.h"
#endif

#ifdef HAVE_LIBCANBERRA
    #include <canberra.h>
#endif

#include "mainwindow.h"
#include "termwidget.h"
#include "config.h"
#include "properties.h"
#include "qterminalapp.h"
#include "terminalcontext.h"
#include "tmuxmanager.h"

static int TermWidgetCount = 0;


TermWidgetImpl::TermWidgetImpl(TerminalConfig &cfg, QWidget * parent)
    : QTermWidget(0, parent)
#ifdef HAVE_LIBCANBERRA
    , libcanberra_context(nullptr)
#endif
{
    TermWidgetCount++;
    QString name(QStringLiteral("TermWidget_%1"));
    setObjectName(name.arg(TermWidgetCount));

    setFlowControlEnabled(FLOW_CONTROL_ENABLED);
    setFlowControlWarningEnabled(FLOW_CONTROL_WARNING_ENABLED);

    m_hasCommand = cfg.hasCommand();

    propertiesChanged();

    setWorkingDirectory(cfg.getWorkingDirectory());

    QStringList shell = cfg.getShell();
    if (!m_hasCommand && TmuxManager::instance()->isActive())
    {
        // every terminal shows a tmux session of its own: it outlives the terminal
        m_tmuxSession = cfg.tmuxSession().isEmpty() ? TmuxManager::instance()->newSessionName() : cfg.tmuxSession();
        shell = TmuxManager::instance()->clientCommand(m_tmuxSession, cfg.getWorkingDirectory(),
                                                       Properties::Instance()->shell);
    }
    if (!shell.isEmpty())
    {
        setShellProgram(shell.at(0));
        shell.removeAt(0);
        if (!shell.isEmpty())
            setArgs(shell);
    }

    setEnvironment(QStringList(QStringLiteral("TERM=%1").arg(Properties::Instance()->term)));

    setMotionAfterPasting(Properties::Instance()->m_motionAfterPaste);
    disableBracketedPasteMode(Properties::Instance()->m_disableBracketedPasteMode);

    setContextMenuPolicy(Qt::CustomContextMenu);

    if(Properties::Instance()->swapMouseButtons2and3)
    {
        connect(this, &QWidget::customContextMenuRequested,
                this, &TermWidgetImpl::pasteSelection);
    }
    else
    {
        connect(this, &QWidget::customContextMenuRequested,
                this, &TermWidgetImpl::customContextMenuCall);
    }

    connect(this, &QTermWidget::urlActivated, this, &TermWidgetImpl::activateUrl);
    connect(this, &QTermWidget::bell, this, &TermWidgetImpl::bell);

    startShellProgram();
}

TermWidgetImpl::~TermWidgetImpl()
{
    // closing a terminal ends its session, quitting the program keeps it
    if (!m_tmuxSession.isEmpty() && !TmuxManager::instance()->isQuitting())
        TmuxManager::instance()->killSession(m_tmuxSession);
#ifdef HAVE_LIBCANBERRA
    if (libcanberra_context) {
        ca_context_destroy (libcanberra_context);
    }
#endif
}

TermWidgetImpl::TermWidgetImpl(TermWidgetImpl *live, QWidget *parent)
    : QTermWidget(0, parent)
#ifdef HAVE_LIBCANBERRA
    , libcanberra_context(nullptr)
#endif
    , m_hasCommand(false)
    , m_live(live)
{
    setObjectName(live->objectName() + QStringLiteral("_history"));
    setFlowControlEnabled(false);
    setFlowControlWarningEnabled(false);
    propertiesChanged();
    setScrollBarPosition(QTermWidget::NoScrollBar); // the bar of the terminal drives this view
    // the menu is opened by TmuxScrollBar, which gets the click first
    setContextMenuPolicy(Qt::NoContextMenu);
    connect(this, &QTermWidget::urlActivated, this, &TermWidgetImpl::activateUrl);
}

void TermWidgetImpl::propertiesChanged()
{
    setMargin(Properties::Instance()->terminalMargin);
    setColorScheme(Properties::Instance()->colorScheme);
    setTerminalFont(Properties::Instance()->font);
    setMotionAfterPasting(Properties::Instance()->m_motionAfterPaste);
    disableBracketedPasteMode(Properties::Instance()->m_disableBracketedPasteMode);
    setConfirmMultilinePaste(Properties::Instance()->confirmMultilinePaste);
    setWordCharacters(Properties::Instance()->wordCharacters);
    autoHideMouseAfter(Properties::Instance()->mouseAutoHideDelay);
    setTrimPastedTrailingNewlines(Properties::Instance()->trimPastedTrailingNewlines);
    setTerminalSizeHint(Properties::Instance()->showTerminalSizeHint);

    if (Properties::Instance()->historyLimited)
    {
        setHistorySize(Properties::Instance()->historyLimitedTo);
    }
    else
    {
        // Unlimited history
        setHistorySize(-1);
    }

    setKeyBindings(Properties::Instance()->emulation);
    setTerminalOpacity(1.0 - Properties::Instance()->termTransparency/100.0);
    setTerminalBackgroundImage(Properties::Instance()->backgroundImage);
    setTerminalBackgroundMode(Properties::Instance()->backgroundMode);
    setBidiEnabled(Properties::Instance()->enabledBidiSupport);
    setDrawLineChars(!Properties::Instance()->useFontBoxDrawingChars);
    setBoldIntense(Properties::Instance()->boldIntense);

    /* be consequent with qtermwidget.h here */
    // under tmux the history is tmux's: TmuxScrollBar shows it instead
    switch(m_tmuxSession.isEmpty() ? Properties::Instance()->scrollBarPos : 0) {
    case 0:
        setScrollBarPosition(QTermWidget::NoScrollBar);
        break;
    case 1:
        setScrollBarPosition(QTermWidget::ScrollBarLeft);
        break;
    case 2:
    default:
        setScrollBarPosition(QTermWidget::ScrollBarRight);
        break;
    }

    switch(Properties::Instance()->keyboardCursorShape) {
    case 1:
        setKeyboardCursorShape(QTermWidget::KeyboardCursorShape::UnderlineCursor);
        break;
    case 2:
        setKeyboardCursorShape(QTermWidget::KeyboardCursorShape::IBeamCursor);
        break;
    default:
    case 0:
        setKeyboardCursorShape(QTermWidget::KeyboardCursorShape::BlockCursor);
        break;
    }

    setBlinkingCursor(Properties::Instance()->keyboardCursorBlink);

    update();
}

QString TermWidgetImpl::currentDirectory()
{
    if (m_live)
        return m_live->currentDirectory();
    if (!m_tmuxSession.isEmpty())
    {
        const QString path = TmuxManager::instance()->paneCurrentPath(m_tmuxSession);
        if (!path.isEmpty())
            return path;
    }
    return workingDirectory();
}

void TermWidgetImpl::clearTerminal()
{
    clear();
    // tmux owns the screen and would draw its content again
    if (!m_tmuxSession.isEmpty())
        TmuxManager::instance()->clearPane(m_tmuxSession);
}

bool TermWidgetImpl::hasForegroundJob()
{
    if (m_hasCommand)
        return true;
    if (!m_tmuxSession.isEmpty())
        return TmuxManager::instance()->sessionBusy(m_tmuxSession);
    return getForegroundProcessId() != getShellPID();
}

static QWidget *terminalDisplayOf(const QWidget *term)
{
    const auto kids = term->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget *kid : kids)
    {
        if (kid->inherits("Konsole::TerminalDisplay"))
            return kid;
    }
    return nullptr;
}

// The geometry follows Konsole::TerminalDisplay (calcGeometry, fontChange and
// getCharacterPosition), whose cell lookup is not exported.
bool TermWidgetImpl::cellAt(const QPoint &pos, int &row, int &column) const
{
    QWidget *display = terminalDisplayOf(this);
    if (!display)
        return false;
    TermWidgetImpl *self = const_cast<TermWidgetImpl*>(this);
    const QPoint p = display->mapFrom(self, pos);
    const QRect area = display->contentsRect();
    if (!area.contains(p))
        return false;

    static const char REPCHAR[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                  "abcdefghijklmnopqrstuvwxyz"
                                  "0123456789./+@";
    const QFontMetrics fm(display->font());
    const int cellHeight = qMax(1, fm.height());
    const int cellWidth = qMax(1, qRound(double(fm.horizontalAdvance(QLatin1String(REPCHAR))) / double(qstrlen(REPCHAR))));

    int leftMargin = getMargin();
    const auto bars = display->findChildren<QScrollBar*>(QString(), Qt::FindDirectChildrenOnly);
    for (QScrollBar *bar : bars)
    {
        const bool transient = bar->style()->styleHint(QStyle::SH_ScrollBar_Transient, nullptr, bar);
        if (!bar->isHidden() && !transient && bar->geometry().center().x() < display->width() / 2)
            leftMargin += bar->width();
    }

    row = qBound(0, (p.y() - area.top() - getMargin()) / cellHeight, qMax(0, self->screenLinesCount() - 1));
    column = qBound(0, (p.x() - area.left() - leftMargin) / cellWidth, qMax(0, self->screenColumnsCount() - 1));
    return true;
}

// Text of a screen line, counted from the oldest line of the history.
// One cell is one character: wide characters shift what follows them.
QString TermWidgetImpl::screenLine(int absoluteRow) const
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    const_cast<TermWidgetImpl*>(this)->saveHistory(&buffer);
    buffer.close();
    const QList<QByteArray> lines = bytes.split('\n');
    return absoluteRow >= 0 && absoluteRow < lines.size() ? QString::fromUtf8(lines.at(absoluteRow)) : QString();
}

// Everything the menu entries need is read here, when the menu opens: the
// entries must act on what was under the pointer then.
TermWidgetImpl::ContextTarget TermWidgetImpl::contextTargetAt(const QPoint &pos)
{
    ContextTarget target;
    if (!cellAt(pos, target.row, target.column))
        return target;
    target.hasCell = true;

    QWidget *display = terminalDisplayOf(this);
    int scrolled = 0;
    if (display)
    {
        const auto bars = display->findChildren<QScrollBar*>(QString(), Qt::FindDirectChildrenOnly);
        if (!bars.isEmpty())
            scrolled = bars.first()->value();
    }
    target.absoluteRow = scrolled + target.row;

    if (Konsole::Filter::HotSpot *spot = getHotSpotAt(target.row, target.column))
    {
        if (spot->type() == Konsole::Filter::HotSpot::Link)
        {
            if (auto *regexpSpot = dynamic_cast<Konsole::RegExpFilter::HotSpot*>(spot))
            {
                QString text = regexpSpot->capturedTexts().value(0);
                if (!text.contains(QLatin1String("://")))
                {
                    text.prepend(text.contains(QLatin1Char('@')) ? QStringLiteral("mailto:")
                                                                 : QStringLiteral("http://"));
                }
                target.url = QUrl(text, QUrl::StrictMode);
            }
        }
    }

    target.line = screenLine(target.absoluteRow);
    if (!target.url.isValid() || target.url.isEmpty())
    {
        const QString path = TerminalContext::pathAt(target.line, target.column);
        if (!path.isEmpty())
        {
            const QString absolute = TerminalContext::absolutePath(path, currentDirectory(), QDir::homePath());
            if (QDir::isAbsolutePath(absolute))
                target.path = absolute;
        }
    }
    return target;
}

void TermWidgetImpl::selectCells(int absoluteRow, int firstColumn, int lastColumn)
{
    setSelectionStart(absoluteRow, firstColumn);
    setSelectionEnd(absoluteRow, lastColumn);
    // The selection is set on the screen, behind the back of the screen window
    // that feeds the display: re-scrolling to the same line makes the window
    // read the screen again, then the display redraws.
    if (QWidget *display = terminalDisplayOf(this))
    {
        QMetaObject::invokeMethod(display, "scrollBarPositionChanged", Q_ARG(int, 0));
        QMetaObject::invokeMethod(display, "updateImage");
    }
}

void TermWidgetImpl::openUrlWithBrave(const QUrl &url)
{
    // a remote program can print links with any scheme
    if (!TerminalContext::isSafeUrl(url, Properties::Instance()->openFileLinks))
        return;
    QString brave = QStandardPaths::findExecutable(QStringLiteral("brave"));
    if (brave.isEmpty())
        brave = QStandardPaths::findExecutable(QStringLiteral("brave-browser"));
    if (brave.isEmpty())
    {
        QMessageBox::warning(this, QStringLiteral("Apri link nel browser Brave"),
                             QStringLiteral("Il browser Brave (comando \"brave\") non è stato trovato."));
        return;
    }
    QProcess::startDetached(brave, {url.toString(QUrl::FullyEncoded)});
}

void TermWidgetImpl::openInFileManager(const QString &path)
{
    // the folder itself, or the one that holds the file
    const QFileInfo info(path);
    const QString folder = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
    if (QFileInfo(folder).isDir())
        QDesktopServices::openUrl(QUrl::fromLocalFile(folder)); // does not wait for the file manager
}

void TermWidgetImpl::pasteWithoutNewlines()
{
    QString text = TerminalContext::stripLineBreaks(QApplication::clipboard()->text(QClipboard::Clipboard));
    if (text.isEmpty())
        return;
    // a view of the history has no program: what is pasted goes to the terminal it shows
    TermWidgetImpl *target = m_live ? m_live : this;
    target->bracketText(text); // keeps the bracketed paste mode, if the program asked for it
    target->sendText(text);
}

void TermWidgetImpl::customContextMenuCall(const QPoint & pos)
{
    auto mainWindow = findParent<MainWindow>(this);
    if (mainWindow == nullptr)
    {
        return;
    }
    QMenu menu(mainWindow);
    QMap<QString, QAction*> actions = mainWindow->leaseActions();

    const ContextTarget target = contextTargetAt(pos);

    QList<QAction*> extraActions = filterActions(pos);
    for (auto& action : extraActions)
    {
        menu.addAction(action);
    }

    // text entries; the labels are in Italian on purpose
    const QUrl url = target.url;
    if (url.isValid() && !url.isEmpty() && TerminalContext::isSafeUrl(url, Properties::Instance()->openFileLinks))
    {
        menu.addAction(QStringLiteral("Apri link nel browser Brave"), this, [this, url] { openUrlWithBrave(url); });
    }
    menu.addAction(QStringLiteral("Incolla senza a capo"), this, [this] { pasteWithoutNewlines(); });
    if (!target.path.isEmpty())
    {
        const QString path = target.path;
        menu.addAction(QStringLiteral("Copia percorso"), this,
                       [path] { QApplication::clipboard()->setText(path, QClipboard::Clipboard); });
        menu.addAction(QStringLiteral("Apri percorso nel file manager"), this,
                       [this, path] { openInFileManager(path); });
        // only a path that exists: a new terminal cannot start in one that does not
        if (QFileInfo::exists(path))
        {
            menu.addAction(QStringLiteral("Apri questo percorso nel terminale"), this,
                           [mainWindow, path] {
                               // the folder itself, or the one that holds the file
                               const QFileInfo info(path);
                               const QString folder = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
                               mainWindow->addTabIn(folder);
                           });
        }
    }
    if (target.hasCell)
    {
        const int row = target.absoluteRow;
        const int lastColumn = qMax(0, screenColumnsCount() - 1);
        const QString line = target.line;

        const QPair<int, int> word = TerminalContext::wordRangeAt(line, target.column, wordCharacters());
        if (word.first >= 0)
        {
            menu.addAction(QStringLiteral("Seleziona parola e copia testo"), this, [this, row, word, line] {
                selectCells(row, word.first, word.second);
                QApplication::clipboard()->setText(line.mid(word.first, word.second - word.first + 1), QClipboard::Clipboard);
            });
        }
        menu.addAction(QStringLiteral("Seleziona riga e copia testo"), this, [this, row, lastColumn, line] {
            selectCells(row, 0, lastColumn);
            QString text = TerminalContext::stripLineBreaks(line);
            while (text.endsWith(QLatin1Char(' ')))
                text.chop(1);
            QApplication::clipboard()->setText(text, QClipboard::Clipboard);
        });
        menu.addAction(QStringLiteral("Seleziona comando e copia testo"), this, [this, row, lastColumn, line] {
            selectCells(row, 0, lastColumn);
            QApplication::clipboard()->setText(TerminalContext::cleanCommand(line), QClipboard::Clipboard);
        });
    }

    if (!actions.isEmpty())
    {
        menu.addSeparator();
    }

    menu.addAction(actions[QStringLiteral(COPY_SELECTION)]);
    menu.addAction(actions[QStringLiteral(PASTE_CLIPBOARD)]);
    menu.addAction(actions[QStringLiteral(PASTE_SELECTION)]);
    menu.addAction(actions[QStringLiteral(ZOOM_IN)]);
    menu.addAction(actions[QStringLiteral(ZOOM_OUT)]);
    menu.addAction(actions[QStringLiteral(ZOOM_RESET)]);
    menu.addSeparator();
    menu.addAction(actions[QStringLiteral(CLEAR_TERMINAL)]);
    menu.addAction(actions[QStringLiteral(SPLIT_HORIZONTAL)]);
    menu.addAction(actions[QStringLiteral(SPLIT_VERTICAL)]);
    // warning TODO/FIXME: disable the action when there is only one terminal
    menu.addAction(actions[QStringLiteral(SUB_COLLAPSE)]);
    menu.addSeparator();
    menu.addAction(actions[QStringLiteral(TOGGLE_MENU)]);
    menu.addAction(actions[QStringLiteral(TOGGLE_BOOKMARKS)]);
    menu.addAction(actions[QStringLiteral(HIDE_WINDOW_BORDERS)]);
    menu.addAction(actions[QStringLiteral(PREFERENCES)]);

    // The disabled actions should be updated before showing the menu because
    // the "Actions" menu of the main window may have not been shown yet.
    mainWindow->updateDisabledActions();

    menu.exec(mapToGlobal(pos));
}

void TermWidgetImpl::zoomIn()
{
    QTermWidget::zoomIn();
// note: do not save zoom here due the #74 Zoom reset option resets font back to Monospace
//    Properties::Instance()->font = getTerminalFont();
//    Properties::Instance()->saveSettings();
}

void TermWidgetImpl::zoomOut()
{
    QTermWidget::zoomOut();
// note: do not save zoom here due the #74 Zoom reset option resets font back to Monospace
//    Properties::Instance()->font = getTerminalFont();
//    Properties::Instance()->saveSettings();
}

void TermWidgetImpl::zoomReset()
{
// note: do not save zoom here due the #74 Zoom reset option resets font back to Monospace
//    Properties::Instance()->font = Properties::Instance()->font;
    setTerminalFont(Properties::Instance()->font);
//    Properties::Instance()->saveSettings();
}

void TermWidgetImpl::activateUrl(const QUrl & url, bool fromContextMenu) {
    if (QApplication::keyboardModifiers() & Qt::ControlModifier || fromContextMenu) {
        QDesktopServices::openUrl(url);
    }
}

void TermWidgetImpl::bell() {
    if (Properties::Instance()->audibleBell) {
#ifdef HAVE_LIBCANBERRA
        if (!libcanberra_context) {
            ca_context_create (&libcanberra_context);
        }
        ca_context_play (libcanberra_context, 0,
                         CA_PROP_EVENT_ID, "bell-terminal",
                         NULL);
#else
        qWarning() << "Bell! But QTerminal is not built with libcanberra, so there's no sound.";
#endif
    }
}

bool TermWidget::eventFilter(QObject * obj, QEvent * ev)
{
    if (ev->type() == QEvent::MouseButtonRelease
        && static_cast<QMouseEvent*>(ev)->button() == Qt::RightButton
        && m_rightPressHandled)
    {
        // keep the release away from a program that tracks the mouse (tmux)
        m_rightPressHandled = false;
        return true;
    }
    if (ev->type() == QEvent::KeyPress && obj && obj->inherits("Konsole::TerminalDisplay")
        && !impl()->tmuxSession().isEmpty() && !Properties::Instance()->tmuxMouse)
    {
        // Shift+PageUp/PageDown: a page of tmux's history, like the wheel
        auto *key = static_cast<QKeyEvent*>(ev);
        if (key->modifiers() == Qt::ShiftModifier && (key->key() == Qt::Key_PageUp || key->key() == Qt::Key_PageDown))
        {
            const int page = qMax(1, impl()->screenLinesCount() - 1);
            const bool up = key->key() == Qt::Key_PageUp;
            if (!m_tmuxBar || !m_tmuxBar->scrollBy(up ? page : -page))
                TmuxManager::instance()->scrollProgram(impl()->tmuxSession(), up ? page : -page, 0, 0, true);
            return true;
        }
    }
    if (ev->type() == QEvent::KeyPress && m_tmuxBar && m_tmuxBar->scrollbackOpen())
    {
        // typing brings the cursor back to the prompt: the view of the history goes away
        // before the key reaches the shell
        auto *key = static_cast<QKeyEvent*>(ev);
        const bool modifierOnly = key->key() == Qt::Key_Shift || key->key() == Qt::Key_Control
                                  || key->key() == Qt::Key_Alt || key->key() == Qt::Key_Meta
                                  || key->key() == Qt::Key_AltGr || key->key() == Qt::Key_CapsLock;
        const bool shortcut = (key->modifiers() & Qt::ControlModifier) && (key->modifiers() & Qt::ShiftModifier);
        if (!modifierOnly && !shortcut)
            m_tmuxBar->closeScrollback();
    }
    if (ev->type() == QEvent::FontChange && m_tmuxBar && obj && obj->inherits("Konsole::TerminalDisplay"))
        m_tmuxBar->closeScrollback(); // zoom: the view has the old grid
    if (ev->type() == QEvent::Wheel && obj && obj->inherits("Konsole::TerminalDisplay")
        && (static_cast<QWheelEvent*>(ev)->modifiers() & Qt::ControlModifier))
    {
        // Ctrl+wheel zooms, like Ctrl++ and Ctrl+-
        const int delta = static_cast<QWheelEvent*>(ev)->angleDelta().y();
        if (delta > 0)
            impl()->zoomIn();
        else if (delta < 0)
            impl()->zoomOut();
        return true;
    }
    if (ev->type() == QEvent::Wheel && obj && obj->inherits("Konsole::TerminalDisplay")
        && !impl()->tmuxSession().isEmpty() && !Properties::Instance()->tmuxMouse)
    {
        // The history is tmux's, and tmux gets no mouse (to keep the native
        // selection): the wheel drives the view of the history, or the program that scrolls by itself.
        auto *wheel = static_cast<QWheelEvent*>(ev);
        m_wheelDelta += wheel->angleDelta().y();
        if (m_tmuxBar && !m_tmuxBar->programOwnsScroll())
        {
            // 3 lines for a notch, and the fractions of a notch of fine wheels add up
            const int lines = m_wheelDelta / 40;
            m_wheelDelta -= lines * 40;
            m_tmuxBar->scrollBy(lines);
        }
        else
        {
            const int notches = m_wheelDelta / 120;
            if (notches != 0)
            {
                m_wheelDelta -= notches * 120;
                int row = 0, column = 0;
                impl()->cellAt(impl()->mapFromGlobal(wheel->globalPosition().toPoint()), row, column);
                TmuxManager::instance()->scrollProgram(impl()->tmuxSession(), notches * 3, column, row);
                if (m_tmuxBar)
                    m_tmuxBar->refresh();
            }
        }
        return true;
    }
    if (ev->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent *mev = static_cast<QMouseEvent*>(ev);
        if (mev->button() == Qt::RightButton && obj && obj->isWidgetType()
            && obj->inherits("Konsole::TerminalDisplay"))
        {
            // The terminal itself hands a right click to the running program when
            // that tracks the mouse. The menu must open anyway, also on the click
            // that gives the focus to the window.
            m_rightPressHandled = true;
            const QPoint pos = static_cast<QWidget*>(obj)->mapTo(impl(), mev->position().toPoint());
            impl()->setFocus(Qt::MouseFocusReason);
            emit termGetFocus(this);
            // opened right here, as the terminal does: the click that closes the
            // menu must not be taken for the end of this one
            if (Properties::Instance()->swapMouseButtons2and3)
                impl()->pasteSelection();
            else
                impl()->customContextMenuCall(pos);
            m_rightPressHandled = false;
            return true;
        }
        if (mev->button() == Qt::MiddleButton)
        {
            if(Properties::Instance()->swapMouseButtons2and3)
            {
                impl()->customContextMenuCall(mev->pos());
            }
            else
            {
                impl()->pasteSelection();
            }
            return true;
        }
    }
    else if ((ev->type() == QEvent::MouseMove
              || ev->type() == QEvent::Enter
              || ev->type() == QEvent::HoverEnter)
             && Properties::Instance()->focusOnMoueOver)
    {
        impl()->setFocus();
    }
    return false;
}

namespace {
// ends what capture-pane wrote; what follows is the state of the pane
const QByteArray STATE_MARK = QByteArrayLiteral("\x01QTSTATE ");
const char STATE_FORMAT[] = "#{alternate_on} #{?mouse_any_flag,#{mouse_sgr_flag},0} #{pane_height} #{pane_in_mode}"
                            " #{history_size} #{?scroll_position,#{scroll_position},0}";
}

TmuxScrollBar::TmuxScrollBar(const QString &session, TermWidgetImpl *live, QWidget *host)
    : QScrollBar(Qt::Vertical, host)
    , m_session(session)
    , m_live(live)
    , m_host(host)
{
    setFocusPolicy(Qt::NoFocus);
    setRange(0, 0);
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &TmuxScrollBar::poll);
    m_readyTimer.setInterval(5);
    connect(&m_readyTimer, &QTimer::timeout, this, &TmuxScrollBar::waitReady);
    connect(this, &QScrollBar::valueChanged, this, [this](int value) {
        if (!m_internal)
            goTo(value);
    });
}

TmuxScrollBar::~TmuxScrollBar()
{
    closeScrollback();
}

void TmuxScrollBar::refresh()
{
    if (!isVisible() || m_view || m_capturing)
        return;
    m_dirty = true;
    if (!m_timer.isActive() && !m_process)
        m_timer.start(100);
}

void TmuxScrollBar::showEvent(QShowEvent *event)
{
    QScrollBar::showEvent(event);
    refresh();
}

// The bar counts from the oldest line, like the view does: the value is the line at the top.
void TmuxScrollBar::goTo(int value)
{
    if (m_view)
    {
        if (m_ready)
            m_viewBar->setValue(value);
        else
            m_target = value;
        return;
    }
    if (value >= maximum())
    {
        // back at the bottom, before the view came up
        m_target = -1;
        return;
    }
    m_target = value;
    if (m_capturing)
        return;
    // the history and the state of the pane, in one call: a program that took the screen a moment ago
    // must not get a view of the history over it
    if (m_process)
    {
        m_process->disconnect(this);
        m_process->kill();
        m_process->deleteLater();
        m_process = nullptr;
    }
    const QString target = QLatin1Char('=') + m_session + QLatin1Char(':');
    m_process = TmuxManager::instance()->start(
        {QStringLiteral("capture-pane"), QStringLiteral("-p"), QStringLiteral("-e"), QStringLiteral("-t"), target,
         QStringLiteral("-S"), QStringLiteral("-"), QStringLiteral("-E"), QStringLiteral("-"), QStringLiteral(";"),
         QStringLiteral("display-message"), QStringLiteral("-p"), QStringLiteral("-t"), target,
         QString::fromLatin1(STATE_MARK) + QLatin1String(STATE_FORMAT)}, this);
    if (!m_process)
    {
        m_target = -1;
        return;
    }
    m_capturing = true;
    QProcess *process = m_process;
    connect(process, &QProcess::finished, this, [this, process] {
        if (process == m_process)
            captureFinished();
    });
}

bool TmuxScrollBar::scrollBy(int lines)
{
    if (programOwnsScroll())
        return false;
    if (lines == 0)
        return true;
    const int base = m_view && m_ready ? m_viewBar->value() : (m_target >= 0 ? m_target : value());
    goTo(qBound(0, base - lines, qMax(maximum(), m_history)));
    return true;
}

void TmuxScrollBar::poll()
{
    if (m_process || m_view || m_capturing || !isVisible())
        return;
    m_dirty = false;
    const QString target = QLatin1Char('=') + m_session + QLatin1Char(':');
    m_process = TmuxManager::instance()->start(
        {QStringLiteral("display-message"), QStringLiteral("-p"), QStringLiteral("-t"), target,
         QLatin1String(STATE_FORMAT)}, this);
    if (!m_process)
        return;
    QProcess *process = m_process;
    connect(process, &QProcess::finished, this, [this, process] {
        if (process == m_process)
            pollFinished();
    });
}

void TmuxScrollBar::pollFinished()
{
    const QStringList f = QString::fromUtf8(m_process->readAllStandardOutput()).trimmed().split(QLatin1Char(' '));
    m_process->deleteLater();
    m_process = nullptr;

    if (f.size() == 6 && !m_view && !m_capturing)
    {
        m_alternate = f.at(0) == QLatin1String("1");
        m_mouseTracked = f.at(1) == QLatin1String("1");
        m_inMode = f.at(3) == QLatin1String("1");
        m_history = f.at(4).toInt();
        m_offset = qMin(f.at(5).toInt(), m_history);

        m_internal = true;
        setRange(0, programOwnsScroll() ? 0 : m_history);
        setPageStep(qMax(1, f.at(2).toInt()));
        setSingleStep(1);
        // not under the hand that drags it
        if (!isSliderDown() && m_target < 0)
            setValue(programOwnsScroll() ? 0 : m_history - m_offset);
        m_internal = false;
    }
    if (m_dirty && !m_timer.isActive())
        m_timer.start(100);
}

void TmuxScrollBar::captureFinished()
{
    const QByteArray out = m_process->readAllStandardOutput();
    m_process->deleteLater();
    m_process = nullptr;
    m_capturing = false;

    const qsizetype mark = out.lastIndexOf(STATE_MARK);
    if (mark < 0)
    {
        m_target = -1;
        return;
    }
    const QList<QByteArray> f = out.mid(mark + STATE_MARK.size()).trimmed().split(' ');
    if (f.size() != 6)
    {
        m_target = -1;
        return;
    }
    m_alternate = f.at(0) == "1";
    m_mouseTracked = f.at(1) == "1";
    m_inMode = f.at(3) == "1";
    const int rows = f.at(2).toInt();
    if (programOwnsScroll() || m_target < 0)
    {
        m_target = -1;
        refresh();
        return;
    }

    QByteArray text = out.left(mark);
    if (text.endsWith('\n'))
        text.chop(1); // its last line must not push one more into the history
    const int history = int(text.count('\n')) + 1 - rows;
    if (history <= 0 || rows <= 0)
    {
        m_target = -1;
        return;
    }
    openView(text, history, rows);
}

void TmuxScrollBar::openView(const QByteArray &text, int history, int rows)
{
    // the live terminal and the view must have the same grid, or the lines would wrap differently
    QWidget *liveDisplay = terminalDisplayOf(m_live);
    if (!liveDisplay)
    {
        m_target = -1;
        return;
    }
    m_view = new TermWidgetImpl(m_live, m_host);
    m_view->setFocusPolicy(Qt::NoFocus); // the keys stay with the live terminal, which closes the view
    m_view->setTerminalFont(liveDisplay->font());
    m_view->setHistorySize(history + rows);
    m_view->setTerminalSizeHint(false);
    m_view->setGeometry(m_live->geometry());
    m_view->show();
    m_view->lower(); // out of sight until it is complete

    if (QWidget *viewDisplay = terminalDisplayOf(m_view))
    {
        viewDisplay->setFocusPolicy(Qt::NoFocus);
        viewDisplay->installEventFilter(this);
        const auto bars = viewDisplay->findChildren<QScrollBar*>(QString(), Qt::FindDirectChildrenOnly);
        if (!bars.isEmpty())
            m_viewBar = bars.first();
    }
    m_viewHistory = history;
    m_ready = false;
    m_fed = 0;
    // hidden cursor, and every line ends as a terminal wants it
    m_text = QByteArrayLiteral("\x1b[0m\x1b[?25l") + text;
    m_text.replace('\n', "\r\n");

    // the grid of the view is known once its first resize was handled
    QTimer::singleShot(0, this, &TmuxScrollBar::startFeeding);
}

void TmuxScrollBar::startFeeding()
{
    if (!m_view)
        return;
    if (!m_viewBar || m_view->screenLinesCount() != m_live->screenLinesCount()
        || m_view->screenColumnsCount() != m_live->screenColumnsCount())
    {
        closeScrollback();
        return;
    }
    m_view->startTerminalTeletype();
    const int fd = m_view->getPtySlaveFd();
    if (fd < 0)
    {
        closeScrollback();
        return;
    }
    // The terminal reads what is written here in this same thread: a write that blocks would never end.
    ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL) | O_NONBLOCK);
    m_feeder = new QSocketNotifier(fd, QSocketNotifier::Write, this);
    m_feeder->setEnabled(false);
    connect(m_feeder, &QSocketNotifier::activated, this, &TmuxScrollBar::feed);
    feed();
}

void TmuxScrollBar::feed()
{
    if (!m_view || !m_feeder)
        return;
    const int fd = int(m_feeder->socket());
    while (m_fed < m_text.size())
    {
        const ssize_t n = ::write(fd, m_text.constData() + m_fed, size_t(qMin<qsizetype>(65536, m_text.size() - m_fed)));
        if (n > 0)
        {
            m_fed += n;
            continue;
        }
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            m_feeder->setEnabled(true); // the terminal has to read first
            return;
        }
        closeScrollback();
        return;
    }
    m_feeder->setEnabled(false);
    m_text.clear();
    m_readyTicks = 0;
    m_readyStable = 0;
    m_readyTimer.start();
}

// The view is complete when its scroll range is the history it was given.
void TmuxScrollBar::waitReady()
{
    if (!m_view || !m_viewBar)
    {
        m_readyTimer.stop();
        return;
    }
    // A short history shows a larger range for a moment (the terminal has not settled yet): the range
    // has to be the history for a few ticks in a row, not only to reach it.
    m_readyStable = m_viewBar->maximum() == m_viewHistory ? m_readyStable + 1 : 0;
    if (m_readyStable < 4 && ++m_readyTicks < 1000)
        return;
    m_readyTimer.stop();
    if (m_viewBar->maximum() != m_viewHistory)
    {
        closeScrollback();
        return;
    }

    const int target = qBound(0, m_target, m_viewHistory);
    m_target = -1;
    m_internal = true;
    setRange(0, m_viewHistory);
    setPageStep(qMax(1, m_view->screenLinesCount()));
    m_internal = false;
    if (target >= m_viewHistory)
    {
        closeScrollback();
        return;
    }
    m_viewBar->setValue(target);
    m_ready = true;
    connect(m_viewBar, &QScrollBar::valueChanged, this, [this](int value) {
        if (!m_ready)
            return;
        m_internal = true;
        setValue(value);
        m_internal = false;
        if (value >= m_viewHistory)
            closeScrollback();
    });
    m_internal = true;
    setValue(target);
    m_internal = false;
    m_view->raise();
}

void TmuxScrollBar::closeScrollback()
{
    const bool wasOpen = m_view || m_capturing;
    m_readyTimer.stop();
    m_ready = false;
    m_target = -1;
    m_capturing = false;
    if (m_process)
    {
        m_process->disconnect(this);
        m_process->kill();
        m_process->deleteLater();
        m_process = nullptr;
    }
    delete m_feeder;
    m_feeder = nullptr;
    m_text.clear();
    if (m_view)
    {
        m_viewBar = nullptr;
        m_view->hide();
        m_view->deleteLater();
        m_view = nullptr;
    }
    if (wasOpen)
    {
        // the live terminal is at the bottom: the bar too, until tmux says otherwise
        m_internal = true;
        setValue(maximum());
        m_internal = false;
        refresh();
    }
}

bool TmuxScrollBar::copyScrollbackSelection()
{
    if (!m_view || m_view->selectedText().isEmpty())
        return false;
    m_view->copyClipboard();
    return true;
}

bool TmuxScrollBar::eventFilter(QObject *obj, QEvent *event)
{
    // Ctrl+wheel zooms the live terminal; the view has a font of its own that would go out of step
    if (event->type() == QEvent::Wheel && (static_cast<QWheelEvent*>(event)->modifiers() & Qt::ControlModifier))
        return true;
    if (m_view && (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease))
    {
        auto *mouse = static_cast<QMouseEvent*>(event);
        const bool swapped = Properties::Instance()->swapMouseButtons2and3;
        const bool menuButton = mouse->button() == (swapped ? Qt::MiddleButton : Qt::RightButton);
        const bool pasteButton = mouse->button() == (swapped ? Qt::RightButton : Qt::MiddleButton);
        if (menuButton || pasteButton)
        {
            if (event->type() == QEvent::MouseButtonPress)
            {
                const QPoint pos = static_cast<QWidget*>(obj)->mapTo(m_view, mouse->position().toPoint());
                m_live->setFocus(Qt::MouseFocusReason);
                if (menuButton)
                    m_view->customContextMenuCall(pos); // opened right here, as the live terminal does
                else
                    m_live->pasteSelection();
            }
            return true; // the view has no program to give a click to
        }
    }
    return QScrollBar::eventFilter(obj, event);
}

TermWidget::TermWidget(TerminalConfig &cfg, QWidget *parent)
    : QWidget(parent)
    , DBusAddressable(QStringLiteral("/terminals"))
    , m_term(new TermWidgetImpl(cfg, this))
    , m_layout(new QVBoxLayout)
    , m_border(palette().color(QPalette::Window))
{

    #ifdef HAVE_QDBUS
    registerAdapter<TerminalAdaptor, TermWidget>(this);
    #endif

    setFocusProxy(m_term);

    setLayout(m_layout);

    m_row = new QHBoxLayout;
    m_row->setContentsMargins(0, 0, 0, 0);
    m_row->setSpacing(0);
    m_layout->addLayout(m_row);
    m_row->addWidget(m_term);
    if (!m_term->tmuxSession().isEmpty())
    {
        m_tmuxBar = new TmuxScrollBar(m_term->tmuxSession(), m_term, this);
        m_row->addWidget(m_tmuxBar);
        connect(m_term, &QTermWidget::receivedData, m_tmuxBar, &TmuxScrollBar::refresh);
    }
    const auto objs = m_term->children();

    for (QObject *o : objs)
    {
        // Find TerminalDisplay
        if (!o->isWidgetType() || qobject_cast<QWidget*>(o)->isHidden())
        {
            continue;
        }
        o->installEventFilter(this);
    }

    propertiesChanged();

    connect(m_term, &QTermWidget::finished, this, &TermWidget::finished);
    connect(m_term, &QTermWidget::termGetFocus, this, &TermWidget::term_termGetFocus);
    connect(m_term, &QTermWidget::termLostFocus, this, &TermWidget::term_termLostFocus);
    connect(m_term, &QTermWidget::titleChanged, this, [this] { emit termTitleChanged(m_term->title(), m_term->icon()); });
}

void TermWidget::propertiesChanged()
{
    if (Properties::Instance()->highlightCurrentTerminal)
        m_layout->setContentsMargins(2, 2, 2, 2);
    else
        m_layout->setContentsMargins(0, 0, 0, 0);

    m_term->propertiesChanged();

    if (m_tmuxBar)
    {
        m_tmuxBar->closeScrollback();
        // same place as the scroll bar of the terminal would be
        const int position = Properties::Instance()->scrollBarPos;
        m_row->removeWidget(m_tmuxBar);
        m_row->insertWidget(position == 1 ? 0 : 1, m_tmuxBar);
        m_tmuxBar->setVisible(position != 0);
        m_tmuxBar->refresh();
    }
}

void TermWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_tmuxBar)
        m_tmuxBar->closeScrollback(); // the view has the old grid
}

void TermWidget::copySelection()
{
    if (!m_tmuxBar || !m_tmuxBar->copyScrollbackSelection())
        m_term->copyClipboard();
}

void TermWidget::term_termGetFocus()
{
    m_border = palette().color(QPalette::Highlight);
    emit termGetFocus(this);
    update();
}

void TermWidget::term_termLostFocus()
{
    m_border = palette().color(QPalette::Window);
    update();
}

void TermWidget::paintEvent (QPaintEvent *)
{
  if (Properties::Instance()->highlightCurrentTerminal)
    {
      QPainter p(this);
      QPen pen(m_border);
      pen.setWidth(3);
      pen.setBrush(m_border);
      p.setPen(pen);
      p.drawRect(0, 0, width()-1, height()-1);
    }
}

#if HAVE_QDBUS

QDBusObjectPath TermWidget::splitHorizontal(const QHash<QString,QVariant> &termArgs)
{
    TermWidgetHolder *holder = findParent<TermWidgetHolder>(this);
    assert(holder != nullptr);
    TerminalConfig cfg = TerminalConfig::fromDbus(termArgs, this);
    return holder->split(this, Qt::Horizontal, cfg)->getDbusPath();
}

QDBusObjectPath TermWidget::splitVertical(const QHash<QString,QVariant> &termArgs)
{
    TermWidgetHolder *holder = findParent<TermWidgetHolder>(this);
    assert(holder != nullptr);
    TerminalConfig cfg = TerminalConfig::fromDbus(termArgs, this);
    return holder->split(this, Qt::Vertical, cfg)->getDbusPath();
}

QDBusObjectPath TermWidget::getTab()
{
    return findParent<TermWidgetHolder>(this)->getDbusPath();
}

void TermWidget::closeTerminal()
{
    TermWidgetHolder *holder = findParent<TermWidgetHolder>(this);
    holder->splitCollapse(this);
}

void TermWidget::sendText(const QString& text)
{
    if (impl())
    {
        impl()->sendText(text);
    }
}

#endif
