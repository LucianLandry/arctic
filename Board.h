//--------------------------------------------------------------------------
//                   Board.h - Board-related functionality.
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

#ifndef BOARD_H
#define BOARD_H

#include <string.h>   // memcmp(3)
#include <vector>

#include "aTypes.h"
#include "list.h"
#include "move.h"
#include "Piece.h"
#include "Position.h"
#include "ref.h"

class MoveList; // forward declaration for GenerateLegalMoves()

// Using protected inheritance since we do not want to give the user the ability
//  to (easily) set a board to an illegal position.
class Board : protected Position
{
public:
    // Constructor.
    // Note: SetPosition() will also effectively re-initialize a Board.
    Board();
    Board(const Board &other);
    
    Board &operator=(const Board &other);

    // Returns: true if succeeded, false otherwise.  Does not clobber object
    //  if we failed.
    bool SetPosition(const Position &position);

    void MakeMove(MoveT move);
    void UnmakeMove();

    // These return 'true' iff the check passed.
    bool ConsistencyCheck(const char *failString) const;

    // Make move generation use an arbitrary piece ordering
    //  (while still preferring various kinds of moves).
    void Randomize();

    int CalcCapWorth(MoveT move) const;

    bool IsNormalStartingPosition() const;

    bool IsDrawInsufficientMaterial() const; // This is an automatic draw.
    inline bool IsDrawFiftyMove() const;     // This draw must be claimed.
    bool IsDrawThreefoldRepetition() const;  // This draw must be claimed.
    // Faster but possibly inaccurate version of the above (relies on
    //  zobrist hash only):
    bool IsDrawThreefoldRepetitionFast() const;

    inline const Position &Position() const;

    inline Piece PieceAt(cell_t coord) const;
    inline int Ply() const;        // Returns current ply.
    inline uint8 Turn() const;     // Returns current turn.
    inline int NcpPlies() const;   // Returns count of plies since last capture
                                   //  or pawn move (aka FEN half-move clock).
    inline cell_t EnPassantCoord() const;
    
    // These functions only (obviously) return whether it *may be* possible to
    //  castle now or in the future.
    inline bool CanCastleOO(uint8 turn) const;
    inline bool CanCastleOOO(uint8 turn) const;
    inline bool CanCastle(uint8 turn) const;

    inline uint64 Zobrist() const; // Returns current zobrist hash.
    
    // Return a vector of all the coords inhabited by 'piece'.
    inline const std::vector<cell_t> &PieceCoords(Piece piece) const;

    // Returns: whether a piece of this sort is on the board.
    inline bool PieceExists(Piece piece) const;

    // Returns: whether the current side to move is in check or not.
    inline bool IsInCheck() const;
    // As above, but returns the cell giving check (or FLAG if none, or
    //  DOUBLE_CHECK if multiple cells are giving check).
    inline cell_t CheckingCoord() const;
    
    // Material strength of a given side.
    inline int MaterialStrength(uint8 player) const;
    // Syntactic sugar; relative strength of player vs all other player(s).
    inline int RelativeMaterialStrength() const;

    // Ply that we can UnmakeMove() to.
    inline int BasePly() const;
    // Returns the last ply that this board has in common with 'other' (or
    //  -1 if no such ply).  This is (relatively) slow.
    int LastCommonPly(const Board &other) const;
    
    // Ply of first repeated position that might contribute to a draw.  If any,
    //  then this is the ply of the 1st repeat, not the original position.
    // If there is no repeated position, this is -1.
    inline int RepeatPly() const;

    // Generate all legal moves, and store them in 'mvlist'.  (Iff
    //  'generateCapturesOnly' == true *and* we are not in check, then
    //  generates capture moves only.)
    void GenerateLegalMoves(MoveList &mvlist, bool generateCapturesOnly) const;

    bool IsLegalMove(MoveT move) const;
    
    void Log(LogLevelT level) const;

    // Returns move made at ply 'ply'.  Currently asserts if that move has not
    //  been recorded.
    MoveT MoveAt(int ply) const;

protected:
    // Says if side to move is currently in check.
    // Follows FLAG:coord:DOUBLE_CHECK convention.
    // Read: no check, check, doublecheck.
    cell_t ncheck;

    uint64 zobrist;  // zobrist hash.  Incrementally updated w/each move.

    // This is a way to quickly look up the number and location of any
    // type of piece on the board.
    std::vector<cell_t> pieceCoords[kMaxPieces];

    cell_t *pPiece[NUM_SQUARES]; // Given a coordinate, this points back to the
                                 //  exact spot in the pieceList that refers to
                                 //  this coord.  Basically a reverse lookup for
                                 //  'pieceCoords'.

    int totalStrength; // material strength of all pieces combined.  Used when
                       // checking for draws.

    // Material (not positional) strength of each side.
    int materialStrength[NUM_PLAYERS];

    // Ply of first repeated position (if any, then the occurence of the 1st
    // repeat, not the original), otherwise -1).
    int repeatPly;

    // This MUST be a power of 2 (to make our hashing work), and MUST be at
    //  least 128 to account for the 50-move rule (100 plies == 50 moves)
    static const int kNumSavedPositions = 128;
    
    // Saved positions.  Used to detect 3-fold repetition.  The fifty-move
    //  rule limits the number I need to 100, and 128 is the next power-of-2
    //  which makes calculating the appropriate position for a given ply easy.
    // (Technically, two non-computer opponents could ignore the fifty-move
    //  rule and then repeat a position 3 times outside this window, but the
    //  game would still be drawn by the fifty-move rule.  However for exotic
    //  variants like Crazyhouse, it would be theoretically possible to repeat
    //  the position thrice w/out invoking the fifty-move rule.  FIXME?
    PositionInfoElementT positions[kNumSavedPositions];

    // This acts as a hash table to store positions that potentially repeat
    // each other.  There are only 128 elements ('positions', above) that are
    // spread among each entry, so hopefully each list here is about 1
    // element in length.
    ListT posList[kNumSavedPositions];

    // This is is filled in by MakeMove() and used by UnmakeMove().
    struct UnMakeT
    {
        MoveT  move;      // saved move
        Piece  capPiece;  // any captured piece.. does not include en passant.
        // Saved-off values from the Board.
        uint8  cbyte;
        cell_t ebyte;
        cell_t ncheck;
        int    ncpPlies;
        int    repeatPly;
        uint64 zobrist;

        bool   mightDraw;
    };
    
    std::vector<UnMakeT> unmakes; 

private:
    cell_t calcNCheck(const char *context) const;
};

inline const Position &Board::Position() const
{
    return *this;
}

inline Piece Board::PieceAt(cell_t coord) const
{
    return Position::PieceAt(coord);
}

inline int Board::Ply() const
{
    return Position::Ply();
}

inline uint8 Board::Turn() const
{
    return Position::Turn();
}

inline int Board::NcpPlies() const
{
    return Position::NcpPlies();
}

inline cell_t Board::EnPassantCoord() const
{
    return Position::EnPassantCoord();
}

inline bool Board::CanCastleOO(uint8 turn) const
{
    return Position::CanCastleOO(turn);
}
inline bool Board::CanCastleOOO(uint8 turn) const
{
    return Position::CanCastleOOO(turn);
}
// (Returns true iff the side can castle at all)
inline bool Board::CanCastle(uint8 turn) const
{
    return Position::CanCastle(turn);
}

inline uint64 Board::Zobrist() const
{
    return zobrist;
}

inline const std::vector<cell_t> &Board::PieceCoords(Piece piece) const
{
    return pieceCoords[piece.ToIndex()];
}

// Returns: whether a piece of this sort is on the board.
inline bool Board::PieceExists(Piece piece) const
{
    return !PieceCoords(piece).empty();
}

inline bool Board::IsInCheck() const
{
    return ncheck != FLAG;
}

inline cell_t Board::CheckingCoord() const
{
    return ncheck;
};

inline bool Board::IsDrawFiftyMove() const
{
    return ncpPlies >= 100;
}

inline int Board::MaterialStrength(uint8 player) const
{
    return materialStrength[player];
}

inline int Board::RelativeMaterialStrength() const
{
    return materialStrength[turn] - materialStrength[turn ^ 1];
}

inline int Board::BasePly() const
{
    return Ply() - unmakes.size();
}

inline int Board::RepeatPly() const
{
    return repeatPly;
}

#endif // BOARD_H
