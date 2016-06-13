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
#include "Board.h"
#include "Clock.h"
#include "saveGame.h"
#include "switcher.h"
#include "Thinker.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool bDone;      // 'true' if game has ended (draw/mate), or
                     //  computer resigned the position.
    bool icsClocks;  // Are we in 'icsMode', where clocks do not start ticking
                     //  (or have increments applied) until the 2nd move?

    SaveGameT sgame;
    SwitcherContextT sw;

    int control[NUM_PLAYERS]; // 0 if player controls; 1 if computer

    Clock origClocks[NUM_PLAYERS]; // Clocks are reset to these values at
                                    // beginning of new game.  This can be set
                                    // w/out affecting saveGame's start clocks.
    
    // Actual allocated space for below.  Most code should access this through
    // the 'clocks' reference.  In xboard, the 1st clock is the opponent's
    // clock, and the 2nd clock is the engine clock.
    Clock actualClocks[NUM_PLAYERS]; 
    Clock *clocks[NUM_PLAYERS];    // Time control for both sides.

    // Computer only: time we want to move at.  For instance if == 30000000,
    // we want to move when there is 30 seconds left on our clock.
    bigtime_t goalTime[NUM_PLAYERS];

    Board savedBoard;
} GameT;

void GameInit(GameT *game);

void GameMoveMake(GameT *game, MoveT *move);
// Like GameMoveMake(), but also kick off UI and thinker state updates.
void GameMoveCommit(GameT *game, MoveT *move, Thinker *th,
                    int declaredDraw);
// Handle a change in computer control or pondering.
void GameCompRefresh(GameT *game, Thinker *th);
void GameNewEx(GameT *game, Thinker *th, Board *board, bool resetClocks,
               bool resetHash);
void GameNew(GameT *game, Thinker *th);

int GameGotoPly(GameT *game, int ply, Thinker *th);
int GameRewind(GameT *game, int numPlies, Thinker *th);
int GameFastForward(GameT *game, int numPlies, Thinker *th);

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

#ifdef __cplusplus
}
#endif

#endif // GAME_H
