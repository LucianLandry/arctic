//--------------------------------------------------------------------------
//                 saveGame.cpp - saveable game structures.
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

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>    // malloc(3)
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h> // stat(2)
#include <unistd.h>

#include "Board.h"
#include "log.h"
#include "MoveList.h"
#include "saveGame.h"

#define SAVEFILE "arctic.sav"

void SaveGameMoveCommit(SaveGameT *sgame, MoveT *move, bigtime_t myTime)
{
    GamePlyT ply;
    int plyOffset = sgame->currentPly++ - sgame->startPosition.Ply();

    ply.move = *move; // struct assign
    ply.myTime = myTime;

    // Dump any redo information.
    sgame->plies.resize(plyOffset);

    sgame->plies.push_back(ply);
}

// returns: 0, if save successful, otherwise -1.
int SaveGameSave(SaveGameT *sgame)
{
    FILE *myFile;
    int retVal = 0;
    int elementsLeft = sgame->plies.size();
    int elementsWritten = 0;
    int saveGameElemSize = offsetof(SaveGameT, plies);
    
    if ((myFile = fopen(SAVEFILE, "w")) == NULL)
    {
        LOG_NORMAL("SaveGameSave(): Could not open file.\n");
        return -1;
    }

    do
    {
        if (fwrite(sgame, saveGameElemSize, 1, myFile) < 1)
        {
            LOG_NORMAL("SaveGameSave(): could not write SaveGameT.\n");
            retVal = -1;
            break;
        }

        while (elementsLeft > 0)
        {
            if ((elementsWritten =
                 fwrite(&sgame->plies[sgame->plies.size() - elementsLeft],
                        sizeof(GamePlyT), elementsLeft, myFile)) == 0)
            {
                LOG_NORMAL("SaveGameSave(): could not write GamePlyT.\n");
                retVal = -1;
                break;
            }
            elementsLeft -= elementsWritten;
        }
    } while (0);

    if (fclose(myFile) == EOF)
    {
        LOG_NORMAL("SaveGameSave(): could not close file.\n");
        retVal = -1;
    }
    return retVal;
}

void SaveGameInit(SaveGameT *sgame)
{
    // Everything besides this should be automatically constructed.
    sgame->currentPly = 0;
}

// Reset a position (w/out adjusting clocks)
void SaveGamePositionSet(SaveGameT *sgame, Board *board)
{
    sgame->startPosition = board->Position();
    sgame->currentPly = sgame->startPosition.Ply();
    sgame->plies.resize(0);
}

void SaveGameClocksSet(SaveGameT *sgame, Clock *clocks)
{
    int i;

    for (i = 0; i < NUM_PLAYERS; i++)
    {
        // Trying to successfully xfer a running clock seems difficult, and
        // we do not have to support it, so ...
        assert(!clocks[i].IsRunning());
        sgame->clocks[i] = clocks[i];
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
int SaveGameGotoPly(SaveGameT *sgame, int ply, Board *board, Clock *clocks)
{
    int i, plyOffset;
    MoveList moveList;
    MoveT move;
    
    Board myBoard;     // temp variables.
    Clock myClocks[2];
    
    if (ply < SaveGameFirstPly(sgame) || ply > SaveGameLastPly(sgame))
    {
        LOG_DEBUG("SaveGameGotoPly(): ply %d out of range (%d, %d)\n",
                  ply, SaveGameFirstPly(sgame), SaveGameLastPly(sgame));
        return -1;
    }

    // struct copies.
    myClocks[0] = sgame->clocks[0];
    myClocks[1] = sgame->clocks[1];

    // Sanity check: SaveGameT structure.
    if (!myBoard.SetPosition(sgame->startPosition))
    {
        LOG_DEBUG("SaveGameGotoPly(): bad board.\n");
        return -1;
    }

    // Sanity check: each move.
    // (We do not sanity check clock time because:
    // -- it would be difficult
    // -- it is possible somebody gave us more time in the middle of the
    // savegame.
    plyOffset = ply - sgame->startPosition.Ply();
    for (i = 0; i < plyOffset; i++)
    {
        move = sgame->plies[i].move;
        myBoard.GenerateLegalMoves(moveList, false);
        if (moveList.Search(move) == NULL)
        {
            LOG_DEBUG("SaveGameGotoPly(): bad move %d\n", i);
            return -1;
        }
        
        myClocks[i & 1].SetTime(sgame->plies[i].myTime);
        myBoard.MakeMove(move);
    }

    sgame->currentPly = ply;

    // Success.  Update external variables if they exist.
    if (clocks != NULL)
    {
        clocks[0] = myClocks[0];
        clocks[1] = myClocks[1];
    }
    if (board != NULL)
        *board = myBoard;

    return 0;
}

// Assumes 'sgame' has been initialized (w/SaveGameInit())
// Returns: 0, if restore successful, otherwise -1.
// 'sgame' is guaranteed to be 'sane' after return, regardless of result.
int SaveGameRestore(SaveGameT *sgame)
{
    FILE *myFile;
    int retVal;
    int elementsLeft;
    int elementsRead;
    int savedCurrentPly;
    struct stat buf;
    SaveGameT mySGame;

    // We read in the entire savegame -- except the 'plies' vector -- in one
    //  chunk.  Rather hacky.
    int saveGameElemSize = offsetof(SaveGameT, plies);
    
    SaveGameInit(&mySGame);

    while ((retVal = stat(SAVEFILE, &buf)) < 0 && errno == EINTR)
        ;
    if (retVal < 0)
    {
        LOG_NORMAL("SaveGameRestore(): stat() failed\n");
        return -1;
    }
    elementsLeft = (buf.st_size - saveGameElemSize) / sizeof(GamePlyT);

    if ((myFile = fopen(SAVEFILE, "r")) == NULL)
    {
        LOG_NORMAL("SaveGameRestore(): Could not open file.\n");
        return -1;
    }

    do
    {
        // Read in SaveGameT.  (except for the 'plies' vector)
        if (fread(&mySGame, saveGameElemSize, 1, myFile) < 1)
        {
            LOG_NORMAL("SaveGameRestore(): could not read SaveGameT.\n");
            retVal = -1;
            break;
        }

        // Sanity check: we should have read a legal position.
        //  (SaveGameGotoPly would also catch this below; we just want to be
        //   obvious about it.)
        std::string errString;
        if (!mySGame.startPosition.IsLegal(errString))
        {
            LOG_NORMAL("SaveGameRestore(): illegal position read: %s\n",
                      errString.c_str());
            retVal = -1;
            break;
        }
        
        // Sanity check: firstPly.  Upper limit is arbitrary, we just want
        // to prevent wraparound.
        if (mySGame.startPosition.Ply() > 1000000)
        {
            LOG_NORMAL("SaveGameRestore(): bad firstPly (%d)\n",
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
                LOG_NORMAL("SaveGameRestore(): could not read GamePlyT.\n");
                retVal = -1;
                break;
            }
            elementsLeft -= elementsRead;
        }       

        savedCurrentPly = mySGame.currentPly;

        // Sanity check: SaveGameT and GamePlyT structures.
        if (SaveGameGotoPly(&mySGame, SaveGameLastPly(&mySGame), NULL,
                            NULL) < 0 ||
            // (going back to, and validating 'currentPly' at the same time)
            SaveGameGotoPly(&mySGame, savedCurrentPly, NULL, NULL) < 0)
        {
            LOG_NORMAL("SaveGameRestore(): bad GameT or GamePlyT.\n");
            retVal = -1;
            break;
        }
    } while (0);

    if (fclose(myFile) == EOF)
    {
        LOG_NORMAL("SaveGameRestore(): could not close file.\n");
        retVal = -1;
    }

    if (retVal != -1)
    {
        // Everything checks out.  Overwrite 'sgame'.
        SaveGameCopy(sgame, &mySGame);
    }

    return retVal;
}

// Assumes 'src' and 'dst' are both non-NULL, and have both been initialized
//  w/SaveGameInit().  Clobbers 'dst', but in a safe fashion.
void SaveGameCopy(SaveGameT *dst, SaveGameT *src)
{
    // Basically we just do a memberwise copy here.
    for (int i = 0; i < NUM_PLAYERS; i++)
        dst->clocks[i] = src->clocks[i];

    dst->startPosition = src->startPosition;
    dst->currentPly = src->currentPly;
    dst->plies = src->plies;
}
