//--------------------------------------------------------------------------
//              gPreCalc.h - all constant (or init-time) globals.
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

#ifndef GPRECALC_H
#define GPRECALC_H

#include "aTypes.h"
#include "ref.h"

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
    // 8 - night move.
    // 9 - night move (special, used only for calculating black night moves.
    //     This is so forward night-moves are always tried first.
    // 10 - white pawn move (not a valid direction)
    // 11 - black pawn move (not a valid direction)
    // Each 'list' is terminated with a FLAG.
    uint8 *moves[12] [NUM_SQUARES];

    // pre-calculated direction from one square to another.
    uint8 dir[NUM_SQUARES] [NUM_SQUARES]; 

#if 0
#define ATTACKS_OFFSET BISHOP
    /* Out, because this is not a win.
       (pre-calculated) bool table that tells us if a piece can attack in a
       certain direction.  Currently only defined for bishop, rook, and
       queen, but I could extend it.
       'attacks' could be [DIRFLAG + 1] [BQUEEN + 1], but optimized for
       alignment.
    */
    uint8 attacks[DIRFLAG + 1] [8];
#endif

    /* pre-calculated identification of friend, enemy, or unoccupied. */
    uint8 check[BQUEEN + 1] [NUM_PLAYERS];

    int worth[BQUEEN + 1]; /* pre-calculated worth of pieces.  Needs to be
			      signed (Kk is -1). */

    // pre-calculated distance from one square to another.  Does not take
    // diagonal moves into account (by design).
    uint8 distance[NUM_SQUARES] [NUM_SQUARES];

    // pre-calculated distance from one square to center of board.  Does not
    // take diagonal moves into account (by design).
    uint8 centerDistance[NUM_SQUARES];

    // (pre-calculated) hashing support.
    struct
    {
	uint64 coord[BQUEEN + 1] [NUM_SQUARES];
	uint64 turn;
	uint64 cbyte[16];
	uint64 ebyte[NUM_SQUARES];
    } zobrist;

    int numProcs;
    bool userSpecifiedHashSize;

    // For convenience.
    uint8 *normalStartingPieces;
} GPreCalcT;
extern GPreCalcT gPreCalc;

void gPreCalcInit(bool userSpecifiedHashSize, int numCpuThreads);
uint64 random64(void); // generate a 64-bit random number.

#define FRIEND 0
#define UNOCCD 1
#define ENEMY 2

// returns FRIEND, ENEMY, or UNOCCD
#define CHECK(piece, turn)  (gPreCalc.check[(piece)] [(turn)])

#define WORTH(piece) (gPreCalc.worth[piece])

#endif /* GPRECALC_H */
