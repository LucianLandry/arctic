//--------------------------------------------------------------------------
//                clockUtil.cpp - supplementary clock routines
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU Lesser General Public License as
//   published by the Free Software Foundation; either version 2.1 of the
//   License, or (at your option) any later version.
//
//--------------------------------------------------------------------------

#include <assert.h>
#include <sys/time.h> // gettimeofday(2)

#include "clockUtil.h"

#define CLOCK_TIME_INFINITE_STR "inf"

// We want either xx:yy:zz, yy:zz, or (:)zz.
// (or CLOCK_TIME_INFINITE_STR, --> "inf")
// But we try to be permissive in what we accept.
bool TimeStringIsValid(char *str)
{
    int colCount = 0;
    bool isValid = true;

    if (!strcmp(str, CLOCK_TIME_INFINITE_STR))
    {
        return true;
    }

    if (*str == '\0')
    {
        return false;
    }
    do
    {
        if (// must be digit or ':'
            (!isdigit(*str) && *str != ':') ||
            // ':' must have a number after it, and cannot be > 2 numbers.
            (*str == ':' && (++colCount > 2 || !isdigit(*(str + 1)))))
        {
            isValid = false;
            break;
        }
    } while (*(++str) != '\0');

    return isValid;
}


// Count number of occurances of 'needle' in 'haystack'.
static int strCount(const char *haystack, const char *needle)
{
    int retVal = 0;
    const char *occur;

    for (retVal = 0;
         (occur = strstr(haystack, needle)) != NULL;
         haystack = occur + strlen(needle), retVal++)
        ; // no-op

    return retVal;
}


// Returns numerical form of time.  Asserts if invalid time.  This is to make
// sure we catch coding errors -- user input should be checked with
// TimeStringIsValid().  (This does currently mean we cannot convert a negative
// time, even though TimeStringFromBigTime can go the other way with a negative
// time.)
bigtime_t TimeStringToBigTime(char *str)
{
    int hours = 0, minutes = 0, seconds = 0;

    if (!TimeStringIsValid(str))
    {
        assert(0);
    }

    if (!strcmp(str, CLOCK_TIME_INFINITE_STR))
    {
        return CLOCK_TIME_INFINITE;
    }

    switch(strCount(str, ":"))
    {
    case 2:
        if (sscanf(str, "%d:%d:%d", &hours, &minutes, &seconds) < 3 &&
            sscanf(str, ":%d:%d", &minutes, &seconds) < 2)
        {
            assert(0);
        }
        break;
    case 1:
        if (sscanf(str, "%d:%d", &minutes, &seconds) < 2 &&
            sscanf(str, ":%d", &seconds) < 1)
        {
            assert(0);
        }
        break;
    case 0:
        if (sscanf(str, "%d", &seconds) < 1)
        {
            assert(0);
        }
        break;
    default:
        assert(0);
    }

    return ((bigtime_t) (hours * 3600 + minutes * 60 + seconds)) * 1000000;
}


// stores string representation of 'myTime' into 'result'.
// Also returns 'result'.
// 'result' must be at least CLOCK_TIME_STR_LEN bytes long.
char *TimeStringFromBigTime(char *result, bigtime_t myTime)
{
    int hours, minutes, seconds;
    int negative;

    if (myTime == CLOCK_TIME_INFINITE)
    {
        sprintf(result, CLOCK_TIME_INFINITE_STR);
        return result;
    }

    // This rounds us up to the nearest second.
    // (The -1000000 check gets the math right for negative numbers.)
    if (myTime > -1000000 && myTime % 1000000)
    {
        myTime += 1000000;
    }

    negative = (myTime < 0);
    myTime = llabs(myTime);

    // Direction of truncation is machine-dependent for negative numbers,
    // so make sure we only use positive numbers here.
    myTime /= 1000000; // convert to seconds representation

    hours = myTime / 3600;
    myTime -= (hours * 3600);
    minutes = myTime / 60;
    myTime -= (minutes * 60);
    seconds = myTime;

    if (hours)
    {
        sprintf(result, "%s%d:%02d:%02d",
                negative ? "-" : "", hours, minutes, seconds);
    }
    else if (minutes)
    {
        sprintf(result, "%s%d:%02d",
                negative ? "-" : "", minutes, seconds);
    }
    else
    {
        sprintf(result, "%s:%02d",
                negative ? "-" : "", seconds);
    }
    return result;
}


void ClocksReset(GameT *game)
{
    for (int i = 0; i < NUM_PLAYERS; i++)
        game->actualClocks[i] = game->origClocks[i];
    
    if (GameCurrentPly(game) == 0)
    {
        // Propagate changes to the SaveGameT -- we assume the game is not
        // in progress.
        SaveGameClocksSet(&game->sgame, game->clocks);
    }
}


void ClocksStop(GameT *game)
{
    int i;
    for (i = 0; i < NUM_PLAYERS; i++)
        game->clocks[i]->Stop();
}


void ClocksPrint(GameT *game, char *context)
{
    int i;
    Clock *myClock;
    for (i = 0; i < NUM_PLAYERS; i++)
    {
        myClock = game->clocks[i];
        printf("ClocksPrint(%s): clock %d: %lld %lld %d %lld %c\n",
               context ? context : "",
               i,
               (long long) myClock->Time(),
               (long long) myClock->Increment(),
               myClock->TimeControlPeriod(),
               (long long) myClock->PerMoveLimit(),
               myClock->IsRunning() ? 'r' : 's');
    }
}


// Syntactic sugar.
bool ClocksICS(GameT *game)
{
    return game->icsClocks && game->savedBoard.Ply() < 2;
}

// Expected number of moves in a game.  Actually a little lower, as this is
// biased toward initial moves.  The idea is that we would rather have less
// time at the end to think about a won position than more time to think about
// a lost position.
#define GAME_NUM_MOVES 40
// Minimum time we want left on the clock, presumably to compensate for
// lag, in usec (however, normally we rely on timeseal to compensate for
// network lag)
#define MIN_TIME 500000
// The clock doesn't run on the first move in an ICS game.
// But as a courtesy, refuse to think over 5 seconds (unless our clock has
// infinite time anyway)
#define ICS_FIRSTMOVE_LIMIT 5000000
void GoaltimeCalc(GameT *game)
{
    uint8 turn = game->savedBoard.Turn();
    int ply = game->savedBoard.Ply();
    Clock *myClock = game->clocks[turn];
    bigtime_t myTime, calcTime, altCalcTime, myInc, safeTime,
        myPerMoveLimit, safeMoveLimit;
    int myTimeControlPeriod, numMovesToNextTimeControl;
    int numIncs;

    myTime = myClock->Time();
    myPerMoveLimit = myClock->PerMoveLimit();
    myTimeControlPeriod = myClock->TimeControlPeriod();
    numMovesToNextTimeControl = myClock->NumMovesToNextTimeControl();
    myInc = myClock->Increment();

    safeMoveLimit =
        myPerMoveLimit == CLOCK_TIME_INFINITE ? CLOCK_TIME_INFINITE :
        myPerMoveLimit - MIN_TIME;      

    if (ClocksICS(game))
    {
        safeMoveLimit = MIN(safeMoveLimit, ICS_FIRSTMOVE_LIMIT);
    }
    safeMoveLimit = MAX(safeMoveLimit, 0);

    // Degenerate case.
    if (myClock->IsInfinite())
    {
        game->goalTime[turn] =
            myPerMoveLimit == CLOCK_TIME_INFINITE ?
            CLOCK_TIME_INFINITE :
            safeMoveLimit;
        return;
    }

    safeTime = MAX(myTime - MIN_TIME, 0);

    // 'calcTime' is the amount of time we want to think.
    calcTime = safeTime / GAME_NUM_MOVES;

    if (myTimeControlPeriod || numMovesToNextTimeControl)
    {
        // Anticipate the additional time we will possess to make our
        // GAME_NUM_MOVES moves due to time-control increments.
        if (myTimeControlPeriod)
        {
            numMovesToNextTimeControl =
                myTimeControlPeriod - ((ply >> 1) % myTimeControlPeriod);
        }
        numIncs = GAME_NUM_MOVES <= numMovesToNextTimeControl ? 0 :
            1 + (myTimeControlPeriod ?
                 ((GAME_NUM_MOVES - numMovesToNextTimeControl - 1) /
                  myTimeControlPeriod) :
                 0);

        calcTime += (myClock->StartTime() * numIncs) / GAME_NUM_MOVES;
        // However, say we have :30 on the clock, 10 moves to make, and a one-
        // minute increment every two moves.  We want to burn only :15.
        altCalcTime = safeTime / MIN(GAME_NUM_MOVES,
                                     numMovesToNextTimeControl);
        calcTime = MIN(calcTime, altCalcTime);
    }

    // Anticipate the additional time we will possess to make our
    // GAME_NUM_MOVES moves due to increments.
    if (myInc)
    {
        numIncs = GAME_NUM_MOVES - 1;
        calcTime += (myInc * numIncs) / GAME_NUM_MOVES;
        // Fix cases like 10 second start time, 22 second increment
        calcTime = MIN(calcTime, safeTime);
    }

    // Do not think over any per-move limit.
    if (safeMoveLimit != CLOCK_TIME_INFINITE)
    {
        calcTime = MIN(calcTime, safeMoveLimit);
    }

    // Refuse to think for a "negative" time.
    calcTime = MAX(calcTime, 0);

    game->goalTime[turn] = myTime - calcTime;
}


bigtime_t getBigTime(void)
{
    struct timeval tmvalue;
    gettimeofday(&tmvalue, NULL);
    return ((bigtime_t) tmvalue.tv_sec) * 1000000 + tmvalue.tv_usec;
}
