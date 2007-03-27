/***************************************************************************
                 main.c - main initialization and runtime loop
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


#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h> // poll(2)
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include "ref.h"


UIFuncTableT *gUI;


static void usage(char *programName)
{
    printf("usage: %s [-h=<numhashentries> (in KiB, default 128k entries)]\n"
	   "'numhashentries' must be 0, or a power of 2 between 1 and 512 "
	   "inclusive.\n", programName);
    exit(0);
}


int isPow2(int c)
{
    return c != 0 && (c & (c - 1)) == 0;
}


void playloop(BoardT *board, ThinkContextT *th, SwitcherContextT *sw,
	      GameStateT *gameState)
{
    struct pollfd pfds[2];
    int res;

    union {
	CompStatsT stats;
	PvT pv;
	uint8 comstr[4];
    } rspBuf;
    eThinkMsgT rsp;

    /* setup the pollfd array. */
    pfds[0].fd = fileno(stdin);
    pfds[0].events = POLLIN;
    pfds[1].fd = th->mainSock;
    pfds[1].events = POLLIN;

    while(1)
    {
	/* poll for input from either stdin, or the computer. */
	res = poll(pfds, 2, -1);

	if (res == -1 && errno == EINTR) continue;
	assert(res > 0); /* other errors, or timeout, should not happen. */
	assert(!(pfds[1].revents & (POLLERR | POLLHUP | POLLNVAL)));

	if (pfds[0].revents & (POLLERR | POLLHUP | POLLNVAL))
	{
	    LOG_DEBUG("stdin recvd event 0x%x, bailing\n", pfds[0].revents);
	    return;
	}

	if (pfds[0].revents & POLLIN)
	{
	    SwitcherSwitch(sw, gameState->mainCookie);
	}
	/* 'else' because user-input handler may change the state on us,
	   so we need to re-poll... */
	else if (pfds[1].revents & POLLIN)
	{
	    rsp = ThinkerRecvRsp(th, &rspBuf, sizeof(rspBuf));
	    if (rsp != eRspStats &&
		!gameState->control[gameState->boardCopy.ply & 1])
	    {
		LOG_EMERG("bad rsp %d\n", rsp);
		assert(0);
	    }
	    switch(rsp)
	    {
	    case eRspStats:
		gUI->notifyComputerStats(&rspBuf.stats);
		break;
	    case eRspPv:
		gUI->notifyPV(&rspBuf.pv);
		break;
	    case eRspDraw:
		if (rspBuf.comstr[0] != FLAG)
		{
		    commitmove(board, rspBuf.comstr, th, gameState, 1);
		}

		gameState->bDone[board->ply & 1] = 1;
		gUI->notifyReady();

		if (drawFiftyMove(board))
		{
		    gUI->notifyDraw("fifty-move rule");
		}
		else if (drawThreefoldRepetition(board))
		{
		    gUI->notifyDraw("threefold repetition");
		}
		else
		{
		    assert(0);
		}
		break;
	    case eRspMove:
		gUI->notifyMove(rspBuf.comstr);
		commitmove(board, rspBuf.comstr, th, gameState, 0);
		break;
	    case eRspResign:
		gameState->bDone[board->ply & 1] = 1;
		gUI->notifyReady();
		gUI->notifyResign(board->ply & 1);
		break;
	    default:
		assert(0);
	    }
	}
    }
}


int main(int argc, char *argv[])
{
    int numHashEntries = 128; /* in pow2 terms */
    int i;
    BoardT board;
    ThinkContextT th;
    SwitcherContextT sw;
    GameStateT gameState;

    /* parse any cmd-line args. */
    for (i = 1; i < argc; i++)
    {
	if (strstr(argv[i], "-h=") != NULL)
	{
	    if (sscanf(argv[i], "-h=%d", &numHashEntries) != 1 ||
		(numHashEntries != 0 &&
		 (!isPow2(numHashEntries) || numHashEntries > 512)))
	    {
		usage(argv[0]);
	    }
	}
	else
	{
	    // Unrecognized argument.
	    usage(argv[0]);
	}
    }

    memset(&gameState, 0, sizeof(GameStateT));

    gUI = isatty(fileno(stdin)) && isatty(fileno(stdout)) ?
	UIInit() : xboardInit();

    preCalcInit(numHashEntries * 1024);
    ThinkerInit(&th);
    SwitcherInit(&sw);

    gameState.mainCookie = SwitcherGetCookie(&sw);
    gameState.lastTime = time(NULL);

    board.maxLevel = 0;
    board.hiswin = 1;	/* set for killer move heuristic */
    newgame(&board);

    commitmove(&board, NULL, &th, &gameState, 0);

    compThreadInit(&board, &th);
    playerThreadInit(&board, &th, &sw, &gameState);

    gUI->notifyReady();
    playloop(&board, &th, &sw, &gameState);
    gUI->exit();
    return 0;
}
