//--------------------------------------------------------------------------
//               game.cpp - current game and associated state.
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

#include "clockUtil.h"
#include "game.h"
#include "gDynamic.h"
#include "log.h"
#include "MoveList.h"
#include "switcher.h"
#include "ui.h"

// Helper function.
// Assume a change in thinking is necessary.
static void compRefresh(GameT *game, Thinker *th)
{
    uint8 turn = game->savedBoard.Turn(); // shorthand

    // Stop anything going on.
    // ThinkerCmd(Think,Ponder) do this for us, but I guess this lets us
    // GoaltimeCalc() and gUI->notify(Thinking,Ponder) faster :P
    th->CmdBail();
    
    if (!game->bDone && game->control[turn])
    {
        // Computer needs to make next move; let it do so.
        GoaltimeCalc(game);
        gUI->notifyThinking();
        th->CmdSetBoard(game->savedBoard);
        th->CmdThink();
    }
    else if (!game->bDone && game->control[turn ^ 1] && gVars.ponder)
    {
        // Computer is playing other side (only) and is allowed to ponder.
        // Do so.
        gUI->notifyPonder();
        th->CmdSetBoard(game->savedBoard);
        th->CmdPonder();
    }
    else
    {
        // We should not be thinking at all.
        gUI->notifyReady();
    }
}


void GameInit(GameT *game, Thinker *th)
{
    int i;

    // origClocks[], actualClocks[], and savedBoard are automatically
    //  constructed.
    game->bDone = false;
    game->icsClocks = false;

    SaveGameInit(&game->sgame);
    SwitcherInit(&game->sw);
    SwitcherRegister(&game->sw);

    for (i = 0; i < NUM_PLAYERS; i++)
    {
        game->control[i] = 0;
        game->clocks[i] = &game->actualClocks[i];
        game->goalTime[i] = CLOCK_TIME_INFINITE;
    }
    ClocksReset(game);

    game->th = th;
}


// Handle a change in computer control or pondering.
void GameCompRefresh(GameT *game)
{
    uint8 turn = game->savedBoard.Turn(); // shorthand
    Thinker *th = game->th; // shorthand
    
    if ((!game->bDone && th->CompIsThinking() && game->control[turn]) ||
        (!game->bDone && th->CompIsPondering() && gVars.ponder &&
         !game->control[turn] && game->control[turn ^ 1]))
    {
        // No change in thinking necessary.  Do not restart think cycle.
        return;
    }

    compRefresh(game, th);
}


void GameMoveMake(GameT *game, MoveT *move)
{
    Board *board = &game->savedBoard; // shorthand.
    Clock *myClock;
    uint8 turn = board->Turn();

    // Give computer a chance to re-evaluate the position, if we insist
    // on changing the board.
    game->bDone = false;

    myClock = game->clocks[turn];
    myClock->Stop();

    if (move != NULL)
    {
        LOG_DEBUG("making move (%d %d): ",
                  board->Ply() >> 1, turn);
        LogMove(eLogDebug, board, *move, 0);

        if (ClocksICS(game)) // have to check this before we make the move
        {
            board->MakeMove(*move);
            // normally would expect this to trigger on plies 1 and 2.
            myClock->Reset(); // pretend like nothing happened to the clock
        }
        else
        {
            board->MakeMove(*move);
            myClock->ApplyIncrement(board->Ply());
        }
        SaveGameMoveCommit(&game->sgame, move, myClock->Time());
        
        // switched sides to another player.
        gVars.pv.Decrement(*move);
    }
    board->ConsistencyCheck("GameMoveMake");
}


void GameMoveCommit(GameT *game, MoveT *move, bool declaredDraw)
{
    MoveList mvlist;
    Board *board = &game->savedBoard; // shorthand.
    int turn;
    Thinker *th = game->th; // shorthand.
    
    GameMoveMake(game, move);

#if 0 // bldbg: here is a way to turn on logging for one ply only.
    if (board->Ply() == 29)
        LogSetLevel(eLogDebug);
    if (board->Ply() == 30)
        LogSetLevel(eLogNormal);
#endif

    turn = board->Turn(); // needs reset.

    gUI->positionRefresh(board->Position());

    // I do not particularly like starting the clock before status is drawn
    // (as opposed to afterwards), but the user should know his clock is
    // running ASAP instead of one tick later (or, if the clock has infinite
    // time, we would never update the status correctly).
    // statusDraw() should be quick enough that it is still fair.
    game->clocks[turn]->Start();
    gUI->statusDraw(game);

    board->GenerateLegalMoves(mvlist, false);
    
    if (board->IsDrawInsufficientMaterial())
    {
        ClocksStop(game);
        gUI->notifyDraw("insufficient material", NULL);
        game->bDone = true;
    }
    else if (!mvlist.NumMoves())
    {
        ClocksStop(game);
        if (!board->IsInCheck())
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


void GameNewEx(GameT *game, Board *board, bool resetClocks)
{
    assert(board->Position().IsLegal());
    game->savedBoard = *board;
    SaveGamePositionSet(&game->sgame, board);
    if (resetClocks)
        ClocksReset(game);
    game->th->CmdNewGame();
    GameMoveCommit(game, NULL, false);
}

void GameNew(GameT *game)
{
    Board myBoard;
    GameNewEx(game, &myBoard, true);
}

int GameGotoPly(GameT *game, int ply)
{
    int plyDiff = ply - GameCurrentPly(game);
    Thinker *th = game->th; // shorthand
    
    if (ply < GameFirstPly(game) || ply > GameLastPly(game))
    {
        return -1;
    }

    th->CmdBail();
    gVars.pv.FastForward(plyDiff);

    // We could th->NewGame() (which would reset the transposition table and
    //  other state), but if the user tracks back and forth through the history,
    //  we might not diverge that much.
    SaveGameGotoPly(&game->sgame, ply, &game->savedBoard, game->clocks);
    GameMoveCommit(game, NULL, false);
    return 0;
}

int GameRewind(GameT *game, int numPlies)
{
    return GameGotoPly(game, GameCurrentPly(game) - numPlies);
}

int GameFastForward(GameT *game, int numPlies)
{
    return GameGotoPly(game, GameCurrentPly(game) + numPlies);
}
