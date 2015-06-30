//--------------------------------------------------------------------------
//                  position.h - position-related functions.
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

#ifndef POSITION_H
#define POSITION_H

#include "aTypes.h"
#include "list.h"
#include "ref.h"

#ifdef __cplusplus
extern "C" {
#endif

// Inherits from ListElementT.
typedef struct {
    ListElementT el;
    uint64 zobrist;
} PositionElementT;

typedef struct {
    int lowBound;
    int highBound;
} PositionEvalT;

#define POSITIONEVAL_STRING_MAX (26)
// Writes to 'result', which is assumed to be at least POSITIONEVAL_STRING_MAX
//  chars long.
char *PositionEvalToLogString(char *result, PositionEvalT *pe);

// Convenience constants used for initialization.
extern const PositionEvalT gPELoss;



#ifdef __cplusplus
}
#endif

#endif // POSITION_H
