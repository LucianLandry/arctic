//--------------------------------------------------------------------------
//                  Eval.cpp - (position) evaluation class.
//                           -------------------
//  copyright            : (C) 2016 by Lucian Landry
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

#include <stdio.h>

#include "Eval.h"

// Writes to and returns 'result', which is assumed to be at least
//  kMaxEvalStringLen chars long.
char *Eval::ToLogString(char *result) const
{
    sprintf(result, "{(Eval) %d %d}", lowBound, highBound);
    return result;
}
