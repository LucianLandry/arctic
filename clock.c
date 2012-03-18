//--------------------------------------------------------------------------
//                          clock.c - clock control
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

#include <stdio.h>     // sprintf()
#include <ctype.h>     // isdigit()
#include <assert.h>
#include <sys/time.h>  // gettimeofday()
#include <time.h>
#include <stdlib.h>    // abs()
#include "ref.h"
#include "clock.h"
#include "clockUtil.h"
#include "uiUtil.h"

#define CLOCK_TIME_INFINITE_STR "inf"

static bigtime_t calcTimeTaken(ClockT *myClock)
{
    return getBigTime() - myClock->turnStartTime;
}

void ClockInit(ClockT *myClock)
{
    memset(myClock, 0, sizeof(myClock));
    myClock->startTime = CLOCK_TIME_INFINITE;
    myClock->time = CLOCK_TIME_INFINITE;
    myClock->perMoveLimit = CLOCK_TIME_INFINITE;
}


void ClockReset(ClockT *myClock)
{
    ClockStop(myClock);
    myClock->time = myClock->startTime;
}


void ClockStop(ClockT *myClock)
{
    if (myClock->bRunning)
    {
	myClock->bRunning = false;
	myClock->timeTaken = calcTimeTaken(myClock);

	if (!ClockIsInfinite(myClock))
	{
	    myClock->time -= myClock->timeTaken;
	}
    }
}


void ClockStart(ClockT *myClock)
{
    if (!myClock->bRunning)
    {
	myClock->bRunning = true;
	myClock->turnStartTime = getBigTime();
    }
}


static void ClockAddTime(ClockT *myClock, bigtime_t myTime)
{
    if (myClock->time == CLOCK_TIME_INFINITE)
    {
	return;
    }
    if (myTime == CLOCK_TIME_INFINITE)
    {
	myClock->time = myTime;
    }
    else
    {
	myClock->time += myTime;
    }
}

// Adjust clock by its appropriate increment.  Meant to be applied just
// *after* we make our move (meaning: it is no longer our turn).
//
// We do this, since in chess you normally adjust time after your
// move is made.
void ClockApplyIncrement(ClockT *myClock, int ply)
{
    if (ClockIsInfinite(myClock))
    {
	return;
    }

    // Apply per-move increment (if any)
    ClockAddTime(myClock, myClock->inc);

    // Add any time from a new time control.
    if (myClock->timeControlPeriod)
    {
	if (// add 2 instead of 1, if want to apply 'before' move
	    ((ply + 1) >> 1) % myClock->timeControlPeriod == 0)
	{
	    ClockAddTime(myClock, myClock->startTime);
	}
    }
    else if (myClock->numMovesToNextTimeControl == 1)
    {
	ClockAddTime(myClock, myClock->startTime);
    }
}



bigtime_t ClockGetTime(ClockT *myClock)
{
    if (myClock->bRunning && !ClockIsInfinite(myClock))
    {
	return myClock->time - calcTimeTaken(myClock);
    }
    return myClock->time;
}

// Get the remaining per-move time of a running clock.
// Returns the per-move limit if the clock is not actually running.
bigtime_t ClockGetPerMoveTime(ClockT *myClock)
{
    if (myClock->perMoveLimit == CLOCK_TIME_INFINITE ||
	!ClockIsRunning(myClock))
    {
	return myClock->perMoveLimit;
    }
    return myClock->perMoveLimit - calcTimeTaken(myClock);
}

void ClockSetTime(ClockT *myClock, bigtime_t myTime)
{
    int wasRunning = ClockIsRunning(myClock);

    // This sequence resets the turnStartTime.
    ClockStop(myClock);
    myClock->time = myTime;
    if (wasRunning)
    {
	ClockStart(myClock);
    }
}

bigtime_t ClockTimeTaken(ClockT *myClock)
{
    return ClockIsRunning(myClock) ?
	calcTimeTaken(myClock) : myClock->timeTaken;
}


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
static int strCount(char *haystack, char *needle)
{
    int retVal = 0;
    char *occur;

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
    memcpy(&game->actualClocks[0],
	   &game->origClocks[0],
	   sizeof(ClockT) * NUM_PLAYERS);
    
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
    {
	ClockStop(game->clocks[i]);
    }
}


void ClocksPrint(GameT *game, char *context)
{
    int i;
    ClockT *myClock;
    for (i = 0; i < NUM_PLAYERS; i++)
    {
	myClock = game->clocks[i];
	printf("ClocksPrint(%s): clock %d: %lld %lld %d %lld %c\n",
	       context ? context : "",
	       i,
	       (long long) ClockGetTime(myClock),
	       (long long) ClockGetInc(myClock),
	       ClockGetTimeControlPeriod(myClock),
	       (long long) ClockGetPerMoveLimit(myClock),
	       ClockIsRunning(myClock) ? 'r' : 's');
    }
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
void GoaltimeCalc(GameT *game)
{
    int turn = game->savedBoard.turn;
    int ply = game->savedBoard.ply;
    ClockT *myClock = game->clocks[turn];
    bigtime_t myTime, calcTime, altCalcTime, myInc, safeTime,
	myPerMoveLimit, safeMoveLimit;
    int myTimeControlPeriod, numMovesToNextTimeControl;
    int numIncs;

    myTime = ClockGetTime(myClock);
    myPerMoveLimit = ClockGetPerMoveLimit(myClock);
    myTimeControlPeriod = ClockGetTimeControlPeriod(myClock);
    numMovesToNextTimeControl = ClockGetNumMovesToNextTimeControl(myClock);
    myInc = ClockGetInc(myClock);

    safeMoveLimit =
	myPerMoveLimit == CLOCK_TIME_INFINITE ?
	myPerMoveLimit :
	MAX(myPerMoveLimit - MIN_TIME, 0);

    // Degenerate case.
    if (ClockIsInfinite(myClock))
    {
	game->goalTime[turn] =
	    myPerMoveLimit == CLOCK_TIME_INFINITE ?
	    CLOCK_TIME_INFINITE :
	    myTime - safeMoveLimit;
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

	calcTime += (ClockGetStartTime(myClock) * numIncs) / GAME_NUM_MOVES;

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
