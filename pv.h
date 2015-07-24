//--------------------------------------------------------------------------
//                    pv.h - preferred variation handling.
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


#ifndef PV_H
#define PV_H

#include "move.h"

#ifdef __cplusplus
extern "C" {
#endif


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
  
void PvInit(PvT *pv);
void PvDecrement(PvT *pv, MoveT *move);
void PvRewind(PvT *pv, int numPlies);
void PvFastForward(PvT *pv, int numPlies);

struct BoardS;
// Writes out a sequence of moves in the PV using style 'moveStyle'.
// Returns the number of moves successfully converted.
int PvBuildMoveString(PvT *pv, char *dstStr, int dstLen,
                      const MoveStyleT *moveStyle, struct BoardS *board);

void CvInit(CvT *cv);

#ifdef __cplusplus
}
#endif

#endif // PV_H
