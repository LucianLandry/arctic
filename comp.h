//--------------------------------------------------------------------------
//                   comp.h - computer 'AI' functionality.
//                           -------------------
//  copyright            : (C) 2016 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#ifndef COMP_H
#define COMP_H

#include "Thinker.h"

// These are called only by Thinker instances:

// Think on 'th's position, and recommend either: a move, draw, or resign.
void computermove(Thinker *th, bool bPonder);

Eval tryMove(Thinker *th, MoveT move, int alpha, int beta,
             SearchPv *newPv, int *hashHitOnly);

#endif // COMP_H
