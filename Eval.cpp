//--------------------------------------------------------------------------
//                  Eval.cpp - (position) evaluation class.
//                           -------------------
//  copyright            : (C) 2016 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#include <stdio.h>

#include "Eval.h"

// Writes to and returns 'result', which is assumed to be at least
//  kMaxEvalStringLen chars long.
char *Eval::ToLogString(char *result) const
{
    sprintf(result, "{(Eval) %d %d}", lowBound, highBound);
    return result;
}
