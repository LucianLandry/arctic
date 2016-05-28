//--------------------------------------------------------------------------
//                TransTable.cpp - Transposition table support.
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

#include <assert.h>
#include <stdlib.h>

#include "aSpinlock.h"
#include "aSystem.h"
#include "aTypes.h"
#include "comp.h"
#include "log.h"
#include "TransTable.h"
#include "uiUtil.h"

TransTable gTransTable; // Global transposition table instantiation.

#ifdef ENABLE_DEBUG_LOGGING
static const MoveStyleT gMoveStyleTT = { mnCAN, csOO, true };
#endif

static int calcNumLeadingZeros(size_t numEntries)
{
    int shiftCount = 0;

    // Allow for upto 22 potentially non-zero consecutive bits.
    // This limit increases the fairness of the hashentry calculation, and
    // still allows us (for example) a full 4GB of hash entries w/out being
    // more than 1k entries off.
    while (numEntries > 0x3fffff)
    {
        numEntries >>= 1;
        shiftCount++;
    }
    return shiftCount;
}

// This turns (for example) 3 leading zeros into a hash mask of '111b'.
static size_t calcHashMask(size_t numEntries)
{
    int leadingZeros = calcNumLeadingZeros(numEntries);
    size_t result = 0;
    int i;

    for (i = 0; i < leadingZeros; i++)
    {
        result <<= 1;
        result |= 1;
    }
    return result;
}

size_t TransTable::normalizeNumEntries(size_t numEntries)
{
    int shiftCount = 0;

    if (numEntries < 1)
    {
        return 0;
    }

    // numEntries should be a mult of kNumHashLocks
    numEntries /= kNumHashLocks;
    numEntries *= kNumHashLocks;

#if 0    
    // A version that forces 'numEntries' to be a power of 2.
    // Strip off low '1' bits as long as doing so would not make 'numEntries'
    // zero.
    while ((numEntries & (numEntries - 1)))
    {
        numEntries &= (numEntries - 1);
    }
#endif

    // This version makes sure that numEntries looks like:
    // 0000xxxxxxxx0000 (binary size_t)
    // where "xxxxxxxx" can be up to 22 bits wide,
    // but is (optionally) preceded by and (optionally) followed by filler 0s
    // to make a total of 64 bits.  Also, the last set of filler 0s cannot
    // be more than 32 bits.  So:
    numEntries = MIN(((uint64) numEntries), 0x3fffff00000000);
    shiftCount = calcNumLeadingZeros(numEntries);
    numEntries >>= shiftCount;
    numEntries <<= shiftCount;

    return numEntries;
}

static int64 constrainSize(int64 size)
{
    size = MAX(size, 0);
    size = MIN(size, INT64_MAX);
    size = MIN((uint64) size, SIZE_MAX);
    return size;
}

int64 TransTable::normalizeSize(int64 size)
{
    int64 result = constrainSize(size) / sizeof(HashPositionT); // calc numEntries
    result = normalizeNumEntries(result);
    return result * sizeof(HashPositionT);
}

// Returns the maximum *possible* size you could configure the transposition
//  table to (in bytes).  SetDesiredSize() and Reset() requests are capped
//  to this size.
size_t TransTable::MaxSize()
{
    // Refuse to go over (total detected system memory - 32M)
    int64 result = SystemTotalMemory() - 32 * 1024 * 1024;
    return normalizeSize(result);
}

// Returns the default size (in bytes) of the transposition table (ie, what
//  size is used if you Reset(void) the table at startup).
size_t TransTable::DefaultSize() const
{
    // As a convenience, pick MIN(1/3 total memory, 512M).
    int64 size = SystemTotalMemory() / 3;
    size = MIN(size, 512 * 1024 * 1024);
    return normalizeSize(size);
}

size_t TransTable::sanitizeSize(int64 size)
{
    return
        size < -1 ? 0 : // bad parameter
        MIN((uint64) normalizeSize(size), MaxSize());
}

void TransTable::resetEntries()
{
    HashPositionT newHashEntry;

    memset(&newHashEntry, 0, sizeof(newHashEntry));
    newHashEntry.depth = HASH_NOENTRY;
    newHashEntry.move = MoveNone;

    std::fill(hash.begin(), hash.end(), newHashEntry);
}

// (re-)initialize everything calcEntry() needs to work properly.
void TransTable::prepCalcEntry()
{
    int numEntries = hash.size();
    int numLeadingZeros = calcNumLeadingZeros(numEntries);

    hashMask = calcHashMask(numEntries);
    shiftedNumEntries = numEntries >> numLeadingZeros;
    shiftCount = 32 - numLeadingZeros;
}

// Initialize the global transposition table to size 'size'.
TransTable::TransTable()
{
    for (auto &lock : locks)
        SpinlockInit(&lock);

    size = 0;
    nextSize = DefaultSize();
    prepCalcEntry();
}

// Sets desired size of the transposition table.  Does not take effect until
//  the next 'Reset(void)' call.  (used for lazy initialization)
void TransTable::SetDesiredSize(int64 sizeInBytes)
{
    nextSize = sanitizeSize(sizeInBytes);
}

// Clear the global transposition table.
void TransTable::Reset()
{
    if (nextSize != size)
    {
        // Resizing to 0 first since I'd hate to have the new memory allocated
        //  at the same time as the old memory (and there is no need to preserve
        //  the old memory).
        hash.resize(0);
        hash.shrink_to_fit();
        hash.resize(nextSize / sizeof(HashPositionT));
        size = nextSize;
        prepCalcEntry();
    }

    resetEntries();
}

// Clears the transposition table, and sets its size to 'sizeInBytes'.
void TransTable::Reset(int64 sizeInBytes)
{
    SetDesiredSize(sizeInBytes);
    Reset();
}

#define QUIESCING (searchDepth < 0)

// If this shows up during profiling, we could put it in a private namespace.
//  (I think that would preclude making it a HashPositionT member function,
//   though.)
bool TransTable::entryMatches(const HashPositionT &hp, uint64 zobrist,
                              int alpha, int beta, int searchDepth)
{
    return
        hp.zobrist == zobrist &&
        // Should not need since we blank the zobrist at reset time.
        // hp.depth != HASH_NOENTRY &&

        // know eval exactly?
        (hp.eval.IsExactVal() ||
         // know it's good enough?
         hp.eval >= beta ||
         // know it's bad enough?
         hp.eval <= alpha) &&

        // is the hashed search deep enough?
        (QUIESCING || searchDepth <= hp.depth ||
         // For detected win/loss, depth does not matter.
         hp.eval.DetectedWinOrLoss());
}

size_t TransTable::calcEntry(uint64 zobrist) const
{
    // slow (although AMD is better than intel in this regard):
    // return zobrist % hash.size();

    // fastest, but only does good distribution for tables of size
    // (numEntries * 2^n):
    // return zobrist & (hash.size() - 1);

    // The basic idea here is that we want to map a number in the range
    // (0 .. 2^32) to a number in the range (0 .. numEntries - 1).  One way to
    // do that mapping is to observe (x * y) / x = y.  So we can take the
    // input (our zobrist), multiply by numEntries, divide by 2^32
    // (ie right-shift 32 bits) to achieve this result.
    //
    // We can't use the entire 64-bits of the zobrist key for the multiplication
    // because we can't get at the top 64 bits of the 128-bit result (in 32-bit
    // C++ ... although some CPU architectures probably support doing this in
    // assembly).
    //
    // Due to that limitation, and the fact that we might be working w/a 64-bit
    // 'numEntries', we restrict numEntries to 22 consecutive "really"
    // significant (but possibly shifted) bits to achieve a good distribution
    // of keys.  To fill in the bottom bits, we XOR in randomness from the top
    // 32 bits of the zobrist key.
    //
    // Finally, the calculation below restricts our max numEntries to (32 + 22)
    // bits.
#if 1
    return
        (((zobrist & 0xffffffffUL) * shiftedNumEntries) >> shiftCount) ^
        ((zobrist >> 32) & hashMask);
#endif
}

// Fills in 'hashEval' and 'hashMove' iff we had a successful hit.
// Should only be called by IsHit(), which does some pre-checks.
bool TransTable::hitTest(Eval *hashEval, MoveT *hashMove, uint64 zobrist,
                         int searchDepth, uint16 basePly, int alpha, int beta,
                         ThinkerStatsT *stats, size_t entry)
{
    HashPositionT &vHp = hash[entry];
    int8 hashDepth;

    SpinlockT &lock = locks[entry & (kNumHashLocks - 1)];
    SpinlockLock(&lock);

    if (!entryMatches(vHp, zobrist, alpha, beta, searchDepth))
    {
        SpinlockUnlock(&lock);
        return false;
    }

    // re-record items in the hit hash position to "reinforce" it
    // against future removal:
    // 1) base ply for this move.
    if (vHp.basePly != basePly)
    {
        stats->hashWroteNew++;
        vHp.basePly = basePly;
    }
    // 2) search depth (in case of checkmate, it might go up.  Not
    //    proven to be better.)
    hashDepth = MAX(vHp.depth, searchDepth);
    vHp.depth = hashDepth;
    stats->hashHitGood++;
    *hashEval = vHp.eval;
    *hashMove = vHp.move;

    SpinlockUnlock(&lock);

#ifdef ENABLE_DEBUG_LOGGING
    char tmpStr[MOVE_STRING_MAX];
    char peStr[kMaxEvalStringLen];
    LOG_DEBUG("hashHit alhbdmz: %d %s %d %d %s 0x%" PRIx64 "\n",
              alpha,
              hashEval->ToLogString(peStr),
              beta, hashDepth,
              hashMove->ToString(tmpStr, &gMoveStyleTT, NULL),
              zobrist);
#endif

    return true;
}

void TransTable::Prefetch(uint64 zobrist) const
{
    if (Size())
    {
        __builtin_prefetch(&hash[calcEntry(zobrist)]);
    }
}

void TransTable::ConditionalUpdate(Eval eval, MoveT move, uint64 zobrist,
                                   int searchDepth, uint16 basePly,
                                   ThinkerStatsT *stats)
{
    if (!Size())
        return;
    
    size_t entry = calcEntry(zobrist);
    HashPositionT &vHp = hash[entry];

    // Do we want to update the table?
    // (HASH_NOENTRY should always trigger here)
    if (searchDepth > vHp.depth ||
        // Replacing entries that came before this search is aggressive,
        // but it works better than a 'numPieces' comparison.  We use "!="
        // instead of "<" because we may move backwards in games as well
        // (undoing moves, or setting positions etc.)
        vHp.basePly != basePly ||
        // Otherwise, use the position that gives us as much info as
        // possible, and after that the most recently used (ie this move).
        (searchDepth == vHp.depth &&
         eval.Range() <= vHp.eval.Range()))
    {
        // We only lock the hashtable once we know we want to do an update.
        // This lets us do slightly lazier locking.

        // Every single element of this structure (except 'pad') should
        // always be updated, since:
        // -- it is not blanked for a newgame
        // -- the hash entry might have been overwritten in the meantime
        // (by another thread, or at a different ply).
        SpinlockT &lock = locks[entry & (kNumHashLocks - 1)];
        SpinlockLock(&lock);

        vHp.zobrist = zobrist;
        vHp.eval = eval;
        vHp.move = move; // may be MoveNone

        if (((volatile HashPositionT &)vHp).basePly != basePly)
        {
            stats->hashWroteNew++;
            vHp.basePly = basePly;
        }

        vHp.depth = searchDepth;

        SpinlockUnlock(&lock);

#ifdef ENABLE_DEBUG_LOGGING
        char tmpStr[MOVE_STRING_MAX];
        char peStr[kMaxEvalStringLen];
        
        LOG_DEBUG("hashupdate lhdpmz: %s %d %d %s 0x%" PRIx64 "\n",
                  eval.ToLogString(peStr),
                  searchDepth, basePly,
                  move.ToString(tmpStr, &gMoveStyleTT, NULL),
                  zobrist);
#endif
    }
}
