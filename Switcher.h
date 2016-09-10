//--------------------------------------------------------------------------
//          Switcher.h - rudimentary context-switching functionality.
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
