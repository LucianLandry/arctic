//--------------------------------------------------------------------------
//                  saveGame.c - saveable game structures.
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

#include <assert.h>
#include <errno.h>
#include <sys/types.h> // stat(2)
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>    // malloc(3)
#include <string.h>
#include <unistd.h>

#include "board.h"
#include "log.h"
#include "moveList.h"
#include "saveGame.h"

#define SAVEFILE "arctic.sav"

void SaveGameMoveCommit(SaveGameT *sgame, MoveT *move, bigtime_t myTime)
{
    GamePlyT *newSpace;
    int newPlies;
    GamePlyT *ply;
    int plyOffset = sgame->currentPly - sgame->firstPly;

    if (plyOffset == sgame->numAllocatedPlies)
    {
        // Get more space to store moves.
        if ((newSpace = (GamePlyT *)
             realloc(sgame->plies,
                     (newPlies = sgame->numAllocatedPlies + 100) *
                     sizeof(GamePlyT))) == NULL &&
            (newSpace = (GamePlyT *)
             realloc(sgame->plies,
                     (newPlies = sgame->numAllocatedPlies + 1) *
                     sizeof(GamePlyT))) == NULL)
        {
            LOG_EMERG("could not allocate space for more moves.\n");
            assert(0);
        }

        sgame->plies = newSpace;
        sgame->numAllocatedPlies = newPlies;
    }

    ply = &sgame->plies[plyOffset];
    ply->move = *move; // struct assign
    ply->myTime = myTime;

    /* Lose all redo information. */
    sgame->numPlies = ++sgame->currentPly;
}


/* returns: 0, if save successful, otherwise -1. */
int SaveGameSave(SaveGameT *sgame)
{
    FILE *myFile;
    int retVal = 0;
    int elementsLeft = sgame->numPlies;
    int elementsWritten = 0;

    if ((myFile = fopen(SAVEFILE, "w")) == NULL)
    {
        LOG_DEBUG("SaveGameSave(): Could not open file.\n");
        return -1;
    }

    do
    {
        if (fwrite(sgame, sizeof(SaveGameT), 1, myFile) < 1)
        {
            LOG_DEBUG("SaveGameSave(): could not write SaveGameT.\n");
            retVal = -1;
            break;
        }

        while (elementsLeft > 0)
        {
            if ((elementsWritten =
                 fwrite(&sgame->plies[sgame->numPlies - elementsLeft],
                        sizeof(GamePlyT), elementsLeft, myFile)) == 0)
            {
                LOG_DEBUG("SaveGameSave(): could not write GamePlyT.\n");
                retVal = -1;
                break;
            }
            elementsLeft -= elementsWritten;
        }
    } while (0);

    if (fclose(myFile) == EOF)
    {
        LOG_DEBUG("SaveGameSave(): could not close file.\n");
        retVal = -1;
    }
    return retVal;
}


void SaveGameInit(SaveGameT *sgame)
{
    int i;

    memset(sgame, 0, sizeof(SaveGameT));
    for (i = 0; i < NUM_PLAYERS; i++)
    {
        ClockInit(&sgame->clocks[i]);
    }
}


// Reset a position (w/out adjusting clocks)
void SaveGamePositionSet(SaveGameT *sgame, BoardT *board)
{
    memcpy(sgame->coord, board->coord, NUM_SQUARES);
    sgame->cbyte = board->cbyte;
    sgame->ebyte = board->ebyte;
    sgame->turn = board->turn;

    sgame->numPlies = board->ncpPlies;
    sgame->firstPly = board->ply;
    sgame->currentPly = sgame->firstPly;
}


void SaveGameClocksSet(SaveGameT *sgame, ClockT *clocks[])
{
    int i;

    for (i = 0; i < NUM_PLAYERS; i++)
    {
        // Trying to successfully xfer a running clock seems difficult, and
        // we do not have to support it, so ...
        assert(!ClockIsRunning(clocks[i]));

        sgame->clocks[i] = *clocks[i];  // struct copy
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
// Notice 'clocks' is an array of ptrs!  This is for better coordination
// w/GameT.
int SaveGameGotoPly(SaveGameT *sgame, int ply, BoardT *board, ClockT *clocks[])
{
    int i, plyOffset;
    MoveListT moveList;
    MoveT *move;

    BoardT myBoard;     // temp variables.
    ClockT myClocks[2];

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
    BoardSet(&myBoard, sgame->coord, sgame->cbyte, sgame->ebyte, sgame->turn,
             sgame->firstPly, sgame->ncpPlies);
    if (BoardSanityCheck(&myBoard, 1) == -1)
    {
        LOG_DEBUG("SaveGameGotoPly(): bad board.\n");
        return -1;
    }

    // Sanity check: each move.
    // (We do not sanity check clock time because:
    // -- it would be difficult
    // -- it is possible somebody gave us more time in the middle of the
    // savegame.
    plyOffset = ply - sgame->firstPly;
    for (i = 0; i < plyOffset; i++)
    {
        move = &sgame->plies[i].move;
        mlistGenerate(&moveList, &myBoard, 0);
        if (mlistSearch(&moveList, move, sizeof(MoveT)) == NULL)
        {
            LOG_DEBUG("SaveGameGotoPly(): bad move %d\n", i);
            return -1;
        }
        
        ClockSetTime(&myClocks[i & 1], sgame->plies[i].myTime);
        BoardPositionSave(&myBoard);
        BoardMoveMake(&myBoard, move, NULL);
    }

    sgame->currentPly = ply;

    // Success.  Update external variables if they exist.
    if (clocks != NULL)
    {
        // struct copies.
        *(clocks[0]) = myClocks[0];
        *(clocks[1]) = myClocks[1];
    }

    if (board != NULL)
    {
        BoardCopy(board, &myBoard);
    }

    return 0;
}


// Assumes 'sgame' has been initialized (w/SaveGameInit())
// Returns: 0, if save successful, otherwise -1.
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

    memset(&mySGame, 0, sizeof(SaveGameT));

    while ((retVal = stat(SAVEFILE, &buf)) < 0 && errno == EINTR)
        ;
    if (retVal < 0)
    {
        LOG_DEBUG("SaveGameRestore(): stat() failed\n");
        return -1;
    }
    elementsLeft = (buf.st_size - sizeof(SaveGameT)) / sizeof(GamePlyT);

    if ((myFile = fopen(SAVEFILE, "r")) == NULL)
    {
        LOG_DEBUG("SaveGameRestore(): Could not open file.\n");
        return -1;
    }

    do {
        /* Read in SaveGameT. */
        if (fread(&mySGame, sizeof(SaveGameT), 1, myFile) < 1)
        {
            LOG_DEBUG("SaveGameRestore(): could not read SaveGameT.\n");
            mySGame.plies = NULL;
            retVal = -1;
            break;
        }
        mySGame.plies = NULL;

        /* Sanity check: number of GamePlyTs. */
        if (mySGame.numPlies != elementsLeft)
        {
            LOG_DEBUG("SaveGameRestore(): GamePlyT count mismatch.\n");
            retVal = -1;
            break;
        }

        // Sanity check: firstPly.  Upper limit is arbitrary, we just want
        // to prevent wraparound.
        if (mySGame.firstPly < 0 || mySGame.firstPly > 1000000)
        {
            LOG_DEBUG("SaveGameRestore(): bad firstPly (%d)\n",
                      mySGame.firstPly);
            retVal = -1;
            break;
        }

        // Allocate space for GamePlyTs.
        mySGame.numAllocatedPlies =
            MIN(mySGame.numAllocatedPlies, mySGame.numPlies + 100);

        if ((mySGame.plies = (GamePlyT *)
             malloc(mySGame.numAllocatedPlies * sizeof(GamePlyT))) == NULL &&
            (mySGame.plies = (GamePlyT *)
             malloc((mySGame.numAllocatedPlies = mySGame.numPlies)
                    * sizeof(GamePlyT))) == NULL)
        {
            LOG_DEBUG("SaveGameRestore(): Cannot allocate GamePlyT space.\n");
            retVal = -1;
            break;
        }

        /* Read in GamePlyTs. */
        while (elementsLeft > 0)
        {
            if ((elementsRead =
                 fread(&mySGame.plies[mySGame.numPlies - elementsLeft],
                       sizeof(GamePlyT), elementsLeft, myFile)) == 0)
            {
                LOG_DEBUG("SaveGameRestore(): could not read GamePlyT.\n");
                retVal = -1;
                break;
            }
            elementsLeft -= elementsRead;
        }       

        savedCurrentPly = mySGame.currentPly;

        /* Sanity check: SaveGameT and GamePlyT structures. */
        if (SaveGameGotoPly(&mySGame, SaveGameLastPly(&mySGame), NULL,
                            NULL) < 0 ||
            // (going back to, and validating 'currentPly' at the same time)
            SaveGameGotoPly(&mySGame, savedCurrentPly, NULL, NULL) < 0)
        {
            LOG_DEBUG("SaveGameRestore(): bad GameT or GamePlyT.\n");
            retVal = -1;
            break;
        }
    } while (0);

    if (fclose(myFile) == EOF)
    {
        LOG_DEBUG("SaveGameRestore(): could not close file.\n");
        retVal = -1;
    }
    if (retVal == -1 && mySGame.plies != NULL)
    {
        free(mySGame.plies);
    }

    if (retVal != -1)
    {
        /* Everything checks out.  Overwrite 'sgame'. */
        if (sgame->plies != NULL)
            free(sgame->plies); // assumes 'sgame' is initialized.
        memcpy(sgame, &mySGame, sizeof(SaveGameT));
    }

    return retVal;
}

// Assumes 'dst' is non-NULL and has been initialized w/SaveGameInit().
// Clobbers 'dst', but in a safe fashion.
void SaveGameCopy(SaveGameT *dst, SaveGameT *src)
{
    int savedNumAllocatedPlies = dst->numAllocatedPlies;
    GamePlyT *savedPlies = dst->plies;

    if (src->numPlies > dst->numAllocatedPlies)
    {
        if ((savedPlies = (GamePlyT *)
             realloc(dst->plies, src->numPlies * sizeof(GamePlyT))) == NULL)
        {
            LOG_EMERG("SaveGameCopy: could not allocate space for moves.\n");
            assert(0);
        }
        savedNumAllocatedPlies = src->numPlies;
    }
    *dst = *src; // struct copy
    dst->numAllocatedPlies = savedNumAllocatedPlies;
    dst->plies = savedPlies;
    memcpy(dst->plies, src->plies, dst->numPlies * sizeof(GamePlyT));
}
