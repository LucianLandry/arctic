//--------------------------------------------------------------------------
//                          clock.h - clock control
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

#ifndef CLOCK_H
#define CLOCK_H

#include "aTypes.h" // bigtime_t

// Note: for a clock, there is no "negative" infinity.
#define CLOCK_TIME_INFINITE 0x7fffffffffffffffLL

class Clock
{
public:
    Clock(); // initializes clock to infinite time, no per-move limit, stopped.

    Clock &operator=(const Clock &) = default;

    inline bool IsRunning() const;
    inline bool IsInfinite() const;

    bigtime_t TimeTaken() const; // elapsed time of the last start-stop cycle.

    // Returns the amount of time a player has to make their move before they
    //  could be flagged, taking per-move limits into account.
    bigtime_t PerMoveTime() const;

    // All of the following operations may be chained.
    Clock &ReInit(); // re-initializes clock
    Clock &Stop();
    Clock &Start();
    Clock &ApplyIncrement(int ply);
    Clock &Reset(); // Stops the clock and resets the time to the starting time.
    Clock &AddTime(bigtime_t myTime); // add some time to a clock.

    // (Setter functions.)
    Clock &SetTime(bigtime_t myTime);
    inline Clock &SetStartTime(bigtime_t myStartTime);
    inline Clock &SetIncrement(bigtime_t myInc);
    inline Clock &SetTimeControlPeriod(int myTimeControlPeriod);
    // This alternative interface to SetTimeControlPeriod() is useful for UCI.
    // It should not be used at the same time as SetTimeControlPeriod() (because
    // conflicts may occur)
    inline Clock &SetNumMovesToNextTimeControl(int numMoves);
    // Is the clock really intended to run during the first move.  For FICS
    //  games, this is false.  Default = true.  The semantics of this is weird
    //  as we still want the clock to run for stats-keeping purposes (we don't
    //  want to special-case the time taken to move).
    inline Clock &SetIsFirstMoveFree(bool isIt);
    // This is not normally used for human players, but I include it here
    //  because it could be.
    inline Clock &SetPerMoveLimit(bigtime_t myLimit);
    // (Sets all of the above at once, but does not touch the rest of the
    //  clock state like operator=().)
    void SetParameters(const Clock &other);
    
    // Getter functions.
    bigtime_t Time() const;
    inline bigtime_t StartTime() const;
    inline bigtime_t Increment() const;
    inline int TimeControlPeriod() const;
    inline int NumMovesToNextTimeControl() const;
    inline bigtime_t PerMoveLimit() const;
    inline bool IsFirstMoveFree() const;
    
private:
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
    bigtime_t turnStartTime; // time this turn started (absolute).  Do not
                             // confuse this with startTime!
    bigtime_t timeTaken; // time of the last start-stop cycle.
    bigtime_t perMoveLimit; // per-move limit (infinite if no limit)

    bool running; // is the clock currently running?
    bool isFirstMoveFree; // are we dealing with ICS clocks; see above
    bool incrementApplied; // Have we called ApplyIncrement() since reset?
                           // (ab)used to track whether 'isFirstMoveFree' should
                           // apply.  (note that this may be 'true' even if
                           // increments are 0.)

    bigtime_t calcTimeTaken() const;
};

inline bool Clock::IsRunning() const
{
    return running;
}

inline bool Clock::IsInfinite() const
{
    return time == CLOCK_TIME_INFINITE;
}

inline Clock &Clock::SetStartTime(bigtime_t myStartTime)
{
    startTime = myStartTime;
    return *this;
}
inline bigtime_t Clock::StartTime() const
{
    return startTime;
}

inline Clock &Clock::SetIncrement(bigtime_t myInc)
{
    inc = myInc;
    return *this;
}
inline bigtime_t Clock::Increment() const
{
    return inc;
}

inline Clock &Clock::SetTimeControlPeriod(int myTimeControlPeriod)
{
    timeControlPeriod = myTimeControlPeriod;
    return *this;
}
inline int Clock::TimeControlPeriod() const
{
    return timeControlPeriod;
}

inline Clock &Clock::SetNumMovesToNextTimeControl(int numMoves)
{
    numMovesToNextTimeControl = numMoves;
    return *this;
}
inline int Clock::NumMovesToNextTimeControl() const
{
    return numMovesToNextTimeControl;
}

inline Clock &Clock::SetPerMoveLimit(bigtime_t myLimit)
{
    perMoveLimit = myLimit;
    return *this;
}
inline bigtime_t Clock::PerMoveLimit() const
{
    return perMoveLimit;
}

inline Clock &Clock::SetIsFirstMoveFree(bool isIt)
{
    isFirstMoveFree = isIt;
    return *this;
}
inline bool Clock::IsFirstMoveFree() const
{
    return isFirstMoveFree;
}

#endif // CLOCK_H
