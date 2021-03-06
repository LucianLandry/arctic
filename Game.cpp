//--------------------------------------------------------------------------
//               Game.cpp - current game and associated state.
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#include <assert.h>

#include "Game.h"
#include "log.h"
#include "MoveList.h"
#include "ui.h"

Game::Game(Engine *eng) : eng(eng)
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

    setEngineRspHandler(*eng);
}

// Some game state changed; handle it.
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
        gUI->statusDraw();

        MoveList mvlist;
        board.GenerateLegalMoves(mvlist, false);
    
        if (board.IsDrawInsufficientMaterial())
        {
            stopClocks();
            gUI->notifyDraw("insufficient material", NULL);
            done = true;
        }
        else if (!mvlist.NumMoves())
        {
            stopClocks();
            if (!board.IsInCheck())
                gUI->notifyDraw("stalemate", NULL);
            else
                gUI->notifyCheckmated(turn);
            done = true;
        }
    }
    
    if (state == State::Stopped ||
        (!done && eng->IsThinking() && engineControl[turn]) ||
        (!done && eng->IsPondering() && ponder &&
         !engineControl[turn] && engineControl[turn ^ 1]))
    {
        return; // No change in thinking necessary.  Do not restart think cycle.
    }

    // Stop anything going on.
    eng->CmdBail();
    
    if (!done && engineControl[turn])
    {
        // Computer needs to make next move; let it do so.
        gUI->notifyThinking();
        if (searchList.NumMoves())
            eng->CmdThink(clocks[turn], searchList);
        else
            eng->CmdThink(clocks[turn]);            
    }
    else if (!done && engineControl[turn ^ 1] && ponder)
    {
        // Computer is playing other side (only) and is allowed to ponder.
        // Do so.
        gUI->notifyPonder();
        if (searchList.NumMoves())
            eng->CmdPonder(searchList);
        else
            eng->CmdPonder();
    }
    else
    {
        // We should not be thinking at all.
        gUI->notifyReady();
    }
}

void Game::makeMove(MoveT move, bool moveEngines)
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

    if (moveEngines)
        eng->CmdMakeMove(move);
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
    eng->CmdNewGame();
    eng->CmdSetBoard(savedBoard);
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
            eng->CmdUnmakeMove();
    }
    else
    {
        // Need to move forward.  I guess I prefer querying a Board over a
        //  SaveGame to get the proper move.
        for (int i = origPly; i < ply; i++)
            eng->CmdMakeMove(savedBoard.MoveAt(i));
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
            eng->CmdUnmakeMove();
        for (int i = lastCommonPly; i < other.Ply(); i++)
            MakeMove(other.MoveAt(i));
    }
    else
    {
        // Do it the 'hard' way.  We prefer to set the Engine in one swoop
        //  (as opposed to via SetBoard + MakeMove()) to (kindof) preserve the
        //  PV better and to minimize traffic.
        eng->CmdSetBoard(other);
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
    return eng->Config();
}

bool Game::Stop()
{
    if (state == State::Stopped)
        return false;
    state = State::Stopped;
    eng->CmdBail();
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
    while (eng->IsBusy())
        eng->ProcessOneRsp();
}

void Game::MoveNow()
{
    eng->CmdMoveNow();
    WaitForEngineIdle();
}

// Handlers for Engine responses.
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

void Game::onEngineRspDraw(Engine &eng, MoveT move)
{
    class Board &board = savedBoard; // shorthand
    
    sanityCheckBadRsp(__func__);
    if (!engineControl[board.Turn()])
    {
        // Decided (or forced) to draw while pondering.  Ignore and let the
        // player make their move.
        if (!autoPlayEngineMoves)
            gUI->notifyMove(move); // (hacky) UCI wants a bestmove.
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
        gUI->notifyDraw("fifty-move rule", &move);
    else if (board.IsDrawThreefoldRepetition())
        gUI->notifyDraw("threefold repetition", &move);
    else
        assert(0);
    if (wasRunning)
        Go(); // resets the state, but should not get far since done == true
}

void Game::onEngineRspMove(Engine &eng, MoveT move)
{
    sanityCheckBadRsp(__func__);
    if (!engineControl[savedBoard.Turn()])
    {
        // Decided (or forced) to move while pondering.  Ignore and let the
        // player make their move.
        if (!autoPlayEngineMoves)
            gUI->notifyMove(move); // (hacky) UCI wants a bestmove.
        gUI->notifyReady();
        return;
    }

    gUI->notifyMove(move);
    if (autoPlayEngineMoves)
        MakeMove(move);
    LogFlush();
}

void Game::onEngineRspResign(Engine &eng)
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
    gUI->notifyResign(turn);
}

void Game::onEngineRspNotifyStats(Engine &eng, const EngineStatsT &stats)
{
    sanityCheckBadRsp(__func__);
    gUI->notifyComputerStats(&stats);
}

void Game::onEngineRspNotifyPv(Engine &eng, const EnginePvArgsT &pvArgs)
{
    sanityCheckBadRsp(__func__);
    gUI->notifyPV(&pvArgs);
}

void Game::setEngineRspHandler(Engine &eng)
{
    Engine::RspHandlerT rspHandler;
    rspHandler.Draw = std::bind(&Game::onEngineRspDraw, this,
                                std::placeholders::_1, std::placeholders::_2);
    rspHandler.Move = std::bind(&Game::onEngineRspMove, this,
                                std::placeholders::_1, std::placeholders::_2);
    rspHandler.Resign = std::bind(&Game::onEngineRspResign, this,
                                  std::placeholders::_1);
    rspHandler.NotifyStats =
        std::bind(&Game::onEngineRspNotifyStats, this,
                  std::placeholders::_1, std::placeholders::_2);
    rspHandler.NotifyPv =
        std::bind(&Game::onEngineRspNotifyPv, this,
                  std::placeholders::_1, std::placeholders::_2);
    // .SearchDone is left unset for now; if it is actually called we should
    // terminate with a badfunc exception.
    eng.SetRspHandler(rspHandler);
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
    eng->CmdNewGame(); // Reset engines
    eng->CmdSetBoard(savedBoard);
    if (wasRunning)
        Go();
    return 0;
}
