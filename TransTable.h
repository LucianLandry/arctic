//--------------------------------------------------------------------------
//                 TransTable.h - Transposition table support.
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

#include "aSpinlock.h"
#include "aTypes.h"
#include "Eval.h"
#include "ThinkerTypes.h"

// This class does lazy initialization, and must be Reset() before it can
//  actually be used (otherwise its effective size will be 0).
class TransTable
{
public:
    TransTable();

    // Clears the transposition table.  Does not change its size, unless
    //  'SetDesiredSize()' has been called in the meantime.
    void Reset();
    // Clears the transposition table, and sets its size to 'sizeInBytes'.
    void Reset(int64 sizeInBytes);

    // Sets desired size of the transposition table.  Does not take effect until
    //  the next 'Reset(void)' call.  (used for lazy initialization)
    void SetDesiredSize(int64 sizeInBytes);

    // Returns the number of unique entries that can currently be stored in the
    //  transposition table.
    size_t NumEntries() const;

    // Returns the default size (in bytes) of the transposition table (ie, what
    //  size is used if you Reset(void) the table at startup).
    size_t DefaultSize() const;

    // Returns the current size (in bytes) of the transposition table.
    size_t Size() const;

    // Pre-cache a transtable entry for later use.
    void Prefetch(uint64 zobrist) const;

    // Fills in 'hashEval' and 'hashMove' iff we had a successful hit.
    // (Does alter the hash table as a side effect, so cannot be const)
    bool IsHit(Eval *hashEval, MoveT *hashMove, uint64 zobrist,
               int searchDepth, uint16 basePly, int alpha, int beta,
               ThinkerStatsT *stats);

    // (Maybe) update the transposition table with the new position.  The
    //  table code itself decides whether it is optimal to actually do the
    //  update.
    void ConditionalUpdate(Eval eval, MoveT move, uint64 zobrist,
                           int searchDepth, uint16 basePly,
                           ThinkerStatsT *stats);

    // Returns the maximum *possible* size you could configure the transposition
    //  table to (in bytes).  SetDesiredSize() and Reset() requests are capped
    //  to this size.
    static size_t MaxSize();

private:
    struct HashPositionT
    {
        uint64 zobrist;
        Eval eval;
        MoveT move;     // stores preferred move for this position.
        uint16 basePly; // lets us evaluate if this entry is 'too old'.
        int8 depth;     // needs to be plys from quiescing, due to incremental
                        //  search.
        int8 pad;       // unused
    };
    static_assert(sizeof(HashPositionT) == 24, "HashPositionT is broken");
    
    static const int kNumHashLocks = 1024;
    SpinlockT locks[kNumHashLocks];

    // The below entries are size_t because it does not make sense to try to
    //  force a 64-bit size on a 32-bit platform.  (realloc() would fail)  I
    //  would use 'long' but that is defined to 32 bits on win64.
    // size_t numEntries; same as hash.Size()
    size_t size; // in bytes, current size of hash table
    size_t nextSize; // in bytes, takes effect on next reset
    std::vector<HashPositionT> hash; // the transposition table proper.

    // Support for quick(er) hash entry calculation (all initialized by
    //  prepCalcEntry()):
    size_t hashMask; // could be 'int' if needed
    size_t shiftedNumEntries; // could be 'int' if needed
    int shiftCount;

    // (re-)initialize everything calcEntry() needs to work properly.
    void prepCalcEntry();

    size_t calcEntry(uint64 zobrist) const;

    void resetEntries();

    bool hitTest(Eval *hashEval, MoveT *hashMove, uint64 zobrist,
                 int searchDepth, uint16 basePly, int alpha, int beta,
                 ThinkerStatsT *stats, size_t entry);

    static size_t normalizeNumEntries(size_t numEntries);
    static int64 normalizeSize(int64 size);
    static size_t sanitizeSize(int64 size);

    static bool entryMatches(const HashPositionT &hp, uint64 zobrist,
                             int alpha, int beta, int searchDepth);
};

// Our global transposition table.
extern TransTable gTransTable;

inline size_t TransTable::Size() const
{
    return size;
}

inline size_t TransTable::NumEntries() const
{
    return hash.size();
}

inline bool TransTable::IsHit(Eval *hashEval, MoveT *hashMove, uint64 zobrist,
                              int searchDepth, uint16 basePly,
                              int alpha, int beta,
                              ThinkerStatsT *stats)
{
    if (!Size())
        return false;

    size_t entry = calcEntry(zobrist);

    // Do an unlocked check.  Not threadsafe, but we will recheck in a safe
    //  manner if we actually get a hit.
    if (hash[entry].zobrist != zobrist)
        return false;

    return hitTest(hashEval, hashMove, zobrist, searchDepth, basePly,
                   alpha, beta, stats, entry);
}


#endif // TRANSTABLE_H
