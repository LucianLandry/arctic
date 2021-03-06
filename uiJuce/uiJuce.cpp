//--------------------------------------------------------------------------
//               uiJuce.cpp - Juce-based GUI interface for Arctic
//                           -------------------
//  copyright            : (C) 2012 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "ui.h"
#include "ArcticApp.h"

static juce::JUCEApplicationBase *createArcticInstance()
{
    return new ArcticApp();
}

static void juceInit(Game *game, Switcher *sw)
{
    // This is the dance we do to get around needing a START_JUCE_APPLICATION()
    // macro.  It is platform-specific.
    const char *fakeArgv[] = {"arctic", NULL};

    juce::JUCEApplication::createInstance = createArcticInstance;
    // faking passing of cmdline args as we do not need it right now
    juce::JUCEApplication::main(1, fakeArgv);
    exit(0);
}

static void jucePlayerMove()
{
}

static void jucePositionRefresh(const Position &position)
{
}

static void juceExit()
{
}

static void juceStatusDraw()
{
}

static void juceNotifyTick()
{
}

static void juceNotifyMove(MoveT move)
{
}

static void juceNotifyError(char *reason)
{
}

static void juceNotifyPV(const EnginePvArgsT *pvArgs)
{
}

static void juceNotifyThinking()
{
}

static void juceNotifyPonder()
{
}

static void juceNotifyReady()
{
}

static void juceNotifyComputerStats(const EngineStatsT *stats)
{
}

static void juceNotifyDraw(const char *reason, MoveT *move)
{
}

static void juceNotifyCheckmated(int turn)
{
}

static void juceNotifyResign(int turn)
{
}

UIFuncTableT *uiJuceOps()
{
    // Designated initializers are C99 and do not work in C++, so here we
    // initialize manually.
    static UIFuncTableT juceUIFuncTable;
    juceUIFuncTable.init = juceInit;
    juceUIFuncTable.playerMove = jucePlayerMove;
    juceUIFuncTable.positionRefresh = jucePositionRefresh;
    juceUIFuncTable.exit = juceExit;
    juceUIFuncTable.statusDraw = juceStatusDraw;
    juceUIFuncTable.notifyTick = juceNotifyTick;
    juceUIFuncTable.notifyMove = juceNotifyMove;
    juceUIFuncTable.notifyError = juceNotifyError;
    juceUIFuncTable.notifyPV = juceNotifyPV;
    juceUIFuncTable.notifyThinking = juceNotifyThinking;
    juceUIFuncTable.notifyPonder = juceNotifyPonder;
    juceUIFuncTable.notifyReady = juceNotifyReady;
    juceUIFuncTable.notifyComputerStats = juceNotifyComputerStats;
    juceUIFuncTable.notifyDraw = juceNotifyDraw;
    juceUIFuncTable.notifyCheckmated = juceNotifyCheckmated;
    juceUIFuncTable.notifyResign = juceNotifyResign;

    return &juceUIFuncTable;
}
