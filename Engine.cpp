//--------------------------------------------------------------------------
//                Engine.cpp - a top-level engine control API
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#include <limits.h>     // INT_MAX

#include "Engine.h"
#include "HistoryWindow.h"
#include "Variant.h"

/* Communication between the main program ("Engine" interface) and the Thinker
   thread proper is done via 2 EventQueues (Thinker's cmdQueue and
   Engine's rspQueue).

   The Engine may post:
   OnCmdThink
   OnCmdPonder
   OnCmdMoveNow # also used for bail, where the move may be discarded.
   OnCmdSearch

   The Thinker may post:
   RspDraw<move, MoveNone if none>
   RspMove<move>
   RspResign
   RspStats<EngineStatsT>
   RspPv<EnginePvArgsT>
   RspSearchDone<EngineSearchDoneArgsT>
*/   

void Engine::onMaxDepthChanged(const Config::SpinItem &item)
{
    if (!th->IsRootThinker())
        return;
    volatile int &maxLevel = th->SharedContext().maxLevel; // shorthand
    maxLevel = item.Value() - 1;
    if (maxLevel != Thinker::DepthNoLimit && th->Context().maxDepth > maxLevel)
        CmdMoveNow();
}
void Engine::onMaxNodesChanged(const Config::SpinItem &item)
{
    if (!th->IsRootThinker())
        return;
    th->SharedContext().maxNodes = item.Value();
    // The engine itself should shortly notice that it has exceeded
    //  maxNodes (if applicable), and return.
}
void Engine::onRandomMovesChanged(const Config::CheckboxItem &item)
{
    if (!th->IsRootThinker())
        return;
    th->SharedContext().randomMoves = item.Value();
}
void Engine::onCanResignChanged(const Config::CheckboxItem &item)
{
    if (!th->IsRootThinker())
        return;
    th->SharedContext().canResign = item.Value();
}
void Engine::onHistoryWindowChanged(const Config::SpinItem &item)
{
    if (!th->IsRootThinker())
        return;
    gHistoryWindow.SetWindow(item.Value());
}

void Engine::restoreState(Thinker::State state)
{
    Thinker::ContextT &context = th->Context(); // shorthand

    switch (state) // continue where we were interrupted, if applicable.
    {
        case Thinker::State::Pondering:
            CmdPonder(context.mvlist);
            break;
        case Thinker::State::Thinking:
            CmdThink(context.clock, context.mvlist);
            break;
        case Thinker::State::Searching:
            CmdSearch(context.searchArgs.alpha, context.searchArgs.beta,
                      context.searchArgs.move, context.depth, context.maxDepth);
            break;
        default:
            break;
    }
}

void Engine::onMaxMemoryChanged(const Config::SpinItem &item)
{
    if (!th->IsRootThinker())
        return;
    Thinker::State origState = state;
    if (IsBusy())
        CmdBail();
    th->SharedContext().transTable.Reset(uint64(item.Value()) * 1024 * 1024);
    restoreState(origState);
}

void Engine::onMaxThreadsChanged(const Config::SpinItem &item)
{
    if (!th->IsRootThinker())
        return;
    Thinker::State origState = state;
    if (IsBusy())
        CmdBail();
    th->SharedContext().maxThreads = item.Value();
    SearchersSetNumThreads(item.Value());
    restoreState(origState);
}

// ctor.
Engine::Engine() :
    rspQueue(std::unique_ptr<Pollable>(new Pollable)),
    state(Thinker::State::Idle), moveNowState(MoveNowState::IdleOrBusy)
{
    Thinker::RspHandlerT tmp;
    tmp.Draw = std::bind(&Engine::onRspDraw, this, std::placeholders::_1);
    tmp.Move = std::bind(&Engine::onRspMove, this, std::placeholders::_1);
    tmp.Resign = std::bind(&Engine::onRspResign, this);
    tmp.NotifyStats = std::bind(&Engine::onRspNotifyStats, this,
                                std::placeholders::_1);
    tmp.NotifyPv = std::bind(&Engine::onRspNotifyPv, this,
                             std::placeholders::_1);
    tmp.SearchDone = std::bind(&Engine::onRspSearchDone, this,
                               std::placeholders::_1);
    
    th.reset(new Thinker(rspQueue, tmp));
    
    // Register config callbacks.
    Config().Register(
        Config::SpinItem(Config::MaxDepthSpin, Config::MaxDepthDescription,
                         0, 0, INT_MAX,
                         std::bind(&Engine::onMaxDepthChanged, this,
                                   std::placeholders::_1)));
    Config().Register(
        Config::SpinItem(Config::MaxNodesSpin, Config::MaxNodesDescription,
                         0, 0, INT_MAX,
                         std::bind(&Engine::onMaxNodesChanged, this,
                                   std::placeholders::_1)));
    Config().Register(
        Config::CheckboxItem(Config::RandomMovesCheckbox,
                             Config::RandomMovesDescription,
                             false,
                             std::bind(&Engine::onRandomMovesChanged, this,
                                       std::placeholders::_1)));
    Config().Register(
        Config::CheckboxItem(Config::CanResignCheckbox,
                             Config::CanResignDescription,
                             true,
                             std::bind(&Engine::onCanResignChanged, this,
                                       std::placeholders::_1)));
    Config().Register(
        Config::SpinItem(Config::HistoryWindowSpin,
                         Config::HistoryWindowDescription,
                         0, gHistoryWindow.Window(), INT_MAX,
                         std::bind(&Engine::onHistoryWindowChanged, this,
                                   std::placeholders::_1)));
    Config().Register(
        Config::SpinItem(Config::MaxMemorySpin, Config::MaxMemoryDescription,
                         0, (th->SharedContext().transTable.DefaultSize() /
                             (1024 * 1024)),
                         (th->SharedContext().transTable.MaxSize() /
                          (1024 * 1024)),
                         std::bind(&Engine::onMaxMemoryChanged, this,
                                   std::placeholders::_1)));
    Config().Register(
        Config::SpinItem(Config::MaxThreadsSpin, Config::MaxThreadsDescription,
                         1, th->SharedContext().maxThreads,
                         th->SharedContext().maxThreads,
                         std::bind(&Engine::onMaxThreadsChanged, this,
                                   std::placeholders::_1)));
}

// dtor
Engine::~Engine()
{
    th = nullptr;
}

void Engine::CmdNewGame()
{
    Thinker::ContextT &context = th->Context();
    Thinker::SharedContextT &sharedContext = th->SharedContext();

    CmdBail();
    if (th->IsRootThinker())
    {
        sharedContext.transTable.Reset();
        gHistoryWindow.Clear();
        sharedContext.pv.Clear();
        sharedContext.gameCount++;
        // This enables a bit of lazy initialization.  If maxThreads is
        //  configured down before we NewGame(), we won't need to create the
        //  extra threads.
        SearchersSetNumThreads(sharedContext.maxThreads);
    }
    if (!context.board.SetPosition(Variant::Current()->StartingPosition()))
        assert(0);
}

void Engine::CmdSetBoard(const Board &board)
{
    CmdBail(); // If we were previously thinking, just start over.
    
    Thinker::ContextT &context = th->Context();
    Thinker::SharedContextT &sharedContext = th->SharedContext();

    // Make a best effort at PV tracking (in case the boards are similar).  
    if (th->IsRootThinker())
    {
        sharedContext.pv.Rewind(context.board.Ply() - board.Ply());
        // If the ply happened to be the same, we still want to start the search
        //  over.
        sharedContext.pv.ResetSearchStartLevel();
    }
    context.board = board;
}

void Engine::CmdMakeMove(MoveT move)
{
    CmdBail(); // If we were previously thinking, just start over.

    Thinker::ContextT &context = th->Context();
    Thinker::SharedContextT &sharedContext = th->SharedContext();

    context.board.MakeMove(move);
    if (th->IsRootThinker())
        sharedContext.pv.Decrement(move);
}

void Engine::CmdUnmakeMove()
{
    CmdBail(); // If we were previously thinking, just start over.

    Thinker::ContextT &context = th->Context();
    Thinker::SharedContextT &sharedContext = th->SharedContext();

    context.board.UnmakeMove();
    if (th->IsRootThinker())
        sharedContext.pv.Rewind(1);
}

void Engine::doThink(bool isPonder, const MoveList *mvlist)
{
    CmdBail(); // If we were previously thinking, just start over.

    Thinker::ContextT &context = th->Context();

    // Copy over search args.
    if (mvlist != NULL)
        context.mvlist = *mvlist;
    else
        context.mvlist.DeleteAllMoves();
    
    if (!isPonder)
    {
        state = Thinker::State::Thinking;
        th->PostCmd(std::bind(&Thinker::OnCmdThink, th.get()));
    }
    else
    {
        state = Thinker::State::Pondering;
        th->PostCmd(std::bind(&Thinker::OnCmdPonder, th.get()));
    }
}

void Engine::CmdThink(const Clock &myClock, const MoveList &mvlist)
{
    Thinker::ContextT &context = th->Context();

    context.clock = myClock;
    // Because of the way we may stop and restart the Thinker (see:
    //  restoreState()), we should never actually stop this clock.
    context.clock.Start();
    doThink(false, &mvlist);
}

void Engine::CmdThink(const Clock &myClock)
{
    Thinker::ContextT &context = th->Context();

    context.clock = myClock;
    context.clock.Start();
    doThink(false, nullptr);
}

void Engine::CmdPonder(const MoveList &mvlist)
{
    doThink(true, &mvlist);
}

void Engine::CmdPonder()
{
    doThink(true, nullptr);
}

void Engine::CmdSearch(int alpha, int beta, MoveT move, int curDepth,
                       int maxDepth)
{
    // If we were previously thinking, just start over.
    CmdBail();

    Thinker::ContextT &context = th->Context();

    // Copy over search args.
    context.searchArgs.alpha = alpha;
    context.searchArgs.beta = beta;
    context.searchArgs.move = move;
    context.depth = curDepth;
    context.maxDepth = maxDepth;
    state = Thinker::State::Searching;
    th->PostCmd(std::bind(&Thinker::OnCmdSearch, th.get()));
}

// Force the computer to move in the very near future.  This is asynchronous.
// For a synchronous analogue, see Game->MoveNow().
void Engine::CmdMoveNow()
{
    if (IsBusy() && moveNowState == MoveNowState::IdleOrBusy)
    {
        moveNowState = MoveNowState::MoveNowRequested;
        th->PostCmd(std::bind(&Thinker::OnCmdMoveNow, th.get()));
    }
}

void Engine::CmdBail()
{
    if (IsBusy())
    {
        CmdMoveNow();
        moveNowState = MoveNowState::BailRequested;

        // Wait for, and discard, the computer's move.
        do
        {
            ProcessOneRsp();
        } while (moveNowState != MoveNowState::IdleOrBusy);
    }
    assert(!IsBusy());
}

void Engine::SetRspHandler(const RspHandlerT &rspHandler)
{
    this->rspHandler = rspHandler;
}

void Engine::onRspNotifyStats(const EngineStatsT &stats)
{
    if (moveNowState != MoveNowState::BailRequested)
        rspHandler.NotifyStats(*this, stats);
}

void Engine::onRspNotifyPv(const EnginePvArgsT &pv)
{
    if (moveNowState != MoveNowState::BailRequested)
        rspHandler.NotifyPv(*this, pv);
}

void Engine::onRspDraw(MoveT move)
{
    bool bailing = moveNowState == MoveNowState::BailRequested;
    moveToIdleState();
    if (!bailing)
        rspHandler.Draw(*this, move);
}

void Engine::onRspMove(MoveT move)
{
    bool bailing = moveNowState == MoveNowState::BailRequested;
    moveToIdleState();
    if (!bailing)
        rspHandler.Move(*this, move);
}

void Engine::onRspResign()
{
    bool bailing = moveNowState == MoveNowState::BailRequested;
    moveToIdleState();
    if (!bailing)
        rspHandler.Resign(*this);
}

void Engine::onRspSearchDone(const EngineSearchDoneArgsT &args)
{
    bool bailing = moveNowState == MoveNowState::BailRequested;
    moveToIdleState();
    if (!bailing)
        rspHandler.SearchDone(*this, args);
}
