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

#include "aSemaphore.h"

namespace arctic
{

Semaphore::Semaphore(int count) : count(count) {}

void Semaphore::wait()
{
    std::unique_lock<std::mutex> lock(mutex);

    if (--count >= 0)
        return;

    cv.wait(lock, [this] { return count >= 0; });
}

bool Semaphore::try_wait()
{
    std::unique_lock<std::mutex> lock(mutex);

    if (count > 0)
    {
        --count;
        return true;
    }
    return false;
}

void Semaphore::post()
{
    std::unique_lock<std::mutex> lock(mutex);

    if (++count <= 0)
        cv.notify_one();
}

int Semaphore::get_value()
{
    // I considered not locking here, since it is really not threadsafe to
    // use such a count, but presumably any caller would want as accurate an
    // estimate as possible (for diagnostics?) and isn't concerned about speed.
    std::lock_guard<std::mutex> lock(mutex);
    return count;
}

} // end namespace arctic
