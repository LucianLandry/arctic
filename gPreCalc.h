//--------------------------------------------------------------------------
//              gPreCalc.h - all constant (or init-time) globals.
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

#ifndef GPRECALC_H
#define GPRECALC_H

#include "aTypes.h"
#include "Piece.h"
#include "ref.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* moves[NUM_SQUARES] [12] is more intuitive, but less aligned... */

    // pre-calculated list of moves from any given square in any given
    // direction.  The directions (from White's perspective) are:
    // 0 - northwest
    // 1 - north
    // 2 - northeast
    // 3 - east
    // 4 - southeast
    // 5 - south
    // 6 - southwest
    // 7 - west
    // 8 - knight move.
    // 9 - knight move (special, used only for calculating black night moves.
    //     This is so forward knight-moves are always tried first.)
    // 10 - white pawn move (not a valid direction)
    // 11 - black pawn move (not a valid direction)
    // Each 'list' is terminated with a FLAG.
    cell_t *moves[12] [NUM_SQUARES];

    // pre-calculated direction from one square to another.
    uint8 dir[NUM_SQUARES] [NUM_SQUARES]; 

    // pre-calculated distance from one square to another.  Does not take
    // diagonal moves into account (by design).
    uint8 distance[NUM_SQUARES] [NUM_SQUARES];

    // pre-calculated distance from one square to center of board.  Does not
    // take diagonal moves into account (by design).
    uint8 centerDistance[NUM_SQUARES];

    // (pre-calculated) hashing support.
    struct
    {
        uint64 coord[kMaxPieces] [NUM_SQUARES];
        uint64 turn;
        uint64 cbyte[16];
        uint64 ebyte[NUM_SQUARES];
    } zobrist;

    uint8 castleMask[NUM_SQUARES];

    int userSpecifiedNumThreads;
    int64 userSpecifiedHashSize;

    // For convenience.
    Piece *normalStartingPieces;
} GPreCalcT;
extern GPreCalcT gPreCalc;

void gPreCalcInit(int64 userSpecifiedHashSize, int userSpecifiedNumThreads);
uint64 random64(void); // generate a 64-bit random number.

#ifdef __cplusplus
}
#endif

#endif // GPRECALC_H
