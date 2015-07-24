//--------------------------------------------------------------------------
//                   saveGame.h - saveable game structures.
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

#ifndef SAVEGAME_H
#define SAVEGAME_H

#include "aTypes.h"
#include "clock.h"
#include "board.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    MoveT move;
    bigtime_t myTime; // Time left on player's clock after move 'move'
                      // (includes any increment)
} GamePlyT;

// Contains minimal game save + restore + undo + redo information.
typedef struct SaveGameS {
    ClockT clocks[NUM_PLAYERS]; // starting time.
    Piece coord[NUM_SQUARES];   // all the squares on the board.
    uint8 cbyte;
    uint8 ebyte;
    uint8 turn;
    int firstPly;          // initial ply.  Usually 0, but some FEN positions
                           // w/incomplete information can be non-zero.
    int ncpPlies;          // number of non-capture or pawn-push plies.
                           // Usually 0, but some FEN positions w/... well, see
                           // above.

    int numAllocatedPlies; // pretty straightforward hopefully ...
    int numPlies;          // Starts at 0, since the initial ply has no move.
    int currentPly;        // Ply index to write the next move into.
    GamePlyT *plies;
} SaveGameT;


void SaveGameInit(SaveGameT *game);

// Assumes 'dst' is non-NULL and has been initialized w/SaveGameInit().
// Clobbers 'dst', but in a safe fashion.
void SaveGameCopy(SaveGameT *dst, SaveGameT *src);

void SaveGameMoveCommit(SaveGameT *game, MoveT *move, bigtime_t myTime);
int SaveGameSave(SaveGameT *game);

// Assumes 'sgame' has been initialized (w/SaveGameInit())
// Returns: 0, if save successful, otherwise -1.
// 'sgame' is guaranteed to be 'sane' after return, regardless of result.
int SaveGameRestore(SaveGameT *sgame);
int SaveGameRestore(SaveGameT *game);
void SaveGamePositionSet(SaveGameT *game, BoardT *board);
void SaveGameClocksSet(SaveGameT *sgame, ClockT *clocks[]);
int SaveGameGotoPly(SaveGameT *game, int ply, BoardT *board, ClockT *clocks[]);

static inline int SaveGameCurrentPly(SaveGameT *sgame)
{
    return sgame->currentPly;
}
static inline int SaveGameFirstPly(SaveGameT *sgame)
{
    return sgame->firstPly;
}
static inline int SaveGameLastPly(SaveGameT *sgame)
{
    return sgame->numPlies + sgame->firstPly;
}

#ifdef __cplusplus
}
#endif

#endif // SAVEGAME_H
