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

#include "terminalcontext.h"

#include <QDir>
#include <QRegularExpression>

namespace TerminalContext
{

namespace
{

bool isPromptSymbol(QChar c)
{
    return c == QLatin1Char('$') || c == QLatin1Char('#') || c == QLatin1Char('>');
}

// characters that end a path token (a path itself may contain any other)
bool isTokenSeparator(QChar c)
{
    if (c.isSpace())
        return true;
    switch (c.unicode())
    {
    case '"': case '\'': case '`': case '(': case ')': case '[': case ']':
    case '{': case '}': case ',': case ';': case '|': case '<':
        return true;
    default:
        return false;
    }
}

bool hasPathPrefix(const QString &s, int from)
{
    return s.mid(from, 1) == QLatin1String("/")
           || s.mid(from, 2) == QLatin1String("./")
           || s.mid(from, 3) == QLatin1String("../")
           || s.mid(from, 2) == QLatin1String("~/");
}

}

QString stripLineBreaks(const QString &text)
{
    QString out;
    out.reserve(text.size());
    for (const QChar c : text)
    {
        if (c != QLatin1Char('\n') && c != QLatin1Char('\r')
            && c != QChar(0x2028) && c != QChar(0x2029))
        {
            out.append(c);
        }
    }
    return out;
}

QString cleanCommand(const QString &line)
{
    QString s = stripLineBreaks(line);

    static const QRegularExpression leading(QStringLiteral("^\\s+"));
    static const QRegularExpression lineNumber(QStringLiteral("^\\d+\\s+"));
    static const QRegularExpression diffMarker(QStringLiteral("^[+-]\\s*"));
    static const QRegularExpression prompt(QStringLiteral("^(?:[$#>]\\s+|❯\\s*)"));

    s.remove(leading);
    s.remove(lineNumber);
    s.remove(diffMarker);
    s.remove(prompt);

    int end = s.size();
    while (end > 0 && s.at(end - 1).isSpace())
        --end;
    s.truncate(end);
    return s;
}

QString pathAt(const QString &line, int index)
{
    if (index < 0 || index >= line.size() || isTokenSeparator(line.at(index)))
        return QString();

    int begin = index;
    while (begin > 0 && !isTokenSeparator(line.at(begin - 1)))
        --begin;
    int end = index + 1;
    while (end < line.size() && !isTokenSeparator(line.at(end)))
        ++end;

    QString token = line.mid(begin, end - begin);
    if (token.contains(QLatin1String("://")))
        return QString(); // a web address, not a path

    // the prompt symbol glued to the end is not part of the path
    while (!token.isEmpty() && isPromptSymbol(token.at(token.size() - 1)))
        token.chop(1);
    if (index - begin >= token.size())
        return QString();

    // the path may follow "user@host:" in a prompt or "option=" in an argument
    int start = -1;
    for (int p = 0; p < token.size(); ++p)
    {
        if (!hasPathPrefix(token, p))
            continue;
        if (p > 0)
        {
            const QChar before = token.at(p - 1);
            if (before != QLatin1Char(':') && before != QLatin1Char('='))
                continue;
        }
        start = p;
        break;
    }
    if (start < 0 || index - begin < start)
        return QString();

    QString path = token.mid(start);
    static const QRegularExpression position(QStringLiteral(":\\d+(?::\\d+)?$"));
    auto chopPunctuation = [&path]() {
        while (path.size() > 1 && path != QLatin1String("./") && path != QLatin1String("../")
               && (path.endsWith(QLatin1Char('.')) || path.endsWith(QLatin1Char(':'))
                   || path.endsWith(QLatin1Char('!')) || path.endsWith(QLatin1Char('?'))))
        {
            path.chop(1);
        }
    };
    chopPunctuation();
    path.remove(position);
    chopPunctuation();
    return path.size() < 2 ? QString() : path;
}

QString absolutePath(const QString &path, const QString &cwd, const QString &home)
{
    if (path.isEmpty())
        return path;
    if (path.startsWith(QLatin1String("~/")))
        return home.isEmpty() ? path : QDir::cleanPath(home + path.mid(1));
    if (path.startsWith(QLatin1Char('/')))
        return QDir::cleanPath(path);
    return cwd.isEmpty() ? path : QDir::cleanPath(cwd + QLatin1Char('/') + path);
}

bool isSafeUrl(const QUrl &url, bool allowFile)
{
    if (!url.isValid())
        return false;
    const QString scheme = url.scheme().toLower();
    return scheme == QLatin1String("http") || scheme == QLatin1String("https")
           || scheme == QLatin1String("mailto")
           || (allowFile && scheme == QLatin1String("file"));
}

QPair<int, int> wordRangeAt(const QString &line, int index, const QString &wordCharacters)
{
    auto isWordChar = [&wordCharacters](QChar c) {
        return c.isLetterOrNumber() || wordCharacters.contains(c);
    };
    if (index < 0 || index >= line.size() || !isWordChar(line.at(index)))
        return {-1, -1};
    int first = index;
    while (first > 0 && isWordChar(line.at(first - 1)))
        --first;
    int last = index;
    while (last + 1 < line.size() && isWordChar(line.at(last + 1)))
        ++last;
    return {first, last};
}

}
