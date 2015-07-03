//--------------------------------------------------------------------------
//                  position.c - position-related functions.
//                           -------------------
//  copyright            : (C) 2013 by Lucian Landry
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

#include <stdio.h>
#include "position.h"

const PositionEvalT gPELoss = { EVAL_LOSS, EVAL_LOSS };

char *PositionEvalToLogString(char *result, PositionEvalT *pe)
{
    sprintf(result, "{(PosEval) %d %d}", pe->lowBound, pe->highBound);
    return result;
}
