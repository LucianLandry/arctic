//--------------------------------------------------------------------------
//                   ui.h - generic UI interface for Arctic
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#ifndef UI_H
#define UI_H

#include "aTypes.h"
#include "Board.h"
#include "EngineTypes.h"
#include "Game.h"
#include "Switcher.h"

typedef struct {
    void (*init)(Game *game, Switcher *sw);
    void (*playerMove)();
    void (*positionRefresh)(const Position &position);
    void (*exit)();
    void (*statusDraw)();
    void (*notifyTick)();
    void (*notifyMove)(MoveT move);
    void (*notifyError)(char *reason);
    void (*notifyPV)(const EnginePvArgsT *pvArgs);
    void (*notifyThinking)();
    void (*notifyPonder)();
    void (*notifyReady)();
    void (*notifyComputerStats)(const EngineStatsT *stats);
    void (*notifyDraw)(const char *reason, MoveT *move);
    void (*notifyCheckmated)(int turn);
    void (*notifyResign)(int turn);
} UIFuncTableT;
extern UIFuncTableT *gUI;

// uiNcurses.cpp
UIFuncTableT *uiNcursesOps();

// uiXboard.cpp
UIFuncTableT *uiXboardOps();
void processXboardCommand(Game *game, Switcher *sw);

// uiUci.cpp
UIFuncTableT *uiUciOps();
void processUciCommand(Game *game, Switcher *sw);

#ifdef ENABLE_UI_JUCE
// uiJuce.cpp
UIFuncTableT *uiJuceOps();
#endif

#ifdef ENABLE_UI_QT
// uiQt.cpp
UIFuncTableT *uiQtOps();
#endif

#endif // UI_H
