//--------------------------------------------------------------------------
//            HistoryWindow.cpp - History Heuristic functionality
//                            -------------------
//  copyright            : (C) 2016 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#include "HistoryWindow.h"

HistoryWindow gHistoryWindow;

HistoryWindow::HistoryWindow()
{
    SetWindow(1); // Set for killer move heuristic
    Clear();
}

void HistoryWindow::Clear()
{
    // reset history table.
    for (int i = 0; i < NUM_PLAYERS; i++)
    {
        for (int j = 0; j < NUM_SQUARES; j++)
        {
            for (int k = 0; k < NUM_SQUARES; k++)
            {
                // -50, not -1, because -1 might trigger accidentally if
                //  we expand the history window beyond killer moves.
                hist[i] [j] [k] = -50;
            }
        }
    }
}

