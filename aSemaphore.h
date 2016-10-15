//--------------------------------------------------------------------------
//               aSemaphore.h - portable semaphore abstraction.
//                           -------------------
//  copyright            : (C) 2016 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#ifndef ASEMAPHORE_H
#define ASEMAPHORE_H

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace arctic
{
    
// Since the c++ standard library fails to provide a semaphore abstraction, we
//  do so here.
// Technically, this is a counting semaphore; for a definition of that, see:
//  https://en.wikipedia.org/wiki/Semaphore_%28programming%29
// Since I anticipate semaphores becoming standard in the far future, I
//  use std's snake_case naming convention to name the member functions.
// However, it was a deliberate decision to use wait()/post() instead of
//  lock()/unlock().  The latter could be used with a std::lock_guard, but the
//  former's semantics are clearer (and closer to the C versions).
class Semaphore
{
public:
    // The default sem value, somewhat arbitrarily chosen (it matches our code
    //  better, and would appear somewhat "safer"), requires a post() to satisfy
    //  any initial wait().
    // If you want a semaphore you can treat as a (heavy-weight) std::mutex,
    //  pass '1' instead.
    Semaphore(int count = 0);

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
