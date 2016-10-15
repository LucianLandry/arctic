//--------------------------------------------------------------------------
//                 clockUtil.h - supplementary clock routines
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
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
