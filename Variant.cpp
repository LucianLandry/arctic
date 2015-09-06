//--------------------------------------------------------------------------
//                variant.cpp - (rudimentary) variant support
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

#include "Variant.h"

static Variant gChess;
Variant *Variant::current = &gChess;

Variant::Variant()
{
    const CastleCoordsT normalCastling[NUM_PLAYERS] =
    {
        // White
        { .start  = { .king = 4, .rookOO = 7, .rookOOO = 0 },
          .endOO  = { .king = 6, .rook = 5 },
          .endOOO = { .king = 2, .rook = 3 }
        },
        // Black
        { .start  = { .king = 60, .rookOO = 63, .rookOOO = 56 },
          .endOO  = { .king = 62, .rook = 61 },
          .endOOO = { .king = 58, .rook = 59 }
        }
    };
    const Piece normalStartingPieces[NUM_SQUARES] =
    {
        // 1st row
        Piece(0, PieceType::Rook),   Piece(0, PieceType::Knight),
        Piece(0, PieceType::Bishop), Piece(0, PieceType::Queen),
        Piece(0, PieceType::King),   Piece(0, PieceType::Bishop),
        Piece(0, PieceType::Knight), Piece(0, PieceType::Rook),
        // 2nd row
        Piece(0, PieceType::Pawn), Piece(0, PieceType::Pawn),
        Piece(0, PieceType::Pawn), Piece(0, PieceType::Pawn),
        Piece(0, PieceType::Pawn), Piece(0, PieceType::Pawn),
        Piece(0, PieceType::Pawn), Piece(0, PieceType::Pawn),
        // 3rd row
        Piece(), Piece(), Piece(), Piece(), Piece(), Piece(), Piece(), Piece(),
        // 4th row    
        Piece(), Piece(), Piece(), Piece(), Piece(), Piece(), Piece(), Piece(),
        // 5th row
        Piece(), Piece(), Piece(), Piece(), Piece(), Piece(), Piece(), Piece(),
        // 6th row
        Piece(), Piece(), Piece(), Piece(), Piece(), Piece(), Piece(), Piece(),
        // 7th row
        Piece(1, PieceType::Pawn), Piece(1, PieceType::Pawn),
        Piece(1, PieceType::Pawn), Piece(1, PieceType::Pawn),
        Piece(1, PieceType::Pawn), Piece(1, PieceType::Pawn),
        Piece(1, PieceType::Pawn), Piece(1, PieceType::Pawn),
        // 8th row
        Piece(1, PieceType::Rook),   Piece(1, PieceType::Knight),
        Piece(1, PieceType::Bishop), Piece(1, PieceType::Queen),
        Piece(1, PieceType::King),   Piece(1, PieceType::Bishop),
        Piece(1, PieceType::Knight), Piece(1, PieceType::Rook)
    };

    int i;
    
    for (i = 0; i < NUM_PLAYERS; i++)
        castling[i] = normalCastling[i];

    for (i = 0; i < NUM_SQUARES; i++)
        startingPosition.SetPiece(i, normalStartingPieces[i]);

    startingPosition.EnableCastling();
}

bool Variant::IsLegalPiece(Piece piece) const
{
    // The way Piece is composed, 'turn' currently cannot be illegal, so we
    //  do not check that.
    if (piece.IsEmpty())
        return true;

    // If additional variants were added, we would try to check against a vector
    //  or something here.
    switch(piece.Type())
    {
    case PieceType::Pawn:
    case PieceType::Knight:
    case PieceType::Bishop:
    case PieceType::Rook:
    case PieceType::Queen:
    case PieceType::King:
        return true;
    default:
        break;
    }

    return false;
}
