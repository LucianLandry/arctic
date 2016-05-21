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
#include "Eval.h"

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

// Pre-cache a transtable entry for later use.
void TransTablePrefetch(uint64 zobrist);

// Run this before the full (slower) TransTableHit() func.
bool TransTableQuickHitTest(uint64 zobrist);

// Fills in 'hashEval' and 'hashMove' iff we had a successful hit.
// Assumes a non-zero-size hash.
bool TransTableHit(Eval *hashEval, MoveT *hashMove, uint64 zobrist,
                   int searchDepth, uint16 basePly, int alpha, int beta);

void TransTableConditionalUpdate(Eval eval, MoveT move, uint64 zobrist,
                                 int searchDepth, uint16 basePly);

#endif // TRANSTABLE_H
