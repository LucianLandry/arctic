//--------------------------------------------------------------------------
//                 Variant.h - (rudimentary) variant support
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

#include "Position.h"
#include "ref.h"

// queen-side and king-side rooks should be mapped to what (PGN-style) "O-O"
// and "O-O-O" do, not whether the move is traditionally a "queen-side" or
// "king-side" castle (or "left" or "right").  In standard chess it obviously
// makes no difference, but for example in a variant like FICS wild 0, "O-O"
// would denote short-side castling even for black.
typedef struct {
    cell_t king, rookOO, rookOOO;
} CastleStartCoordsT;

typedef struct {
    cell_t king, rook; // end position of the castled king + rook
} CastleEndCoordsT;

typedef struct {
    CastleStartCoordsT start;
    CastleEndCoordsT endOO, endOOO;
} CastleCoordsT;

enum class VariantType
{
    // We only support normal chess for now.
    Chess
};

class Variant
{
public:
    Variant();

    // Returns a struct containing castling information for this variant.
    inline const CastleCoordsT &Castling(uint8 turn) const;
    // Returns starting position of a normal game.  If there is no such position
    //  (for instance, chess960) then a nominal legal position is returned.
    inline const Position &StartingPosition() const;
    bool IsLegalPiece(Piece piece) const;
    static inline const Variant *Current();
private:    
    // obviously this only applies to chess
    CastleCoordsT castling[NUM_PLAYERS];
    Position startingPosition;
    static Variant *current;
};

inline const CastleCoordsT &Variant::Castling(uint8 turn) const
{
    return castling[turn];
}

inline const Position &Variant::StartingPosition() const
{
    return startingPosition;
}

inline const Variant *Variant::Current()
{
    return current;
}

#endif // VARIANT_H
