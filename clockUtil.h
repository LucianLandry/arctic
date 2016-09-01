//--------------------------------------------------------------------------
//                 clockUtil.h - supplementary clock routines
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

#ifndef CLOCKUTIL_H
#define CLOCKUTIL_H

#include "aTypes.h"
#include "Game.h"

// 2^32 / 3600 = 7 digits, + 2 colons, +minutes+seconds + terminator
#define CLOCK_TIME_STR_LEN 14

bool TimeStringIsValid(char *str);
bigtime_t TimeStringToBigTime(char *str);
char *TimeStringFromBigTime(char *result, bigtime_t myTime);
bigtime_t CurrentTime(); // Returns 'now' in bigtime_t units.

#endif // CLOCKUTIL_H
