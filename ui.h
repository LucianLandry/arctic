//--------------------------------------------------------------------------
//                   ui.h - generic UI interface for Arctic
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
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

#ifndef UI_H
#define UI_H

#include "aTypes.h"
#include "Board.h"
#include "Game.h"
#include "switcher.h"
#include "Thinker.h"

typedef struct {
    void (*init)(Game *game, SwitcherContextT *sw);
    void (*playerMove)(Game *game);
    void (*positionRefresh)(const Position &position);
    void (*exit)(void);
    void (*statusDraw)(Game *game);
    void (*notifyTick)(Game *game);
    void (*notifyMove)(Game *game, MoveT move);
    void (*notifyError)(char *reason);
    void (*notifyPV)(Game *game, const RspPvArgsT *pvArgs);
    void (*notifyThinking)(void);
    void (*notifyPonder)(void);
    void (*notifyReady)(void);
    void (*notifyComputerStats)(Game *game, const ThinkerStatsT *stats);
    void (*notifyDraw)(Game *game, const char *reason, MoveT *move);
    void (*notifyCheckmated)(int turn);
    void (*notifyResign)(Game *game, int turn);
} UIFuncTableT;
extern UIFuncTableT *gUI;

// uiNcurses.c
UIFuncTableT *uiNcursesOps(void);

// uiXboard.c
UIFuncTableT *uiXboardOps(void);
void processXboardCommand(Game *game, SwitcherContextT *sw);

// uiUci.c
UIFuncTableT *uiUciOps(void);
void processUciCommand(Game *game, SwitcherContextT *sw);

// uiJuce.c
UIFuncTableT *uiJuceOps(void);

#endif // UI_H
