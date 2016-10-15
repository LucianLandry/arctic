//--------------------------------------------------------------------------
//
//                     aSpinlock.h - spinlock abstraction.
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

// This bundles up C++11's low-level spinlock functionality as a (slightly)
//  more accessible class.  For details on implementation, see (for example):
// http://en.cppreference.com/w/cpp/atomic/atomic_flag
//
// Since I suspect this might become standardized in the future (and for use
//  with std::lock_guard), the public member functions are named 'lock()' and
//  'unlock()'.

#ifndef ASPINLOCK_H
#define ASPINLOCK_H

#include <atomic>

namespace arctic
{

class Spinlock
{
public:
    void lock();
    void unlock();
private:
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
};

inline void Spinlock::lock()
{
    while (flag.test_and_set(std::memory_order_acquire))
        ;
}

inline void Spinlock::unlock()
{
    flag.clear(std::memory_order_release);
}

} // end namespace 'arctic'

#endif // ASPINLOCK_H
