//--------------------------------------------------------------------------
//                game.c - current game and associated state.
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

#include "clockUtil.h"
#include "game.h"
#include "gDynamic.h" // gPvDecrement()
#include "gPreCalc.h"
#include "log.h"
#include "moveList.h"
#include "switcher.h"
#include "transTable.h"
#include "ui.h"
#include <stdio.h> // debug

// Helper function.
// Assume a change in thinking is necessary.
static void compRefresh(GameT *game, ThinkContextT *th)
{
    int turn = game->savedBoard.turn; // shorthand

    // Stop anything going on.
    // ThinkerCmd(Think,Ponder) do this for us, but I guess this lets us
    // GoaltimeCalc() and gUI->notify(Thinking,Ponder) faster :P
    ThinkerCmdBail(th);
    
    if (!game->bDone && game->control[turn])
    {
	// Computer needs to make next move; let it do so.
	GoaltimeCalc(game);
	gUI->notifyThinking();
	ThinkerCmdThink(th, &game->savedBoard, &game->sgame);
    }
    else if (!game->bDone && game->control[turn ^ 1] && gVars.ponder)
    {
	// Computer is playing other side (only) and is allowed to ponder.
	// Do so.
	gUI->notifyPonder();
	ThinkerCmdPonder(th, &game->savedBoard, &game->sgame);
    }
    else
    {
	// We should not be thinking at all.
	gUI->notifyReady();
    }
}


void GameInit(GameT *game)
{
    int i;

    memset(game, 0, sizeof(GameT));

    SaveGameInit(&game->sgame);
    SwitcherInit(&game->sw);
    SwitcherRegister(&game->sw);

    for (i = 0; i < NUM_PLAYERS; i++)
    {
	game->goalTime[i] = CLOCK_TIME_INFINITE;
	game->clocks[i] = &game->actualClocks[i];
	ClockInit(&game->origClocks[i]);
    }
    ClocksReset(game);
    BoardInit(&game->savedBoard);
}


// Handle a change in computer control or pondering.
void GameCompRefresh(GameT *game, ThinkContextT *th)
{
    int turn = game->savedBoard.turn; // shorthand

    if ((!game->bDone && ThinkerCompIsThinking(th) && game->control[turn]) ||
	(!game->bDone && ThinkerCompIsPondering(th) && gVars.ponder &&
	 !game->control[turn] && game->control[turn ^ 1]))
    {
	// No change in thinking necessary.  Do not restart think cycle.
	return;
    }

    compRefresh(game, th);
}


void GameMoveMake(GameT *game, MoveT *move)
{
    BoardT *board = &game->savedBoard; // shorthand.
    ClockT *myClock;
    int turn = board->turn;

    // Give computer a chance to re-evaluate the position, if we insist
    // on changing the board.
    game->bDone = false;

    myClock = game->clocks[turn];
    ClockStop(myClock);

    if (move != NULL)
    {
	BoardPositionSave(board);
	LOG_DEBUG("making move (%d %d): ",
		  board->ply >> 1, turn);
	LogMove(eLogDebug, board, move);

	if (ClocksICS(game)) // have to check this before we make the move
	{
	    BoardMoveMake(board, move, NULL);
	    // normally would expect this to trigger on plies 1 and 2.
	    ClockReset(myClock); // pretend like nothing happened to the clock
	}
	else
	{
	    BoardMoveMake(board, move, NULL);
	    ClockApplyIncrement(myClock, board->ply);
	}
	SaveGameMoveCommit(&game->sgame, move, ClockGetTime(myClock));
	
        // switched sides to another player.
	gPvDecrement(move);
    }
    BoardConsistencyCheck(board, "GameMoveMake", 1);
}


void GameMoveCommit(GameT *game, MoveT *move, ThinkContextT *th,
		    int declaredDraw)
{
    MoveListT mvlist;
    BoardT *board = &game->savedBoard; // shorthand.
    int turn;

    GameMoveMake(game, move);

#if 0 // bldbg: here is a way to turn on logging for one ply only.
    if (board->ply == 29)
	LogSetLevel(eLogDebug);
    if (board->ply == 30)
	LogSetLevel(eLogNormal);
#endif

    turn = board->turn; // needs reset.

    gUI->boardRefresh(board);

    // I do not particularly like starting the clock before status is drawn
    // (as opposed to afterwards), but the user should know his clock is
    // running ASAP instead of one tick later (or, if the clock has infinite
    // time, we would never update the status correctly).
    // statusDraw() should be quick enough that it is still fair.
    ClockStart(game->clocks[turn]);
    gUI->statusDraw(game);

    mlistGenerate(&mvlist, board, 0);

    if (BoardDrawInsufficientMaterial(board))
    {
	ClocksStop(game);
	gUI->notifyDraw("insufficient material", NULL);
	game->bDone = true;
    }
    else if (!mvlist.lgh)
    {
	ClocksStop(game);
	if (board->ncheck[turn] == FLAG)
	{
	    gUI->notifyDraw("stalemate", NULL);
	}
	else
	{
	    gUI->notifyCheckmated(turn);
	}
	game->bDone = true;
    }
    else if (declaredDraw)
    {
	// Some draws are not automatic, and need to be notified separately.
	game->bDone = true;
    }

    // Cannot use GameCompRefresh() here, since perhaps we changed the
    // board out from under the computer and therefore always need to restart
    // thinking.
    compRefresh(game, th);
}


void GameNewEx(GameT *game, ThinkContextT *th, BoardT *board, int resetClocks,
	       int resetHash)
{
    assert(BoardSanityCheck(board, 1) == 0);
    BoardCopy(&game->savedBoard, board);
    SaveGamePositionSet(&game->sgame, board);
    if (resetClocks)
    {
	ClocksReset(game);
    }
    // It might be possible to put logic in here to make the hash-resetting
    // option useless.  For instance, if board != game->savedBoard, gotoply()
    // on the savedboard to the same ply as the board.  Then if we have a
    // position hit, and we are w/in x (5?) plies of the original ply, do not
    // clear the hash.  Random moves might not work quite as intended due to
    // hash hits re-ordering our moves, but that should not be a huge issue.
    if (resetHash)
    {
	TransTableReset();
	gHistInit();
    }
    gPvInit();
    GameMoveCommit(game, NULL, th, 0);
}


void GameNew(GameT *game, ThinkContextT *th)
{
    BoardT myBoard;
    BoardSet(&myBoard, gPreCalc.normalStartingPieces, CASTLEALL, FLAG, 0, 0,
	     0);
    GameNewEx(game, th, &myBoard, 1, 1);
}


int GameGotoPly(GameT *game, int ply, ThinkContextT *th)
{
    int plyDiff = ply - GameCurrentPly(game);

    if (ply < GameFirstPly(game) || ply > GameLastPly(game))
    {
	return -1;
    }

    ThinkerCmdBail(th);
    gPvFastForward(plyDiff);

    // We could TransTableReset()/gHistInit() here, but if the user tracks
    // back and forth through the history, we might not diverge that much.
    SaveGameGotoPly(&game->sgame, ply, &game->savedBoard, game->clocks);
    GameMoveCommit(game, NULL, th, 0);
    return 0;
}

int GameRewind(GameT *game, int numPlies, ThinkContextT *th)
{
    return GameGotoPly(game, GameCurrentPly(game) - numPlies, th);
}

int GameFastForward(GameT *game, int numPlies, ThinkContextT *th)
{
    return GameGotoPly(game, GameCurrentPly(game) + numPlies, th);
}
