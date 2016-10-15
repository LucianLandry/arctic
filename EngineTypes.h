//--------------------------------------------------------------------------
//         EngineTypes.h - types used to communicate with the Engine.
//                           -------------------
//  copyright            : (C) 2016 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#ifndef ENGINETYPES_H
#define ENGINETYPES_H

#include <string.h> // memset()
#include "Pv.h"

// NOTE: these are not exact counts, since we do not want the speed hit that
//  comes from updating these atomically.  We could have the child threads
//  maintain their own stats while they are searching, but this still does not
//  work for 'nodes' because the children need to quickly know when maxNodes
//  has been met.
struct EngineStatsT
{
    int nodes;        // node count (how many times was 'minimax' invoked)
    int nonQNodes;    // non-quiesce node count
    int moveGenNodes; // how many times was mListGenerate() called
    int hashHitGood;  // hashtable hits that returned immediately.
    int hashWroteNew; // how many times (in this ply) we wrote to a unique
                      //  hash entry.  Used for UCI hashfull stats.
    int hashFullPerMille; // how "full" is the hash (in parts per thousand).
    EngineStatsT();  // This struct can initialize itself.
    void Clear();
};

inline EngineStatsT::EngineStatsT()
{
    Clear();
}

inline void EngineStatsT::Clear()
{
    memset(this, 0, sizeof(EngineStatsT));
}

struct EnginePvArgsT
{
    EngineStatsT stats;
    DisplayPv pv;
};

struct EngineSearchDoneArgsT
{
    EngineSearchDoneArgsT() : pv(0) {}
    EngineSearchDoneArgsT(MoveT move, Eval eval, const SearchPv &pv) :
        move(move), eval(eval), pv(pv) {}
    MoveT move;
    Eval eval;
    SearchPv pv;
};

#endif // ENGINETYPES_H
