//--------------------------------------------------------------------------
//               Game.cpp - current game and associated state.
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

#include "Game.h"
#include "log.h"
#include "MoveList.h"
#include "ui.h"

Game::Game(Thinker *th) : th(th)
{
    // origClocks[], clocks[], sgame, and savedBoard are automatically
    //  constructed.
    done = false;
    ponder = false;
    state = State::Stopped;
    autoPlayEngineMoves = true;

    for (int i = 0; i < NUM_PLAYERS; i++)
        engineControl[i] = false;
    ResetClocks(); // necessary (to set initial clocks for savegame)

    setThinkerRspHandler(*th);
}

// Handle a change in computer control or pondering.
void Game::refresh()
{
    if (state == State::Stopped)
        return;

    class Board &board = savedBoard; // shorthand.
    uint8 turn = board.Turn(); // shorthand

    if (board.Position() != lastRefreshedPosition)
    {
        gUI->positionRefresh(board.Position());
        lastRefreshedPosition = board.Position();
    }
        
    if (!done)
    {
        // I do not particularly like starting the clock before status is drawn
        // (as opposed to afterwards), but the user should know his clock is
        // running ASAP instead of one tick later (or, if the clock has infinite
        // time, we would never update the status correctly).
        // statusDraw() should be quick enough that it is still fair.
        clocks[turn].Start();
        gUI->statusDraw(this);

        MoveList mvlist;
        board.GenerateLegalMoves(mvlist, false);
    
        if (board.IsDrawInsufficientMaterial())
        {
            stopClocks();
            gUI->notifyDraw(this, "insufficient material", NULL);
            done = true;
        }
        else if (!mvlist.NumMoves())
        {
            stopClocks();
            if (!board.IsInCheck())
                gUI->notifyDraw(this, "stalemate", NULL);
            else
                gUI->notifyCheckmated(turn);
            done = true;
        }
    }
    
    if (state == State::Stopped ||
        (!done && th->CompIsThinking() && engineControl[turn]) ||
        (!done && th->CompIsPondering() && ponder &&
         !engineControl[turn] && engineControl[turn ^ 1]))
    {
        return; // No change in thinking necessary.  Do not restart think cycle.
    }

    // Stop anything going on.
    th->CmdBail();
    
    if (!done && engineControl[turn])
    {
        // Computer needs to make next move; let it do so.
        gUI->notifyThinking();
        if (searchList.NumMoves())
            th->CmdThink(clocks[turn], searchList);
        else
            th->CmdThink(clocks[turn]);            
    }
    else if (!done && engineControl[turn ^ 1] && ponder)
    {
        // Computer is playing other side (only) and is allowed to ponder.
        // Do so.
        gUI->notifyPonder();
        if (searchList.NumMoves())
            th->CmdPonder(searchList);
        else
            th->CmdPonder();
    }
    else
    {
        // We should not be thinking at all.
        gUI->notifyReady();
    }
}

void Game::makeMove(MoveT move, bool moveThinkers)
{
    if (move == MoveNone) // degenerate case
        return;

    class Board &board = savedBoard; // shorthand.
    uint8 turn = board.Turn();

    assert(board.IsLegalMove(move));

    // Give computer a chance to re-evaluate the position, if we insist
    // on changing the board.
    done = false;
    class Clock &myClock = clocks[turn]; // shorthand.
    bool wasRunning = myClock.IsRunning();
    myClock.Stop();

    if (moveThinkers)
        th->CmdMakeMove(move);
    LOG_DEBUG("making move (%d %d): ",
              board.Ply() >> 1, turn);
    LogMove(eLogDebug, &board, move, 0);

    board.MakeMove(move);
    if (wasRunning)
        myClock.ApplyIncrement(board.Ply());
    sgame.CommitMove(move, myClock.Time());

    board.ConsistencyCheck(__func__);
    refresh();
}

void Game::MakeMove(MoveT move)
{
    return makeMove(move, true);
}

void Game::NewGame(const class Board &board, bool resetClocks)
{
    assert(board.Position().IsLegal());

    // We must Stop() manually since we don't want things firing after
    //  ResetClocks().
    bool wasRunning = Stop();
    done = false;
    savedBoard = board;
    sgame.SetStartPosition(savedBoard);
    if (resetClocks)
        ResetClocks();
    th->CmdNewGame();
    th->CmdSetBoard(savedBoard);
    if (wasRunning)
        Go();
}

void Game::NewGame()
{
    class Board myBoard; // use default board
    NewGame(myBoard, true);
}

int Game::GotoPly(int ply)
{
    // Assumes that we are still in the current game.  If that is not the case,
    //  the user should have invoked NewGame() instead.
    // Assumes the engine board(s) are in sync with the savedBoard.
    if (ply < FirstPly() || ply > LastPly())
        return -1;
        
    int plyDiff = ply - CurrentPly();
    if (plyDiff == 0)
        return 0;
        
    done = false;
    int origPly = CurrentPly();
    sgame.GotoPly(ply, &savedBoard, clocks);

    if (plyDiff < 0)
    {
        for (int i = 0; i > plyDiff; i--)
            th->CmdUnmakeMove();
    }
    else
    {
        // Need to move forward.  I guess I prefer querying a Board over a
        //  SaveGame to get the proper move.
        for (int i = origPly; i < ply; i++)
            th->CmdMakeMove(savedBoard.MoveAt(i));
    }
    refresh();
    return 0;
}

void Game::SetBoard(const class Board &other)
{
    assert(other.Position().IsLegal());
    done = false;
    bool wasRunning = Stop();
    int lastCommonPly = savedBoard.LastCommonPly(other);
    int myPlyDiff = savedBoard.Ply() - lastCommonPly;
    int otherPlyDiff = other.Ply() - lastCommonPly;

    // We try to pack in some intelligence here in case somebody just wanted
    //  to set a Position that is much like the current one (happens a lot with
    //  UCI and Polyglot, for instance).  This is so that engines do not
    //  unnecessarily lose their PVs (in addition to possibly being easier to
    //  send over a wire).
    // We could do the same kind of thing in the engine instead, but this seems
    //  like a better place for it (there's only one game).
    if (lastCommonPly >= 0 &&
        // Always fall back if the other board has more information.
        other.BasePly() >= savedBoard.BasePly() &&
        // Prefer the 'shortest distance to goal'.
        myPlyDiff + otherPlyDiff < other.Ply() - other.BasePly())
    {
        // Attempt the 'shortcut'.
        // This replicates a bit of what GotoPly() does, but w/out messing with
        //  the clocks.
        sgame.GotoPly(lastCommonPly, &savedBoard, nullptr);
        for (int i = 0; i < myPlyDiff; i++)
            th->CmdUnmakeMove();
        for (int i = lastCommonPly; i < other.Ply(); i++)
            MakeMove(other.MoveAt(i));
    }
    else
    {
        // Do it the 'hard' way.  We prefer to set the Thinker in one swoop
        //  (as opposed to via SetBoard + MakeMove()) to (kindof) preserve the
        //  PV better and to minimize traffic.
        th->CmdSetBoard(other);
        class Board tmpBoard = other;
        while (tmpBoard.Ply() != tmpBoard.BasePly()) // goto the base position
            tmpBoard.UnmakeMove();
        savedBoard = tmpBoard;
        sgame.SetStartPosition(tmpBoard);
        for (int i = other.BasePly(); i < other.Ply(); i++)
            makeMove(other.MoveAt(i), false);
    }
    
    if (wasRunning)
        Go();
}

int Game::Rewind(int numPlies)
{
    return GotoPly(CurrentPly() - numPlies);
}

int Game::FastForward(int numPlies)
{
    return GotoPly(CurrentPly() + numPlies);
}

void Game::ResetClocks()
{
    for (int i = 0; i < NUM_PLAYERS; i++)
        clocks[i] = initialClocks[i];
    
    if (CurrentPly() == 0)
    {
        // Propagate changes to the SaveGame -- we assume the game is not
        // in progress.
        sgame.SetClocks(clocks);
    }
    refresh();
}

// Note: internal routine; does *not* refresh, since that would probably just
//  start one of the clocks back up. :P
void Game::stopClocks()
{
    for (int i = 0; i < NUM_PLAYERS; i++)
        clocks[i].Stop();
}

void Game::LogClocks(const char *context) const
{
    for (int i = 0; i < NUM_PLAYERS; i++)
    {
        LOG_DEBUG("%s (%s): clock %d: %lld %lld %d %lld %c\n",
                  __func__,
                  context ? context : "",
                  i,
                  (long long) clocks[i].Time(),
                  (long long) clocks[i].Increment(),
                  clocks[i].TimeControlPeriod(),
                  (long long) clocks[i].PerMoveLimit(),
                  clocks[i].IsRunning() ? 'r' : 's');
    }
}

const class Clock &Game::Clock(int player) const
{
    assert(player >= 0 && player < NUM_PLAYERS);
    return clocks[player];
}

const class Clock &Game::InitialClock(int player) const
{
    assert(player >= 0 && player < NUM_PLAYERS);
    return initialClocks[player];
}

void Game::SetInitialClock(int player, const class Clock &other)
{
    assert(player >= 0 && player < NUM_PLAYERS);
    initialClocks[player].SetParameters(other);
    // Assume a refresh() is not needed because initial clocks should only
    //  take effect after a NewGame().
}

void Game::SetClock(int player, const class Clock &other)
{
    assert(player >= 0 && player < NUM_PLAYERS);
    clocks[player].SetParameters(other);
    refresh();
}

bool Game::EngineControl(int player) const
{
    assert(player >= 0 && player < NUM_PLAYERS);
    return engineControl[player];
}

void Game::SetEngineControl(int player, bool value)
{
    assert(player >= 0 && player < NUM_PLAYERS);
    if (engineControl[player] == value)
        return;

    engineControl[player] = value;
    refresh();
}

void Game::ToggleEngineControl(int player)
{
    SetEngineControl(player, !EngineControl(player));
}

void Game::SetPonder(bool value)
{
    if (ponder == value)
        return;
    ponder = value;
    refresh();
}

void Game::TogglePonder()
{
    SetPonder(!Ponder());
}

Config &Game::EngineConfig()
{
    return th->Config();
}

bool Game::Stop()
{
    if (state == State::Stopped)
        return false;
    state = State::Stopped;
    th->CmdBail();
    stopClocks();
    return true;
}

bool Game::StopAndForce()
{
    bool result = Stop();
    for (int i = 0; i < NUM_PLAYERS; i++)
        engineControl[i] = false;
    return result;
}

bool Game::Go()
{
    if (state == State::Running)
        return false;
    state = State::Running;
    refresh();
    return true;
}

bool Game::Go(const MoveList &searchList)
{
    this->searchList = searchList;
    bool retVal = Go();
    this->searchList.DeleteAllMoves();
    return retVal;
}

void Game::SetAutoPlayEngineMoves(bool value)
{
    assert(state == State::Stopped);
    autoPlayEngineMoves = value;
}

void Game::WaitForEngineIdle()
{
    while (th->CompIsBusy())
        th->ProcessOneRsp();
}

void Game::MoveNow()
{
    th->CmdMoveNow();
    WaitForEngineIdle();
}

// Handlers for Thinker responses.
void Game::sanityCheckBadRsp(const char *context) const
{
    // The engine should not emit anything when it isn't pondering *and*
    // it is not the engine's turn.
    if (!ponder && !engineControl[savedBoard.Turn()])
    {
        LOG_EMERG("unexpected response received (%s)\n", context);
        assert(0);
    }
}

void Game::onThinkerRspDraw(Thinker &th, MoveT move)
{
    class Board &board = savedBoard; // shorthand
    
    sanityCheckBadRsp(__func__);
    if (!engineControl[board.Turn()])
    {
        // Decided (or forced) to draw while pondering.  Ignore and let the
        // player make their move.
        if (!autoPlayEngineMoves)
            gUI->notifyMove(this, move); // (hacky) UCI wants a bestmove.
        gUI->notifyReady();
        return;
    }

    bool wasRunning = Stop();
    if (move != MoveNone && autoPlayEngineMoves)
    {
        if (wasRunning)
        {
            // We must do this manually since we are Stopped.
            clocks[board.Turn()].ApplyIncrement(board.Ply());
        }
        MakeMove(move);
        gUI->positionRefresh(board.Position());
        lastRefreshedPosition = board.Position();
    }
    done = true; // must happen after MakeMove()
    gUI->notifyReady();

    if (board.IsDrawFiftyMove())
        gUI->notifyDraw(this, "fifty-move rule", &move);
    else if (board.IsDrawThreefoldRepetition())
        gUI->notifyDraw(this, "threefold repetition", &move);
    else
        assert(0);
    if (wasRunning)
        Go(); // resets the state, but should not get far since done == true
}

void Game::onThinkerRspMove(Thinker &th, MoveT move)
{
    sanityCheckBadRsp(__func__);
    if (!engineControl[savedBoard.Turn()])
    {
        // Decided (or forced) to move while pondering.  Ignore and let the
        // player make their move.
        if (!autoPlayEngineMoves)
            gUI->notifyMove(this, move); // (hacky) UCI wants a bestmove.
        gUI->notifyReady();
        return;
    }

    gUI->notifyMove(this, move);
    if (autoPlayEngineMoves)
        MakeMove(move);
    LogFlush();
}

void Game::onThinkerRspResign(Thinker &th)
{
    sanityCheckBadRsp(__func__);
    int turn =
        // Computer resigned its position while pondering?
        !engineControl[savedBoard.Turn()] ?
        savedBoard.Turn() ^ 1 : // (yes)
        savedBoard.Turn();      // (no)

    stopClocks();
    done = true;
    gUI->notifyReady();
    LOG_DEBUG("%d resigns\n", turn);
    LogFlush();
    gUI->notifyResign(this, turn);
}

void Game::onThinkerRspNotifyStats(Thinker &th, const ThinkerStatsT &stats)
{
    sanityCheckBadRsp(__func__);
    gUI->notifyComputerStats(this, &stats);
}

void Game::onThinkerRspNotifyPv(Thinker &th, const RspPvArgsT &pvArgs)
{
    sanityCheckBadRsp(__func__);
    gUI->notifyPV(this, &pvArgs);
}

void Game::setThinkerRspHandler(Thinker &th)
{
    Thinker::RspHandlerT rspHandler;
    rspHandler.Draw = std::bind(&Game::onThinkerRspDraw, this,
                                std::placeholders::_1, std::placeholders::_2);
    rspHandler.Move = std::bind(&Game::onThinkerRspMove, this,
                                std::placeholders::_1, std::placeholders::_2);
    rspHandler.Resign = std::bind(&Game::onThinkerRspResign, this,
                                  std::placeholders::_1);
    rspHandler.NotifyStats =
        std::bind(&Game::onThinkerRspNotifyStats, this,
                  std::placeholders::_1, std::placeholders::_2);
    rspHandler.NotifyPv =
        std::bind(&Game::onThinkerRspNotifyPv, this,
                  std::placeholders::_1, std::placeholders::_2);
    // .SearchDone is left unset for now; if it is actually called we should
    // terminate with a badfunc exception.
    th.SetRspHandler(rspHandler);
}

int Game::Save()
{
    return sgame.Save();
}

int Game::Restore()
{
    if (sgame.Restore() < 0)
        return -1;
    bool wasRunning = Stop();
    done = false;
    // We can't call ::NewGame() here since that clobbers savegame info.
    // We can't call ::SetBoard(), since that assumes the SaveGame and the
    //  savedBoard are in sync.
    // And we can't call ::GotoPly(), since that assumes the Thinker is in sync.

    // Could goto current ply instead of numPlies.  I am assuming
    // here that the user is absent-minded and might forget (or might not
    // know) the current ply is persistent.
    sgame.GotoPly(LastPly(), &savedBoard, clocks);
    th->CmdNewGame(); // Reset thinkers, etc.
    th->CmdSetBoard(savedBoard);
    if (wasRunning)
        Go();
    return 0;
}
