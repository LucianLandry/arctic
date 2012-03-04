//--------------------------------------------------------------------------
//                game.h - current game and associated state.
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

#ifndef GAME_H
#define GAME_H

#include "aTypes.h"
#include "board.h"
#include "clock.h"
#include "saveGame.h"
#include "switcher.h"
#include "thinker.h"

typedef struct {
    SaveGameT sgame;
    int control[NUM_PLAYERS]; // 0 if player controls; 1 if computer
    int bDone;                // Boolean.  'true' if game has ended
                              // (draw/mate), or computer resigned the
                              // position.

    // Actual allocated space for below.  Most code should access this through
    // the 'clocks' reference.  In xboard, the 1st clock is the opponent's
    // clock, and the 2nd clock is the engine clock.
    ClockT actualClocks[NUM_PLAYERS]; 
    ClockT *clocks[NUM_PLAYERS];    // Time control for both sides.

    ClockT origClocks[NUM_PLAYERS]; // Clocks are reset to these values at
                                    // beginning of new game.  This can be set
                                    // w/out affecting saveGame's start clocks.

    // Computer only: time we want to move at.  For instance if == 30000000,
    // we want to move when there is 30 seconds left on our clock.
    bigtime_t goalTime[NUM_PLAYERS];

    SwitcherContextT sw;
    BoardT savedBoard;
} GameT;

void GameInit(GameT *game);

void GameMoveMake(GameT *game, MoveT *move);
// Like GameMoveMake(), but also kick off UI and thinker state updates.
void GameMoveCommit(GameT *game, MoveT *move, ThinkContextT *th,
		    int declaredDraw);
// Handle a change in computer control or pondering.
void GameCompRefresh(GameT *game, ThinkContextT *th);
void GameNewEx(GameT *game, ThinkContextT *th, BoardT *board, int resetClocks,
	       int resetHash);
void GameNew(GameT *game, ThinkContextT *th);

int GameGotoPly(GameT *game, int ply, ThinkContextT *th);
int GameRewind(GameT *game, int numPlies, ThinkContextT *th);
int GameFastForward(GameT *game, int numPlies, ThinkContextT *th);

// These are wrappers for SaveGameT.
static inline int GameCurrentPly(GameT *game)
{
    return SaveGameCurrentPly(&game->sgame);
}
static inline int GameFirstPly(GameT *game)
{
    return SaveGameFirstPly(&game->sgame);
}
static inline int GameLastPly(GameT *game)
{
    return SaveGameLastPly(&game->sgame);
}

#endif // GAME_H
