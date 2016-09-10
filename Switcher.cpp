//--------------------------------------------------------------------------
//         Switcher.cpp - rudimentary context-switching functionality.
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

#include <assert.h>
#include <string.h>

#include "Switcher.h"

using arctic::Semaphore;

Switcher::Switcher() : currentUser(0), numUsers(0) {}

void Switcher::Register()
{
    // Not thread-safe, but could fairly easily be made so.
    if (numUsers == kMaxUsers)
        assert(0);

    int myUser = numUsers++;
    
    // Every thread but the 'initial' one blocks, waiting to run.
    if (myUser != 0)
        sems[myUser].wait();
}

void Switcher::Switch()
{
    Semaphore *mySem = &sems[currentUser];

    // Goto next user.
    if (++currentUser == numUsers)
        currentUser = 0;

    // Let any other threads run.
    sems[currentUser].post();
    mySem->wait();
}
