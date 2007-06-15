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
#include <stdlib.h> /* exit(3), qsort(3) */
#include <assert.h>
#include <pthread.h> /* pthread_create(3) */
#include <time.h>
#include <sys/time.h>
#include "ref.h"


/* Count number of occurances of 'needle' in 'haystack'. */
int strCount(char *haystack, char *needle)
{
    int retVal = 0;
    char *occur;

    for (retVal = 0;
	 (occur = strstr(haystack, needle)) != NULL;
	 haystack = occur + strlen(needle), retVal++)
	; /* no-op */

    return retVal;
}


/* (one extra space for \0.) */
static char gPieceUITable[BQUEEN + 2] = "  KkPpNnBbRrQq";

int nativeToAscii(uint8 piece)
{
    return piece > BQUEEN ? ' ' : gPieceUITable[piece];
}


int nativeToBoardAscii(uint8 piece)
{
    int ascii = nativeToAscii(piece);
    return ISPAWN(piece) ? tolower(ascii) :
	(piece & 1) ? toupper(ascii) : ascii;
}


int asciiToNative(uint8 ascii)
{
    char *mychr = strchr(gPieceUITable, ascii);
    return mychr != NULL ? mychr - gPieceUITable : 0;
}


uint8 *searchlist(MoveListT *mvlist, uint8 *comstr, int howmany)
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


static void getSanMove(BoardT *board, char *sanStr, uint8 *comstr)
{
    /* See the 'algebraic notation' (chess) article on Wikipedia for details
       about SAN. */
    uint8 *coord = board->coord;
    uint8 src = comstr[0];
    uint8 dst = comstr[1];
    uint8 mypiece = coord[src];
    char *origSanStr = sanStr;
    int i;
    int isCapture = coord[dst] || ISPAWN(comstr[2]);
    int isCastle = ISKING(mypiece) && Abs(dst - src) == 2;
    int isPromote = comstr[2] && !ISPAWN(comstr[2]);
    int sameFile = 0, sameRank = 0;
    MoveListT mvlist;
    UnMakeT unmake;

    if (isCastle)
    {
	sprintf(sanStr, dst == 6 || dst == 62 ?
                /* (PGN wants this, but FIDE wants 0 instead of O.) */
		"O-O" : "O-O-O");
	return;
    }


    if (!ISPAWN(mypiece))
	/* Print piece (type) to move. */
	sanStr += sprintf(sanStr, "%c", nativeToBoardAscii(mypiece));
    else if (isCapture)
	/* Need to spew the file we are capturing from. */
	sanStr += sprintf(sanStr, "%c", File(src) + 'a');

    mlistGenerate(&mvlist, board, 0);

    /* Is there ambiguity about which piece will be moved? */
    for (i = 0; i < mvlist.lgh; i++)
    {
	if (!ISPAWN(mypiece) /* already taken care of, above */ &&
	    mvlist.list[i] [0] != src &&
	    mvlist.list[i] [1] == dst &&
	    coord[mvlist.list[i] [0]] == mypiece)
	{
	    /* Yes.  Note: Three pieces of the same type could trigger
	       both conditions. */
	    if (!sameFile)
		sameFile = File(mvlist.list[i] [0]) == File(src);
	    if (!sameRank)
		sameRank = Rank(mvlist.list[i] [0]) == Rank(src);
	}
    }

    /* ... disambiguate the src piece, if necessary. */
    if (!sameFile && sameRank)
	sanStr += sprintf(sanStr, "%c", File(src) + 'a');
    else if (!sameRank && sameFile)
	sanStr += sprintf(sanStr, "%c", Rank(src) + '1');
    else if (sameRank && sameFile)
	sanStr += sprintf(sanStr, "%c%c", File(src) + 'a', Rank(src) + '1');

    if (isCapture)
	sanStr += sprintf(sanStr, "x");

    /* spew the destination coord. */
    sanStr += sprintf(sanStr, "%c%c", File(dst) + 'a', Rank(dst) + '1');

    if (isPromote)
	/* spew piece type to promote to. */
	sanStr += sprintf(sanStr, "%c", nativeToBoardAscii(comstr[2]));	

    if (comstr[3] != FLAG)
    {
	/* piece in check. */
	/* Is this checkmate? */
	makemove(board, comstr, &unmake);
	mlistGenerate(&mvlist, board, 0);
	unmakemove(board, comstr, &unmake);

	sanStr += sprintf(sanStr, "%c", mvlist.lgh ? '+' : '#');
    }

    assert(sanStr - origSanStr < 8);
}


void buildSanString(BoardT *board, char *dstStr, int dstLen, PvT *pv)
{
    char myStrSpace[MAX_PV_DEPTH * 8 + 1];
    char *myStr = myStrSpace;
    char sanStr[8]; /* longest move: Qb5xb6+ */
    BoardT myBoard;
    int howmany;
    uint8 *moves;
    int lastLen = 0, myStrLen;

    BoardCopy(&myBoard, board);

    for (howmany = pv->depth + 1, moves = pv->pv;
	 howmany > 0;
	 howmany--, moves += 4)
    {
	getSanMove(&myBoard, sanStr, moves);
	makemove(&myBoard, moves, NULL);

	/* Build up the result string. */
	myStr += sprintf(myStr, " %s", sanStr);
	myStrLen = myStr - myStrSpace;
	assert(myStrLen < sizeof(myStrSpace));
	if (myStrLen > dstLen)
	{
	    /* We wrote too much information.  Chop the last move off. */
	    myStrSpace[lastLen] = '\0';
	    break;
	}
	lastLen = myStrLen;
    }

    strcpy(dstStr, myStrSpace);
}


void BoardUpdatePlayPtrs(BoardT *board)
{
    int i, j, piece;

    for (i = 0; i < 64; i++)
    {
	board->playptr[i] = NULL;
	if ((piece = board->coord[i]))
	{
	    for (j = 0; j < board->playlist[piece].lgh; j++)
	    {
		if (board->playlist[piece].list[j] == i)
		{
		    board->playptr[i] = &board->playlist[piece].list[j];
		}
	    }
	}
    }
}


/* Random-move support. */
typedef struct {
    int coord;
    int randPos;
} RandPosT;
typedef int (*RAND_COMPAREFUNC)(const void *, const void *);
static int randCompareHelper(const RandPosT *p1, const RandPosT *p2)
{
    return p1->randPos - p2->randPos;
}


void BoardRandomize(BoardT *board)
{
    int i, j, len;
    RandPosT randPos[64];
    CoordListT *playList;

    memset(randPos, 0, sizeof(randPos));
    for (i = 0; i < BQUEEN + 1; i++)
    {
	playList = &board->playlist[i];

	len = playList->lgh;
	for (j = 0; j < len; j++)
	{
	    randPos[j].coord = playList->list[j];
	    randPos[j].randPos = random();
	}

	qsort(randPos, len, sizeof(RandPosT),
	      (RAND_COMPAREFUNC) randCompareHelper);

	for (j = 0; j < len; j++)
	{
	    playList->list[j] = randPos[j].coord;
	}
    }

    BoardUpdatePlayPtrs(board);
}


bigtime_t getBigTime(void)
{
    struct timeval tmvalue;
    gettimeofday(&tmvalue, NULL);
    return ((bigtime_t) tmvalue.tv_sec) * 1000000 + tmvalue.tv_usec;
}



ThreadArgsT gThreadDummyArgs = { NULL };

typedef void *(*PTHREAD_FUNC)(void *);
void ThreadCreate(void *childFunc, void *args)
{
    pthread_t myThread;
    sem_t mySem;
    int err;

    ((ThreadArgsT *) args)->mySem = &mySem;

    sem_init(&mySem, 0, 0);
    err = pthread_create(&myThread, NULL, (PTHREAD_FUNC) childFunc, args);
    assert(err == 0);
    sem_wait(&mySem);
    sem_destroy(&mySem);
}

void ThreadNotifyCreated(char *name, void *args)
{
    sem_post(((ThreadArgsT *) args)->mySem);
    LOG_DEBUG("created thread \"%s\" %p\n", name, (void *) pthread_self());
}


typedef struct {
    ThreadArgsT args;
    BoardT *board;
    ThinkContextT *th;
    SwitcherContextT *sw;
    GameStateT *gameState;
} PlayerArgsT;


static void playerThread(PlayerArgsT *args)
{
    PlayerArgsT myArgs = *args; // struct copy
    ThreadNotifyCreated("playerThread", args);

    myArgs.gameState->playCookie = SwitcherGetCookie(myArgs.sw);
    while(1)
    {
	gUI->playerMove(myArgs.board, myArgs.th, myArgs.sw, myArgs.gameState);
    }    
}

void playerThreadInit(BoardT *board, ThinkContextT *th, SwitcherContextT *sw,
		      GameStateT *gameState)
{
    PlayerArgsT args = {gThreadDummyArgs, board, th, sw, gameState};
    ThreadCreate(playerThread, &args);
}
