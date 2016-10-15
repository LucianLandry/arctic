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
    void (*playerMove)(Game *game);
    void (*positionRefresh)(const Position &position);
    void (*exit)(void);
    void (*statusDraw)(Game *game);
    void (*notifyTick)(Game *game);
    void (*notifyMove)(Game *game, MoveT move);
    void (*notifyError)(char *reason);
    void (*notifyPV)(Game *game, const EnginePvArgsT *pvArgs);
    void (*notifyThinking)(void);
    void (*notifyPonder)(void);
    void (*notifyReady)(void);
    void (*notifyComputerStats)(Game *game, const EngineStatsT *stats);
    void (*notifyDraw)(Game *game, const char *reason, MoveT *move);
    void (*notifyCheckmated)(int turn);
    void (*notifyResign)(Game *game, int turn);
} UIFuncTableT;
extern UIFuncTableT *gUI;

// uiNcurses.cpp
UIFuncTableT *uiNcursesOps(void);

// uiXboard.cpp
UIFuncTableT *uiXboardOps(void);
void processXboardCommand(Game *game, Switcher *sw);

// uiUci.cpp
UIFuncTableT *uiUciOps(void);
void processUciCommand(Game *game, Switcher *sw);

#ifdef ENABLE_UI_JUCE
// uiJuce.cpp
UIFuncTableT *uiJuceOps(void);
#endif

#endif // UI_H
