/***************************************************************************
            switcher.c - poor man's context-switching functionality.
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
#include "ref.h"

void SwitcherInit(SwitcherContextT *sw)
{
    sem_init(&sw->sem1, 0, 0);
    sem_init(&sw->sem2, 0, 0);
    sw->cookie = NULL;
}


void *SwitcherGetCookie(SwitcherContextT *sw)
{
    void *result;

    if (sw->cookie == &sw->sem2)
    {
	assert(0);
    }

    result = sw->cookie = (sw->cookie == NULL ? &sw->sem1 : &sw->sem2);
    if (sw->cookie != &sw->sem1)
    {
	/* Every thread but the 'initial' one blocks, waiting to run. */
	sem_wait(sw->cookie);
    }

    return result;
}


/* switch between 2 threads. */
void SwitcherSwitch(SwitcherContextT *sw, void *cookie)
{
    sem_t *mySem = (sem_t *) cookie;

    /* Let the other thread run. */
    sem_post(mySem == &sw->sem1 ? &sw->sem2 : &sw->sem1);
    sem_wait(mySem);
}
