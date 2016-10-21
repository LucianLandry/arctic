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

#include <limits.h>
#include <sys/socket.h>
#include <type_traits>  // std::is_trivially_copyable<>
#include <unistd.h>     // close(2)

#include "Engine.h"
#include "HistoryWindow.h"
#include "TransTable.h"
#include "Variant.h"
#include "Workarounds.h"

/* Okay, here's the lowdown.
   Communication between the main program and engine thread is done via 2
   sockets, via Thinker::Messages.

   Message format is:
   msglen (1 byte)
   msg ('msglen' bytes)

   Possible Messages the UI layer can send:
   CmdThink
   CmdPonder
   CmdMoveNow # also used for bail, where the move may be discarded.

   Possible Messages the computer can send:
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

void Engine::restoreState(Engine::State state)
{
    Thinker::ContextT &context = th->Context(); // shorthand

    switch (state) // continue where we were interrupted, if applicable.
    {
        case State::Pondering:
            CmdPonder(context.mvlist);
            break;
        case State::Thinking:
            CmdThink(context.clock, context.mvlist);
            break;
        case State::Searching:
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
    State origState = state;
    if (IsBusy())
        CmdBail();
    gTransTable.Reset(uint64(item.Value()) * 1024 * 1024);
    restoreState(origState);
}

void Engine::onMaxThreadsChanged(const Config::SpinItem &item)
{
    if (!th->IsRootThinker())
        return;
    State origState = state;
    if (IsBusy())
        CmdBail();
    th->SharedContext().maxThreads = item.Value();
    SearchersSetNumThreads(item.Value());
    restoreState(origState);
}

// ctor.
Engine::Engine()
{
    int err;
    int socks[2];

    err = socketpair(AF_UNIX, SOCK_STREAM, 0, socks);
    assert(err == 0);

    masterSock = socks[0];
    state = State::Idle;

    th.reset(new Thinker(socks[1]));
    
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
                         0, gTransTable.DefaultSize() / (1024 * 1024),
                         gTransTable.MaxSize() / (1024 * 1024),
                         std::bind(&Engine::onMaxMemoryChanged, this,
                                   std::placeholders::_1)));
    Config().Register(
        Config::SpinItem(Config::MaxThreadsSpin, Config::MaxThreadsDescription,
                         1, th->SharedContext().maxThreads,
                         th->SharedContext().maxThreads,
                         std::bind(&Engine::onMaxThreadsChanged, this,
                                   std::placeholders::_1)));
    
    goalTime = CLOCK_TIME_INFINITE;
}

// dtor
Engine::~Engine()
{
    close(masterSock);
    th = nullptr;
}

void Engine::sendCmd(Thinker::Message cmd) const
{
    th->sendMessage(masterSock, cmd, nullptr, 0);
}

Thinker::Message Engine::recvRsp(void *args, int argsLen)
{
    Thinker::Message msg = th->recvMessage(masterSock, args, argsLen);

    assert(msg == Thinker::Message::RspStats ||
           msg == Thinker::Message::RspPv ||
           Thinker::IsFinalResponse(msg));
    
    if (Thinker::IsFinalResponse(msg))
    {
        state = State::Idle;
        th->Context().moveNow = false;
    }
    return msg;
}

void Engine::CmdNewGame()
{
    Thinker::ContextT &context = th->Context();
    Thinker::SharedContextT &sharedContext = th->SharedContext();

    CmdBail();
    if (th->IsRootThinker())
    {
        gTransTable.Reset();
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

void Engine::doThink(Thinker::Message cmd, const MoveList *mvlist)
{
    CmdBail(); // If we were previously thinking, just start over.

    Thinker::ContextT &context = th->Context();

    // Copy over search args.
    if (mvlist != NULL)
        context.mvlist = *mvlist;
    else
        context.mvlist.DeleteAllMoves();
    
    if (cmd == Thinker::Message::CmdThink)
    {
        state = State::Thinking;
    }
    else
    {
        assert(cmd == Thinker::Message::CmdPonder);
        state = State::Pondering;
    }
    sendCmd(cmd);
}

// Expected number of moves in a game.  Actually a little lower, as this is
// biased toward initial moves.  The idea is that we would rather have less
// time at the end to think about a won position than more time to think about
// a lost position.
#define GAME_NUM_MOVES 40
// Minimum time we want left on the clock, presumably to compensate for
// lag, in usec (however, normally we rely on timeseal to compensate for
// network lag)
#define MIN_TIME 500000
// The clock doesn't run on the first move in an ICS game.
// But as a courtesy, refuse to think over 5 seconds (unless our clock has
// infinite time anyway)
#define ICS_FIRSTMOVE_LIMIT 5000000
void Engine::calcGoalTime(const Clock &myClock)
{
    const Board &board = th->Context().board; // shorthand.
    int ply = board.Ply();
    bigtime_t myTime, calcTime, altCalcTime, myInc, safeTime,
        myPerMoveLimit, safeMoveLimit;
    int myTimeControlPeriod, numMovesToNextTimeControl;
    int numIncs;
    
    myTime = myClock.Time();
    myPerMoveLimit = myClock.PerMoveLimit();
    myTimeControlPeriod = myClock.TimeControlPeriod();
    numMovesToNextTimeControl = myClock.NumMovesToNextTimeControl();
    myInc = myClock.Increment();

    safeMoveLimit =
        myPerMoveLimit == CLOCK_TIME_INFINITE ? CLOCK_TIME_INFINITE :
        myPerMoveLimit - MIN_TIME;      

    if (myClock.IsFirstMoveFree() && ply < NUM_PLAYERS)
        safeMoveLimit = MIN(safeMoveLimit, ICS_FIRSTMOVE_LIMIT);

    safeMoveLimit = MAX(safeMoveLimit, 0);

    // Degenerate case.
    if (myClock.IsInfinite())
    {
        goalTime =
            myPerMoveLimit == CLOCK_TIME_INFINITE ?
            CLOCK_TIME_INFINITE :
            MIN_TIME;
        return;
    }

    safeTime = MAX(myTime - MIN_TIME, 0);

    // 'calcTime' is the amount of time we want to think.
    calcTime = safeTime / GAME_NUM_MOVES;

    if (myTimeControlPeriod || numMovesToNextTimeControl)
    {
        // Anticipate the additional time we will possess to make our
        // GAME_NUM_MOVES moves due to time-control increments.
        if (myTimeControlPeriod)
        {
            numMovesToNextTimeControl =
                myTimeControlPeriod - ((ply >> 1) % myTimeControlPeriod);
        }
        numIncs = GAME_NUM_MOVES <= numMovesToNextTimeControl ? 0 :
            1 + (myTimeControlPeriod ?
                 ((GAME_NUM_MOVES - numMovesToNextTimeControl - 1) /
                  myTimeControlPeriod) :
                 0);

        calcTime += (myClock.StartTime() * numIncs) / GAME_NUM_MOVES;
        // However, say we have :30 on the clock, 10 moves to make, and a one-
        // minute increment every two moves.  We want to burn only :15.
        altCalcTime = safeTime / MIN(GAME_NUM_MOVES,
                                     numMovesToNextTimeControl);
        calcTime = MIN(calcTime, altCalcTime);
    }

    // Anticipate the additional time we will possess to make our
    // GAME_NUM_MOVES moves due to increments.
    if (myInc)
    {
        numIncs = GAME_NUM_MOVES - 1;
        calcTime += (myInc * numIncs) / GAME_NUM_MOVES;
        // Fix cases like 10 second start time, 22 second increment
        calcTime = MIN(calcTime, safeTime);
    }

    // Do not think over any per-move limit.
    if (safeMoveLimit != CLOCK_TIME_INFINITE)
        calcTime = MIN(calcTime, safeMoveLimit);

    // Refuse to think for a "negative" time.
    calcTime = MAX(calcTime, 0);

    goalTime = myTime - calcTime;
}

void Engine::CmdThink(const Clock &myClock, const MoveList &mvlist)
{
    Thinker::ContextT &context = th->Context();

    context.clock = myClock;
    context.clock.Start();
    calcGoalTime(myClock);
    doThink(Thinker::Message::CmdThink, &mvlist);
}

void Engine::CmdThink(const Clock &myClock)
{
    Thinker::ContextT &context = th->Context();

    context.clock = myClock;
    context.clock.Start();
    calcGoalTime(myClock);
    doThink(Thinker::Message::CmdThink, nullptr);
}

void Engine::CmdPonder(const MoveList &mvlist)
{
    doThink(Thinker::Message::CmdPonder, &mvlist);
}

void Engine::CmdPonder()
{
    doThink(Thinker::Message::CmdPonder, nullptr);
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
    state = State::Searching;

    sendCmd(Thinker::Message::CmdSearch);
}

// Force the computer to move in the very near future.  This is asynchronous.
// For a synchronous analogue, see Game->MoveNow().
void Engine::CmdMoveNow()
{
    if (IsBusy())
    {
        th->Context().moveNow = true;
        // I do not think this is necessary until we try to support clustering.
        // sendCmd(Thinker::Message::CmdMoveNow);
    }
}

void Engine::CmdBail()
{
    if (IsBusy())
    {
        CmdMoveNow();

        // Wait for, and discard, the computer's move.
        Thinker::Message rsp;
        do
        {
            rsp = recvRsp(nullptr, 0);
        } while (!Thinker::IsFinalResponse(rsp));
    }
    assert(!IsBusy());
}

void Engine::SetRspHandler(const RspHandlerT &rspHandler)
{
    this->rspHandler = rspHandler;
}

void Engine::ProcessOneRsp()
{
    alignas(sizeof(void *))
        char rspBuf[MAX4(sizeof(EngineStatsT), sizeof(EnginePvArgsT),
                         sizeof(MoveT), sizeof(EngineSearchDoneArgsT))];
    EngineStatsT *stats = (EngineStatsT *)rspBuf;
    EnginePvArgsT *pvArgs = (EnginePvArgsT *)rspBuf;
    MoveT *move = (MoveT *)rspBuf;
    EngineSearchDoneArgsT *searchDoneArgs = (EngineSearchDoneArgsT *)rspBuf;

    static_assert(std::is_trivially_copyable<EngineStatsT>::value,
                  "EngineStatsT must be trivially copyable to copy it "
                  "through a socket!");
    static_assert(std::is_trivially_copyable<MoveT>::value,
                  "MoveT must be trivially copyable to copy it "
                  "through a socket!");
#if 0 // Cannot check these, since their member 'SearchPv' has an optimized
      // (but non-trivial) copy constructor.
    static_assert(std::is_trivially_copyable<EnginePvArgsT>::value,
                  "EnginePvStatsT must be trivially copyable to copy it "
                  "through a socket!");
    static_assert(std::is_trivially_copyable<EngineSearchDoneArgsT>::value,
                  "EngineSearchDoneArgsT must be trivially copyable to copy it "
                  "through a socket!");
#endif

    Thinker::Message rsp = recvRsp(&rspBuf, sizeof(rspBuf));

    switch (rsp)
    {
        case Thinker::Message::RspDraw:
            rspHandler.Draw(*this, *move);
            break;
        case Thinker::Message::RspMove:
            rspHandler.Move(*this, *move);
            break;
        case Thinker::Message::RspResign:
            rspHandler.Resign(*this);
            break;
        case Thinker::Message::RspStats:
            rspHandler.NotifyStats(*this, *stats);
            break;
        case Thinker::Message::RspPv:
            rspHandler.NotifyPv(*this, *pvArgs);
            break;
        case Thinker::Message::RspSearchDone:
            rspHandler.SearchDone(*this, *searchDoneArgs);
            break;
        default:
            LOG_EMERG("%s: unknown response type %d\n", __func__, int(rsp));
            assert(0);
            break;
    }
}
