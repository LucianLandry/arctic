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

// This lockless (therefore not entirely- but mostly- accurate)
// algorithm uses ideas from Bob Hyatt's lockless transposition table paper,
// seen at http://www.cis.uab.edu/hyatt/hashing.html.
//
// Since we use 3 64-bit entries to store a position rather than two, its
// guarantees against false hits are slightly weaker.
//
// I attempted making TransTable(Read(),Hit(),Write()) static inline, but in
// my testing that actually slowed things down.  I dismissed the idea of
// using a function to locate a particular zobrist's hashPosition and feeding
// the result to the above functions (to save % ops) just due to the API
// complication and the overhead of another function call.

#include <assert.h>
#include <stdlib.h>

#include "aSystem.h"
#include "aTypes.h"
#include "log.h"
#include "transTable.h"

typedef struct {
    uint64 word1;
    uint64 word2;
} HashWordsT;

typedef union {
    HashInfoT info;
    HashWordsT w;
} HashUnionT;

typedef struct {
    uint64 zobrist;
    HashInfoT info;
} HashPositionT;

// The below entries are size_t because it does not make sense to try to
// force a 64-bit size on a 32-bit platform.  (realloc() would fail)  I would
// use 'long' but that is defined to 32 bits on win64.
static struct {
    size_t numEntries;
    size_t size; // in bytes, current size of hash table
    size_t nextSize; // in bytes, takes effect on next reset
    HashPositionT *hash; // transposition table.
    uint8 salt; // random counter.
} gHash;


static int64 normalizeSize(int64 size)
{
    return (size / sizeof(HashPositionT)) * sizeof(HashPositionT);
}

// Return the maximum possible size of the transposition table.
size_t TransTableMaxSize(void)
{
    // Refuse to go over (total detected system memory - 32M)
    int64 result = SystemTotalMemory() - 32 * 1024 * 1024;
    result = MAX(result, 0);
    result = MIN(result, INT64_MAX);
    result = MIN(result, SIZE_MAX);
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
	MIN(normalizeSize(size), TransTableMaxSize());
}

static void sanityCheck(void)
{
    assert(sizeof(HashInfoT) == 16);
    assert(sizeof(HashPositionT) == 24);
    assert(sizeof(HashWordsT) == 16);
    assert(sizeof(HashUnionT) == 16);
}

// Initialize the global transposition table to size 'size'.
void TransTableInit(int64 size)
{
    HashPositionT *newHash, newHashEntry;
    int i;
    sanityCheck();

    size = sanitizeSize(size);

    if (size != gHash.size)
    {
	newHash = realloc(gHash.hash, size);
	if (size == 0 || newHash != NULL)
	{
	    gHash.size = size;
	    gHash.nextSize = size;
	    gHash.hash = newHash;
	    gHash.numEntries = size / sizeof(HashPositionT);
	}
	else
	{
	    LOG_EMERG("Failed to init hash (size %"PRId64")\n", size);
	    exit(0);
	}
    }

    memset(&newHashEntry, 0, sizeof(newHashEntry));
    newHashEntry.info.depth = HASH_NOENTRY;
    for (i = 0; i < gHash.numEntries; i++)
    {
	gHash.hash[i] = newHashEntry; // struct assign
    }
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
    int i;

    if (gHash.nextSize != gHash.size)
    {
	TransTableInit(gHash.nextSize);
	return;
    }

    for (i = 0; i < gHash.numEntries; i++)
    {
	gHash.hash[i].info.depth = HASH_NOENTRY;
    }
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

// Read an entry from the transposition table w/out checking if there is a hit.
void TransTableRead(HashInfoT *result, uint64 zobrist)
{
    *result = (volatile HashInfoT) gHash.hash[zobrist % gHash.numEntries].info;
}

// Read the transposition table; return 'true' if the entry matched 'zobrist'
// or 'false' otherwise.
bool TransTableHit(HashInfoT *result, uint64 zobrist)
{
    HashPositionT *hp = &gHash.hash[zobrist % gHash.numEntries];
    *result = (volatile HashInfoT) hp->info;
    return hp->zobrist ==
	(zobrist ^
	 ((HashUnionT *) result)->w.word1 ^
	 ((HashUnionT *) result)->w.word2);
}

// Write an entry to the transposition table.
void TransTableWrite(HashInfoT *hInfo, uint64 zobrist)
{
    HashPositionT *hp = &gHash.hash[zobrist % gHash.numEntries];

    hInfo->salt = gHash.salt++;

    hp->zobrist =
	zobrist ^
	((HashUnionT *) hInfo)->w.word1 ^
	((HashUnionT *) hInfo)->w.word2;
    hp->info = (volatile HashInfoT) *hInfo;
}
