//--------------------------------------------------------------------------
//                    Pv.h - preferred variation handling.
//                           -------------------
//  copyright            : (C) 2013 by Lucian Landry
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

// I'd like quiescing to be reported, but not used for move hinting?
// How can I do that?
// 1) Let DisplayPv include all moves in its pv.  Everything upto and including
//    'level' should be non-quiescing.
// 2) Make HintPv never allow quiescing moves in its movelist.

#ifndef PV_H
#define PV_H

#include "Eval.h"
#include "log.h"
#include "move.h"

class Board; // forward decl

// max PV moves we care to display.
// I want this to be at least 20 (because I have seen depth 18 in endgames).
static const int kMaxPvMoves = 20;
static const int kMaxPvStringLen = (kMaxPvMoves * MOVE_STRING_MAX);

// A "fast" but limited version of PV tracking.  Used by the thinker to track
//  its PV as it is searching.  Could be used for other variations than the
//  principal one.
class SearchPv
{
public:
    explicit SearchPv(int startDepth);
    SearchPv &operator=(const SearchPv &other);

    void Clear();

    // As a convenience, these return true iff this is the root node.
    bool Update(MoveT move);
    bool UpdateFromChildPv(MoveT move, const SearchPv &child);

    // Writes out a sequence of moves in the PV using style 'moveStyle'.
    // Returns the number of moves successfully converted.
    int BuildMoveString(char *dstStr, int dstLen,
                        const MoveStyleT &moveStyle, const Board &board) const;
    MoveT Moves(int idx) const;
    void Log(LogLevelT logLevel) const;
    void SetStartDepth(int depth);
    void Decrement(); // assumes we play the move at Moves(0) (if any)

private:
    int startDepth; // depth of the node this object was instantiated at (root
                    //  node == depth 0; increases with search depth).
    int numMoves; // including quiescing.  Not tied to level (could be lower if
                  //  using hashedmove or mate was found or ran into
                  //  kMaxPvDepth, could be higher as well with a lot of
                  //  q-moves.)
    MoveT moves[kMaxPvMoves];

    bool updateFromChildPv(MoveT move, const SearchPv *child);
};

// Pv class sent in Thinker -> UI notifications.
class DisplayPv
{
public:
    DisplayPv(); // ctor
    DisplayPv &operator=(const DisplayPv &other) = default;
    
    // (Forwarding function)
    // Writes out a sequence of moves in the PV using style 'moveStyle'.
    // Returns the number of moves successfully converted.
    int BuildMoveString(char *dstStr, int dstLen,
                        const MoveStyleT &moveStyle, const Board &board) const;
    int Level() const; // Getters.
    Eval Eval() const;
    MoveT Moves(int idx) const;
    
    // Setter.  Would do this in the constructor but with our current C code
    //  it's easier to do it this way.
    void Set(int level, class Eval eval, const SearchPv &pv);
    void Log(LogLevelT logLevel) const;
    void Decrement(); // assumes we play the first move (if any)
    
private:
    int level;    // nominal search depth. (not including quiescing)
    class Eval eval; // evaluation of the position.  Normally an exact value.
    SearchPv pv;  // actual movelist.
};

// Used by thinkers for move hints and guiding starting search depth.
// It is a bit different than the other PV classes because it:
// -- does not include quiescing moves
// -- moves are merely hints, and are more likely to be illegal, and
// -- all indices of 'moves' may be checked.
class HintPv
{
public:
    HintPv();
    HintPv &operator=(const HintPv &other) = default;

    void Clear(); // reset.

    // Calling this 'Update' instead of operator= since some hint moves may be
    //  preserved.
    void Update(const DisplayPv &dispPv);

    void Decrement(MoveT move); // Shrink PV by depth 1 after a move
    void FastForward(int numPlies); // Shrink it by 'numPlies' moves.
    void Rewind(int numPlies); // Annnnd move it back.

    inline MoveT Hint(int depth); // Assumes 'depth' >= 0.
    int SuggestSearchStartLevel();
    void ResetSearchStartLevel();
    
    // Call this when you are finished calling Update() for a given search
    //  level.  Allows SuggestSearchStartLevel() to be more aggressive.
    void CompletedSearch();
    void Log(LogLevelT logLevel) const;

private:
    int level; // nominal search depth. (not including quiescing)
    MoveT moves[kMaxPvMoves];

    // evaluation of the position.  Normally an exact value.  Used by
    //  SuggestSearchStartLevel().  Could be used for more aggressive search
    //  bounds.
    Eval eval;

    // Did we complete search for this level, or was it just best move found
    //  so far?
    bool completedSearch;
};


inline SearchPv::SearchPv(int startDepth) : startDepth(startDepth), numMoves(0)
{}

inline void SearchPv::Clear()
{
    numMoves = 0;
}

inline bool SearchPv::Update(MoveT move)
{
    return updateFromChildPv(move, nullptr);
}

inline bool SearchPv::UpdateFromChildPv(MoveT move, const SearchPv &child)
{
    return updateFromChildPv(move, &child);
}

inline MoveT SearchPv::Moves(int idx) const
{
    return idx < 0 || idx >= numMoves ? MoveNone : moves[idx];
}

inline void SearchPv::SetStartDepth(int depth)
{
    startDepth = depth;
}

inline DisplayPv::DisplayPv() : level(0), pv(0) {}

inline void DisplayPv::Set(int level, class Eval eval, const SearchPv &pv)
{
    this->level = level;
    this->eval = eval;
    this->pv = pv;
}

inline int DisplayPv::Level() const
{
    return level;
}

inline Eval DisplayPv::Eval() const
{
    return eval;
}

inline int DisplayPv::BuildMoveString(char *dstStr, int dstLen,
                                      const MoveStyleT &moveStyle,
                                      const Board &board) const
{
    return pv.BuildMoveString(dstStr, dstLen, moveStyle, board);
}

inline MoveT DisplayPv::Moves(int idx) const
{
    return pv.Moves(idx);
}

// Assumes 'depth' >= 0.
inline MoveT HintPv::Hint(int depth)
{
    return depth < kMaxPvMoves ? moves[depth] : MoveNone;
}

#endif // PV_H
