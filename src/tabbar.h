/***************************************************************************
 *   Copyright (C) 2017 by Nathan Osman                                    *
 *   nathan@quickmediasolutions.com                                        *
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

#ifndef TABBAR_H
#define TABBAR_H

#include <QSize>
#include <QTabBar>

class QToolButton;

class TabBar : public QTabBar
{
    Q_OBJECT

public:

    explicit TabBar(QWidget *parent);

    void setFixedWidth(bool fixedWidth);
    void setFixedWidthValue(int value);
    void updateWidth();

signals:
    //! The "new tab" button next to the last tab was clicked
    void newTabRequested();

protected:

    QSize tabSizeHint(int index) const override;
    void tabLayoutChange() override;
    void tabInserted(int index) override;
    void tabRemoved(int index) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void placeNewTabButton();

    QToolButton *mNewTabButton;

    bool mFixedWidth;
    int mFixedWidthValue;
};

#endif // TABBAR_H
