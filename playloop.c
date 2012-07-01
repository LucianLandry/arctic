//--------------------------------------------------------------------------
//                playloop.c - main loop and support routines.
//                           -------------------
//  copyright            : (C) 2008 by Lucian Landry
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
#include <stdio.h>    // fileno(3)
#include <stdlib.h>   // abs(3)
#include <sys/poll.h> // poll(2)

#include "clockUtil.h"
#include "gDynamic.h"
#include "log.h"
#include "playloop.h"
#include "ui.h"


static eThinkMsgT PlayloopCompProcessRsp(GameT *game, ThinkContextT *th)
{
    union {
	CompStatsT stats;
	PvRspArgsT pvArgs;
	MoveT move;
    } rspBuf;

    int turn;
    BoardT *board = &game->savedBoard; // shorthand.
    eThinkMsgT rsp = ThinkerRecvRsp(th, &rspBuf, sizeof(rspBuf));

    if (!gVars.ponder && !game->control[board->turn])
    {
	LOG_EMERG("bad rsp %d\n", rsp);
	assert(0);
    }

    switch(rsp)
    {
    case eRspStats:
	gUI->notifyComputerStats(game, &rspBuf.stats);
	break;
    case eRspPv:
	gUI->notifyPV(game, &rspBuf.pvArgs);
	break;
    case eRspDraw:
	if (!game->control[board->turn])
	{
	    // Decided or forced to draw while pondering.  Ignore and let the
	    // player make their move.
	    gUI->notifyReady();
	    break;
	}

	// Only claimed draws, not automatic draws, should go through
	// this path.
	if (rspBuf.move.src != FLAG && gUI->shouldCommitMoves())
	{
	    GameMoveCommit(game, &rspBuf.move, th, 1);
	}

	ClocksStop(game);
	game->bDone = true;
	gUI->notifyReady();

	if (BoardDrawFiftyMove(board))
	{
	    gUI->notifyDraw("fifty-move rule", &rspBuf.move);
	}
	else if (BoardDrawThreefoldRepetition(board))
	{
	    gUI->notifyDraw("threefold repetition", &rspBuf.move);
	}
	else
	{
	    assert(0);
	}
	break;
    case eRspMove:
	if (!game->control[board->turn])
	{
	    // Decided or forced to move while pondering.  Ignore and let the
	    // player make their move.
	    gUI->notifyReady();
	    break;
	}

	gUI->notifyMove(&rspBuf.move);
	if (gUI->shouldCommitMoves())
	{
	    GameMoveCommit(game, &rspBuf.move, th, 0);
	}
	LogFlush();
	break;
    case eRspResign:
	turn =
	    // Computer resigned its position while pondering?
	    !game->control[board->turn] ?
	    board->turn ^ 1 : // (yes)
	    board->turn;      // (no)

	ClocksStop(game);
	game->bDone = true;
	gUI->notifyReady();
	LOG_DEBUG("%d resigns\n", turn);
	LogFlush();
	gUI->notifyResign(turn);
	break;
    default:
	assert(0);
    }

    return rsp;
}


void PlayloopCompMoveNowAndSync(GameT *game, ThinkContextT *th)
{
    eThinkMsgT rsp;
    if (!ThinkerCompIsBusy(th))
    {
	return;
    }

    // At this point, even if the computer moved in the meantime, we
    // know we haven't processed its response yet ...
    ThinkerCmdMoveNow(th);

    do
    {
	rsp = PlayloopCompProcessRsp(game, th);
    } while (rsp == eRspStats || rsp == eRspPv);

    // ... but now we definitely have (processed the response).
}


// Main play loop.
void PlayloopRun(GameT *game, ThinkContextT *th)
{
    struct pollfd pfds[2];
    int res;
    int pollTimeout;
    int moveNowOnTimeout; // bool.
    bigtime_t myTime, myPerMoveTime;
    int turn;

    /* setup the pollfd array. */
    pfds[0].fd = fileno(stdin);
    pfds[0].events = POLLIN;
    pfds[1].fd = th->masterSock;
    pfds[1].events = POLLIN;

    while(1)
    {
	pollTimeout = -1;
	turn = game->savedBoard.turn;
	moveNowOnTimeout = 0;
	myTime = ClockGetTime(game->clocks[turn]);
	myPerMoveTime = ClockGetPerMoveTime(game->clocks[turn]);
	myTime = MIN(myTime, myPerMoveTime);

	if (ClockIsRunning(game->clocks[turn]) &&
	    myTime != CLOCK_TIME_INFINITE)
	{
	    // Try to keep the UI time display refreshed.

	    // Start by finding usec until next tick.
	    pollTimeout =
		(myTime < 0 ?
		 // Avoid machine-dependent behavior of / and % w/negative
		 // numbers.
		 1000000 - (llabs(myTime) % 1000000) :
		 myTime % 1000000); // normal case (positive)

	    pollTimeout /= 1000; // ... converted to msec
	    pollTimeout += 1; // ... and adjusted for division truncation

	    // Do we need to move before the next polltimeout?
	    if (game->control[turn] &&
		game->goalTime[turn] != CLOCK_TIME_INFINITE &&
		(myTime - game->goalTime[turn]) / 1000 < pollTimeout)
	    {
		// Yes.
		moveNowOnTimeout = 1;
		pollTimeout = (myTime - game->goalTime[turn]) / 1000;
		pollTimeout = MAX(pollTimeout, 0);
	    }
	}

	// poll for input from either stdin (UI), or the computer, or timeout.
	res = poll(pfds, 2, pollTimeout);

	if (res == 0)
	{
	    // poll timed out.  Do appropriate action and re-poll.
	    if (moveNowOnTimeout)
	    {
		// So we do not trigger again
		game->goalTime[turn] = CLOCK_TIME_INFINITE;
		ThinkerCmdMoveNow(th);
	    }
	    else
	    {
		// Tick, tock...
		gUI->notifyTick(game);
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
	    SwitcherSwitch(&game->sw);
	}
	// 'else' because user-input handler may change the state on us,
	// so we need to re-poll...
	else if (pfds[1].revents & POLLIN)
	{
	    PlayloopCompProcessRsp(game, th);
	}
    }
}
