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

#include <QAction>
#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QSvgWidget>
#include <stdio.h>
#include "ui.h"

static void qtInit(Game *game, Switcher *sw)
{
    // faking passing of cmdline args as we do not need it right now
    int fakeArgc = 1;
    char arcticStr[] = "arctic";
    char *fakeArgv[] = {arcticStr, nullptr};
    
    QApplication app(fakeArgc, fakeArgv);

    // Setup the main window:
    QMainWindow window;
    window.setCentralWidget(new QSvgWidget("../src/resources/Chess_ndt45.svg", nullptr));
    QMenuBar *menuBar = new QMenuBar;
    QMenu *fileMenu = new QMenu("&File");

    // Must do it like this to make fileMenu take ownership
    QAction *quitAction = fileMenu->addAction("Quit");
    // Hard-code 'quit' if it is not bound, for instance under Cinnamon (and
    //  hope it is not bound to anything else).
    if (!QKeySequence::keyBindings(QKeySequence::Quit).isEmpty())
        quitAction->setShortcuts(QKeySequence::Quit);
    else
        quitAction->setShortcut(Qt::CTRL + Qt::Key_Q);
    quitAction->setStatusTip("Quit the application");
    quitAction->setShortcutContext(Qt::ApplicationShortcut);
    QObject::connect(quitAction, &QAction::triggered,
                     &app, &QApplication::quit);

    menuBar->addMenu(fileMenu);
    window.setMenuBar(menuBar);
    window.show();
    
    app.exec();
    printf("bye.\n");
    exit(0); // This crashes occasionally on main window close w/Qt 5.3.2
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
