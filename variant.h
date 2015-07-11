//--------------------------------------------------------------------------
//                 variant.h - (rudimentary) variant support
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

#ifndef VARIANT_H
#define VARIANT_H

#include "ref.h"

#ifdef __cplusplus
extern "C" {
#endif

// queen-side and king-side rooks should be mapped to what (PGN-style) "O-O"
// and "O-O-O" do, not whether the move is traditionally a "queen-side" or
// "king-side" castle (or "left" or "right").  In standard chess it obviously
// makes no difference, but for example in a variant like FICS wild 0, "O-O"
// would denote short-side castling even for black.
typedef struct {
    uint8 king, rookOO, rookOOO;
} CastleStartCoordsT;

typedef struct {
    uint8 king, rook; // end position of the castled king + rook
} CastleEndCoordsT;

typedef struct {
    CastleStartCoordsT start;
    CastleEndCoordsT endOO, endOOO;
} CastleCoordsT;

typedef struct {
    // obviously this only applies to chess
    CastleCoordsT castling[NUM_PLAYERS];
} VariantT;

extern VariantT *gVariant;

#ifdef __cplusplus
}
#endif
    
#endif // VARIANT_H
