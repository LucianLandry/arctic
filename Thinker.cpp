//--------------------------------------------------------------------------
//            Thinker.cpp - Thinker thread (thinks, ponders, etc.)
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#include <poll.h>

#include "aSystem.h"    // SystemTotalProcessors()
#include "comp.h"
#include "Engine.h"
#include "Thinker.h"

Thinker *Thinker::rootThinker = nullptr;

// An internal global resource.  Might be split later if we need sub-searchers.
struct SearcherGroupT
{
    std::vector<Engine *> searchers;
    // pfds[0] now refers to the Thinker's cmdqueue, so all other pfds'
    //  indices are offset by +1 from their respective searchers.
    std::vector<struct pollfd> pfds;
    int numSearching; // number of currently searching searchers

    // Normally this vector is empty, but if we lower the core count, the extra
    //  searchers are kept in a thread pool here.
    std::vector<Engine *> freePool;
};

static SearcherGroupT gSG;

// Returns time (from now, ie relative timeout) that we want to move at.  May
//  be CLOCK_TIME_INFINITE, in which case we have no timeout.
// This is a bit bizarre compared to just returning the absolute time we
//  want to move at, but it helps us with displaying ticks, and time
//  management should be internal in the future anyway.
static bigtime_t calcGoalTime(const Board &board, const Clock &myClock)
{
    // Expected number of moves in a game.  Actually a little lower, as this is
    //  biased toward initial moves.  The idea is that we would rather have less
    //  time at the end to think about a won position than more time to think
    //  about a lost position.
    static const int kNumGameMoves = 40;

    // Minimum time we want left on the clock, presumably to compensate for
    //  lag, in usec (however, normally we rely on timeseal to compensate for
    //  network lag)
    static const bigtime_t kMinTime = 500000;

    // The clock doesn't run on the first move in an ICS game.
    // But as a courtesy, refuse to think over 5 seconds (unless our clock has
    // infinite time anyway)
    static const bigtime_t kIcsFirstMoveLimit = 5000000;

    int ply = board.Ply();
    bigtime_t myTime, calcTime, altCalcTime, myInc, safeTime, safeMoveLimit;
    int myTimeControlPeriod, numMovesToNextTimeControl;
    int numIncs;

    safeMoveLimit =
        myClock.PerMoveLimit() == CLOCK_TIME_INFINITE ? CLOCK_TIME_INFINITE :
        myClock.PerMoveTime() - kMinTime;      

    if (myClock.IsFirstMoveFree() && ply < NUM_PLAYERS)
        safeMoveLimit = MIN(safeMoveLimit, kIcsFirstMoveLimit);

    safeMoveLimit = MAX(safeMoveLimit, 0);
    
    // Degenerate case.
    if (myClock.IsInfinite())
        return safeMoveLimit;
    
    myTime = myClock.Time();
    myTimeControlPeriod = myClock.TimeControlPeriod();
    numMovesToNextTimeControl = myClock.NumMovesToNextTimeControl();
    myInc = myClock.Increment();

    safeTime = MAX(myTime - kMinTime, 0);

    // 'calcTime' is the amount of time we want to think.
    calcTime = safeTime / kNumGameMoves;

    if (myTimeControlPeriod || numMovesToNextTimeControl)
    {
        // Anticipate the additional time we will possess to make our
        // kNumGameMoves moves due to time-control increments.
        if (myTimeControlPeriod)
        {
            numMovesToNextTimeControl =
                myTimeControlPeriod - ((ply >> 1) % myTimeControlPeriod);
        }
        numIncs = kNumGameMoves <= numMovesToNextTimeControl ? 0 :
            1 + (myTimeControlPeriod ?
                 ((kNumGameMoves - numMovesToNextTimeControl - 1) /
                  myTimeControlPeriod) :
                 0);

        calcTime += (myClock.StartTime() * numIncs) / kNumGameMoves;
        // However, say we have :30 on the clock, 10 moves to make, and a one-
        // minute increment every two moves.  We want to burn only :15.
        altCalcTime = safeTime / MIN(kNumGameMoves,
                                     numMovesToNextTimeControl);
        calcTime = MIN(calcTime, altCalcTime);
    }

    // Anticipate the additional time we will possess to make our
    // kNumGameMoves moves due to increments.
    if (myInc)
    {
        numIncs = kNumGameMoves - 1;
        calcTime += (myInc * numIncs) / kNumGameMoves;
        // Fix cases like 10 second start time, 22 second increment
        calcTime = MIN(calcTime, safeTime);
    }

    // Do not think over any per-move limit.
    if (safeMoveLimit != CLOCK_TIME_INFINITE)
        calcTime = MIN(calcTime, safeMoveLimit);

    // Refuse to think for a "negative" time.
    return MAX(calcTime, 0);
}

Thinker::ContextT::ContextT() : maxDepth(0), depth(0)
{
    searchArgs.alpha = Eval::Loss;
    searchArgs.beta = Eval::Win;
    searchArgs.move = MoveNone;
}

Thinker::SharedContextT::SharedContextT() :
    maxLevel(DepthNoLimit), maxNodes(0), randomMoves(false), canResign(true),
    maxThreads(SystemTotalProcessors()), gameCount(0) {}

Thinker::Thinker(EventQueue &rspQueue, const RspHandlerT &handler) :
    cmdQueue(std::unique_ptr<Pollable>(new Pollable)), rspQueue(rspQueue),
    rspHandler(handler), state(State::Idle), epoch(0), moveNow(false)
{
    // Assume the first Thinker created is the rootThinker.
    if (rootThinker == nullptr)
    {
        rootThinker = this;
        sharedContext = std::make_shared<SharedContextT>();
        SearchersSetCmdQueue(cmdQueue);
    }
    else
    {
        sharedContext = rootThinker->sharedContext;
    }
    
    context.board.SetTransTable(&sharedContext->transTable);
    thread = new std::thread(&Thinker::threadFunc, this);
    thread->detach();
    if (IsRootThinker())
        SearchersSetNumThreads(1); // We need at least one searcher thread.
}

// dtor
Thinker::~Thinker()
{
    // We do not support Thinker destruction yet.  To do that, we will need
    //  to:
    // -- not detach this->thread
    // -- implement an internal CmdExit() or something and use it here
    // thread->join()
    assert(0);
    delete thread;
}

void Thinker::RspDraw(MoveT move)
{
    rspQueue.Post(std::bind(rspHandler.Draw, move));
    moveToIdleState();
}

void Thinker::RspMove(MoveT move)
{
    rspQueue.Post(std::bind(rspHandler.Move, move));
    moveToIdleState();
}

void Thinker::RspResign()
{
    rspQueue.Post(rspHandler.Resign);
    moveToIdleState();
}

void Thinker::RspSearchDone(MoveT move, Eval eval, const SearchPv &pv)
{
    EngineSearchDoneArgsT args = {move, eval, pv};
    rspQueue.Post(std::bind(rspHandler.SearchDone, args));
    moveToIdleState();
}

void Thinker::RspNotifyStats(const EngineStatsT &stats) const
{
    rspQueue.Post(std::bind(rspHandler.NotifyStats, stats));
}

void Thinker::RspNotifyPv(const EngineStatsT &stats, const DisplayPv &pv) const
{
    EnginePvArgsT args = { stats, pv };
    if (!args.pv.Sanitize(context.board))
    {
        LogPrint(eLogNormal, "%s: game %d: note: illegal move detected, "
                 "probably zobrist collision\n",
                 __func__, sharedContext->gameCount);
    }

    rspQueue.Post(std::bind(rspHandler.NotifyPv, args));
}

void Thinker::moveToIdleState()
{
    if (state == State::Idle) // sanity check (shouldn't trigger, though)
        return;
    if (state == State::Thinking)
        moveTimer.Stop();
    state = State::Idle;
    moveNow = false;
    epoch++;
}

void Thinker::onMoveTimerExpired(int epoch)
{
    // In the future, this could be more intelligent.
    if (this->epoch == epoch)
        OnCmdMoveNow();
}

void Thinker::OnCmdMoveNow()
{
    // Ignore MoveNows received after our move timer expires.
    if (state == State::Idle)
        return;
    moveNow = true;
    // Perhaps, we should also signal any sub-searchers to move.
}

void Thinker::OnCmdThink()
{
    bigtime_t goalTime = calcGoalTime(context.board, context.clock);
    if (goalTime != CLOCK_TIME_INFINITE)
    {
        moveTimer.SetHandler(std::bind(&Thinker::onMoveTimerExpired, this,
                                       epoch))
            .SetRelativeTimeout(goalTime / 1000)
            .Start();
    }
    state = State::Thinking;
    computermove(this, false);
}

void Thinker::OnCmdPonder()
{
    state = State::Pondering;
    computermove(this, true);
}

void Thinker::OnCmdSearch()
{
    state = State::Searching;

    // If we make the constructor use a memory pool, we should probably
    //  still micro-optimize this.
    SearchPv pv(context.depth + 1);
        
    // Make the appropriate move, bump depth etc.
    Eval eval = tryMove(this, context.searchArgs.move,
                        context.searchArgs.alpha,
                        context.searchArgs.beta, &pv, nullptr);

    RspSearchDone(context.searchArgs.move, eval, pv);
}

void Thinker::threadFunc()
{
    // Run commands as they are issued.
    while (true)
        cmdQueue.RunOne();
}

void Thinker::PostCmd(const EventQueue::HandlerFunc &handler)
{
    cmdQueue.Post(handler);
}

void Thinker::PostCmd(EventQueue::HandlerFunc &&handler)
{
    cmdQueue.Post(handler);    
}

// Get an available searcher.
static Engine *searcherGet()
{
    for (int i = 0; i < int(gSG.searchers.size()); i++)
    {
        if (!gSG.searchers[i]->IsBusy())
            return gSG.searchers[i];
    }
    assert(0);
    return nullptr;
}

// Returns (bool) whether we successfully delegated a move.
bool SearchersDelegateSearch(int alpha, int beta, MoveT move, int curDepth,
                             int maxDepth)
{
    if (gSG.numSearching < int(gSG.searchers.size()))
    {
        // Delegate a move.
        gSG.numSearching++;
        searcherGet()->CmdSearch(alpha, beta, move, curDepth, maxDepth);
        return true;
    }
    return false;
}

// The purpose of Searchers(Un)MakeMove is to keep all search threads' boards
//  in lock-step with the 'masterNode's board.  All moves but the PV are
//  delegated even on a uni-processor.
void SearchersMakeMove(MoveT move)
{
    for (int i = 0; i < int(gSG.searchers.size()); i++)
        gSG.searchers[i]->CmdMakeMove(move);
}

void SearchersUnmakeMove()
{
    for (int i = 0; i < int(gSG.searchers.size()); i++)
        gSG.searchers[i]->CmdUnmakeMove();
}

// Waits for a searcher to finish, then grabs the response from it.
// Returns: whether we were interrupted by the cmdQueue or not.
static bool searcherWaitOne()
{
    int res;
    int i;
    
    while ((res = poll(gSG.pfds.data(), gSG.pfds.size(), -1)) == -1 &&
           errno == EINTR)
    {
        continue;
    }
    assert(res > 0); // other errors should not happen
    for (i = 0; i < int(gSG.pfds.size()); i++)
    {
        assert(!(gSG.pfds[i].revents & (POLLERR | POLLHUP | POLLNVAL)));
        if (gSG.pfds[i].revents & POLLIN)
        {
            if (i == 0)
                return true;
            // Received response from slave.
            gSG.searchers[i - 1]->ProcessOneRsp();
            gSG.numSearching--;
            return false;
        }
    }
    assert(0);
}

bool SearchersWaitOne(Thinker &parent, Eval &eval, MoveT &move, SearchPv &pv)
{
    EngineSearchDoneArgsT &args = parent.Context().searchResult;

    bool rc = searcherWaitOne();
    if (!rc)
    {
        eval = args.eval;
        move = args.move;
        pv = args.pv;
    }
    else
    {
        parent.PollOneCmd();
    }
    return rc;
}

void SearchersBail()
{
    int i;
    for (i = 0; gSG.numSearching > 0 && i < int(gSG.searchers.size()); i++)
    {
        if (gSG.searchers[i]->IsSearching())
        {
            gSG.searchers[i]->CmdBail();
            gSG.numSearching--;
        }
    }
    assert(gSG.numSearching == 0);
}

// Returns (bool) whether any searchers are searching.
bool SearchersAreSearching()
{
    return gSG.numSearching > 0;
}

void SearchersSetBoard(const Board &board)
{
    for (int i = 0; i < int(gSG.searchers.size()); i++)
        gSG.searchers[i]->CmdSetBoard(board);
}

static void onEngineRspSearchDone(Engine &searcher,
                                  const EngineSearchDoneArgsT &args,
                                  Thinker &rootThinker)
{
    rootThinker.Context().searchResult = args;
}

// Should only be called when the engine is idle.
void SearchersSetNumThreads(int numThreads)
{
    assert(numThreads > 0);
    
    if (numThreads == int(gSG.searchers.size()))
        return; // no adjustment necessary
    
    if (numThreads < int(gSG.searchers.size()))
    {
        while (int(gSG.searchers.size()) > numThreads)
        {
            gSG.freePool.push_back(gSG.searchers.back());
            gSG.searchers.pop_back();
            gSG.pfds.pop_back();
        }
        return;
    }

    // At this point, we know we need more threads.
    while (int(gSG.searchers.size()) < numThreads)
    {
        Engine *eng;
        if (!gSG.freePool.empty())
        {
            eng = gSG.freePool.back();
            gSG.freePool.pop_back();
        }
        else
        {
            eng = new Engine;
            Engine::RspHandlerT rspHandler;
            rspHandler.SearchDone =
                std::bind(onEngineRspSearchDone,
                          std::placeholders::_1, std::placeholders::_2,
                          std::ref(Thinker::RootThinker()));
            eng->SetRspHandler(rspHandler);
        }
        gSG.searchers.push_back(eng);

        struct pollfd pfd;
        pfd.fd = eng->MasterSock();
        pfd.events = POLLIN;
        gSG.pfds.push_back(pfd);

        eng->CmdNewGame();
        // We rely on the caller to set the board properly afterwards.  This
        //  happens to be done for every searcher thread every time we start
        //  a search (although that's a bit hacky).
        // We also rely on the caller to Config()ure the new searchers properly,
        //  although currently it is not necessary.
    }
}

void SearchersSetCmdQueue(const EventQueue &cmdQueue)
{
    // This should basically only be called at startup.
    assert(gSG.pfds.empty());

    struct pollfd pfd;
    pfd.fd = cmdQueue.PollableObject()->Fd();
    pfd.events = POLLIN;
    gSG.pfds.push_back(pfd);
}
