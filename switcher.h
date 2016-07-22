//--------------------------------------------------------------------------
//          switcher.h - rudimentary context-switching functionality.
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

#ifndef SWITCHER_H
#define SWITCHER_H

#include "aSemaphore.h" // arctic::Semaphore

#define SWITCHER_MAX_USERS 2

typedef struct {
    arctic::Semaphore sems[SWITCHER_MAX_USERS];
    int currentUser;
    int numUsers;
} SwitcherContextT;

void SwitcherInit(SwitcherContextT *sw);
void SwitcherRegister(SwitcherContextT *sw);
void SwitcherSwitch(SwitcherContextT *sw);

#endif /* SWITCHER_H */
