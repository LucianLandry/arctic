/***************************************************************************
                 main.c - main initialization and runtime loop
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


#include <assert.h>
#include <stdio.h>
#include <stdlib.h>       // exit()
#include <unistd.h>       // isatty(3)
#include <sys/time.h>     // get+setrlimit(2)
#include <sys/resource.h>

#include "board.h"
#include "clockUtil.h"
#include "comp.h"
#include "game.h"
#include "gDynamic.h"
#include "gPreCalc.h"
#include "log.h"
#include "playloop.h"
#include "thinker.h"
#include "ui.h"
#include "uiUtil.h"


static void usage(char *programName)
{
    printf("usage: %s [-h=<numhashentries>] [-p=<numcputhreads>]\n"
	   "\t'numhashentries' in units of 1024\n"
	   "\t'numhashentries' must be 0, or a power of 2 between 1 and 512 "
	   "(inclusive).\n"
	   "\t'numhashentries' default == 1/4 total memory\n\n"
	   "\t'numcputhreads' in range 1-%d\n"
	   "\t'numcputhreads' default == number of online processors\n",
	   programName, MAX_NUM_PROCS);
    exit(0);
}


static int isPow2(int c)
{
    return c > 0 && (c & (c - 1)) == 0;
}


void enableCoreFile(void)
{
    struct rlimit rlimit;
    int rv;

    rv = getrlimit(RLIMIT_CORE, &rlimit);
    assert(rv == 0);

    if (rlimit.rlim_cur < rlimit.rlim_max)
    {
	rlimit.rlim_cur = rlimit.rlim_max;

	rv = setrlimit(RLIMIT_CORE, &rlimit);
	assert(rv == 0);
    }
}


// Do some init-time sanity checking to confirm various assumptions.
static void startupSanityCheck(void)
{
    // Some structure sanity checks we probably should not rely on.
    // See board.h comments for 'zobrist' field.
    assert(offsetof(BoardT, zobrist) + 4 == offsetof(BoardT, hashCoord));
    assert(offsetof(BoardT, zobrist) + 4 + 32 == offsetof(BoardT, cbyte));
    assert(offsetof(BoardT, zobrist) + 4 + 32 + 1 == offsetof(BoardT, ebyte));

    assert(NUM_SAVED_POSITIONS >= 128 && isPow2(NUM_SAVED_POSITIONS));
}


int main(int argc, char *argv[])
{
    int numHashEntries = -1;
    int numCpuThreads = -1;
    int i;
    ThinkContextT th;
    GameT game;

#ifdef ENABLE_DEBUG_LOGGING // bldbg
    FILE *errFile = fopen("errlog", "w");
    assert(errFile != NULL);
    LogSetFile(errFile);
    // turn off buffering -- useful when we're crashing, but slow.
    setbuf(errFile, NULL);
    // LogSetLevel(eLogDebug);
#endif

    startupSanityCheck();

    // enableCoreFile(); for debugging.

    /* parse any cmd-line args. */
    for (i = 1; i < argc; i++)
    {
	if (!strncmp(argv[i], "-h=", 3))
	{
	    // Manually set hash table size.
	    if (sscanf(argv[i], "-h=%d", &numHashEntries) != 1 ||
		(numHashEntries != 0 &&
		 (!isPow2(numHashEntries) || numHashEntries > 512)))
	    {
		usage(argv[0]);
	    }
	}
	else if (!strncmp(argv[i], "-p=", 3))
	{
	    // Manually set number of CPU threads.
	    if (sscanf(argv[i], "-p=%d", &numCpuThreads) != 1 ||
		numCpuThreads < 1 ||
		numCpuThreads > MAX_NUM_PROCS)
	    {
		usage(argv[0]);
	    }	    
	}
	else
	{
	    // Unrecognized argument.
	    usage(argv[0]);
	}
    }

    // must be done before seeding, if we want reproducable results.
    gPreCalcInit(numHashEntries > 0 ? numHashEntries * 1024 : numHashEntries,
		 numCpuThreads);

    srandom(getBigTime() / 1000000);

    ThinkerInit(&th);

    gVars.maxLevel = 0; // (these are not really necessary as gVars is static)
    gVars.ponder = 0;

    gVars.maxNodes = NO_LIMIT;
    gVars.hiswin = 2;	// set for killer move heuristic
    gVars.resignThreshold = EVAL_LOSS_THRESHOLD;

    GameInit(&game);
    
    gUI = isatty(fileno(stdin)) && isatty(fileno(stdout)) ?
	uiNcursesInit(&game) : uiXboardInit();

    GameNew(&game, &th);

    CompThreadInit(&th);
    uiThreadInit(&th, &game);

    gUI->notifyReady();
    ClockStart(game.clocks[0]);

    // Enter main play loop.
    PlayloopRun(&game, &th);

    gUI->exit();

    return 0;
}
