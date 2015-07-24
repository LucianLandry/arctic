//--------------------------------------------------------------------------
//                  Piece.h - basic chess pieces for Arctic.
//                           -------------------
//  copyright            : (C) 2015 by Lucian Landry
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

#ifndef PIECE_H
#define PIECE_H

#include "aTypes.h"
#include "ref.h"

enum class PieceType : uint8
{
    Empty  = 0, // "No" piece.
    King   = 1, //  01b
    Pawn   = 2, //  10b
    Knight = 3, //  11b
    Bishop = 4, // 100b
    Rook   = 5, // 101b
    Queen  = 6  // 110b
};

enum class PieceRelationship
{
    // The specific values of these enums are optimized for use with the move
    //  generator.
    Self  = 0,
    Empty = 1,
    Enemy = 2
};

// Sentinel value.  (Do we want to expose this?  It would seem to be easy to
//  confuse with kMaxPieces.)
// const int kMaxPieceTypes = PieceType::Queen + 1;

// You can declare arrays of this size, for use with piece.ToIndex().
// It is declared in this awkward way just in case somebody makes NUM_PLAYERS
//  not be a multiple of 2.
const int kMaxPieces =
    ((int(PieceType::Queen) << NUM_PLAYERS_BITS) | NUM_PLAYERS_MASK) + 1;

class Piece
{
public:
    inline Piece();
    inline Piece(int player, PieceType type);
    inline Piece(const Piece &other);

    inline bool operator==(const Piece &other) const;
    inline bool operator!=(const Piece &other) const;
    
    // Returns an int-version of 'Piece' that you can use as an index into
    //  an array.  It will be unique for each (turn, type) tuple, and a fairly
    //  low number (so that this function is actually usable).
    inline int ToIndex() const;

    // Returns: who owns this piece.  Not defined for PieceType::Empty pieces.
    inline int Player() const;

    // Returns: type of this piece.
    inline PieceType Type() const;
    
    // Returns: is the piece capable of attacking like a rook, bishop, etc.
    // A queen, for instance, can attack like both.
    inline bool AttacksLikeRook() const;
    inline bool AttacksLikeBishop() const;

    inline bool IsEmpty() const;
    inline bool IsKing() const;
    inline bool IsPawn() const;
    inline bool IsKnight() const;
    inline bool IsBishop() const;
    inline bool IsRook() const;
    inline bool IsQueen() const;

    // Returns: material worth of piece.
    inline int Worth() const;

    inline PieceRelationship Relationship(int player) const;
    inline bool IsEnemy(int player) const;
    
    // Generally, only code that deals with variants should call these routines.
    static void SetWorth(PieceType type, int materialWorth);
    static void ClearAllWorth();

    // Must be called once at program startup.
    static void Init();
private:
    // internal representation.
    // Current layout strategy is (pieceType << NUM_PLAYERS_BITS) | turn.
    // The thought process was that we would usually have more piece types
    //  than players, so we would have to do less bit-shifting to get to the
    //  piece type (also, the piece type is often constant so it wouldn't
    //  matter).
    uint8 piece;

    // Pre-calculated material worth of pieces.  For flexibility (giveaway
    //  variants?), should be a signed type.
    static int worth[kMaxPieces];

    // Pre-calculated identification of friend, enemy, or unoccupied.
    static PieceRelationship relationship[kMaxPieces] [NUM_PLAYERS];
};

//--------------------------------------------------------------------------
// BEGIN PRIVATE PART
//--------------------------------------------------------------------------

inline Piece::Piece() : piece(uint8(PieceType::Empty))
{
}

inline Piece::Piece(int player, PieceType type)
{
    piece = (int(type) << NUM_PLAYERS_BITS) | player;
}

inline Piece::Piece(const Piece &other)
{
    piece = other.piece;
}

inline bool Piece::operator==(const Piece &other) const
{
    return piece == other.piece;
}

inline bool Piece::operator!=(const Piece &other) const
{
    return piece != other.piece;
}

inline int Piece::ToIndex() const
{
    return piece;
}

inline int Piece::Player() const
{
    return piece & NUM_PLAYERS_MASK;
}

inline PieceType Piece::Type() const
{
    return PieceType(piece >> NUM_PLAYERS_BITS);
}

inline bool Piece::AttacksLikeRook() const
{
    return piece >= int(PieceType::Rook) << NUM_PLAYERS_BITS;
}

inline bool Piece::AttacksLikeBishop() const
{
    return (piece ^ (0x1 << NUM_PLAYERS_BITS)) >=
        int(PieceType::Rook) << NUM_PLAYERS_BITS;
}

inline bool Piece::IsEmpty() const
{
    return piece == 0;
}

inline bool Piece::IsKing() const
{
    // These are all written out the hard way (instead of checking, for example,
    //  Type() == PieceType::King) since the theory is that a mask might be
    //  faster than a shift operation (that is, if we ever support
    //  NUM_PLAYERS_BITS > 1).
    return (piece & ~NUM_PLAYERS_MASK) ==
        int(PieceType::King) << NUM_PLAYERS_BITS;
}

inline bool Piece::IsPawn() const
{
    return (piece & ~NUM_PLAYERS_MASK) ==
        int(PieceType::Pawn) << NUM_PLAYERS_BITS;
}

inline bool Piece::IsKnight() const
{
    return (piece & ~NUM_PLAYERS_MASK) ==
        int(PieceType::Knight) << NUM_PLAYERS_BITS;
}

inline bool Piece::IsBishop() const
{
    return (piece & ~NUM_PLAYERS_MASK) ==
        int(PieceType::Bishop) << NUM_PLAYERS_BITS;
}

inline bool Piece::IsRook() const
{
    return (piece & ~NUM_PLAYERS_MASK) ==
        int(PieceType::Rook) << NUM_PLAYERS_BITS;
}

inline bool Piece::IsQueen() const
{
    return (piece & ~NUM_PLAYERS_MASK) ==
        int(PieceType::Queen) << NUM_PLAYERS_BITS;
}

inline int Piece::Worth() const
{
    return worth[piece];
}

inline PieceRelationship Piece::Relationship(int player) const
{
    return relationship[piece] [player];
}

inline bool Piece::IsEnemy(int player) const
{
    return Relationship(player) == PieceRelationship::Enemy;
}

#endif // PIECE_H
