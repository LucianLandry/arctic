/***************************************************************************
                    position.h - position-related typedefs.
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

#ifndef POSITION_H
#define POSITION_H

#include "aTypes.h"
#include "list.h"
#include "ref.h"

typedef struct {
    int zobrist;  // for hashing.  Incrementally updated w/each move.
    // 4-bit version of board->coord[NUM_SQUARES].
    uint8 hashCoord[NUM_SQUARES / 2]; 
} PositionT;

/* Inherits from ListElementT. */
typedef struct {
    ListElementT el;
    PositionT p;
} PositionElementT;

typedef struct {
    int lowBound;
    int highBound;
} PositionEvalT;


/* Inherits from PositionT. */
typedef struct {
    PositionT p;
    PositionEvalT eval;
    MoveT move;       // stores preferred move for this position.
    uint16 basePly;   // lets us evaluate if this entry is 'too old'.
    int8 depth;       // needs to be plys from quiescing, due to incremental
                      // search.
} HashPositionT;


#endif /* POSITION_H */
