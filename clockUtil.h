//--------------------------------------------------------------------------
//                 clockUtil.h - supplementary clock routines
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

#ifndef CLOCKUTIL_H
#define CLOCKUTIL_H

#include "aTypes.h"
#include "game.h"

#ifdef __cplusplus
extern "C" {
#endif

void ClocksReset(GameT *game);
void ClocksStop(GameT *game);
void ClocksPrint(GameT *game, char *context);
void GoaltimeCalc(GameT *game);
bigtime_t getBigTime(void);

#ifdef __cplusplus
}
#endif

#endif // CLOCKUTIL_H
