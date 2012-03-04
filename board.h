//--------------------------------------------------------------------------
//                  board.h - BoardT-related functionality.
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

#ifndef BOARD_H
#define BOARD_H

#include <string.h>   // memcmp(3)
#include "aTypes.h"
#include "ref.h"
#include "position.h"
#include "list.h"

// This MUST be a power of 2 (to make our hashing work), and MUST be at least
// 128 to account for the 50-move rule (100 plies == 50 moves)
#define NUM_SAVED_POSITIONS 128

typedef struct {
    uint8 coord[NUM_SQUARES]; // all the squares on the board.

    // The following 2 variables should be kept together, in this order.
    // It is used by BoardPositionSave() which assumes it can cast this
    // to a PositionT.
    int zobrist;         // zobrist hash.  Incrementally updated w/each move.

    uint8 hashCoord[NUM_SQUARES / 2]; // this is a 4-bit version of 'coord'.
                                      // Useful for hashing, or possibly
                                      // speeding up move generation (probably
                                      // not).

    uint8 cbyte;         // castling byte.

    uint8 ebyte;         // en passant byte.  Set to the destination coord
                         // of an a2a4-style move (or FLAG otherwise).

    uint8 turn;      // Whose turn is it.  0 == white, 1 == black.

    int ply;	     // (aka 1/2-move.) Usually, white's 1st move is '0'.
                     // (NOTE: this is not always the case; some edited
                     //  positions might have black to move first.)

    int ncheck[NUM_PLAYERS]; // Says if either side is currently in check.

    // This is a way to quickly look up the number and location of any
    // type of piece on the board.
    CoordListT pieceList[BQUEEN + 1];

    uint8 *pPiece[NUM_SQUARES]; // Given a coordinate, this points back to the
                                // exact spot in the pieceList that refers to
                                // this coord.  Basically a reverse lookup for
                                // 'pieceList'.

    int totalStrength; // material strength of all pieces combined.  Used when
                       // checking for draws.

    // strength (material, not position) of each side.
    int playerStrength[NUM_PLAYERS];

    // How many plies has it been since last capture or pawn-move.  If 100
    // plies passed, the game can be drawn by the fifty-move rule.
    //
    // More specifically, (also,) if the next move will trigger the fifty-move
    // rule, one side can announce its intention to draw and then play the
    // move.
    //
    // We ignore that, currently -- two players can decide whether to draw, and
    // the computer will check before and after its move if the fifty-move
    // draw rule applies.
    //
    // Because of the optional nature of this draw, ncpPlies is 'int' instead
    // of 'uint8'.
    int ncpPlies;

    // Ply of first repeated position (if any, then the occurence of the 1st
    // repeat, not the original), otherwise -1).
    int repeatPly;

    // Saved positions.  Used to detect 3-fold repitition.  The fifty-move
    // rule limits the number I need to 100, and 128 is the next power-of-2
    // which makes calculating the appropriate position for a given ply easy.
    //
    // (Technically two non-computer opponents could ignore the fifty-move
    //  rule and then repeat a position 3 times outside this window.  We would
    //  not catch that scenario.  FIXME?)
    PositionElementT positions[NUM_SAVED_POSITIONS];

    // This acts as a hash table to store positions that potentially repeat
    // each other.  There are only 128 elements ('positions', above) that are
    // spread among each entry, so hopefully each list here is about 1
    // element in length.
    ListT posList[NUM_SAVED_POSITIONS];

    // Note: we do not attempt to save/restore the below variables.
    // saveGame.c (via BoardCopy()) assumes 'depth' is the first
    // non-saved/restored variable.
    int depth; // depth we're currently searching at (Searching from root == 0)

    int level; // depth we're currently authorized to search at (can break this
               // w/quiescing).  This doesn't *have* to be per-board right now.
               // Ply extensions might change that?

    CvT cv;    // current variation for this board.  Useful for debugging.
} BoardT;


typedef struct {
    uint8  cappiece;  // any captured piece.. does not include en passant.
    uint8  cbyte;     // castling, en passant bytes
    uint8  ebyte;

    int    ncpPlies;
    uint32 zobrist;   // saved-off hash.
    int    repeatPly; // needs to be signed, as it is the saved value of
                      // board->repeatPly.
} UnMakeT;

// Very basic init.  Currently, BoardSet() will also (re-)initialize a BoardT.
void BoardInit(BoardT *board);

// 'unmake' is filled in by BoardMakeMove() and used by BoardUnmakeMove().
void BoardMoveMake(BoardT *board, MoveT *move, UnMakeT *unmake);
void BoardMoveUnmake(BoardT *board, MoveT *move, UnMakeT *unmake);
int BoardSanityCheck(BoardT *board, int silent);
int BoardConsistencyCheck(BoardT *board, char *failString, int checkz);
void BoardRandomize(BoardT *board);
void BoardCopy(BoardT *dest, BoardT *src);
void BoardSet(BoardT *board, uint8 pieces[], int cbyte, int ebyte, int turn,
	      int firstPly, int ncpPlies);
// These routines are like BoardSet(), but only for one part.
void BoardPieceSet(BoardT *board, int coord, int piece);
void BoardCbyteSet(BoardT *board, int cbyte);
void BoardEbyteSet(BoardT *board, int ebyte);
void BoardTurnSet(BoardT *board, int turn);


int BoardIsNormalStartingPosition(BoardT *board);

int BoardDrawInsufficientMaterial(BoardT *board);
// Threefold repetition and the fifty-move rule are both claimed draws, that
// is, they are not automatic.
int BoardDrawThreefoldRepetition(BoardT *board);
static inline int BoardDrawFiftyMove(BoardT *board)
{
    return board->ncpPlies >= 100;
}

static inline void BoardPositionSave(BoardT *board)
{
    PositionElementT *myElem =
	&board->positions[board->ply & (NUM_SAVED_POSITIONS - 1)];
    memcpy(&myElem->p,
	   &board->zobrist,
	   sizeof(PositionT));
    ListPush(&board->posList[board->zobrist & (NUM_SAVED_POSITIONS - 1)],
	     myElem);
}


static inline int BoardPositionHit(BoardT *board, PositionT *position)
{
    // A simple memcmp would suffice, but the inline zobrist check
    // is liable to be faster.
    return board->zobrist == position->zobrist &&
	memcmp(board->hashCoord,
	       position->hashCoord,
	       sizeof(position->hashCoord)) == 0;
}

static inline int BoardPositionsSame(BoardT *board1, BoardT *board2)
{
    return BoardPositionHit(board1, (PositionT *) &board2->zobrist);
}

int BoardCapWorthCalc(BoardT *board, MoveT *move);

#endif // BOARD_H