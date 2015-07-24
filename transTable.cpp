//--------------------------------------------------------------------------
//                  transTable.c - Transposition table support.
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
#include "transTable.h"
#include "uiUtil.h"

typedef struct {
    uint64 zobrist;
    PositionEvalT eval;
    MoveT move;       // stores preferred move for this position.
    uint16 basePly;   // lets us evaluate if this entry is 'too old'.
    int8 depth;       // needs to be plys from quiescing, due to incremental
                      // search.
    int8 pad;         // unused
} HashPositionT;

#define NUM_HASH_LOCKS (1024)

// The below entries are size_t because it does not make sense to try to
// force a 64-bit size on a 32-bit platform.  (realloc() would fail)  I would
// use 'long' but that is defined to 32 bits on win64.
static struct {
    bool locksInitialized;
    SpinlockT locks[NUM_HASH_LOCKS];
    size_t numEntries;
    size_t size; // in bytes, current size of hash table
    size_t nextSize; // in bytes, takes effect on next reset
    HashPositionT *hash; // transposition table.

    // Support for quick(er) hash entry calculation:
    size_t hashMask; // could be 'int' if needed
    size_t shiftedNumEntries; // could be 'int' if needed
    int shiftCount;
    int numLeadingZeros;
} gHash;

#ifdef ENABLE_DEBUG_LOGGING
static const MoveStyleT gMoveStyleTT = { mnCAN, csOO, true };
#endif

int calcNumLeadingZeros(size_t numEntries)
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
size_t calcHashMask(size_t numEntries)
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

static size_t normalizeNumEntries(size_t numEntries)
{
    int shiftCount = 0;

    if (numEntries < 1)
    {
        return 0;
    }

    // numEntries should be a mult of NUM_HASH_LOCKS
    numEntries /= NUM_HASH_LOCKS;
    numEntries *= NUM_HASH_LOCKS;

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
    // 0000xxxxxxxx0000 (binary)
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

static int64 normalizeSize(int64 size)
{
    int64 result = constrainSize(size) / sizeof(HashPositionT); // calc numEntries
    result = normalizeNumEntries(result);
    return result * sizeof(HashPositionT);
}

// Return the maximum possible size of the transposition table.
size_t TransTableMaxSize(void)
{
    // Refuse to go over (total detected system memory - 32M)
    int64 result = SystemTotalMemory() - 32 * 1024 * 1024;
    return normalizeSize(result);
}

// Return the default size of the transposition table.
size_t TransTableDefaultSize(void)
{
    // As a convenience, pick MIN(1/3 total memory, 512M).
    int64 size = SystemTotalMemory() / 3;
    size = MIN(size, 512 * 1024 * 1024);
    return normalizeSize(size);
}

static size_t sanitizeSize(int64 size)
{
    return
        size == TRANSTABLE_DEFAULT_SIZE ? TransTableDefaultSize() :
        size < -1 ? 0 : // bad parameter
        MIN((uint64) normalizeSize(size), TransTableMaxSize());
}

static void sanityCheck(void)
{
    assert(sizeof(HashPositionT) == 24);
}

static void resetEntries(void)
{
    HashPositionT newHashEntry;
    uint64 i;

    memset(&newHashEntry, 0, sizeof(newHashEntry));
    newHashEntry.depth = HASH_NOENTRY;
    newHashEntry.move = gMoveNone;

    for (i = 0; i < gHash.numEntries; i++)
    {
        gHash.hash[i] = newHashEntry; // struct assign
    }
}

// Initialize the global transposition table to size 'size'.
void TransTableInit(int64 size)
{
    HashPositionT *newHash;
    int i;
    sanityCheck();

    size = sanitizeSize(size);

    if (!gHash.locksInitialized)
    {
        for (i = 0; i < NUM_HASH_LOCKS; i++)
        {
            SpinlockInit(&gHash.locks[i]);
        }
        gHash.locksInitialized = true;
    }

    if ((uint64) size != gHash.size)
    {
        newHash = (HashPositionT *) realloc(gHash.hash, size);
        if (size == 0 || newHash != NULL)
        {
            gHash.size = size;
            gHash.nextSize = size;
            gHash.hash = newHash;
            gHash.numEntries = size / sizeof(HashPositionT);
            gHash.numLeadingZeros = calcNumLeadingZeros(gHash.numEntries);
            gHash.hashMask = calcHashMask(gHash.numEntries);
            gHash.shiftedNumEntries = gHash.numEntries >> gHash.numLeadingZeros;
            gHash.shiftCount = 32 - gHash.numLeadingZeros;
        }
        else
        {
            LOG_EMERG("Failed to init hash (size %" PRId64 ")\n", size);
            exit(0);
        }
    }

    resetEntries();
}

// As above, but postpone the actual initialization until the next
// TransTableReset() call.
void TransTableLazyInit(int64 size)
{
    sanityCheck();
    gHash.nextSize = sanitizeSize(size);
}

// Clear the global transposition table.
void TransTableReset(void)
{
    if (!gHash.locksInitialized || gHash.nextSize != gHash.size)
    {
        TransTableInit(gHash.nextSize);
        return;
    }

    resetEntries();
}

// Return the number of unique entries that can be stored in the transposition
// table.
size_t TransTableNumEntries(void)
{
    return gHash.numEntries;
}

// Return the current size of the transposition table.
size_t TransTableSize(void)
{
    return gHash.size;
}

#define QUIESCING (searchDepth < 0)

static inline bool entryMatches(HashPositionT *hp, uint64 zobrist,
                                int alpha, int beta, int searchDepth)
{
    return
        hp->zobrist == zobrist &&
        // Should not need since we blank the zobrist at reset time.
        // hp->depth != HASH_NOENTRY &&

        // know eval exactly?
        (hp->eval.highBound == hp->eval.lowBound ||
         // know it's good enough?
         hp->eval.lowBound >= beta ||
         // know it's bad enough?
         hp->eval.highBound <= alpha) &&

        // is the hashed search deep enough?
        (QUIESCING || searchDepth <= hp->depth ||
         // For detected win/loss, depth does not matter.
         hp->eval.highBound <= EVAL_LOSS_THRESHOLD ||
         hp->eval.lowBound >= EVAL_WIN_THRESHOLD);
}

static inline size_t calcEntry(uint64 zobrist)
{
    // slow (although AMD is better than intel in this regard):
    // return zobrist % gHash.numEntries;

    // fastest, but only does good distribution for tables of size
    // (numEntries * 2^n):
    // return zobrist & (gHash.numEntries - 1);

    // The basic idea here is that we want to map a number in the range
    // (0 .. 2^32) to a number in the range (0 .. numEntries - 1).  One way to
    // do that mapping is to observe (x * y) / x = y.  So we can take the
    // input (our zobrist), multiply by numEntries, divide by 2^32
    // (ie right-shift 32 bits) to achieve this result.
    //
    // We can't use the entire 64-bits of the zobrist key for the multiplication
    // because we can't get at the top 64 bits of the 128-bit result (in C ...
    // although some CPU architectures probably support doing this in assembly).
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
        (((zobrist & 0xffffffffUL) * gHash.shiftedNumEntries) >>
         gHash.shiftCount) ^
        ((zobrist >> 32) & gHash.hashMask);
#endif
}

// Fills in 'hashEval' and 'hashMove' iff we had a successful hit.
// Assumes TransTableQuickHitTest() returned true.
bool TransTableHit(PositionEvalT *hashEval, MoveT *hashMove, uint64 zobrist,
                   int searchDepth, uint16 basePly, int alpha, int beta)
{
    size_t entry = calcEntry(zobrist);
    HashPositionT *vHp = &gHash.hash[entry];
    int8 hashDepth;
    SpinlockT *lock;

#ifdef ENABLE_DEBUG_LOGGING
    char tmpStr[MOVE_STRING_MAX];
    char peStr[POSITIONEVAL_STRING_MAX];
#endif

    lock = &gHash.locks[entry & (NUM_HASH_LOCKS - 1)];
    SpinlockLock(lock);
    if (!entryMatches(vHp, zobrist, alpha, beta, searchDepth))
    {
        SpinlockUnlock(lock);
        return false;
    }

    // re-record items in the hit hash position to "reinforce" it
    // against future removal:
    // 1) base ply for this move.
    if (vHp->basePly != basePly)
    {
        gStats.hashWroteNew++;
        vHp->basePly = basePly;
    }
    // 2) search depth (in case of checkmate, it might go up.  Not
    //    proven to be better.)
    hashDepth = MAX(vHp->depth, searchDepth);
    vHp->depth = hashDepth;
    gStats.hashHitGood++;
    *hashEval = vHp->eval;
    *hashMove = vHp->move;

    SpinlockUnlock(lock);
    LOG_DEBUG("hashHit alhbdmz: %d %s %d %d %s 0x%" PRIx64 "\n",
              alpha,
              PositionEvalToLogString(peStr, hashEval),
              beta, hashDepth,
              MoveToString(tmpStr, *hashMove, &gMoveStyleTT, NULL),
              zobrist);

    return true;
}

void TransTablePrefetch(uint64 zobrist)
{
    if (gHash.size)
    {
        __builtin_prefetch(&gHash.hash[calcEntry(zobrist)]);
    }
}

// The point of this function is to avoid passing in a bunch of params and
// locking the hash when we don't need to.
bool TransTableQuickHitTest(uint64 zobrist)
{
    return gHash.size &&
        gHash.hash[calcEntry(zobrist)].zobrist == zobrist;
}

void TransTableConditionalUpdate(PositionEvalT eval, MoveT move, uint64 zobrist,
                                 int searchDepth, uint16 basePly)
{
    size_t entry = calcEntry(zobrist);
    HashPositionT *vHp = &gHash.hash[entry];
#ifdef ENABLE_DEBUG_LOGGING
    char tmpStr[MOVE_STRING_MAX];
    char peStr[POSITIONEVAL_STRING_MAX];
#endif

    // Do we want to update the table?
    // (HASH_NOENTRY should always trigger here)
    if (searchDepth > vHp->depth ||
        // Replacing entries that came before this search is aggressive,
        // but it works better than a 'numPieces' comparison.  We use "!="
        // instead of "<" because we may move backwards in games as well
        // (undoing moves, or setting positions etc.)
        vHp->basePly != basePly ||
        // Otherwise, use the position that gives us as much info as
        // possible, and after that the most recently used (ie this move).
        (searchDepth == vHp->depth &&
         (eval.highBound - eval.lowBound) <=
         (vHp->eval.highBound - vHp->eval.lowBound)))
    {
        // We only lock the hashtable once we know we want to do an update.
        // This lets us do slightly lazier locking.

        // Every single element of this structure (except 'pad') should
        // always be updated, since:
        // -- it is not blanked for a newgame
        // -- the hash entry might have been overwritten in the meantime
        // (by another thread, or at a different ply).
        SpinlockT *lock = &gHash.locks[entry & (NUM_HASH_LOCKS - 1)];
        SpinlockLock(lock);

        vHp->zobrist = zobrist;
        vHp->eval = eval;
        vHp->move = move; // may be gMoveNone

        if (((volatile HashPositionT *)vHp)->basePly != basePly)
        {
            gStats.hashWroteNew++;
            vHp->basePly = basePly;
        }

        vHp->depth = searchDepth;

        SpinlockUnlock(lock);

        LOG_DEBUG("hashupdate lhdpmz: %s %d %d %s 0x%" PRIx64 "\n",
                  PositionEvalToLogString(peStr, &eval),
                  searchDepth, basePly,
                  MoveToString(tmpStr, move, &gMoveStyleTT, NULL),
                  zobrist);
    }
}
