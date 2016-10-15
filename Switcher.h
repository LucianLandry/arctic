//--------------------------------------------------------------------------
//          Switcher.h - rudimentary context-switching functionality.
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#ifndef SWITCHER_H
#define SWITCHER_H

#include "aSemaphore.h" // arctic::Semaphore

class Switcher
{
public:
    Switcher();
    void Register();
    void Switch();   // Switches between threads, round-robin style.
private:
    static const int kMaxUsers = 2;
    arctic::Semaphore sems[kMaxUsers];
    int currentUser;
    int numUsers;
};

#endif // SWITCHER_H
