//--------------------------------------------------------------------------
//         ThinkerTypes.h - types used to communicate with the Thinker.
//                           -------------------
//  copyright            : (C) 2016 by Lucian Landry
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

#ifndef THINKERTYPES_H
#define THINKERTYPES_H

#include <string.h> // memset()

// NOTE: these are not exact counts, since we do not want the speed hit that
//  comes from updating these atomically.  We could have the child threads
//  maintain their own stats while they are searching, but this still does not
//  work for 'nodes' because the children need to quickly know when maxNodes
//  has been met.
struct ThinkerStatsT
{
    int nodes;        // node count (how many times was 'minimax' invoked)
    int nonQNodes;    // non-quiesce node count
    int moveGenNodes; // how many times was mListGenerate() called
    int hashHitGood;  // hashtable hits that returned immediately.
    int hashWroteNew; // how many times (in this ply) we wrote to a unique
                      //  hash entry.  Used for UCI hashfull stats.
    int hashFullPerMille; // how "full" is the hash (in parts per thousand).
    ThinkerStatsT();  // This struct can initialize itself.
    void Clear();
};

inline ThinkerStatsT::ThinkerStatsT()
{
    Clear();
}

inline void ThinkerStatsT::Clear()
{
    memset(this, 0, sizeof(ThinkerStatsT));
}

#endif // THINKERTYPES_H
