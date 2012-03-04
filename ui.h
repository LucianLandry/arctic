//--------------------------------------------------------------------------
//                   ui.h - generic UI interface for Arctic
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU Library General Public License as
//   published by the Free Software Foundation; either version 2 of the
//   License, or (at your option) any later version.
//
//--------------------------------------------------------------------------

#ifndef UI_H
#define UI_H

#include "aTypes.h"
#include "board.h"
#include "thinker.h"
#include "game.h"
#include "ref.h"

typedef struct {
    void (*playerMove)(ThinkContextT *th, GameT *game);
    void (*boardRefresh)(const BoardT *board);
    void (*exit)(void);
    void (*statusDraw)(GameT *game);
    void (*notifyTick)(GameT *game);
    void (*notifyMove)(MoveT *move);
    void (*notifyError)(char *reason);
    void (*notifyPV)(GameT *game, PvRspArgsT *pvArgs);
    void (*notifyThinking)(void);
    void (*notifyPonder)(void);
    void (*notifyReady)(void);
    void (*notifyComputerStats)(GameT *game, CompStatsT *stats);
    void (*notifyDraw)(char *reason, MoveT *move);
    void (*notifyCheckmated)(int turn);
    void (*notifyResign)(int turn);
    int (*shouldCommitMoves)(void);
} UIFuncTableT;
extern UIFuncTableT *gUI;

// uiNcurses.c
UIFuncTableT *uiNcursesInit(GameT *game);

// uiXboard.c
UIFuncTableT *uiXboardInit(void);

// uiUci.c
UIFuncTableT *uiUciInit(void);

#endif // UI_H