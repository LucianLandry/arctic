/***************************************************************************
                            clock.c - clock control
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
#include <ctype.h>
#include <stdlib.h>
#include "ref.h"

#define CLOCK_TIME_INFINITE_STR "inf"

void ClockStop(ClockT *myClock)
{
    if (myClock->bRunning)
    {
	myClock->bRunning = 0;
	myClock->timeTaken = getBigTime() - myClock->startTime;

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
	myClock->bRunning = 1;
	myClock->startTime = getBigTime();
    }
}


// Adjust clock by its appropriate increment.  Meant to be applied just
// *before* we make our move (meaning: it is still our turn).
//
// This is somewhat confusing, since in chess you adjust time after your
// move is made.  It probably leads to slightly simpler code, though.
void ClockApplyIncrement(ClockT *myClock, BoardT *board)
{
    if (!ClockIsInfinite(myClock) &&
	myClock->incPeriod &&
	// add 1 instead of 2, if want to apply 'after' move
	((board->ply + 2) & ~0x1) % (myClock->incPeriod * 2) == 0)
    {
	myClock->time += myClock->inc;
    }
}


bigtime_t ClockCurrentTime(ClockT *myClock)
{
    if (myClock->bRunning && !ClockIsInfinite(myClock))
    {
	return myClock->time - (getBigTime() - myClock->startTime);
    }
    return myClock->time;
}


void ClockSetInfinite(ClockT *myClock)
{
    ClockSetTime(myClock, CLOCK_TIME_INFINITE);
}


void ClockSetTime(ClockT *myClock, bigtime_t myTime)
{
    int wasRunning = ClockIsRunning(myClock);

    // This sequence resets the startTime.
    ClockStop(myClock);
    myClock->time = myTime;
    if (wasRunning)
    {
	ClockStart(myClock);
    }
}


void ClocksStop(GameStateT *gameState)
{
    int i;
    for (i = 0; i < 2; i++)
    {
	ClockStop(gameState->clocks[i]);
    }
}


void ClocksPrint(GameStateT *gameState, char *context)
{
    int i;
    ClockT *myClock;
    for (i = 0; i < 2; i++)
    {
	myClock = gameState->clocks[i];
	printf("ClocksPrint(%s): clock %d: %lld %lld %d %c\n",
	       context ? context : "",
	       i,
	       ClockCurrentTime(myClock),
	       ClockGetInc(myClock),
	       ClockGetIncPeriod(myClock),
	       ClockIsRunning(myClock) ? 'r' : 's');
    }
}


/* auxilliary functions that I do not really know where to put. */

/*
   Want either xx:yy:zz, yy:zz, or (:)zz.
   But we try to be very permissive in what we accept.
*/
int TimeStringIsValid(char *str)
{
    int colCount = 0;
    int isValid = 1;

    if (!strcmp(str, CLOCK_TIME_INFINITE_STR))
    {
	return 1;
    }

    if (*str == '\0')
    {
	return 0;
    }
    do
    {
	if (/* must be digit or ':' */
	    (!isdigit(*str) && *str != ':') ||
	    /* : must have a number after it, and cannot be > 2. */
	    (*str == ':' && (++colCount > 2 || !isdigit(*(str + 1)))))
	{
	    isValid = 0;
	    break;
	}
    } while (*(++str) != '\0');

    return isValid;
}


/* Returns numerical form of time.  Asserts if invalid time.  (This is so
   zero can be returned as a 'valid time' (for inc), and we do not have to
   make bigtime_t signed. */
bigtime_t TimeStringToBigtime(char *str)
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


/* stores string representation of 'myTime' into 'result'.
   Also returns 'result'.
   'result' must be at least TIME_STR_LEN bytes long.
*/
char *TimeStringFromBigtime(char *result, bigtime_t myTime)
{
    int hours, minutes, seconds;
    int negative = myTime < 0;

    if (myTime == CLOCK_TIME_INFINITE)
    {
	sprintf(result, CLOCK_TIME_INFINITE_STR);
	return result;
    }

    myTime = abs(myTime);

    /* This rounds us up to the nearest second. */
    if (myTime % 1000000)
    {
	myTime += 1000000;
    }

    myTime /= 1000000; /* convert to seconds representation */
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


// Expected number of moves in a game.  Biased toward initial moves.
#define GAME_NUM_MOVES 40
// compensation for any (presumably network) lag, in usec
#define LAG_TIME 2000000
void GoaltimeCalc(GameStateT *gameState, BoardT *board)
{
    int turn = board->ply & 1;
    ClockT *myClock = gameState->clocks[turn];
    bigtime_t myTime, calcTime, myInc;
    int myIncPeriod, numMovesToIncPeriod;
    int altCalcTime;

    // Degenerate case.
    if (ClockIsInfinite(myClock))
    {
	gameState->goalTime[turn] = CLOCK_TIME_INFINITE;
	return;
    }

    myTime = ClockCurrentTime(myClock);
    myInc = ClockGetInc(myClock);
    myIncPeriod = ClockGetIncPeriod(myClock);

    // 'calcTime' is the amount of time we want to think.
    calcTime = (myTime - LAG_TIME) / GAME_NUM_MOVES;

    if (myInc && myIncPeriod)
    {
	numMovesToIncPeriod =
	    myIncPeriod - (((board->ply & ~0x1) / 2) % myIncPeriod);

	calcTime += myInc / (myIncPeriod + numMovesToIncPeriod);

	// Try not to run out of time at the end of a period.
	altCalcTime = (myTime - LAG_TIME) /
	    MIN(GAME_NUM_MOVES, numMovesToIncPeriod);
	calcTime = MIN(calcTime, altCalcTime);
    }

    // Always try to leave 2 seconds on the clock, to account for any lag.
    if (myTime - calcTime < LAG_TIME)
	calcTime = myTime - LAG_TIME;

    calcTime = MAX(calcTime, 0);  // do not think for a "negative" time

    gameState->goalTime[turn] = myTime - calcTime;
}
