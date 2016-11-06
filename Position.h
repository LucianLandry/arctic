//--------------------------------------------------------------------------
//                  Position.h - position-related functions.
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#ifndef POSITION_H
#define POSITION_H

#include <string>
#include <string.h>

#include "aList.h"
#include "aTypes.h"
#include "log.h"
#include "Piece.h"
#include "ref.h"

// Inherits from ListElementT.
typedef struct {
    arctic::ListElement el;
    uint64 zobrist;
} PositionInfoElementT;

// Any static board position that can be set by FEN.  May contain an
//  in-progress or "illegal" position that a Board cannot be set to.
class Position
{
public:
    Position(); // initializes to an empty position.
    Position(const Position &other);
    Position &operator=(const Position &other) = default;

    // Returns 'true' iff this position *exactly* matches 'other' (including
    //  ply and ncpPlies).
    bool operator==(const Position &other) const;
    inline bool operator!=(const Position &other) const;

    // Returns 'true' iff this position has the same pieces on the same squares,
    //  side to move, castling rights, and en passant situation as 'other'.
    // Current ply and non-repeatable plies are not considered.
    bool IsRepeatOf(const Position &other) const;

    // Is this a legal position for the current variant?
    inline bool IsLegal(std::string &errStr) const;
    inline bool IsLegal() const;

    // Getter functions.
    inline Piece PieceAt(cell_t coord) const;
    inline int Ply() const;
    inline uint8 Turn() const;
    inline int NcpPlies() const;
    inline cell_t EnPassantCoord() const;
    
    // Returns the cell giving check to the current turn (or FLAG if none, or
    //  DOUBLE_CHECK if multiple cells are giving check).
    // If the position is not legal ... best effort is employed.
    // This function runs much faster for Boards than for Positions.
    cell_t CheckingCoord() const;
    
    // These functions only (obviously) return whether it *may be* possible to
    //  castle now or in the future.
    inline bool CanCastleOO(uint8 turn) const;
    inline bool CanCastleOOO(uint8 turn) const;
    inline bool CanCastle(uint8 turn) const;

    inline void EnableCastlingOO(uint8 turn);
    inline void EnableCastlingOOO(uint8 turn);
    // Enables castling on both sides.
    inline void EnableCastling(uint8 turn);
    // Enables castling on both sides, by both players.
    inline void EnableCastling();
    inline void ClearCastling();
    
    // Setter functions.  The ones that return 'bool' do minor sanity checking
    //  on their input and return 'false' if the value could not be set.
    inline void SetPiece(cell_t coord, Piece piece);
    inline void SetEnPassantCoord(cell_t coord);
    inline bool SetNcpPlies(int newNcpPlies);
    inline bool SetPly(int newPly);
    inline bool SetTurn(uint8 newTurn);

    // Try to make a position as legal as possible.  This is best effort.
    void Sanitize();

    void Log(LogLevelT level) const;

protected:
    Piece coords[NUM_SQUARES]; // all the squares on the board.

    // Note: because of the optional nature of the fifty-move rule draw, 'ply'
    //  and 'ncpPlies' are both 'int' instead of 'uint16' and 'uint8',
    //  respectively.  (For reasons outlined in Google's coding conventions, we
    //  also don't go with uint32, here.)
    int ply;         // (aka 1/2-move.) Usually, white's 1st move is '0'.
                     // (NOTE: this is not always the case; some edited
                     //  positions might have black to move first.)
    // How many plies has it been since last capture or pawn-move.  If 100
    //  plies passed, the game can be drawn by the fifty-move rule.
    // More specifically, (also,) if the next move will trigger the fifty-move
    //  rule, one side can announce its intention to draw and then play the
    //  move.
    // We ignore that, currently -- two players can decide whether to draw, and
    //  the computer will check before and after its move if the fifty-move
    //  draw rule applies.
    int ncpPlies;

    uint8 cbyte;     // castling byte.
                     // Format is 1q-0q-1k-0k, where (1,0) is the turn number
                     //  and (q,k) is OOO or OO castling.  If NUM_PLAYERS is
                     //  expanded, then the offset to the OOO castling bits
                     //  increases.
    cell_t ebyte;    // en passant byte.  Set to the destination coord
                     //  of an a2a4-style move (or FLAG otherwise).
    uint8 turn;      // Whose turn is it.  0 == white, 1 == black.

    uint8 calcNewCByte() const;
    uint8 calcNewEByte() const;

private:
    bool noInterposingPiece(cell_t src, cell_t dest) const;
    bool bishopAttacks(cell_t src, cell_t dest) const;
    bool rookAttacks(cell_t src, cell_t dest) const;
    bool queenAttacks(cell_t src, cell_t dest) const;
    bool pieceAttacks(cell_t src, cell_t dest) const;
    bool attacked(cell_t coord, uint8 onwho) const;
    bool badCByte() const;
    bool badEByte() const;
    bool isLegal(std::string *errReason) const;
};

inline bool Position::operator!=(const Position &other) const
{
    return !(*this == other);
}

inline Piece Position::PieceAt(cell_t coord) const
{
    return coords[coord];
}

inline int Position::Ply() const
{
    return ply;
}

inline uint8 Position::Turn() const
{
    return turn;
}

inline int Position::NcpPlies() const
{
    return ncpPlies;
}

inline cell_t Position::EnPassantCoord() const
{
    return ebyte;
}

inline bool Position::CanCastleOO(uint8 turn) const
{
    return (cbyte >> turn) & CASTLEOO;
}

inline bool Position::CanCastleOOO(uint8 turn) const
{
    return (cbyte >> turn) & CASTLEOOO;
}

// (Returns true iff the side can castle at all)
inline bool Position::CanCastle(uint8 turn) const
{
    return (cbyte >> turn) & CASTLEBOTH;
}

inline bool Position::IsLegal(std::string &errStr) const
{
    return isLegal(&errStr);
}

inline bool Position::IsLegal() const
{
    return isLegal(nullptr);
}

inline void Position::SetPiece(cell_t coord, Piece piece)
{
    coords[coord] = piece;
}

inline void Position::SetEnPassantCoord(cell_t coord)
{
    ebyte = coord;
}

inline bool Position::SetNcpPlies(int newNcpPlies)
{
    if (newNcpPlies < 0)
        return false;
    ncpPlies = newNcpPlies;
    return true;
}

inline bool Position::SetPly(int newPly)
{
    if (newPly < 0)
        return false;
    ply = newPly;
    return true;
}

inline bool Position::SetTurn(uint8 newTurn)
{
    if (newTurn >= NUM_PLAYERS)
        return false;
    turn = newTurn;
    return true;
}

inline void Position::EnableCastlingOO(uint8 turn)
{
    cbyte |= CASTLEOO << turn;
}

inline void Position::EnableCastlingOOO(uint8 turn)
{
    cbyte |= CASTLEOOO << turn;
}

inline void Position::EnableCastling(uint8 turn)
{
    cbyte |= CASTLEBOTH << turn;
}

inline void Position::EnableCastling()
{
    for (int i = 0; i < NUM_PLAYERS; i++)
        EnableCastling(i);
}

inline void Position::ClearCastling()
{
    cbyte = 0;
}

#endif // POSITION_H
