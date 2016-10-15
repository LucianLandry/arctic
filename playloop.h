//--------------------------------------------------------------------------
//                playloop.h - main loop and support routines.
//                           -------------------
//  copyright            : (C) 2008 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#ifndef PLAYLOOP_H
#define PLAYLOOP_H

#include "Engine.h"
#include "Game.h"
#include "Switcher.h"

// Main play loop.
void PlayloopRun(Game &game, Engine &eng, Switcher &sw);

#endif // PLAYLOOP_H
