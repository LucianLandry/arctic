//--------------------------------------------------------------------------
//             HistoryWindow.h - History Heuristic functionality
//                            -------------------
//  copyright            : (C) 2016 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#ifndef HISTORYWINDOW_H
#define HISTORYWINDOW_H

#include <stdlib.h> // abs(3)

#include "move.h"
#include "ref.h"

// For a better description of the history heuristic, see (for example):
// https://chessprogramming.wikispaces.com/History+Heuristic

class HistoryWindow
{
public:
    HistoryWindow();
    void Clear(); // Reset the history table proper.
    inline void StoreMove(MoveT move, int turn, int ply);
    inline bool Hit(MoveT move, int turn, int ply) const;

    // Sets a max limit (non-inclusive) on how many moves we can check
    //  backwards or forwards, and still be a valid 'history' entry.
    // 1 == killer move heuristic.
    // 0 == history window disabled.
    void SetWindow(int numMoves);
    int Window() const;
        
private:
    short hist[NUM_PLAYERS] [NUM_SQUARES] [NUM_SQUARES];
    // Accessed via numMoves, but stored internally as numPlies (for speed):
    int window;
};

extern HistoryWindow gHistoryWindow;

inline void HistoryWindow::SetWindow(int numMoves)
{
    window = numMoves << 1; // convert moves to plies
}

inline int HistoryWindow::Window() const
{
    return window >> 1; // convert plies to moves
}

inline void HistoryWindow::StoreMove(MoveT move, int turn, int ply)
{
    hist[turn] [move.src] [move.dst] = ply;
}

inline bool HistoryWindow::Hit(MoveT move, int turn, int ply) const
{
    return abs(hist[turn] [move.src] [move.dst] - ply) < window;
}

#endif // HISTORYWINDOW_H
