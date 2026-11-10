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

#ifndef TERMINALCONTEXT_H
#define TERMINALCONTEXT_H

#include <QString>
#include <QUrl>

/*!
 * Text helpers behind the terminal's context menu. They work on plain
 * strings only, so they can be tested without any widget.
 */
namespace TerminalContext
{

/*! Removes \n, \r, U+2028 and U+2029 and joins the lines without adding spaces. */
QString stripLineBreaks(const QString &text);

/*!
 * Reduces a terminal line to the command it holds: drops leading spaces, a
 * line number, a diff marker (+ or -) and a prompt symbol ($, #, > or ❯),
 * then drops trailing spaces and line breaks.
 */
QString cleanCommand(const QString &line);

/*!
 * Returns the file path under column \p index of \p line, or an empty string.
 * A path starts with "/", "./", "../" or "~/". A ":line[:column]" suffix and
 * prompt symbols ($, # and >) glued to the end are not part of the path.
 */
QString pathAt(const QString &line, int index);

/*!
 * Makes \p path absolute: "~/" is expanded with \p home and a relative path
 * is joined to \p cwd. A relative path is returned unchanged if \p cwd is empty.
 */
QString absolutePath(const QString &path, const QString &cwd, const QString &home);

/*! Only web and mail links (and local files, if \p allowFile) may be opened. */
bool isSafeUrl(const QUrl &url, bool allowFile);

/*!
 * Returns the [first, last] character range of the word around \p index.
 * A word is made of letters, digits and \p wordCharacters. Returns {-1, -1}
 * if there is no word there.
 */
QPair<int, int> wordRangeAt(const QString &line, int index, const QString &wordCharacters);

}

#endif // TERMINALCONTEXT_H
