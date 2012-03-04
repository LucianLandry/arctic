//--------------------------------------------------------------------------
//                gDynamic.c - all global dynamic variables.
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
#include <assert.h>
#include "gDynamic.h"
#include "gPreCalc.h"
#include "log.h"
#include "aSpinlock.h"

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

void gHashInit(void)
{
    int size = gPreCalc.numHashEntries * sizeof(HashPositionT);
    int i = 0;
    int retVal;

    if (size == 0)
	return; /* degenerate case. */

    if (gVars.hash == NULL)
    {
	/* should only be true once */
	if ((gVars.hash = malloc(size)) == NULL)
	{
	    LOG_EMERG("Failed to init hash (numEntries %d, size %d)\n",
		      gPreCalc.numHashEntries, size);
	    exit(0);
	}
	for (i = 0; i < NUM_HASH_LOCKS; i++)
	{
	    retVal = SpinlockInit(&gVars.hashLocks[i]);
	    assert(retVal == 0);
	}
    }

    for (i = 0; i < gPreCalc.numHashEntries; i++)
    {
	gVars.hash[i].depth = HASH_NOENTRY;
    }
}

void PvInit(PvT *pv)
{
    int i;

    pv->eval = 0;
    pv->depth = 0;
    pv->level = 0;
    for (i = 0; i < MAX_PV_DEPTH; i++)
    {
	pv->moves[i].src = FLAG;
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

void PvDecrement(PvT *pv, MoveT *move)
{
    int bPredictedMove =
	move != NULL &&
	move->src != FLAG &&
	!memcmp(move, &pv->moves[0], sizeof(MoveT));

    // Adjust the principal variation (shrink it by depth one after a move).
    // If we did not make the move the computer predicted, this can
    // result in nonsensical moves being kept around.  But we can still use
    // the PV as a hint as to what moves to prefer.
    memmove(&pv->moves[0], &pv->moves[1], MAX_PV_DEPTH - 1);
    pv->moves[MAX_PV_DEPTH - 1].src = FLAG;
    pv->eval = -pv->eval;
    if (pv->eval >= EVAL_WIN_THRESHOLD && pv->eval < EVAL_WIN)
    {
	pv->eval++;
    }
    else if (pv->eval <= EVAL_LOSS_THRESHOLD && pv->eval > EVAL_LOSS)
    {
	pv->eval--;
    }
    if (pv->depth != PV_COMPLETED_SEARCH)
    {
	pv->depth = MAX(pv->depth - 1, 0);
    }
    // If we successfully predicted the move, we can start the next search
    // at the PV's level.
    pv->level = bPredictedMove ? MAX(pv->level - 1, 0) : 0;
}

void gPvDecrement(MoveT *move)
{
    PvDecrement(&gVars.pv, move);
}

// Preserve the future moves of a variation that may not come to pass.
void PvRewind(PvT *pv, int numPlies)
{
    int i;

    if (numPlies <= 0)
    {
	if (numPlies < 0)
	{
	    PvFastForward(pv, -numPlies);
	}
	return;
    }
    numPlies = MIN(MAX_PV_DEPTH, numPlies);
    memmove(&pv->moves[numPlies], &pv->moves[0], MAX_PV_DEPTH - numPlies);

    for (i = 0; i < numPlies; i++)
    {
	pv->moves[i].src = FLAG;
    }
    // We need to clear everything else out, though, because it is no longer
    // valid since we have no idea what move might be selected.
    pv->eval = pv->level = pv->depth = 0;
}

void gPvRewind(int numPlies)
{
    PvRewind(&gVars.pv, numPlies);
}

void PvFastForward(PvT *pv, int numPlies)
{
    if (numPlies < 0)
    {
	PvRewind(pv, -numPlies);
	return;
    }
    numPlies = MIN(MAX_PV_DEPTH, numPlies);
    for (; numPlies > 0; numPlies--)
    {
	PvDecrement(pv, &pv->moves[0]);
    }
}

void gPvFastForward(int numPlies)
{
    PvFastForward(&gVars.pv, numPlies);
}


void CvInit(CvT *cv)
{
    int i;
    for (i = 0; i < MAX_CV_DEPTH; i++)
    {
	cv->moves[i].src = FLAG;
    }
}
