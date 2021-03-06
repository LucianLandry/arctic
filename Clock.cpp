//--------------------------------------------------------------------------
//                         clock.cpp - clock control
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#include "Clock.h"
#include "clockUtil.h"

bigtime_t Clock::calcTimeTaken() const
{
    return CurrentTime() - turnStartTime;
}

Clock::Clock()
{
    ReInit();
}

Clock &Clock::ReInit()
{
    startTime = CLOCK_TIME_INFINITE;
    time = CLOCK_TIME_INFINITE;
    inc = 0;
    timeControlPeriod = 0;
    numMovesToNextTimeControl = 0;
    running = false;
    turnStartTime = 0;
    timeTaken = 0;
    perMoveLimit = CLOCK_TIME_INFINITE;
    isFirstMoveFree = false;
    incrementApplied = false;
    return *this;
}

Clock &Clock::Reset()
{
    Stop();
    SetTime(StartTime());
    incrementApplied = false;
    return *this;
}

Clock &Clock::Stop()
{
    if (IsRunning())
    {
        running = false;
        timeTaken = calcTimeTaken();

        if (!IsInfinite() && (!IsFirstMoveFree() || incrementApplied))
            time -= timeTaken;
    }
    return *this;
}

Clock &Clock::Start()
{
    if (!IsRunning())
    {
        running = true;
        turnStartTime = CurrentTime();
    }
    return *this;
}

Clock &Clock::AddTime(bigtime_t myTime)
{
    if (time == CLOCK_TIME_INFINITE)
        return *this;

    if (myTime == CLOCK_TIME_INFINITE)
        time = myTime;
    else
        time += myTime;
    return *this;
}

// Adjust clock by its appropriate increment.  Meant to be applied just
// *after* we make our move (meaning: it is no longer our turn).
//
// We do this, since in chess you normally adjust time after your
// move is made.
Clock &Clock::ApplyIncrement(int ply)
{
    if (IsInfinite() || (IsFirstMoveFree() && !incrementApplied))
    {
        incrementApplied = true;
        return *this;
    }
    incrementApplied = true;

    // Apply per-move increment (if any)
    AddTime(inc);

    // Add any time from a new time control.
    if (timeControlPeriod)
    {
        if (// add 2 instead of 1, if want to apply 'before' move
            ((ply + 1) >> 1) % timeControlPeriod == 0)
        {
            AddTime(startTime);
        }
    }
    else if (numMovesToNextTimeControl == 1)
    {
        AddTime(startTime);
    }
    return *this;
}

bigtime_t Clock::Time() const
{
    return
        IsRunning() && !IsInfinite() ?
        time - calcTimeTaken() :
        time;
}

bigtime_t Clock::PerMoveTime() const
{
    bigtime_t result = MIN(perMoveLimit, time);

    return
        result == CLOCK_TIME_INFINITE || !IsRunning() ?
        result :
        result - calcTimeTaken();
}

Clock &Clock::SetTime(bigtime_t myTime)
{
    bool wasRunning = IsRunning();

    // This sequence resets the turnStartTime.
    Stop();
    time = myTime;
    if (wasRunning)
        Start();
    return *this;
}

bigtime_t Clock::TimeTaken() const
{
    return IsRunning() ? calcTimeTaken() : timeTaken;
}

// Like operator=(), but does not change the state (running/stopped etc.) of
//  the destination.
void Clock::SetParameters(const Clock &other)
{
    startTime = other.startTime;
    time = other.time;
    inc = other.inc;
    timeControlPeriod = other.timeControlPeriod;
    numMovesToNextTimeControl = other.numMovesToNextTimeControl;
    // skip 'running', 'turnStartTime', 'timeTaken'
    perMoveLimit = other.perMoveLimit;
    isFirstMoveFree = other.isFirstMoveFree;
    // skip 'incrementApplied'
}
