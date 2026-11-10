/***************************************************************************
 *   Copyright (C) 2022 by LXQt team                                       *
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

#include "qterminal_test.h"

#include "qterminalutils.h"
#include "terminalcontext.h"

#include <QtTest>

// handy shortcut copied from liblxqt
#ifndef QL1S
#define QL1S(x) QLatin1String(x)
#endif

void QTerminalTest::testParseCommand()
{
    /* Common usage */
    // qterminal -e 'fpad -s "PATH/ha ha"'
    // qterminal -e "fpad -s \"PATH/ha ha\"" # \" is decoded as " by the shell
    QCOMPARE(parse_command(QL1S(R"(fpad -s "PATH/ha ha")")),
             QStringList() << QL1S("fpad") << QL1S("-s") << QL1S("PATH/ha ha"));
    // qterminal -e "fpad -s 'PATH/ha ha'"
    QCOMPARE(parse_command(QL1S(R"(fpad -s 'PATH/ha ha')")),
             QStringList() << QL1S("fpad") << QL1S("-s") << QL1S("PATH/ha ha"));
    // qterminal -e 'fpad -s PATH/ha\ ha'
    // qterminal -e "fpad -s PATH/ha\ ha"
    QCOMPARE(parse_command(QL1S(R"(fpad -s PATH/ha\ ha)")),
             QStringList() << QL1S("fpad") << QL1S("-s") << QL1S("PATH/ha ha"));

    /* Uncommon usage */
    // qterminal -e 'fpad -s \"PATH/ha ha\"'
    QCOMPARE(parse_command(QL1S(R"(fpad -s \"PATH/ha ha\")")),
             QStringList() << QL1S("fpad") << QL1S("-s") << QL1S("\\\"PATH/ha") << QL1S("ha\\\""));
    // qterminal -e 'fpad -s "PATH/ha\ ha"'
    QCOMPARE(parse_command(QL1S(R"(fpad -s "PATH/ha\ ha")")),
             QStringList() << QL1S("fpad") << QL1S("-s") << QL1S("PATH/ha ha"));
}


void QTerminalTest::testStripLineBreaks()
{
    QCOMPARE(TerminalContext::stripLineBreaks(QL1S("a\nb\r\nc")), QL1S("abc"));
    QCOMPARE(TerminalContext::stripLineBreaks(QString::fromUtf8("a b c")), QL1S("abc"));
    QCOMPARE(TerminalContext::stripLineBreaks(QL1S("a b\n c")), QL1S("a b c"));
}

void QTerminalTest::testCleanCommand()
{
    QCOMPARE(TerminalContext::cleanCommand(QL1S("      40 +sudo sv status cloudflared")),
             QL1S("sudo sv status cloudflared"));
    QCOMPARE(TerminalContext::cleanCommand(QL1S("$ ls -la  \r\n")), QL1S("ls -la"));
    QCOMPARE(TerminalContext::cleanCommand(QL1S("# make install")), QL1S("make install"));
    QCOMPARE(TerminalContext::cleanCommand(QL1S("> echo hi")), QL1S("echo hi"));
    QCOMPARE(TerminalContext::cleanCommand(QString::fromUtf8("❯ git status")), QL1S("git status"));
    QCOMPARE(TerminalContext::cleanCommand(QString::fromUtf8("12 -$ rm x")), QL1S("rm x"));
    // not a prompt, not a number, not a diff marker
    QCOMPARE(TerminalContext::cleanCommand(QL1S("echo $HOME")), QL1S("echo $HOME"));
    QCOMPARE(TerminalContext::cleanCommand(QL1S("7z a x.7z")), QL1S("7z a x.7z"));
    QCOMPARE(TerminalContext::cleanCommand(QL1S("#comment")), QL1S("#comment"));
    QCOMPARE(TerminalContext::cleanCommand(QL1S("   ")), QString());
}

void QTerminalTest::testPathAt()
{
    const QString prompt = QL1S("~/progetti/app$ ");
    QCOMPARE(TerminalContext::pathAt(prompt, 3), QL1S("~/progetti/app"));
    QCOMPARE(TerminalContext::pathAt(prompt, 14), QString()); // on the "$"
    QCOMPARE(TerminalContext::pathAt(QL1S("cd /usr/local/bin>"), 5), QL1S("/usr/local/bin"));
    QCOMPARE(TerminalContext::pathAt(QL1S("src/main.cpp:12:3: error"), 6), QString()); // no prefix
    QCOMPARE(TerminalContext::pathAt(QL1S("./src/main.cpp:12:3: error"), 5), QL1S("./src/main.cpp"));
    QCOMPARE(TerminalContext::pathAt(QL1S("see ../x/y.txt:42 now"), 6), QL1S("../x/y.txt"));
    QCOMPARE(TerminalContext::pathAt(QL1S("open '/tmp/a b' now"), 7), QL1S("/tmp/a"));
    QCOMPARE(TerminalContext::pathAt(QL1S("lt1@host:~/dir$ ls"), 12), QL1S("~/dir"));
    QCOMPARE(TerminalContext::pathAt(QL1S("--config=/etc/x.conf"), 12), QL1S("/etc/x.conf"));
    QCOMPARE(TerminalContext::pathAt(QL1S("see /etc/hosts."), 6), QL1S("/etc/hosts"));
    QCOMPARE(TerminalContext::pathAt(QL1S("https://example.com/a/b"), 15), QString());
    QCOMPARE(TerminalContext::pathAt(QL1S("plain words"), 2), QString());
    QCOMPARE(TerminalContext::pathAt(QL1S("a / b"), 2), QString());
    QCOMPARE(TerminalContext::pathAt(QL1S("/etc"), 99), QString());
}

void QTerminalTest::testAbsolutePath()
{
    QCOMPARE(TerminalContext::absolutePath(QL1S("~/x/y"), QL1S("/cwd"), QL1S("/home/u")), QL1S("/home/u/x/y"));
    QCOMPARE(TerminalContext::absolutePath(QL1S("./x"), QL1S("/cwd/sub"), QL1S("/home/u")), QL1S("/cwd/sub/x"));
    QCOMPARE(TerminalContext::absolutePath(QL1S("../x"), QL1S("/cwd/sub"), QL1S("/home/u")), QL1S("/cwd/x"));
    QCOMPARE(TerminalContext::absolutePath(QL1S("/a/b/"), QL1S("/cwd"), QL1S("/home/u")), QL1S("/a/b"));
    QCOMPARE(TerminalContext::absolutePath(QL1S("./x"), QString(), QL1S("/home/u")), QL1S("./x"));
}

void QTerminalTest::testIsSafeUrl()
{
    QVERIFY(TerminalContext::isSafeUrl(QUrl(QL1S("https://example.com")), false));
    QVERIFY(TerminalContext::isSafeUrl(QUrl(QL1S("HTTP://example.com")), false));
    QVERIFY(TerminalContext::isSafeUrl(QUrl(QL1S("mailto:a@b.c")), false));
    QVERIFY(!TerminalContext::isSafeUrl(QUrl(QL1S("file:///etc/passwd")), false));
    QVERIFY(TerminalContext::isSafeUrl(QUrl(QL1S("file:///etc/passwd")), true));
    QVERIFY(!TerminalContext::isSafeUrl(QUrl(QL1S("javascript:alert(1)")), true));
    QVERIFY(!TerminalContext::isSafeUrl(QUrl(QL1S("ssh://host")), true));
}

void QTerminalTest::testWordRangeAt()
{
    const QString wc = QL1S(":@-./_~");
    QCOMPARE(TerminalContext::wordRangeAt(QL1S("ls  foo-bar.txt x"), 6, wc), qMakePair(4, 15 - 1));
    QCOMPARE(TerminalContext::wordRangeAt(QL1S("ls  foo"), 2, wc), qMakePair(-1, -1));
    QCOMPARE(TerminalContext::wordRangeAt(QL1S("abc"), 0, wc), qMakePair(0, 2));
    QCOMPARE(TerminalContext::wordRangeAt(QL1S("abc"), 3, wc), qMakePair(-1, -1));
}

QTEST_MAIN(QTerminalTest)
