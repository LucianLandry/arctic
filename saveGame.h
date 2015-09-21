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

#include <vector>

#include "aTypes.h"
#include "Board.h"
#include "Clock.h"

typedef struct {
    MoveT move;
    bigtime_t myTime; // Time left on player's clock after move 'move'
                      // (includes any increment)
} GamePlyT;

// Contains minimal game save + restore + undo + redo information.
typedef struct SaveGameS {
    Clock clocks[NUM_PLAYERS]; // starting time.

    Position startPosition;

    int currentPly;  // Current ply we are at.
                     // currentPly - startPosition.Ply() == 'plies' index to
                     //  write the next move into.
    std::vector<GamePlyT> plies;
} SaveGameT;

void SaveGameInit(SaveGameT *game);

// Assumes 'src' and 'dst' are both non-NULL, and have both been initialized
//  w/SaveGameInit().  Clobbers 'dst', but in a safe fashion.
void SaveGameCopy(SaveGameT *dst, SaveGameT *src);

void SaveGameMoveCommit(SaveGameT *game, MoveT *move, bigtime_t myTime);
int SaveGameSave(SaveGameT *game);

// Assumes 'sgame' has been initialized (w/SaveGameInit())
// Returns: 0, if restore successful, otherwise -1.
// 'sgame' is guaranteed to be 'sane' after return, regardless of result.
int SaveGameRestore(SaveGameT *sgame);
void SaveGamePositionSet(SaveGameT *game, Board *board);
void SaveGameClocksSet(SaveGameT *sgame, Clock *clocks[]);
int SaveGameGotoPly(SaveGameT *game, int ply, Board *board, Clock *clocks[]);

static inline int SaveGameCurrentPly(SaveGameT *sgame)
{
    return sgame->currentPly;
}
static inline int SaveGameFirstPly(SaveGameT *sgame)
{
    return sgame->startPosition.Ply();
}
static inline int SaveGameLastPly(SaveGameT *sgame)
{
    return sgame->startPosition.Ply() + sgame->plies.size();
}

#endif // SAVEGAME_H
