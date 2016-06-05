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
