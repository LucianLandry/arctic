/***************************************************************************
            switcher.c - rudimentary context-switching functionality.
                             -------------------
    copyright            : (C) 2007 by Lucian Landry
    email                : lucian_b_landry@yahoo.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 ***************************************************************************/


#include <assert.h>
#include <string.h>
#include "switcher.h"

void SwitcherInit(SwitcherContextT *sw)
{
    int i;
    int retVal;

    memset(sw, 0, sizeof(SwitcherContextT));

    for (i = 0; i < SWITCHER_MAX_USERS; i++)
    {
	retVal = sem_init(&sw->sems[i], 0, 0);
	assert(retVal == 0);
    }
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
	sem_wait(&sw->sems[numUsers]);
    }
}


/* switch between threads, round-robin style. */
void SwitcherSwitch(SwitcherContextT *sw)
{
    sem_t *mySem = &sw->sems[sw->currentUser];

    /* Goto next user. */
    if (++sw->currentUser == sw->numUsers)
	sw->currentUser = 0;

    /* Let any other threads run. */
    sem_post(&sw->sems[sw->currentUser]);
    sem_wait(mySem);
}
