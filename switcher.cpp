//--------------------------------------------------------------------------
//         switcher.cpp - rudimentary context-switching functionality.
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

#include "switcher.h"

using arctic::Semaphore;

void SwitcherInit(SwitcherContextT *sw)
{
    // Assumes 'sems' have already initialized themselves to 0.
    sw->currentUser = 0;
    sw->numUsers = 0;
}

void SwitcherRegister(SwitcherContextT *sw)
{
    /* Not thread-safe, but could fairly easily be made so. */
    int numUsers = sw->numUsers++;

    if (numUsers == SWITCHER_MAX_USERS)
    {
        assert(0);
    }

    if (numUsers != 0)
    {
        /* Every thread but the 'initial' one blocks, waiting to run. */
        sw->sems[numUsers].wait();
    }
}


/* switch between threads, round-robin style. */
void SwitcherSwitch(SwitcherContextT *sw)
{
    Semaphore *mySem = &sw->sems[sw->currentUser];

    /* Goto next user. */
    if (++sw->currentUser == sw->numUsers)
        sw->currentUser = 0;

    /* Let any other threads run. */
    sw->sems[sw->currentUser].post();
    mySem->wait();
}
