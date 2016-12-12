//--------------------------------------------------------------------------
//                 Timer.cpp - threaded timer functionality.
//                           -------------------
//  copyright            : (C) 2016 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#include <assert.h>
#include <condition_variable> // std::condition_variable_any
#include <mutex>              // std::recursive_mutex
#include <thread>             // std::thread

#include "clockUtil.h"        // CurrentTime()
#include "Timer.h"

class TimerThread
{
public:
    TimerThread();
    ~TimerThread();
    void Reschedule(arctic::Timer &timer, int64 timeoutMs, bool isAbsolute);
    void SetHandler(arctic::Timer &timer,
                    const arctic::Timer::HandlerFunc &handler);
    void Start(arctic::Timer &timer);
    int Stop(arctic::Timer &timer);
private:
    // We use a recursive mutex just so a handler can invoke Timer APIs.
    // We want to run the handler with the mutex locked so that it cannot be
    //  destroyed as it is serviced.
    std::recursive_mutex rMutex;
    arctic::List runningTimers;
    std::condition_variable_any cv;
    std::thread *thread;
    void threadFunc();
};

static TimerThread *gTimerThread;

TimerThread::TimerThread()
{
    thread = new std::thread(&TimerThread::threadFunc, this);
    thread->detach();
}

TimerThread::~TimerThread()
{
    // Currently unsupported, for lack of need.  Supporting this would involve
    //  stopping every running timer, and probably weak references from timers
    //  to timerthreads.
    assert(0);
}

static int compareSoonestTime(arctic::Timer *el1, arctic::Timer *el2)
{
    return el1->CompareNextTimeout(*el2);
}

void TimerThread::Reschedule(arctic::Timer &timer, int64 timeoutMs,
                             bool isAbsolute)
{
    bigtime_t nowTime = CurrentTime();
    std::unique_lock<decltype(rMutex)> lock(rMutex);
    timer.isAbsolute = isAbsolute;
    timer.timeoutMs = timeoutMs;
    bool isRunning = timer.isRunning;
    if (isRunning)
    {
        timer.nextTimeoutAbsMs = timeoutMs;
        if (!timer.isAbsolute)
            timer.nextTimeoutAbsMs += nowTime / 1000;
        runningTimers.Remove(&timer);
        runningTimers.InsertBy((arctic::LIST_COMPAREFUNC) compareSoonestTime,
                               &timer);
    }
    lock.unlock();
    if (isRunning)
        cv.notify_one();
}

void TimerThread::SetHandler(arctic::Timer &timer,
                             const arctic::Timer::HandlerFunc &handler)
{
    std::unique_lock<decltype(rMutex)> lock(rMutex);
    timer.handler = handler;
}

void TimerThread::Start(arctic::Timer &timer)
{
    bigtime_t nowTime = CurrentTime();
    std::unique_lock<decltype(rMutex)> lock(rMutex);
    if (timer.isRunning)
        return;
    timer.isRunning = true;
    timer.startTimeAbsMs = nowTime / 1000;
    timer.nextTimeoutAbsMs = timer.timeoutMs;
    if (!timer.isAbsolute)
        timer.nextTimeoutAbsMs += timer.startTimeAbsMs;
    runningTimers.InsertBy((arctic::LIST_COMPAREFUNC) compareSoonestTime,
                           &timer);
    lock.unlock();
    cv.notify_one();
}

int TimerThread::Stop(arctic::Timer &timer)
{
    std::unique_lock<decltype(rMutex)> lock(rMutex);
    int expireCount = timer.expireCount;
    timer.expireCount = 0;
    if (!timer.isRunning) // already stopped/expired?
        return expireCount;
    timer.isRunning = false;
    runningTimers.Remove(&timer);
    lock.unlock();
    cv.notify_one();
    return expireCount;
}

void TimerThread::threadFunc()
{
    std::unique_lock<decltype(rMutex)> lock(rMutex);
    while (true)
    {
        // We handle a condition variable spurious interrupt by just looping
        //  again.
        if (runningTimers.IsEmpty())
        {
            cv.wait(lock); // just wait for next event
        }
        else
        {
            // Run any expired handlers.
            arctic::Timer *timer = (arctic::Timer *) runningTimers.Head();
            int64 timeoutMs = timer->nextTimeoutAbsMs - CurrentTime() / 1000;
            if (timeoutMs <= 0)
            {
                runningTimers.Pop();
                timer->expireCount++;
                timer->isRunning = false;
                timer->handler();
                continue;
            }
            // Wait for next event, or for a timer to expire.
            cv.wait_for(lock, std::chrono::milliseconds(timeoutMs));
        }
    }
}

namespace arctic
{

Timer::Timer() :
    startTimeAbsMs(0), nextTimeoutAbsMs(0), timeoutMs(0), expireCount(0),
    isRunning(false), isAbsolute(false)
{
    // If you hit this, you did not call InitSubsystem() first.
    assert(gTimerThread != nullptr);
}

Timer::~Timer()
{
    Stop(); // Take ourselves off the running timers list.
}

Timer &Timer::SetAbsoluteTimeout(int64 timeoutMs)
{
    gTimerThread->Reschedule(*this, timeoutMs, true);
    return *this;
}

Timer &Timer::SetRelativeTimeout(int64 timeoutMs)
{
    gTimerThread->Reschedule(*this, timeoutMs, false);
    return *this;
}

Timer &Timer::SetHandler(const HandlerFunc &func)
{
    gTimerThread->SetHandler(*this, func);
    return *this;
}

void Timer::Start()
{
    gTimerThread->Start(*this);
}

int Timer::Stop()
{
    return gTimerThread->Stop(*this);
}

int Timer::CompareNextTimeout(Timer &other) const
{
    return
        nextTimeoutAbsMs < other.nextTimeoutAbsMs ? -1 :
        nextTimeoutAbsMs == other.nextTimeoutAbsMs ? 0 :
        1;
}

void Timer::InitSubsystem()
{
    if (gTimerThread == nullptr)
        gTimerThread = new TimerThread;
}

} // end namespace 'arctic'

#if 0 // test harness
#include <iostream>
#include <unistd.h> // sleep(3)
using arctic::Timer;

void TestTimers()
{
    // We expect b, c, then a to fire.
    Timer::InitSubsystem();
    std::cout << "start timer test at " << CurrentTime() / 1000 << '\n';
    Timer a([] { std::cout << "a fired at " << CurrentTime() / 1000 << '\n'; });
    Timer b([] { std::cout << "b fired at " << CurrentTime() / 1000 << '\n'; });
    Timer c([] { std::cout << "c fired at " << CurrentTime() / 1000 << '\n'; });
    a.SetRelativeTimeout(5000).Start();
    b.SetRelativeTimeout(1000).Start();
    c.SetRelativeTimeout(3000).Start();
    sleep(7); // relies on per-thread (not per-process) sleep(3)
}
#endif
