//--------------------------------------------------------------------------
//                 moveList.h - MoveListT-oriented functions.
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

#ifndef MOVELIST_H
#define MOVELIST_H

#include "aTypes.h"
#include "board.h"

#ifdef __cplusplus
extern "C" {
#endif

// The large number of moves is in case somebody decides to use 'edit position'
// to setup a bunch of queens on the board :P ... 238 is as high as I got.
#define MLIST_MAX_MOVES 240

typedef struct {
    int lgh;                            // Length of list.  Should be 1st,
					// for alignment purposes.
    int insrt;                          // Spot to insert 'preferred' move.
    int ekcoord;                        // Scratchpad for mlistGenerate(), for
					// performance reasons.
    int capOnly;                        // Used for quiescing.
    MoveT moves[MLIST_MAX_MOVES];
} MoveListT;


void mlistGenerate(MoveListT *mvlist, BoardT *board, int capOnly);
void mlistFirstMove(MoveListT *mvlist, MoveT *move);
void mlistSortByCap(MoveListT *mvlist, BoardT *board);
MoveT *mlistSearch(MoveListT *mvlist, MoveT *move, int howmany);
void mlistMoveDelete(MoveListT *mvlist, int idx);
void mlistMoveAdd(MoveListT *mvlist, BoardT *board, MoveT *move);
int calcNCheck(BoardT *board, int myturn, const char *context);

#ifdef __cplusplus
}
#endif

#endif // MOVELIST_H
