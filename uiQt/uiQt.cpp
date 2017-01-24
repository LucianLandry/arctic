//--------------------------------------------------------------------------
//               uiQt.cpp - Qt-based GUI interface for Arctic
//                           -------------------
//  copyright            : (C) 2017 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#include <QApplication>
#include <QPushButton>
#include <stdio.h>
#include "ui.h"

static void qtInit(Game *game, Switcher *sw)
{
    // faking passing of cmdline args as we do not need it right now
    int fakeArgc = 1;
    char arcticStr[] = "arctic";
    char *fakeArgv[] = {arcticStr, nullptr};
    
    QApplication app(fakeArgc, fakeArgv);

    QPushButton hello("Hello, world!", 0);
    hello.resize(100, 30);
    hello.show();

    app.exec();
    printf("bye.\n");
    exit(0);
}

static void qtPlayerMove()
{
}

static void qtPositionRefresh(const Position &position)
{
}

static void qtExit()
{
}

static void qtStatusDraw()
{
}

static void qtNotifyTick()
{
}

static void qtNotifyMove(MoveT move)
{
}

static void qtNotifyError(char *reason)
{
}

static void qtNotifyPV(const EnginePvArgsT *pvArgs)
{
}

static void qtNotifyThinking()
{
}

static void qtNotifyPonder()
{
}

static void qtNotifyReady()
{
}

static void qtNotifyComputerStats(const EngineStatsT *stats)
{
}

static void qtNotifyDraw(const char *reason, MoveT *move)
{
}

static void qtNotifyCheckmated(int turn)
{
}

static void qtNotifyResign(int turn)
{
}

UIFuncTableT *uiQtOps()
{
    // Designated initializers are C99 and do not work in C++, so here we
    // initialize manually.
    static UIFuncTableT qtUIFuncTable;
    qtUIFuncTable.init = qtInit;
    qtUIFuncTable.playerMove = qtPlayerMove;
    qtUIFuncTable.positionRefresh = qtPositionRefresh;
    qtUIFuncTable.exit = qtExit;
    qtUIFuncTable.statusDraw = qtStatusDraw;
    qtUIFuncTable.notifyTick = qtNotifyTick;
    qtUIFuncTable.notifyMove = qtNotifyMove;
    qtUIFuncTable.notifyError = qtNotifyError;
    qtUIFuncTable.notifyPV = qtNotifyPV;
    qtUIFuncTable.notifyThinking = qtNotifyThinking;
    qtUIFuncTable.notifyPonder = qtNotifyPonder;
    qtUIFuncTable.notifyReady = qtNotifyReady;
    qtUIFuncTable.notifyComputerStats = qtNotifyComputerStats;
    qtUIFuncTable.notifyDraw = qtNotifyDraw;
    qtUIFuncTable.notifyCheckmated = qtNotifyCheckmated;
    qtUIFuncTable.notifyResign = qtNotifyResign;

    return &qtUIFuncTable;
}
