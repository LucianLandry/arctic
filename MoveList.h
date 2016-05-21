//--------------------------------------------------------------------------
//                 MoveList.h - MoveListT-oriented functions.
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
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

#ifndef MOVELIST_H
#define MOVELIST_H

#include <vector>

#include "aTypes.h"
#include "log.h"
#include "move.h"

#ifdef ENABLE_DEBUG_LOGGING
#define MOVELIST_LOGDEBUG(mvlist) (mvlist).Log(eLogDebug)
#else
#define MOVELIST_LOGDEBUG(mvlist)
#endif // ENABLE_DEBUG_LOGGING

class Board; // Forward-declare this

class MoveList
{
public:
    MoveList();
    ~MoveList();

    const MoveList &operator=(const MoveList &other);

    inline int NumMoves();

    // Must be fast, so as with normal arrays, 'idx' is not sanity-checked.
    // We implement this instead of operator[] since the latter would normally
    // return a reference.
    inline MoveT Moves(int idx);
    
    void SortByCapWorth(const Board &board);
    
    // Use 'move' as the first move (if it is currently in our movelist,
    //  otherwise no-op).
    void UseAsFirstMove(MoveT move);

    // If there is a move in the movelist that matches the same src and dst,
    //  return a pointer to it, otherwise NULL.  (I'd return an index, but
    //  indexing off -1 is worse than dereferencing NULL).
    MoveT *SearchSrcDst(MoveT move);
    // As above, but this version must also match for the 'promote' field.
    MoveT *SearchSrcDstPromote(MoveT move);
    // Search for an exact match for 'move'.
    MoveT *Search(MoveT move);
    
    // Add the move 'move'.  Useful mostly when one wants to search a restricted
    //  set of moves.  Note: *all* fields in 'move' must be valid (because check
    //  and discovered-check are not recalculated), and dups are not checked
    //  for.
    void AddMove(MoveT move, const Board &board);
    // A fast version of the above that does not take promotion, en passant,
    //  or castling into account.
    void AddMoveFast(MoveT move, const Board &board);
    
    // Delete the move at index 'idx'.
    // Must be fast, so 'idx' is not sanity-checked.
    void DeleteMove(int idx);

    // Delete every move in the movelist.
    inline void DeleteAllMoves();
    
    // Returns: is the move at index 'idx' a 'preferred' (ie capture/check/
    //  history window) move or not.
    // It is (currently) safe to go out of bounds on the upper end, here.
    inline bool IsPreferredMove(int idx);

    // Log an entire movelist.  Usually you should use MOVELIST_LOGDEBUG().
    void Log(LogLevelT level);

protected:
    int insrt; // index of spot to insert 'preferred' move.

    // Let the number of possible moves grow indefinitely (for compatibility
    //  with variants with large numbers of moves).  We reuse the vectors to
    //  cut down on the number of dynamic allocations.
    // We could use a separate vector for preferred moves, but I'm not sure how
    //  that would be a win.
    std::vector<MoveT> &moves;

private:
    // helper function for UseAsFirstMove().
    void useAsFirstMove(MoveT move);
};

inline bool MoveList::IsPreferredMove(int idx)
{
    return idx < insrt;
}

inline int MoveList::NumMoves()
{
    return moves.size();
}

inline MoveT MoveList::Moves(int idx)
{
    return moves[idx];
}

inline void MoveList::UseAsFirstMove(MoveT move)
{
    if (move != MoveNone)
        useAsFirstMove(move);
}

inline void MoveList::DeleteAllMoves()
{
    moves.resize(0);
    insrt = 0;
}

#endif // MOVELIST_H
