//--------------------------------------------------------------------------
//                   pv.cpp - preferred variation handling.
//                           -------------------
//  copyright            : (C) 2013 by Lucian Landry
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


#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "board.h"
#include "gDynamic.h"
#include "log.h"
#include "pv.h"

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

void PvDecrement(PvT *pv, MoveT *move)
{
    bool bPredictedMove =
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

void CvInit(CvT *cv)
{
    int i;
    for (i = 0; i < MAX_CV_DEPTH; i++)
    {
        cv->moves[i].src = FLAG;
    }
}

// Writes out a sequence of moves in the PV using style 'moveStyle'.
// Returns the number of moves successfully converted.
int PvBuildMoveString(PvT *pv, char *dstStr, int dstLen,
                      const MoveStyleT *moveStyle, struct BoardS *board)
{
    char sanStr[MOVE_STRING_MAX];
    char myStrSpace[MAX_PV_DEPTH * MOVE_STRING_MAX + 1] = "";
    char *myStr = myStrSpace;
    BoardT myBoard;
    int i;
    MoveT *moves;
    int lastLen = 0, myStrLen;
    int movesWritten = 0;

    BoardCopy(&myBoard, board);

    for (i = 0, moves = pv->moves;
         i < pv->depth + 1;
         i++, moves++)
    {
        MoveToString(sanStr, *moves, moveStyle, &myBoard);
        if (sanStr[0])
        {
            // Move was legal, advance to next move so we can check it.
            BoardMoveMake(&myBoard, *moves, NULL);
        }
        else
        {
            // Illegal move found, probably a blasted hash.  This can happen
            // but not very often.
            LogPrint(eLogNormal, "%s: game %d: illegal move %d.%d.%d.%d "
                     "baseply %d depth %d maxDepth %d (probably overwritten "
                     "hash), ignoring\n",
                     __func__, gVars.gameCount,
                     moves->src, moves->dst, int(moves->promote), moves->chk,
                     board->ply, i, pv->depth);
            break;
        }

        // Build up the result string.
        myStr += sprintf(myStr, "%s%s",
                         // Do not use leading space before first move.
                         i == 0 ? "" : " ",
                         sanStr);
        myStrLen = myStr - myStrSpace;
        assert(myStrLen < (int) sizeof(myStrSpace));
        if (myStrLen > dstLen)
        {
            // We wrote too much information.  Chop the last move off.
            myStrSpace[lastLen] = '\0';
            break;
        }
        lastLen = myStrLen;
        movesWritten++;
    }

    strcpy(dstStr, myStrSpace);
    return movesWritten;
}
