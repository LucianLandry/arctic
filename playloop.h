//--------------------------------------------------------------------------
//                playloop.h - main loop and support routines.
//                           -------------------
//  copyright            : (C) 2008 by Lucian Landry
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

#ifndef PLAYLOOP_H
#define PLAYLOOP_H

#include "Engine.h"
#include "Game.h"
#include "Switcher.h"

// Main play loop.
void PlayloopRun(Game &game, Engine &eng, Switcher &sw);

#endif // PLAYLOOP_H
