//--------------------------------------------------------------------------
//                playloop.h - main loop and support routines.
//                           -------------------
//  copyright            : (C) 2008 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU Library General Public License as
//   published by the Free Software Foundation; either version 2 of the
//   License, or (at your option) any later version.
//
//--------------------------------------------------------------------------

#ifndef PLAYLOOP_H
#define PLAYLOOP_H

#include "game.h"
#include "thinker.h"

// Synchronous move-now support.
void PlayloopCompMoveNowAndSync(GameT *game, ThinkContextT *th);
// Main play loop.
void PlayloopRun(GameT *game, ThinkContextT *th);

#endif // PLAYLOOP_H
