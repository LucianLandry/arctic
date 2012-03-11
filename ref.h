/***************************************************************************
                    ref.h - basic chess concepts for Arctic.
                             -------------------
    copyright            : (C) 2007 by Lucian Landry
    email                : lucian_b_landry@yahoo.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 ***************************************************************************/


#ifndef REF_H
#define REF_H

#include "aTypes.h"

#define VERSION_STRING_MAJOR "1"
#define VERSION_STRING_MINOR "1"
#define VERSION_STRING_PHASE "devel" // or beta, release

#define FLAG            127
#define FLAG64          0x7f7f7f7f7f7f7f7fLL; /* 8 FLAGs in a row */
#define DIRFLAG         10  /* This is even, in order to optimize rook attack
			       checks, and it is low, so I can define the
			       precalculated 'attacks' array. */
#define DOUBLE_CHECK    255 /* cannot be the same as FLAG. */

// These are intended as markers in case I start trying to support some more
// interesting variants.
#define NUM_PLAYERS     2
#define NUM_SQUARES     64

// Boord coordinates start at the southwest corner of board (0), increments
// by 1 as we move to the right, and increments by row-length (8) as we move
// up.
#define Rank(x) ((x) >> 3)
#define File(x) ((x) & 7)

// Given a rank (0-7) and file (0-7), generate the board coordinate.
static inline int toCoord(int rank, int file)
{
    return (rank << 3) + file;
}

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MAX3(a, b, c) (MAX((a), (MAX((b), (c)))))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MIN3(a, b, c) (MIN((a), (MIN((b), (c)))))

/* (0x0 is reserved for empty position.) */
#define KING   0x2 /*  010b */
#define PAWN   0x4 /*  100b */
#define NIGHT  0x6 /*  110b */
#define BISHOP 0x8 /* 1000b */
#define ROOK   0xa /* 1010b */
#define QUEEN  0xc /* 1100b */

/* These are the corresponding black pieces. */
#define BKING   (KING | 1)
#define BPAWN   (PAWN | 1)
#define BNIGHT  (NIGHT | 1)
#define BBISHOP (BISHOP | 1)
#define BROOK   (ROOK | 1)
#define BQUEEN  (QUEEN | 1)

/* Is the piece capable of attacking like a rook or bishop. */
#define ATTACKROOK(piece)   ((piece) >= ROOK)
#define ATTACKBISHOP(piece) (((piece) ^ 0x2 /* 0010b */) >= ROOK)

#define ISKING(piece)   (((piece) | 1) == BKING)
#define ISPAWN(piece)   (((piece) | 1) == BPAWN)
#define ISNIGHT(piece)  (((piece) | 1) == BNIGHT)
#define ISBISHOP(piece) (((piece) | 1) == BBISHOP)
#define ISROOK(piece)   (((piece) | 1) == BROOK)
#define ISQUEEN(piece)  (((piece) | 1) == BQUEEN)

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

// Our basic structure for representing a chess move.
typedef struct {
    uint8 src;     // For a null/invalid move, src = FLAG (and the contents
                   // of the other fields are undefined.)
    uint8 dst;
    uint8 promote; // Piece to promote pawn to (usually nothing, '0')
                   // Also (ab)used for en passant,
                   // signified by 'p' of opposite color.

    uint8 chk;     // Is this a checking move.  Set to:
                   // FLAG if not a checking move.
                   // Coordinate of checking piece if single check
                   // DOUBLE_CHECK otherwise.
} MoveT;

extern MoveT gMoveNone;

// max PV moves we care to display (may not fit in an 80-char line)
// I want this to be at least 16 (because we can hit that depth in endgames).
#define MAX_PV_DEPTH 16
// Principal variation.
typedef struct {
    int eval;     // evaluation of the position.
    int level;    // nominal search depth.
    int depth;    // including quiescing.  May actually be < level if there are
                  // no associated moves (for example if a mate was found).
    MoveT moves[MAX_PV_DEPTH];
} PvT;

#define PV_COMPLETED_SEARCH (-1)

// Current variation.  Useful for debugging.
#define MAX_CV_DEPTH 128
typedef struct {
    MoveT moves[MAX_CV_DEPTH];
} CvT;

typedef struct {
    int lgh;
    uint8 coords[NUM_SQUARES]; // src coord.  Not usually larger than 16, but
                               // with edit-position (or bughouse) we might get
                               // extra pieces ...
} CoordListT;

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
} CompStatsT;

// bits which define ability to castle (other bits in 'cbyte' are reserved).
#define WHITEKCASTLE 0x1
#define BLACKKCASTLE 0x2
#define WHITEQCASTLE 0x4
#define BLACKQCASTLE 0x8
#define WHITECASTLE (WHITEQCASTLE | WHITEKCASTLE)
#define BLACKCASTLE (BLACKQCASTLE | BLACKKCASTLE)
#define ALLCASTLE   (WHITECASTLE | BLACKCASTLE)

// This is beyond the depth we can quiesce.
#define HASH_NOENTRY -64

#endif /* REF_H */
