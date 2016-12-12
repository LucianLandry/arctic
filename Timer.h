//--------------------------------------------------------------------------
//                  Timer.h - threaded timer functionality.
//                           -------------------
//  copyright            : (C) 2016 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

// Timer implements a threaded timer.  When these timers expire, they execute a
//  handler on their own thread.  As such, you generally want to make sure your
//  handlers do a minimum amount of stuff, in a thread-safe fashion, and don't
//  block.

#ifndef TIMER_H
#define TIMER_H

#include <functional> // std::function
#include "aList.h"
#include "aTypes.h"   // int64

class TimerThread; // forward declaration

namespace arctic
{

class Timer
{
    // Member functions in this class are threadsafe unless otherwise noted.
    friend class ::TimerThread;
public:
    using HandlerFunc = std::function<void()>;
    Timer();
    ~Timer();

    // Manipulators:
    Timer &SetAbsoluteTimeout(int64 timeoutMs); // in UTC
    // Schedules an expiration 'timeoutMs' from "now" (the timeout only fires
    //  if the timer is running)
    Timer &SetRelativeTimeout(int64 timeoutMs);
    Timer &SetHandler(const HandlerFunc &func);
    
    void Start(); // Starts the timer.  When the timer expires, the pollable
                  //  object (if any) will be Ready()d.  If the timer is
                  //  already started, does nothing.
    int Stop();   // Stops the timer if it was running.  Returns (and resets)
                  //  the number of times the timer has expired since the last
                  //  call to Stop().

    // Returns -1, 0, or 1 if this timer will expire sooner, at the same time,
    //  or later than 'other' (respectively).  Running status is ignored.
    // Not threadsafe.
    int CompareNextTimeout(Timer &other) const;

    // Prepares the timer subsystem for use.  Must be called before any Timers
    //  are created.  Not threadsafe.
    static void InitSubsystem();
private:
    ListElement elem; // For timerthread use
    HandlerFunc handler;   // What handler we invoke when we expire

    // Only valid when running.  Abs time we were started.
    int64 startTimeAbsMs;
    // Only valid when running.  Abs time we should expire.
    int64 nextTimeoutAbsMs;
    int64 timeoutMs; // timeout passed from Set*Timeout(); may be absolute or
                     //  relative.
    int expireCount;  // Returned by Stop(); number of times this timer expired.
    bool isRunning;   // Is the timer currently running
    bool isAbsolute;  // Were we set with an absolute timeout or not
};

} // end namespace 'arctic'

#if 0 // test harness
void TestTimers();
#endif

#endif // TIMER_H
