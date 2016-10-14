//--------------------------------------------------------------------------
//                  ref.h - basic chess concepts for Arctic.
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


#ifndef REF_H
#define REF_H

#include "aTypes.h"

#define VERSION_STRING_MAJOR "1"
#define VERSION_STRING_MINOR "2"
#define VERSION_STRING_PHASE "beta" // devel, beta, or release

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

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MAX3(a, b, c) (MAX((a), (MAX((b), (c)))))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MIN3(a, b, c) (MIN((a), (MIN((b), (c)))))
#define MAX4(a, b, c, d) (MAX(MAX((a), (b)), MAX((c), (d))))
#define MIN4(a, b, c, d) (MIN(MIN((a), (b)), MIN((c), (d))))

// bits which define ability to castle.  There is one set of these per-player
// in 'cbyte'.
#define CASTLEOO 0x1
#define CASTLEOOO (0x1 << NUM_PLAYERS)
#define CASTLEBOTH (CASTLEOO | CASTLEOOO)
#define CASTLEALL 0xf // full castling for all sides

// This is beyond the depth we can quiesce.
#define HASH_NOENTRY -128

namespace arctic
{

// Boord coordinates start at the southwest corner of board (0), increments
// by 1 as we move to the right, and increments by row-length (8) as we move
// up.
static inline int Rank(cell_t i)
{
    return i >> 3;
}
static inline int File(cell_t i)
{
    return i & 7;
}

// Given a rank (0-7) and file (0-7), return our internal one-dimensional
//  board coordinate.
static inline cell_t ToCoord(int rank, int file)
{
    return (rank << 3) + file;
}

} // end namespace 'arctic'

#endif // REF_H
