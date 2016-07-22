//--------------------------------------------------------------------------
//                 Piece.cpp - basic chess pieces for Arctic.
//                           -------------------
//  copyright            : (C) 2015 by Lucian Landry
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

#include "Piece.h"

// Pre-calculated material worth of pieces.  For flexibility (giveaway
//  variants?), should be a signed type.
int Piece::worth[kMaxPieces];

// Pre-calculated identification of friend, enemy, or unoccupied.
PieceRelationship Piece::relationship[kMaxPieces] [NUM_PLAYERS];

void Piece::SetWorth(PieceType type, int materialWorth)
{
    for (int i = 0; i < NUM_PLAYERS; i++)
    {
        worth[(int(type) << NUM_PLAYERS_BITS) | i] = materialWorth;
    }
}

void Piece::ClearAllWorth()
{
    for (int i = 0; i < kMaxPieces; i++)
    {
        worth[i] = 0;
    }
}

PieceRelationship relationshipFunc(uint8 piece, int player)
{
    return
        piece < (int(PieceType::King) << NUM_PLAYERS_BITS) ?
        PieceRelationship::Empty :
        (piece & NUM_PLAYERS_MASK) == player ?
        PieceRelationship::Self :
        PieceRelationship::Enemy;
}

void Piece::Init()
{
    // Initialize relationship table.
    for (int i = 0; i < kMaxPieces; i++)
    {
        for (int j = 0; j < NUM_PLAYERS; j++)
        {
            relationship[i][j] = relationshipFunc(i, j);
        }
    }
}
