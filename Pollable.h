//--------------------------------------------------------------------------
//                Pollable.h - Pollable object representation.
//                           -------------------
//  copyright            : (C) 2016 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

// Pollable objects are meant to be a way for normal C++ objects to signal
//  to poll(2), select(2), epoll(2) (etc.) that they are "ready".  Exact
//  mechanisms are OS-dependent.
// Correct usage is to call Ready() only when the higher-level object
//  *transitions* from "not ready" to "ready", and to call NotReady()
//  only when the higher-level object transitions from "ready" to "not ready".
//  Otherwise, the behavior is undefined (you may block unexpectedly, or put
//   the Pollable into a bad state).

#ifndef POLLABLE_H
#define POLLABLE_H

// See https://sourceforge.net/p/predef/wiki/Home/ for a list of macros we may
//  expect to be valid.
#ifdef __linux__
#define HAS_EVENTFD
#endif

// 'Pollable' objects are not necessarily threadsafe; they must be externally
//   protected from concurrent access.
//  All Pollables are initialized to the "not ready" state.
class Pollable
{
public:
    Pollable();
    ~Pollable();
    void Ready();
    void NotReady();
    int Fd() const; // for system polling
private:
#ifdef HAS_EVENTFD
    int eventFd;             // use eventfd(2)
#else
    int readSock, writeSock; // use socketpair(2)
#endif
};

#endif // POLLABLE_H
