/***************************************************************************
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

#ifndef TMUXMANAGER_H
#define TMUXMANAGER_H

#include <QJsonArray>
#include <QObject>
#include <QStringList>
#include <QTimer>

class QProcess;

/*!
 * Runs every terminal inside its own tmux session, on a private tmux server
 * (socket "qterminal", generated config), and keeps the sessions alive across
 * restarts with tmux-resurrect:
 *  - the sessions are saved every few minutes and when the program quits;
 *  - at start the saved sessions are restored and offered again as tabs.
 *
 * tmux-continuum is not used for the saving: it piggybacks on the tmux status
 * line, which is hidden here because the tabs are the program's own.
 */
class TmuxManager : public QObject
{
    Q_OBJECT

public:
    static TmuxManager *instance();

    //! tmux is installed
    bool isAvailable() const;
    //! tmux is installed and the user did not turn it off
    bool isActive() const;
    //! the tmux-resurrect plugin is found
    bool hasResurrect() const;

    /*!
     * Prepares the config, restores the saved sessions if the server is not
     * running and starts the periodic saving. To be called once at startup.
     */
    void start();

    QString newSessionName() const;
    /*! Program and arguments that attach (creating it if needed) a session. */
    QStringList clientCommand(const QString &session, const QString &directory, const QStringList &userShell) const;

    //! Names of the sessions that no terminal shows, oldest first
    QStringList detachedSessions() const;
    QString paneCurrentPath(const QString &session) const;
    //! something other than the shell is running in the session
    bool sessionBusy(const QString &session) const;
    void killSession(const QString &session) const;
    //! Scrolls the program that runs in the session, if it owns the scrolling (alternate screen or mouse
    //! tracking): \p lines > 0 goes back, < 0 forward. \p column and \p row (0-based) are where the wheel
    //! is; \p page: \p lines is a whole page. Does nothing for a program that does not.
    void scrollProgram(const QString &session, int lines, int column = 0, int row = 0, bool page = false) const;
    //! Runs tmux without waiting. The caller reads the output when the process finishes; null if tmux is not available.
    QProcess *start(const QStringList &args, QObject *parent) const;
    //! Empties the history of the session and redraws its screen
    void clearPane(const QString &session) const;

    //! Saves the sessions with tmux-resurrect. \p wait blocks until done.
    void saveNow(bool wait = false);
    //! Runs the tmux-resurrect restore into the running server
    bool restoreSaved();

    //! The last saved arrangement of the tabs (see TabWidget)
    QJsonArray loadLayout() const;
    void saveLayout(const QJsonArray &tabs) const;

    //! Turns tmux off for this run only (the setting is not changed)
    void setDisabled() { m_disabled = true; }

    bool isQuitting() const { return m_quitting; }
    void setQuitting() { m_quitting = true; }

signals:
    void statusMessage(const QString &message);
    //! The arrangement of the tabs should be written (just before a save)
    void layoutRequested();

private:
    TmuxManager();

    bool run(const QStringList &args, QString *out = nullptr, int timeoutMs = 5000) const;
    void prepare(QProcess &process) const;
    QStringList serverArguments() const;
    bool serverRunning() const;
    QString dataDir() const;
    QString configPath() const;
    QString findPlugin(const QString &name) const;
    void writeConfig() const;
    void installPlugins();
    void setupSaveTimer();

    QString m_tmux;
    QTimer m_saveTimer;
    bool m_quitting = false;
    bool m_disabled = false;
    bool m_installing = false;
};

#endif // TMUXMANAGER_H
