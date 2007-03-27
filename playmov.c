/***************************************************************************
             playmov.c - player loop and generic support functions
                             -------------------
    copyright            : (C) 2007 by Lucian Landry
    email                : lucian_b_landry@yahoo.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 ***************************************************************************/


#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h> /* exit(3) */
#include <assert.h>
#include <pthread.h> /* pthread_create(3) */
#include "ref.h"


/* (one extra space for \0.) */
static char gPieceUITable[BQUEEN + 2] = "  KkPpNnBbRrQq";

int nativeToAscii(uint8 piece)
{
    return piece > BQUEEN ? ' ' : gPieceUITable[piece];
}


int asciiToNative(uint8 ascii)
{
    char *mychr = strchr(gPieceUITable, ascii);
    return mychr != NULL ? mychr - gPieceUITable : 0;
}


char *searchlist(MoveListT *mvlist, char *comstr, int howmany)
{
    int i;
    for (i = 0; i < mvlist->lgh; i++)
	if (!memcmp(mvlist->list[i], comstr, howmany))
	    return mvlist->list[i];
    return NULL;
}


// Note: Not a full sanity check!  Only intended for sanity checking edited
// positions.
int BoardSanityCheck(BoardT *board)
{
    /* There are other tests we can add, when necessary:
       -- pawn on 1st/8th ranks
       -- bad ebyte
       -- bad cbyte
    */
    int diff;
    int i;

    /* Check: pawns must not be on 1st or 8th rank. */
    for (i = 0; i < 64; i++)
    {
	if (i == 8) i = 56; /* skip to the 8th rank */
	if (ISPAWN(board->coord[i]))
	{
	    gUI->notifyError("Error: Pawn detected on 1st or 8th rank.");
	    return -1;
	}
    }
    /* Check: only one king (of each color) on board. */
    if (board->playlist[KING].lgh != 1 ||
	board->playlist[BKING].lgh != 1)
    {
	gUI->notifyError("Error: Need one king of each color.");
	return -1;
    }
    /* Check: the side *not* on move must not be in check. */
    if (calcNCheck(board, board->playlist[BKING ^ (board->ply & 1)].list[0],
		   "BoardSanityCheck") != FLAG)
    {
	gUI->notifyError("Error: Side not on move is in check.");
	return -1;
    }
    /* Check: Kings must not be adjacent to each other (calcNCheck() does not
       take this into account). */
    diff = board->playlist[KING].list[0] - board->playlist[BKING].list[0];
    if (diff == 1 || diff == 7 || diff == 8 || diff == 9 ||
	diff == -1 || diff == -7 || diff == -8 || diff == -9)
    {
	gUI->notifyError("Error: Side not on move is in check by king.");
	return -1;
    }

    return 0;
}


typedef struct {
    sem_t *mySem;
    BoardT *board;
    ThinkContextT *th;
    SwitcherContextT *sw;
    GameStateT *gameState;
} PlayerArgsT;


static void playerThread(PlayerArgsT *args)
{
    PlayerArgsT myArgs = *args;
    sem_post(args->mySem);

    myArgs.gameState->playCookie = SwitcherGetCookie(myArgs.sw);
    while(1)
    {
	gUI->playerMove(myArgs.board, myArgs.th, myArgs.sw, myArgs.gameState);
    }    
}

void playerThreadInit(BoardT *board, ThinkContextT *th, SwitcherContextT *sw,
		      GameStateT *gameState)
{
    int err;
    sem_t mySem;
    PlayerArgsT args = {&mySem, board, th, sw, gameState};
    pthread_t myThread;

    sem_init(&mySem, 0, 0);
    err = pthread_create(&myThread, NULL, (PTHREAD_FUNC) playerThread, &args);
    assert(err == 0);
    sem_wait(&mySem);
    sem_destroy(&mySem);
}
