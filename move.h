//--------------------------------------------------------------------------
//                      move.h - chess moves for Arctic
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


#ifndef MOVE_H
#define MOVE_H

#include "aTypes.h"
#include "ref.h"
#include "Piece.h"

// Various ways to represent a move as a string.
// See http://en.wikipedia.org/wiki/Chess_notation
enum MoveNotationT
{
    mnSAN,  // Standard algebraic notation (most human-readable.  Example:
            //  "bxa8Q")

    mnCAN,  // Coordinate albebraic notation (but no dashes, no parenthesis for
            //  promotion.  Example: "b7a8q")
    
    mnDebug // Stringified representation of full MoveT structure, used for
            //  debugging.  Example: "b7a8.12.FF".  Ignores castleStyle and
            //  showCheck.
};

enum MoveCastleStyleT
{
    csOO,     // Use (PGN) "O-O" and "O-O-O" (even for mnCAN).  This is our
              //  preferred internal string representation.
    csFIDE,   // Like above, but use zeros ("0-0") instead of letter Os.
              //  Currently unneeded, but trivial to implement.
    csKxR,    // Use King-captures-rook notation (used by UCI for chess960)
    csK2      // Use King-moves-2-spaces notation (falls back to csOO when this
              //  is impossible, ie some variants)
};

// Obviously the code implements a limited range of move styles.  It can be
// expanded if necessary.
struct MoveStyleT
{
    MoveStyleT(MoveNotationT notation, MoveCastleStyleT castleStyle, bool showCheck);
    MoveNotationT notation;
    MoveCastleStyleT castleStyle;
    bool showCheck; // Append '+' and '#' (when known) to moves?
};

class Board; // forward declaration

// Any stringified-move (including null terminator) is guaranteed to fit into
// a char[MOVE_STRING_MAX].
#define MOVE_STRING_MAX (20) // Need this length for insane strings (rounded
                             //  up to a 4-byte boundary)

// Our basic structure for representing a chess move.
// The normal convention will be to try to pass around MoveTs by structure
//  (not with pointers) because doing so should be cheaper (4 bytes vs 8 bytes
//  on a 64-bit arch).
//
// Castling moves follow a peculiar convention: 'src' =
//  "((isCastleOO ? 1 : 0) << NUM_PLAYERS_BITS) | turn"; and
//  'dst' = 'src'.
// The reasoning is that it is very easy to detect a castle (check src == dst),
//  and the notation is portable across chess variants.  We mask in 'turn'
//  because it becomes easier to convert back to other notations.

struct /* alignas(uint32) makes things slower */ MoveT
{
    MoveT() = default; // allow uninitialized MoveTs.
    inline MoveT(cell_t from, cell_t to, PieceType promote, cell_t chk);
    MoveT(const MoveT &other) = default;
    
    cell_t src;    // For a null/invalid move, src = FLAG (and the contents
                   //  of the other fields are undefined.)
    cell_t dst;
    PieceType promote; // Usually, this is PieceType::Empty.
                       // In case of pawn promotion, it is the PieceType to
                       //  promote the pawn to.
                       // Also (ab)used for en passant, signified by
                       //  PieceType::Pawn.
    
    cell_t chk;    // Is this a checking move.  Set to:
                   // FLAG, if not a checking move.
                   // Coordinate of checking piece, if single check
                   // DOUBLE_CHECK, otherwise.
                   // (this is the same convention as BoardT->ncheck[])
    inline bool operator==(const MoveT &other) const;
    inline bool operator!=(const MoveT &other) const;

    // 'result' should be at least MOVE_STRING_MAX chars long.
    char *ToString(char *result, const MoveStyleT *style,
                   // Used for disambiguation and legality checks, when !NULL.
                   const Board *board) const;
    inline bool IsCastle() const;
    inline bool IsCastleOO() const;
    inline bool IsCastleOOO() const;
    inline bool IsEnPassant() const; // Is this an en passant capture?
    inline bool IsPromote() const;   // Is this a pawn promotion?
    bool IsLegal(const Board &board) const;

    // This is only a partial move creation routine as it does not fill in
    //  'chk', and in fact, clobbers it.
    void CreateFromCastle(bool castleOO, int turn);

    // Attempt to take a king-moves-2 or KxR-style move and convert it
    //  to the correct format.  Does nothing if the move is not actually
    //  detected as a castle.
    // We need a 'board' arg (or at least cbyte) because otherwise (for example)
    //  a king capturing its own rook one space to the right could be confused
    //  with just moving the king one space to the right.
    // Assumes we are 'unmangling' a move from the players whose turn it is.
    void UnmangleCastle(const Board &board);
};

static_assert(sizeof(uint32) == sizeof(MoveT), "MoveT.operator== is broken");

// "No" move (fails moveIsSane(), so do not try to print it)
const MoveT MoveNone(FLAG, 0, PieceType::Empty, FLAG);

inline MoveStyleT::MoveStyleT(MoveNotationT notation,
                              MoveCastleStyleT castleStyle,
                              bool showCheck) :
    notation(notation), castleStyle(castleStyle), showCheck(showCheck) {}

inline MoveT::MoveT(cell_t from, cell_t to, PieceType promote, cell_t chk) :
    src(from), dst(to), promote(promote), chk(chk) {}

inline bool MoveT::operator==(const MoveT &other) const
{
    return
    *reinterpret_cast<const uint32 *>(this) ==
    *reinterpret_cast<const uint32 *>(&other);
}

inline bool MoveT::operator!=(const MoveT &other) const
{
    return !(*this == other);
}

// Writes out a sequence of moves using style 'moveStyle'.
// Returns the number of moves successfully converted.
int MovesToString(char *dstStr, int dstLen,
                  const MoveT *moves, int numMoves,
                  const MoveStyleT &moveStyle,
                  const Board &board);

inline bool MoveT::IsCastle() const
{
    return src == dst;
}

inline bool MoveT::IsCastleOO() const
{
    return IsCastle() && (src >> NUM_PLAYERS_BITS) == 0;
}

inline bool MoveT::IsCastleOOO() const
{
    return IsCastle() && (src >> NUM_PLAYERS_BITS) == 1;
}

inline bool MoveT::IsEnPassant() const
{
    return promote == PieceType::Pawn;
}

inline bool MoveT::IsPromote() const
{
    return promote != PieceType::Empty && !IsEnPassant();
}

#endif // MOVE_H
