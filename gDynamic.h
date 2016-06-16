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

#include "ref.h"
#include "Pv.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NO_LIMIT (-1)

typedef struct {
//    short hist[NUM_PLAYERS] [NUM_SQUARES] [NUM_SQUARES]; // History table.
//    int hiswin;       // Tells us how many plies we can check backwards or
                      //  forwards, and still be a valid 'history' entry.
    int maxLevel;     // Max depth we are authorized to search at.
                      //  NO_LIMIT indicates no (external) depth limit.
    int maxNodes;     // Max nodes we are authorized to search.
                      //  NO_LIMIT indicates no node limit.
    bool randomMoves; // randomize moves? (default: false)
    bool ponder;      // allow computer to ponder? (default: false)
    bool canResign;   // true iff engine is allowed to resign

    HintPv pv;        // Attempts to keep track of principal variation.
    int gameCount;    // for stats keeping.
} GDynamicT;
extern GDynamicT gVars;

#ifdef __cplusplus
}
#endif

#endif // GDYNAMIC_H
