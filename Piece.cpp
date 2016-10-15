//--------------------------------------------------------------------------
//                 Piece.cpp - basic chess pieces for Arctic.
//                           -------------------
//  copyright            : (C) 2015 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
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
