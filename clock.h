//--------------------------------------------------------------------------
//                          clock.h - clock control
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

#ifndef CLOCK_H
#define CLOCK_H

#include "aTypes.h" // bigtime_t

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bigtime_t startTime; // This time is put on the clock whenever the clock
                         // is reset.  It is not the time the clock started
                         // running.
    bigtime_t time; // time left on clock.
                    // Adjusted only when clock is stopped.
    bigtime_t inc;  // post-increment (added to clock when it is stopped)

    // As in xboard, time controls are all the same.
    // 0 -> single time control
    // 1..x -> inc time by 'startTime' after every 'timeControlPeriod' moves by
    //         a given side.
    int timeControlPeriod; // normally overrides the below variable, but do
                           // not rely on this
    int numMovesToNextTimeControl;

    bool bRunning; // boolean: is the clock running?

    bigtime_t turnStartTime; // time this turn started (absolute).  Do not
                             // confuse this with startTime!
    bigtime_t timeTaken; // time of the last start-stop cycle.

    bigtime_t perMoveLimit; // per-move limit (infinite if no limit)
} ClockT;


// clock.c
// 2^32 / 3600 = 7 digits, + 2 colons, +minutes+seconds + terminator
#define CLOCK_TIME_STR_LEN 14
#define CLOCK_TIME_INFINITE 0x7fffffffffffffffLL

 // initialize clock to infinite time, no per-move limit, stopped.
void ClockInit(ClockT *myClock);
void ClockStop(ClockT *myClock);
void ClockStart(ClockT *myClock);
void ClockApplyIncrement(ClockT *myClock, int ply);
void ClockReset(ClockT *myClock);
void ClockSetTime(ClockT *myClock, bigtime_t myTime);
bigtime_t ClockGetTime(ClockT *myClock);
bigtime_t ClockGetPerMoveTime(ClockT *myClock);
bigtime_t ClockTimeTaken(ClockT *myClock);


static inline void ClockSetStartTime(ClockT *myClock, bigtime_t myStartTime)
{
    myClock->startTime = myStartTime;
}
static inline bigtime_t ClockGetStartTime(ClockT *myClock)
{
    return myClock->startTime;
}

static inline void ClockSetInc(ClockT *myClock, bigtime_t myInc)
{
    myClock->inc = myInc;
}
static inline bigtime_t ClockGetInc(ClockT *myClock)
{
    return myClock->inc;
}

static inline void ClockSetPerMoveLimit(ClockT *myClock, bigtime_t myLimit)
{
    myClock->perMoveLimit = myLimit;
}
static inline bigtime_t ClockGetPerMoveLimit(ClockT *myClock)
{
    return myClock->perMoveLimit;
}

static inline void ClockSetTimeControlPeriod(ClockT *myClock,
                                             int myTimeControlPeriod)
{
    myClock->timeControlPeriod = myTimeControlPeriod;
}
static inline int ClockGetTimeControlPeriod(ClockT *myClock)
{
    return myClock->timeControlPeriod;
}

// This is an alternative interface to SetTimeControlPeriod() useful for UCI.
// It should not be used at the same time as SetTimeControlPeriod() (because
// conflicts may occur)
static inline void ClockSetNumMovesToNextTimeControl
(ClockT *myClock, int numMovesToNextTimeControl)
{
    myClock->numMovesToNextTimeControl = numMovesToNextTimeControl;
}
static inline int ClockGetNumMovesToNextTimeControl(ClockT *myClock)
{
    return myClock->numMovesToNextTimeControl;
}

static inline bool ClockIsRunning(ClockT *myClock)
{
    return myClock->bRunning;
}

static inline int ClockIsInfinite(ClockT *myClock)
{
    return myClock->time == CLOCK_TIME_INFINITE;
}


bool TimeStringIsValid(char *str);
bigtime_t TimeStringToBigTime(char *str);
char *TimeStringFromBigTime(char *result, bigtime_t myTime);

#ifdef __cplusplus
}
#endif

#endif // CLOCK_H
