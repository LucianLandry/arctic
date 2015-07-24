//--------------------------------------------------------------------------
//               main.c - main initialization and runtime loop
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
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
#include <ctype.h>        // isdigit(3)
#include <stdio.h>
#include <stdlib.h>       // exit(3)
#include <unistd.h>       // isatty(3)

#include "aSystem.h"
#include "board.h"
#include "clockUtil.h"
#include "comp.h"
#include "game.h"
#include "gDynamic.h"
#include "gPreCalc.h"
#include "log.h"
#include "playloop.h"
#include "thinker.h"
#include "transTable.h"
#include "ui.h"
#include "uiUtil.h"


static void usage(char *programName)
{
    printf("usage: %s [-h=<hashtablesize>] [-p=<numcputhreads>] [--ui=<console,juce,uci,xboard>]\n"
           "\t'hashtablesize' examples: 200000, 100k, 0M, 1G\n"
           // as picked by TransTableDefaultSize()
           "\t'hashtablesize' default == MIN(1/3 total memory, 512M)\n"
           "\t(specifying 'hashtablesize' overrides any xboard/uci option)\n\n"
           "\t'numcputhreads' in range 1-%d\n"
           "\t'numcputhreads' default == number of online processors\n\n"
           "\t'ui' default == console (if stdin is terminal), or xboard (otherwise)\n",
           programName, MAX_NUM_PROCS);
    exit(0);
}

// Parse a user parameter like "400k", "1G", "25M" and return a real number.
// Since this is applied to memory, kibibytes/mebibytes/gibibytes are assumed.
// Returns: size, or -1 if an error occured.
// Strings like '0k' are treated as legit and return 0.
static int64 IECStringToInt64(char *str)
{
    int64 val = 0;
    int64 lastVal;
    int64 multiplier;
    bool parsedNum = false;
    char digitStr[2] = {}; // blank out

    // I considered using sscanf() but we'll just do it the hard way since
    // we want to be relatively strict in our sanity checking.
    for (; *str != '\0'; str++)
    {
        lastVal = val;
        if (isdigit(*str))
        {
            // (We allow leading zeros although they're useless)
            digitStr[0] = *str;
            val *= 10;
            val += atoi(digitStr);
            parsedNum = true;
        }
        // Exactly one suffix is allowed, and it must follow a number.
        else if ((*str == 'k' || *str == 'K' ||
                  *str == 'm' || *str == 'M' ||
                  *str == 'g' || *str == 'G' ||
                  *str == 't' || *str == 'T') && parsedNum && str[1] == '\0')
        {
            multiplier =
                *str == 'k' || *str == 'K' ? 1024 :
                *str == 'm' || *str == 'M' ? 1024 * 1024 :
                *str == 'g' || *str == 'G' ? 1024 * 1024 * 1024 :
                1024LL * 1024LL * 1024LL * 1024LL; // assume 't'/'T'
            val *= multiplier;
        }
        else
        {
            return -1; // error parsing string.
        }
        if (lastVal > val)
        {
            return -1; // detected overflow
        }
    }
    return val;
}

static bool isPow2(int c)
{
    return c > 0 && (c & (c - 1)) == 0;
}

static bool isValidUI(char *str)
{
    return
        !strcmp(str, "console") || !strcmp(str, "juce") ||
        !strcmp(str, "uci") || !strcmp(str, "xboard");
}

// Do some init-time sanity checking to confirm various assumptions.
static void startupSanityCheck(void)
{
    // Some structure sanity checks we probably should not rely on.
    assert(NUM_SAVED_POSITIONS >= 128 && isPow2(NUM_SAVED_POSITIONS));
}

int main(int argc, char *argv[])
{
    int hashTableSize = TRANSTABLE_DEFAULT_SIZE;
    char hashTableSizeString[40];
    int numCpuThreads = -1;
    int i;
    ThinkContextT th;
    GameT game;
    char uiString[80] = "";

    startupSanityCheck();

    LogInit();
    SystemEnableCoreFile(); // for debugging.

    // parse any cmd-line args.
    for (i = 1; i < argc; i++)
    {
        if (!strncmp(argv[i], "-h=", 3))
        {
            // Manually set hash table size.
            if (sscanf(argv[i], "-h=%40s", hashTableSizeString) != 1 ||
                (hashTableSize = IECStringToInt64(hashTableSizeString)) == -1)
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
        else if (!strncmp(argv[i], "--ui=", 5))
        {
            if (sscanf(argv[i], "--ui=%79s", uiString) != 1 ||
                !isValidUI(uiString))
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
    gPreCalcInit(hashTableSize != TRANSTABLE_DEFAULT_SIZE, numCpuThreads);

    TransTableLazyInit(hashTableSize);

    srandom(getBigTime() / 1000000);

    ThinkerInit(&th);

    gVars.maxLevel = 0; // (these are not really necessary as gVars is static)
    gVars.ponder = 0;

    gVars.maxNodes = NO_LIMIT;
    gVars.hiswin = 2;   // set for killer move heuristic
    gVars.canResign = true;

    GameInit(&game);
    
    gUI =
        !strcmp(uiString, "juce") ? uiJuceOps() :
        !strcmp(uiString, "console") ? uiNcursesOps() :
        !strcmp(uiString, "xboard") ? uiXboardOps() :
        !strcmp(uiString, "uci") ? uiUciOps() :
        isatty(fileno(stdin)) && isatty(fileno(stdout)) ?
        uiNcursesOps() : uiXboardOps();

    CompThreadInit(&th);
    uiThreadInit(&th, &game);

    GameNew(&game, &th);
    gUI->notifyReady();
    ClockStart(game.clocks[0]);

    // Enter main play loop.
    PlayloopRun(&game, &th);

    gUI->exit();

    return 0;
}
