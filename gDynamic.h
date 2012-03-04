//--------------------------------------------------------------------------
//                gDynamic.h - all global dynamic variables.
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

#ifndef GDYNAMIC_H
#define GDYNAMIC_H

#include <poll.h>     // struct pollfd
#include "ref.h"
#include "thinker.h"
#include "board.h"
#include "position.h"
#include "aSpinlock.h"

#define NUM_HASH_LOCKS 1024 // must be a power of 2.
#define MAX_NUM_PROCS 8 // maximum number of processors we can take advantage
                        // of.

#define NO_LIMIT (-1)

typedef struct {
    short hist[NUM_PLAYERS] [NUM_SQUARES] [NUM_SQUARES]; // History table.
    int hiswin;              // Tells us how many plies we can check backwards
                             // or forwards, and still be a valid 'history'
                             // entry.
    int maxLevel;            // max depth we are authorized to search at.
                             // NO_LIMIT indicates no (external) depth limit.
    int maxNodes;            // max nodes we are authorized to search.
                             // NO_LIMIT indicates no node limit.
    int randomMoves;         // bool: randomize moves? (default 0)
    int ponder;              // bool: allow computer to ponder? (default 0)
    int resignThreshold;     // Computer will resign if its eval highbound
                             // is <= this score.

    // transposition table and associated locks (should have enough locks
    // to avoid serious contention by multiple threads).
    HashPositionT *hash; // transposition table.
    SpinlockT hashLocks[NUM_HASH_LOCKS];

    PvT pv; // Attempts to keep track of principal variation.
} GDynamicT;
extern GDynamicT gVars;

void gHashInit(void);
void gHistInit(void);

void gPvInit(void);
void gPvUpdate(PvT *goodPv);
void gPvDecrement(MoveT *move);
void gPvRewind(int numPlies);
void gPvFastForward(int numPlies);

// I do not have a better place to put these right now.  FIXME?
void PvInit(PvT *pv);
void PvDecrement(PvT *pv, MoveT *move);
void PvRewind(PvT *pv, int numPlies);
void PvFastForward(PvT *pv, int numPlies);

void CvInit(CvT *cv);

#endif // GDYNAMIC_H
