//--------------------------------------------------------------------------
//                 Position.cpp - position-related functions.
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
#include <string>

#include "gPreCalc.h"
#include "Position.h"
#include "uiUtil.h"   // for logging
#include "Variant.h"

using arctic::File;
using arctic::Rank;
using arctic::ToCoord;
using std::to_string;

const PositionEvalT gPELoss = { EVAL_LOSS, EVAL_LOSS };

char *PositionEvalToLogString(char *result, PositionEvalT *pe)
{
    sprintf(result, "{(PosEval) %d %d}", pe->lowBound, pe->highBound);
    return result;
}

Position::Position()
{
    ply = 0;
    ncpPlies = 0;
    cbyte = 0;
    ebyte = FLAG;
    turn = 0;
}

Position::Position(const Position &other)
{
    for (int i = 0; i < NUM_SQUARES; i++)
        coords[i] = other.coords[i];

    ply = other.ply;
    ncpPlies = other.ncpPlies;
    cbyte = other.cbyte;
    ebyte = other.ebyte;
    turn = other.turn;
}

bool Position::operator==(const Position &other) const
{
    return
        this == &other ||
        // !memcmp(sizeof(Position)) does not work, since there is some spare,
        //  uninitialized space.
        (!memcmp(coords, other.coords, sizeof(Piece) * NUM_SQUARES) &&
         ply == other.ply &&
         ncpPlies == other.ncpPlies &&
         cbyte == other.cbyte &&
         ebyte == other.ebyte &&
         turn == other.turn);
}

bool Position::IsRepeatOf(const Position &other) const
{
    return
        this == &other ||
        (!memcmp(coords, other.coords, sizeof(coords)) &&
         cbyte == other.cbyte &&
         ebyte == other.ebyte &&
         turn == other.turn);
}

// checks to see if there are any occupied squares between 'src' and 'dest'.
// returns: false if blocked, true if nopose.  Note:  doesn't check if
//  dir == DIRFLAG (none) or 8 (knight attack), so shouldn't be called in that
//  case.
// Also does not check if 'src' == 'dest'.
bool Position::noInterposingPiece(cell_t src, cell_t dest) const
{
    int dir = gPreCalc.dir[src] [dest];
    cell_t *to = gPreCalc.moves[dir] [src];
    while (*to != dest)
    {
        if (!coords[*to].IsEmpty())
            return false;   // some sq on the way to dest is occupied.
        to++;
    }
    return true; // notice we always hit dest before we hit end of list.
}

bool Position::bishopAttacks(cell_t src, cell_t dest) const
{
    return !((gPreCalc.dir[src] [dest]) & 0x9) /* !DIRFLAG or nightmove */ &&
        noInterposingPiece(src, dest);
}

bool Position::rookAttacks(cell_t src, cell_t dest) const
{
    return ((gPreCalc.dir[src] [dest]) & 0x1) /* !DIRFLAG */ &&
        noInterposingPiece(src, dest);
}

bool Position::queenAttacks(cell_t src, cell_t dest) const
{
    return (gPreCalc.dir[src] [dest]) < 8 &&
        noInterposingPiece(src, dest);
}

bool Position::pieceAttacks(cell_t src, cell_t dest) const
{
    switch (coords[src].Type())
    {
        case PieceType::Pawn:
        {
            uint8 player = coords[src].Player();
            cell_t *moves = gPreCalc.moves[10 + player] [src];
            return moves[0] == dest || moves[1] == dest;
        }
        case PieceType::Knight:
            return gPreCalc.dir[src] [dest] == 8;
        case PieceType::Bishop:
            return bishopAttacks(src, dest);
        case PieceType::Rook:
            return rookAttacks(src, dest);
        case PieceType::Queen:
            return queenAttacks(src, dest);
        case PieceType::King:
            return
                abs(Rank(src) - Rank(dest)) < 2 &&
                abs(File(src) - File(dest)) < 2;
        default: // assume 'empty' piece
            break;
    }
    return false;
}

bool Position::attacked(cell_t coord, uint8 onwho) const
{
    int i;
    for (i = 0; i < NUM_SQUARES; i++)
    {
        if (i != coord &&
            coords[i].IsEnemy(onwho) &&
            pieceAttacks(i, coord))
        {
            return true;
        }
    }
    return false;
}

cell_t Position::CheckingCoord() const
{
    int i, j;
    cell_t kcoord = FLAG;
    cell_t ncheck = FLAG;
    
    for (i = 0; i < NUM_SQUARES; i++)
    {
        if (coords[i].IsKing() && coords[i].IsSelf(turn))
        {
            kcoord = i;
            for (j = 0; j < NUM_SQUARES; j++)
            {
                if (coords[j].IsEnemy(turn) && pieceAttacks(j, kcoord))
                {
                    ncheck = (ncheck == FLAG ? j : DOUBLE_CHECK);
                    if (ncheck == DOUBLE_CHECK)
                        return DOUBLE_CHECK; // since we know the result.
                }
            }
        }
    }
    return ncheck;
}

uint8 Position::calcNewCByte() const
{
    int i;
    CastleStartCoordsT castleStart;
    uint8 result;
    
    if (cbyte == 0)
    {
        return 0; // be lazy when possible
    }

    result = cbyte;

    for (i = 0; i < NUM_PLAYERS; i++)
    {
        castleStart = Variant::Current()->Castling(i).start;

        if (coords[castleStart.king] != Piece(i, PieceType::King))
        {
            // No O-O or O-O-O castling.
            result &= ~(CASTLEBOTH << i);
        }
        else
        {
            if (coords[castleStart.rookOO] != Piece(i, PieceType::Rook))
            {
                // No O-O castling.
                result &= ~(CASTLEOO << i);
            }
            if (coords[castleStart.rookOOO] != Piece(i, PieceType::Rook))
            {
                // No O-O-O castling.
                result &= ~(CASTLEOOO << i);
            }
        }
    }
    return result;
}

uint8 Position::calcNewEByte() const
{
    return
        (ebyte == FLAG ||
         (coords[ebyte].IsPawn() &&
          coords[ebyte].IsEnemy(turn) &&
          // for black, ebyte must be a4-h4.
          ((turn && ebyte >= 24 && ebyte <= 31) ||
           // for white, ebyte must be a5-h5.
           (!turn && ebyte >= 32 && ebyte <= 39)))) ?
        ebyte :
        FLAG;
}

bool Position::badCByte() const
{
    return calcNewCByte() != cbyte;
}

bool Position::badEByte() const
{
    return calcNewEByte() != ebyte;
}

bool Position::isLegal(std::string *errReason) const
{
    // Check: It must be white or black's turn.
    if (turn >= NUM_PLAYERS)
    {
        if (errReason != nullptr)
            *errReason = "Bad turn value (" + to_string(turn) + ").";
        return false;        
    }

    // Check: ply must be >= ncpPlies.
    // (plies < ncpPlies should not be possible, and could screw up
    //  3fold repetiion calculation.)
    if (ply < 0 || ncpPlies < 0 || ply < ncpPlies)
    {
        if (errReason != nullptr)
        {
            *errReason = "Bad ply/ncpPlies (" + to_string(ply) + ", " +
                to_string(ncpPlies) + ").";
        }
        return false;
    }

    cell_t kcoord[NUM_PLAYERS];
    int kingCount[NUM_PLAYERS];
    int i;
    
    for (i = 0; i < NUM_PLAYERS; i++)
    {
        kcoord[i] = FLAG;
        kingCount[i] = 0;
    }
        
    for (i = 0; i < NUM_SQUARES; i++)
    {
        // Check: All the pieces on this board must be legal for this variant.
        if (!Variant::Current()->IsLegalPiece(coords[i]))
        {
            if (errReason != nullptr)
                *errReason = "Illegal piece, (coord " + to_string(i) + ").";
            return false;
        }

        if (coords[i].IsKing())
        {
            uint8 player = coords[i].Player();
            kcoord[player] = i;
            kingCount[player]++;
        }
    }

    // Check: exactly one king (of each color) on the board.
    for (i = 0; i < NUM_PLAYERS; i++)
    {
        if (kingCount[i] != 1)
        {
            if (errReason != nullptr)
            {
                *errReason = "Need one king of each color (player " +
                    to_string(i) + ", found " + to_string(kingCount[i]) + ").";
            }
            return false;
        }
    }

    // Check: pawns must not be on 1st or 8th rank.
    for (i = 0; i < NUM_SQUARES; i++)
    {
        if (i == 8) i = 56; // skip to the 8th rank.
        if (coords[i].IsPawn())
        {
            if (errReason != nullptr)
                *errReason = "Pawn detected on 1st or 8th rank.";
            return false;
        }
    }

    // Check: the side *not* on move must not be in check.
    if (attacked(kcoord[turn ^ 1], turn ^ 1))
    {
        if (errReason != nullptr)
        {
            *errReason = "Player not on move (" + to_string(turn ^ 1) +
                ")is in check.";
        }
        return false;
    }

    // Check: for bad en passant byte.
    if (badEByte())
    {
        if (errReason != nullptr)
            *errReason = "bad enpassant coord (" + to_string(ebyte) + ").";
        return false;
    }
    
    // Check: for bad castling byte.
    if (badCByte())
    {
        if (errReason != nullptr)
            *errReason = "bad castling byte (" + to_string(cbyte) + ").";
        return false;
    }

    return true; // The position looks legal.
}

void Position::Sanitize()
{
    turn = MIN(turn, NUM_PLAYERS - 1);
    ncpPlies = MAX(ncpPlies, 0);
    ply = MAX(ply, ncpPlies);

    for (int i = 0; i < NUM_SQUARES; i++)
    {
        if (!Variant::Current()->IsLegalPiece(coords[i]) ||
            // Do not allow pawns on first or eighth ranks.
            (coords[i].IsPawn() && (Rank(i) == 0 || Rank(i) == 7)))
        {
            // Remove any illegal pieces.
            coords[i] = Piece();
        }
    }
    cbyte = calcNewCByte();
    ebyte = calcNewEByte();
}

void Position::Log(LogLevelT level) const
{
    if (level > LogLevel())
    {
        return; // no-op
    }

    LogPrint(level, "{(Position %p) coords: ", this);

    for (int rank = 7; rank >= 0; rank--)
    {
        for (int file = 0; file < 8; file++)
        {
            int chr = nativeToAscii(PieceAt(ToCoord(rank, file)));
            LogPrint(level, "%c", chr == ' ' ? '.' : chr);
        }
        LogPrint(level, " ");
    }

    LogPrint(level, "ply %d ncpPlies %d cbyte 0x%x ebyte %d turn %d}",
             ply, ncpPlies, cbyte, ebyte, turn);
}
