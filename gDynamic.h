//--------------------------------------------------------------------------
//                gDynamic.h - all global dynamic variables.
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

#ifndef GDYNAMIC_H
#define GDYNAMIC_H

#include "ref.h"
#include "Pv.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NO_LIMIT (-1)

typedef struct {
    bool ponder;      // allow computer to ponder? (default: false)
    HintPv pv;        // Attempts to keep track of principal variation.
    int gameCount;    // for stats keeping.
} GDynamicT;
extern GDynamicT gVars;

#ifdef __cplusplus
}
#endif

#endif // GDYNAMIC_H
