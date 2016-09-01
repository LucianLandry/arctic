//--------------------------------------------------------------------------
//               uiJuce.cpp - Juce-based GUI interface for Arctic
//                           -------------------
//  copyright            : (C) 2012 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU Lesser General Public License as
//   published by the Free Software Foundation; either version 2.1 of the
//   License, or (at your option) any later version.
//
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

static void juceInit(Game *game, SwitcherContextT *sw)
{
    // This is the dance we do to get around needing a START_JUCE_APPLICATION()
    // macro.  It is platform-specific.
    const char *fakeArgv[] = {"arctic", NULL};

    juce::JUCEApplication::createInstance = createArcticInstance;
    // faking passing of cmdline args as we do not need it right now
    juce::JUCEApplication::main(1, fakeArgv);
    exit(0);
}

static void jucePlayerMove(Game *game)
{
}

static void jucePositionRefresh(const Position &position)
{
}

static void juceExit(void)
{
}

static void juceStatusDraw(Game *game)
{
}

static void juceNotifyTick(Game *game)
{
}

static void juceNotifyMove(Game *game, MoveT move)
{
}

static void juceNotifyError(char *reason)
{
}

static void juceNotifyPV(Game *game, const RspPvArgsT *pvArgs)
{
}

static void juceNotifyThinking(void)
{
}

static void juceNotifyPonder(void)
{
}

static void juceNotifyReady(void)
{
}

static void juceNotifyComputerStats(Game *game, const ThinkerStatsT *stats)
{
}

static void juceNotifyDraw(Game *game, const char *reason, MoveT *move)
{
}

static void juceNotifyCheckmated(int turn)
{
}

static void juceNotifyResign(Game *game, int turn)
{
}

UIFuncTableT *uiJuceOps(void)
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
