//--------------------------------------------------------------------------
//                gDynamic.cpp - all global dynamic variables.
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

#include <stdlib.h> /* malloc(3) */
#include <string.h>
#include <assert.h>

#include "gDynamic.h"
#include "gPreCalc.h"
#include "log.h"

GDynamicT gVars;

void gHistInit(void)
{
    int i, j, k;

    /* reset history table. */
    for (i = 0; i < NUM_PLAYERS; i++)
        for (j = 0; j < NUM_SQUARES; j++)
            for (k = 0; k < NUM_SQUARES; k++)
            {
                /* -50, not -1, because -1 might trigger accidentally if
                   we expand the history window beyond killer moves. */
                gVars.hist[i] [j] [k] = -50;
            }
}

void gPvInit(void)
{
    PvInit(&gVars.pv);
}


// Update the tracked principal variation.
void gPvUpdate(PvT *goodPv)
{
    PvT *pv = &gVars.pv; // shorthand.
    int numMoves = MIN3(MAX_PV_DEPTH,
                        (goodPv->level + 1),
                        (goodPv->depth + 1));

    pv->eval = goodPv->eval;
    pv->level = MAX(pv->level, goodPv->level);
    pv->depth = goodPv->depth;
    // (We purposefully do not track quiescing moves because we do not
    // want to be forced into a capture chain.)
    memcpy(pv->moves, goodPv->moves, numMoves * sizeof(MoveT));
}

void gPvDecrement(MoveT *move)
{
    PvDecrement(&gVars.pv, move);
}

void gPvRewind(int numPlies)
{
    PvRewind(&gVars.pv, numPlies);
}

void gPvFastForward(int numPlies)
{
    PvFastForward(&gVars.pv, numPlies);
}
