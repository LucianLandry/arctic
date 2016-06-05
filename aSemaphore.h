//--------------------------------------------------------------------------
//               aSemaphore.h - portable semaphore abstraction.
//                           -------------------
//  copyright            : (C) 2016 by Lucian Landry
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

#ifndef ASEMAPHORE_H
#define ASEMAPHORE_H

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace arctic
{
    
// Since the c++ standard library fails to provide a semaphore abstraction, we
//  do so here.  Since I anticipate this becoming standard in the far future, I
//  use std's snake_case naming convention.
// However, it was a deliberate decision to use wait/post instead of
//  lock()/unlock().  The latter could be used with a std::lock_guard, but the
//  former's semantics are clearer (and closer to the C versions).
class Semaphore
{
public:
    Semaphore(int count = 1);

    void wait();
    bool try_wait();
    // Based off:
    // http://en.cppreference.com/w/cpp/thread/condition_variable/wait_for
    // Example usage:
    // sem.wait_for(std::chrono::milliseconds(500)); // wait 1/2 a second
    template<class Rep, class Period>
    bool wait_for(const std::chrono::duration<Rep,Period>& rel_time);
    template<class Rep, class Period>
    bool wait_until(const std::chrono::duration<Rep,Period>& abs_time);

    void post();

    // Returns either:
    // (>= 0) the number of times the semaphore can be waited on w/out blocking;
    // or
    // (< 0) the number of current waiters blocked on the semaphore * -1.
    int get_value();

private:
    std::mutex mutex;
    std::condition_variable cv;
    int count;
};

template<class Rep, class Period>
inline bool Semaphore::wait_for(const std::chrono::duration<Rep,Period>& rel_time)
{
    std::unique_lock<std::mutex> lock(mutex);

    if (--count >= 0)
        return true;

    return cv.wait_for(lock, rel_time, [this] { return count >= 0; });
}

template<class Rep, class Period>
inline bool Semaphore::wait_until(const std::chrono::duration<Rep,Period>& abs_time)
{
    std::unique_lock<std::mutex> lock(mutex);

    if (--count >= 0)
        return true;

    return cv.wait_until(lock, abs_time, [this] { return count >= 0; });
}

} // end namespace 'arctic'

#endif // ASEMAPHORE_H