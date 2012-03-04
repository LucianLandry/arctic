//--------------------------------------------------------------------------
//                   comp.h - computer 'AI' functionality.
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
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

#ifndef COMP_H
#define COMP_H

#include "thinker.h"

void CompThreadInit(ThinkContextT *th);
// Returns current variation for main board. 
CvT *CompMainCv(void);
// Returns current search level for main board.
int CompCurrentLevel(void);

#endif // COMP_H
