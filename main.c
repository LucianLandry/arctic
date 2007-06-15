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
    int pollTimeout;
    int moveNowOnTimeout; // bool.
    bigtime_t myTime;
    int turn;

    union {
	CompStatsT stats;
	PvT pv;
	uint8 comstr[4];
    } rspBuf;
    eThinkMsgT rsp;

    /* setup the pollfd array. */
    pfds[0].fd = fileno(stdin);
    pfds[0].events = POLLIN;
    pfds[1].fd = th->masterSock;
    pfds[1].events = POLLIN;

    while(1)
    {
	pollTimeout = -1;
	turn = gameState->savedBoard.ply & 1;
	moveNowOnTimeout = 0;
	if (ClockIsRunning(gameState->clocks[turn]) &&
	    (myTime = ClockCurrentTime(gameState->clocks[turn])) !=
	    CLOCK_TIME_INFINITE)
	{
	    // Try to keep the UI time display refreshed.
	    pollTimeout = myTime % 1000000; // usec
	    pollTimeout /= 1000; // msec
	    pollTimeout += 1; // adjust for truncation

	    // Do we need to move before the next polltimeout?
	    if (gameState->control[turn] &&
		gameState->goalTime[turn] != CLOCK_TIME_INFINITE &&
		(myTime - gameState->goalTime[turn]) / 1000 < pollTimeout)
	    {
		// Yes.
		moveNowOnTimeout = 1;
		pollTimeout = (myTime - gameState->goalTime[turn]) / 1000;
		pollTimeout = MAX(pollTimeout, 0);
	    }
	}

	// poll for input from either stdin, or the computer, or timeout.
	res = poll(pfds, 2, pollTimeout);

	if (res == 0)
	{
	    // poll timed out.  Do appropriate action and re-poll.
	    if (moveNowOnTimeout)
	    {
		// So we do not trigger again
		gameState->goalTime[turn] = CLOCK_TIME_INFINITE;
		ThinkerCmdMoveNow(th);
	    }
	    else
	    {
		// Tick, tock...
		gUI->notifyTick(gameState);
	    }
	    continue;
	}

	if (res == -1 && errno == EINTR) continue;
	assert(res > 0); /* other errors should not happen. */
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
		!gameState->control[gameState->savedBoard.ply & 1])
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
		gUI->notifyPV(&gameState->savedBoard, &rspBuf.pv);
		break;
	    case eRspDraw:
		if (rspBuf.comstr[0] != FLAG)
		{
		    commitmove(board, rspBuf.comstr, th, gameState, 1);
		}

		ClocksStop(gameState);
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
		LogFlush();
		break;
	    case eRspResign:
		ClocksStop(gameState);
		gameState->bDone[board->ply & 1] = 1;
		gUI->notifyReady();
		LOG_DEBUG("%d resigns\n", board->ply & 1);
		LogFlush();
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

#if 0 /* bldbg */
    FILE *errFile = fopen("errlog", "w");
    assert(errFile != NULL);
    LogSetFile(errFile);
    setbuf(errFile, NULL); // turn off buffering
    LogSetLevel(eLogDebug);
#endif

    /* some structure sanity checks we probably should not rely on. */
    assert(offsetof(BoardT, zobrist) + 4 == offsetof(BoardT, hashcoord));
    assert(offsetof(BoardT, zobrist) + 4 + 32 == offsetof(BoardT, cbyte));
    assert(offsetof(BoardT, zobrist) + 4 + 32 + 1 == offsetof(BoardT, ebyte));

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

    // must be done before seeding, if we want reproducable results.
    preCalcInit(numHashEntries * 1024);

    srandom(getBigTime() / 1000000);

    ThinkerInit(&th);
    SwitcherInit(&sw);

    gVars.maxLevel = 0;
    gVars.hiswin = 2;	/* set for killer move heuristic */

    memset(&gameState, 0, sizeof(GameStateT));
    gameState.mainCookie = SwitcherGetCookie(&sw);
    for (i = 0; i < 2; i++)
    {
	gameState.goalTime[i] = CLOCK_TIME_INFINITE;
	gameState.clocks[i] = &gameState.actualClocks[i];
    }
    ClockSetInfinite(&gameState.origClocks[0]);
    ClockSetInfinite(&gameState.origClocks[1]);
    
    gUI = isatty(fileno(stdin)) && isatty(fileno(stdout)) ?
	UIInit(&gameState) : xboardInit();

    ClocksReset(&gameState);
    newgame(&board);
    commitmove(&board, NULL, &th, &gameState, 0);

    compThreadInit(&board, &th);
    playerThreadInit(&board, &th, &sw, &gameState);

    gUI->notifyReady();
    ClockStart(gameState.clocks[0]);

    playloop(&board, &th, &sw, &gameState);

    gUI->exit();
    return 0;
}
