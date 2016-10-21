//--------------------------------------------------------------------------
//                    SaveGame.cpp - saveable game class.
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h> // stat(2)

#include "Board.h"
#include "log.h"
#include "MoveList.h"
#include "SaveGame.h"

#define SAVEFILE "arctic.sav"

SaveGame::SaveGame() : currentPly(0) {}

void SaveGame::CommitMove(MoveT move, bigtime_t myTime)
{
    int plyOffset = currentPly++ - startPosition.Ply();
    GamePlyT ply;

    ply.move = move;
    ply.myTime = myTime;

    plies.resize(plyOffset); // Dump any redo information.
    plies.push_back(ply);
}

// returns: 0, if save successful, otherwise -1.
int SaveGame::Save() const
{
    FILE *myFile;
    int retVal = 0;
    int elementsLeft = plies.size();
    int elementsWritten = 0;
    int saveGameElemSize = offsetof(SaveGame, plies);
    
    if ((myFile = fopen(SAVEFILE, "w")) == NULL)
    {
        LOG_NORMAL("%s: Could not open file.\n", __func__);
        return -1;
    }

    do
    {
        if (fwrite(this, saveGameElemSize, 1, myFile) < 1)
        {
            LOG_NORMAL("%s: could not write SaveGameT.\n", __func__);
            retVal = -1;
            break;
        }

        while (elementsLeft > 0)
        {
            if ((elementsWritten =
                 fwrite(&plies[plies.size() - elementsLeft],
                        sizeof(GamePlyT), elementsLeft, myFile)) == 0)
            {
                LOG_NORMAL("%s: could not write GamePlyT.\n", __func__);
                retVal = -1;
                break;
            }
            elementsLeft -= elementsWritten;
        }
    } while (0);

    if (fclose(myFile) == EOF)
    {
        LOG_NORMAL("%s: could not close file.\n", __func__);
        retVal = -1;
    }
    return retVal;
}

void SaveGame::SetStartPosition(const Board &board)
{
    startPosition = board.Position();
    currentPly = startPosition.Ply();
    plies.resize(0);
}

void SaveGame::SetClocks(const Clock *clocks)
{
    for (int i = 0; i < NUM_PLAYERS; i++)
    {
        // Trying to successfully xfer a running clock seems difficult, and
        // we do not have to support it, so ...
        assert(!clocks[i].IsRunning());
        this->clocks[i] = clocks[i];
    }
}

// Goes to a particular ply in the savegame and makes that the head ply.
// Recording any additional moves clobbers any redo information.
//
// Returns '0' if successful, '-1' otherwise.
//
// Iff the function is successful, 'board' or 'clocks' are updated if they
// are not NULL.  This is the intended main way for the save-game module
// to communicate with everybody else.
//
// Notice 'clocks' is an array of clocks!  This is for better coordination
// w/GameT.
int SaveGame::GotoPly(int ply, Board *board, Clock *clocks)
{
    Board myBoard;     // temp variables.
    Clock myClocks[2];
    
    if (ply < FirstPly() || ply > LastPly())
    {
        LOG_DEBUG("%s: ply %d out of range (%d, %d)\n", __func__,
                  ply, FirstPly(), LastPly());
        return -1;
    }

    // struct copies.
    for (int i = 0; i < NUM_PLAYERS; i++)
        myClocks[i] = this->clocks[i];

    // Sanity check: SaveGameT structure.
    if (!myBoard.SetPosition(startPosition))
    {
        LOG_DEBUG("%s: bad board.\n", __func__);
        return -1;
    }

    // Sanity check: each move.
    // (We do not sanity check clock time because:
    //  -- it would be difficult
    //  -- it is possible somebody gave us more time in the middle of the
    //     savegame.)
    int plyOffset = ply - startPosition.Ply();
    for (int i = 0; i < plyOffset; i++)
    {
        MoveList moveList;
        MoveT move = plies[i].move;
        myBoard.GenerateLegalMoves(moveList, false);
        if (moveList.Search(move) == nullptr)
        {
            LOG_DEBUG("%s: bad move %d\n", __func__, i);
            return -1;
        }
        
        myClocks[i & 1].SetTime(plies[i].myTime);
        myBoard.MakeMove(move);
    }

    currentPly = ply;

    // Success.  Update external variables if they exist.
    if (clocks != nullptr)
    {
        for (int i = 0; i < NUM_PLAYERS; i++)
            clocks[i] = myClocks[i];
    }
    if (board != nullptr)
        *board = myBoard;

    return 0;
}

// Assumes 'sgame' has been initialized (w/SaveGameInit())
// Returns: 0, if restore successful, otherwise -1.
// 'sgame' is guaranteed to be 'sane' after return, regardless of result.
int SaveGame::Restore()
{
    FILE *myFile;
    int retVal;
    int elementsLeft;
    int elementsRead;
    int savedCurrentPly;
    struct stat buf;
    SaveGame mySGame;

    // We read in the entire savegame -- except the 'plies' vector -- in one
    //  chunk.  Rather hacky.
    int saveGameElemSize = offsetof(SaveGame, plies);
    
    while ((retVal = stat(SAVEFILE, &buf)) < 0 && errno == EINTR)
        ;
    if (retVal < 0)
    {
        LOG_NORMAL("%s: stat() failed\n", __func__);
        return -1;
    }
    elementsLeft = (buf.st_size - saveGameElemSize) / sizeof(GamePlyT);

    if ((myFile = fopen(SAVEFILE, "r")) == NULL)
    {
        LOG_NORMAL("%s: Could not open file.\n", __func__);
        return -1;
    }

    do
    {
        // Read in the SaveGame.  (except for the 'plies' vector)
        if (fread(&mySGame, saveGameElemSize, 1, myFile) < 1)
        {
            LOG_NORMAL("%s: could not read SaveGameT.\n", __func__);
            retVal = -1;
            break;
        }

        // Sanity check: we should have read a legal position.
        //  (GotoPly() would also catch this below; we just want to be
        //   obvious about it.)
        std::string errString;
        if (!mySGame.startPosition.IsLegal(errString))
        {
            LOG_NORMAL("%s: illegal position read: %s\n", __func__,
                      errString.c_str());
            retVal = -1;
            break;
        }
        
        // Sanity check: firstPly.  Upper limit is arbitrary, we just want
        // to prevent wraparound.
        if (mySGame.startPosition.Ply() > 1000000)
        {
            LOG_NORMAL("%s: bad firstPly (%d)\n", __func__,
                      mySGame.startPosition.Ply());
            retVal = -1;
            break;
        }

        mySGame.plies.resize(elementsLeft);
        
        // Read in GamePlyTs.
        while (elementsLeft > 0)
        {
            if ((elementsRead =
                 fread(&mySGame.plies[mySGame.plies.size() - elementsLeft],
                       sizeof(GamePlyT), elementsLeft, myFile)) == 0)
            {
                LOG_NORMAL("%s: could not read GamePlyT.\n", __func__);
                retVal = -1;
                break;
            }
            elementsLeft -= elementsRead;
        }       

        savedCurrentPly = mySGame.currentPly;

        // Sanity check: SaveGameT and GamePlyT structures.
        if (mySGame.GotoPly(mySGame.LastPly(), nullptr, nullptr) < 0 ||
            // (going back to, and validating 'currentPly' at the same time)
            mySGame.GotoPly(savedCurrentPly, nullptr, nullptr) < 0)
        {
            LOG_NORMAL("%s: bad GameT or GamePlyT.\n", __func__);
            retVal = -1;
            break;
        }
    } while (0);

    if (fclose(myFile) == EOF)
    {
        LOG_NORMAL("%s: could not close file.\n", __func__);
        retVal = -1;
    }

    if (retVal != -1)
        *this = mySGame; // Everything checks out, so update ourselves.

    return retVal;
}
