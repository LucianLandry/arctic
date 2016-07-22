//--------------------------------------------------------------------------
//            HistoryWindow.cpp - History Heuristic functionality
//                            -------------------
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

