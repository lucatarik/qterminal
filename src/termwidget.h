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

#ifndef TERMWIDGET_H
#define TERMWIDGET_H

#include <qtermwidget6/qtermwidget.h>

#include "terminalconfig.h"

#include <QAction>
#include "dbusaddressable.h"

#ifdef HAVE_LIBCANBERRA
// forwarded declaration from <canberra.h>
struct ca_context;
#endif

#include <QScrollBar>
#include <QTimer>
#include <QUrl>

class QHBoxLayout;
class QProcess;
class QSocketNotifier;

class TermWidgetImpl : public QTermWidget
{
    Q_OBJECT

//        QMap< QString, QAction * > actionMap;

    public:

        TermWidgetImpl(TerminalConfig &cfg, QWidget * parent=nullptr);
        //! A read-only view of the history of \p live (see TmuxScrollBar): no program runs in it, and
        //! what the menu does to the terminal (paste, directory) is done to \p live
        TermWidgetImpl(TermWidgetImpl *live, QWidget *parent);
        virtual ~TermWidgetImpl();
        void propertiesChanged();

        bool hasCommand() const {
            return m_hasCommand;
        }

        //! tmux session shown by this terminal (empty if tmux is not used)
        QString tmuxSession() const {
            return m_tmuxSession;
        }
        //! Working directory of what runs in the terminal. Under tmux
        //! workingDirectory() would be the one of the tmux client.
        QString currentDirectory();
        //! A command other than the shell is running (or a command was given)
        bool hasForegroundJob();
        //! Clears the screen and the scrollback, also under tmux
        void clearTerminal();

    signals:
        void renameSession();
        void removeCurrentSession();

    public slots:
        void zoomIn();
        void zoomOut();
        void zoomReset();
        void customContextMenuCall(const QPoint & pos);
        //! Cell (0-based) under \p pos, in the coordinates of this widget
        bool cellAt(const QPoint &pos, int &row, int &column) const;

    private slots:
        void activateUrl(const QUrl& url, bool fromContextMenu);
        void bell();

    private:
        // what was under the pointer when the context menu opened
        struct ContextTarget
        {
            bool hasCell = false;
            int row = 0;         // row on the screen
            int column = 0;
            int absoluteRow = 0; // row counted from the oldest line of the history
            QString line;
            QUrl url;            // link under the pointer
            QString path;        // absolute path under the pointer
        };

        ContextTarget contextTargetAt(const QPoint &pos);
        QString screenLine(int absoluteRow) const;
        void selectCells(int absoluteRow, int firstColumn, int lastColumn);
        void openUrlWithBrave(const QUrl &url);
        void openInFileManager(const QString &path);
        void pasteWithoutNewlines();

        bool m_hasCommand;
        QString m_tmuxSession;
        TermWidgetImpl *m_live = nullptr; // set in a view of the history
#ifdef HAVE_LIBCANBERRA
        ca_context* libcanberra_context;
#endif
};


//! Scroll bar of a tmux pane. The history is tmux's, and tmux redraws over the terminal widget, so
//! the widget has none of its own. When the user scrolls back, the history is read once
//! (capture-pane) into a read-only terminal view laid over the live one, and the bar drives that
//! view: no tmux call while scrolling, tmux never enters its copy mode. Back at the bottom, the
//! view goes away.
class TmuxScrollBar : public QScrollBar
{
    Q_OBJECT

public:
    TmuxScrollBar(const QString &session, TermWidgetImpl *live, QWidget *host);
    ~TmuxScrollBar() override;

    //! Scrolls \p lines back in the history (> 0) or forward (< 0). False if the program in the pane
    //! scrolls by itself (alternate screen, mouse tracking): the caller sends the wheel to it.
    bool scrollBy(int lines);
    //! Reads the state of the pane (soon, and not more than every 100 ms)
    void refresh();
    //! A full screen program (alternate screen) or one that tracks the mouse owns the scrolling
    bool programOwnsScroll() const { return !m_inMode && (m_alternate || m_mouseTracked); }

    bool scrollbackOpen() const { return m_view != nullptr; }
    //! Removes the view of the history (and what is being read for it)
    void closeScrollback();
    //! Puts the selection made in the view on the clipboard. False if there is none.
    bool copyScrollbackSelection();

protected:
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void goTo(int value);
    void poll();
    void pollFinished();
    void captureFinished();
    void openView(const QByteArray &text, int history, int rows);
    void startFeeding();
    void feed();
    void waitReady();

    QString m_session;
    TermWidgetImpl *m_live;
    QWidget *m_host;
    QTimer m_timer;
    QTimer m_readyTimer;
    QProcess *m_process = nullptr; // state poll or capture, one at a time
    bool m_capturing = false;
    bool m_dirty = false;
    bool m_internal = false;
    int m_target = -1;             // where the view must be put, as a bar value
    int m_history = 0;
    int m_offset = 0;
    bool m_inMode = false;
    bool m_alternate = false;
    bool m_mouseTracked = false;

    TermWidgetImpl *m_view = nullptr;
    QScrollBar *m_viewBar = nullptr;
    QSocketNotifier *m_feeder = nullptr;
    QByteArray m_text;
    qsizetype m_fed = 0;
    int m_viewHistory = 0;         // lines above the screen in the view, when it is complete
    int m_readyTicks = 0;
    int m_readyStable = 0;
    bool m_ready = false;
};

class TermWidget : public QWidget, public DBusAddressable
{
    Q_OBJECT

    TermWidgetImpl * m_term;
    bool m_rightPressHandled = false;
    int m_wheelDelta = 0;
    QVBoxLayout * m_layout;
    QHBoxLayout * m_row = nullptr;
    TmuxScrollBar * m_tmuxBar = nullptr;
    QColor m_border;

    public:
        TermWidget(TerminalConfig &cfg, QWidget * parent=nullptr);

        void propertiesChanged();
        //! scroll bar of the tmux pane, null without tmux
        TmuxScrollBar *tmuxScrollBar() const { return m_tmuxBar; }
        //! Copies the selection: of the history view if it is open and has one, else of the terminal
        void copySelection();
        QStringList availableKeyBindings() { return m_term->availableKeyBindings(); }

        TermWidgetImpl * impl() { return m_term; }

        #ifdef HAVE_QDBUS
        QDBusObjectPath splitHorizontal(const QHash<QString,QVariant> &termArgs);
        QDBusObjectPath splitVertical(const QHash<QString,QVariant> &termArgs);
        QDBusObjectPath getTab();
        void sendText(const QString& text);
        void closeTerminal();
        #endif

        bool eventFilter(QObject * obj, QEvent * evt) override;

    signals:
        void finished();
        void renameSession();
        void removeCurrentSession();
        void splitHorizontal(TermWidget * self);
        void splitVertical(TermWidget * self);
        void splitCollapse(TermWidget * self);
        void termGetFocus(TermWidget * self);
        void termTitleChanged(QString titleText, QString icon);

    public slots:

    protected:
        bool focusNextPrevChild(bool) override {
            // prevent focus change with Tab and, especially, Backtab
            return false;
        }
        void paintEvent (QPaintEvent * event) override;
        void resizeEvent(QResizeEvent *event) override;

    private slots:
        void term_termGetFocus();
        void term_termLostFocus();
};

#endif

