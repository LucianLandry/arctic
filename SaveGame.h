//--------------------------------------------------------------------------
//                     SaveGame.h - saveable game class.
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#ifndef SAVEGAME_H
#define SAVEGAME_H

#include <vector>

#include "aTypes.h"
#include "Board.h"
#include "Clock.h"

// Contains minimal game save + restore + undo + redo information.
class SaveGame
{
public:
    SaveGame();
    SaveGame &operator=(const SaveGame &other) = default;

    void CommitMove(MoveT move, bigtime_t myTime);

    // These return 0 if successful, otherwise -1.
    int Save() const;
    int Restore(); // (Iff failure, object is not altered.)

    // Resets a position (w/out adjusting clocks)
    void SetStartPosition(const Board &board);

    void SetClocks(const Clock *clocks);    
    int GotoPly(int ply, Board *board, Clock *clocks);

    int CurrentPly() const;
    int FirstPly() const;
    int LastPly() const;
private:
    Clock clocks[NUM_PLAYERS]; // starting time.
    Position startPosition;
    int currentPly;  // Current ply we are at.
                     // currentPly - startPosition.Ply() == 'plies' index to
                     //  write the next move into.
    struct GamePlyT
    {
        MoveT move;
        // Time left on player's clock after 'move' (includes any increment)
        bigtime_t myTime;
    };
    std::vector<GamePlyT> plies;
};

inline int SaveGame::CurrentPly() const
{
    return currentPly;
}
inline int SaveGame::FirstPly() const
{
    return startPosition.Ply();
}
inline int SaveGame::LastPly() const
{
    return startPosition.Ply() + plies.size();
}

#endif // SAVEGAME_H
