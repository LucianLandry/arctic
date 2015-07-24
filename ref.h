//--------------------------------------------------------------------------
//                  ref.h - basic chess concepts for Arctic.
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


#ifndef REF_H
#define REF_H

#include "aTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VERSION_STRING_MAJOR "1"
#define VERSION_STRING_MINOR "1"
#define VERSION_STRING_PHASE "devel" // or beta, release

// The identifier for a 'cell' (basically, a board square, but in the future
// perhaps not every type of board will have to use square-shaped cells).
typedef uint8 cell_t;

#define FLAG            127
#define FLAG64          0x7f7f7f7f7f7f7f7fLL; /* 8 FLAGs in a row */
#define DIRFLAG         10  /* This is even, in order to optimize rook attack
                               checks, and it is low, so I can define the
                               precalculated 'attacks' array. */
#define DOUBLE_CHECK    255 /* cannot be the same as FLAG. */

// These are intended as markers in case I start trying to support some more
// interesting variants.
#define NUM_PLAYERS      2 // Is intended as a maximum.
#define NUM_PLAYERS_BITS 1 // How many bits do we need to represent NUM_PLAYERS?
#define NUM_PLAYERS_MASK ((1 << NUM_PLAYERS_BITS) - 1)

#define NUM_SQUARES     64

// Boord coordinates start at the southwest corner of board (0), increments
// by 1 as we move to the right, and increments by row-length (8) as we move
// up.
static inline int Rank(int i)
{
    return i >> 3;
}
static inline int File(int i)
{
    return i & 7;
}

// Given a rank (0-7) and file (0-7), generate the board coordinate.
static inline int toCoord(int rank, int file)
{
    return (rank << 3) + file;
}

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MAX3(a, b, c) (MAX((a), (MAX((b), (c)))))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MIN3(a, b, c) (MIN((a), (MIN((b), (c)))))

// A 'royal' piece in this sense is any piece that loses the game if captured.
// They are in effect invaluable.  Making this '0' lets us implement multiple
// royal pieces on one side w/out screwing up the evaluation.
#define EVAL_ROYAL     0

#define EVAL_KING      EVAL_ROYAL
#define EVAL_PAWN      100
#define EVAL_BISHOP    300
#define EVAL_KNIGHT    300
#define EVAL_ROOK      500
#define EVAL_QUEEN     900

#define EVAL_WIN       100000      // For chess, this is a checkmate.
#define EVAL_LOSS      (-EVAL_WIN)

// For win/loss detection in x plies.  Here, x can be 100 plies.
#define EVAL_WIN_THRESHOLD (EVAL_WIN - 100)
#define EVAL_LOSS_THRESHOLD (-EVAL_WIN_THRESHOLD)

// NOTE: these are not exact counts, since we do not want the speed hit that
// comes from updating these atomically.  We could have the child threads
// maintain their own stats while they are searching, but this still does not
// work for moveCount because the children need to quickly know when maxNodes
// has been met.
typedef struct {
    int nodes;        // node count (how many times was 'minimax' invoked)
    int nonQNodes;    // non-quiesce node count
    int moveGenNodes; // how many times was mListGenerate() called
    int hashHitGood;  // hashtable hits that returned immediately.
    int hashWroteNew; // how many times (in this ply) we wrote to a unique
                      // hash entry.  Used for UCI hashfull stats.
} CompStatsT;

// bits which define ability to castle.  There is one set of these per-player
// in 'cbyte'.
#define CASTLEOO 0x1
#define CASTLEOOO (0x1 << NUM_PLAYERS)
#define CASTLEBOTH (CASTLEOO | CASTLEOOO)
#define CASTLEALL 0xf // full castling for all sides

// This is beyond the depth we can quiesce.
#define HASH_NOENTRY -128

#ifdef __cplusplus
}
#endif

#endif // REF_H
