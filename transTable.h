//--------------------------------------------------------------------------
//                  transTable.h - Transposition table support.
//                           -------------------
//  copyright            : (C) 2012 by Lucian Landry
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

#ifndef TRANSTABLE_H
#define TRANSTABLE_H

#include "aTypes.h"
#include "position.h"

typedef struct {
    PositionEvalT eval;
    int8 salt;        // add a bit of extra randomness to cut down on
                      // accidental aliasing.
    int8 depth;       // needs to be plys from quiescing, due to incremental
                      // search.
    uint16 basePly;   // lets us evaluate if this entry is 'too old'.
    MoveT move;       // stores preferred move for this position.
} HashInfoT;

// Pass this to TransTable(Lazy)Init() to let it pick a semi-sensible default.
#define TRANSTABLE_DEFAULT_SIZE (-1)

// Initialize the global transposition table to size 'size'.
void TransTableInit(int64 size);

// As above, but postpone the actual initialization until the next
// TransTableReset() call.
void TransTableLazyInit(int64 size);

// Clear the global transposition table.
void TransTableReset(void);



// Return the number of unique entries that can be stored in the transposition
// table.
size_t TransTableNumEntries(void); 

// Return the maximum possible size of the transposition table.
size_t TransTableMaxSize(void);

// Return the default size of the transposition table.
size_t TransTableDefaultSize(void);

// Return the current size of the transposition table.
size_t TransTableSize(void);

// Read an entry from the transposition table w/out checking if there is a hit.
void TransTableRead(HashInfoT *result, uint64 zobrist);

// Read the transposition table; return 'true' if the entry matched 'zobrist'
// or 'false' otherwise.
bool TransTableHit(HashInfoT *result, uint64 zobrist);

// Write an entry to the transposition table.
void TransTableWrite(HashInfoT *hInfo, uint64 zobrist);

#endif // TRANSTABLE_H
