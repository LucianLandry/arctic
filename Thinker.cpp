//--------------------------------------------------------------------------
//          Thinker.cpp - chess-oriented message passing interface
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
#include <errno.h>
#include <limits.h>     // INT_MAX
#include <poll.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <thread>
#include <type_traits>  // std::is_trivially_copyable<>
#include <unistd.h>     // close(2)

#include "HistoryWindow.h" // gHistoryWindow
#include "log.h"
#include "Thinker.h"
#include "TransTable.h" // gTransTable
#include "Variant.h"

// HACK workaround for std::is_trivially_copyable not being provided prior to
// libstdc++5.0.  Lifted from https://github.com/mpark/variant/issues/8.
// I should probably put this in some sort of header file.
#if defined(__GLIBCXX__) && __GLIBCXX__ < 20150801
namespace std {
template <typename T>
struct is_trivially_copyable : integral_constant<bool, __has_trivial_copy(T)> {
};
}  // namespace std
#endif  // GLIBCXX macro

// bldbg
#undef LOG_DEBUG
#define LOG_DEBUG(format, ...)

/* Okay, here's the lowdown.
   Communication between the main program and computer is done via 2 sockets,
   via messages.

   Message format is:
   msglen (1 byte)
   msg ('msglen' bytes)

   Possible messages the UI layer can send:
   eCmdThink
   eCmdPonder
   eCmdMoveNow # also used for bail, where the move may be discarded.

   Possible messages the computer can send:
   eRspDraw<move, MoveNone if none>
   eRspMove<move>
   eRspResign
   eRspStats<ThinkerStatsT>
   eRspPv<ThinkerStatsT, DisplayPv (bundled into RspPvArgsT)>

   Extended this for multiple computer threads:
   eRspSearchDone<MoveT move, Eval eval, SearchPv pv
                  (bundled into RspSearchDoneArgsT)>
*/   

static const int kMaxBufLen = 160;

Thinker *Thinker::rootThinker = nullptr;

// An internal global resource.  Might be split later if we need sub-searchers.
struct SearcherGroupT
{
    std::vector<Thinker *> searchers;
    std::vector<struct pollfd> pfds;
    int numSearching; // number of currently searching searchers

    // Normally this vector is empty, but if we lower the core count, the extra
    //  searchers are kept in a thread pool here.
    std::vector<Thinker *> freePool;
};

static SearcherGroupT gSG;

Thinker::SearchArgsT::SearchArgsT() :
    alpha(Eval::Loss),
    beta(Eval::Win),
    move(MoveNone),
    pv(0),
    eval(Eval::Loss, Eval::Win)
{
}

Thinker::ContextT::ContextT() : maxDepth(0), depth(0) {}

Thinker::SharedContextT::SharedContextT() :
    maxLevel(DepthNoLimit), maxNodes(0), randomMoves(false), canResign(true),
    gameCount(0) {}

static void onMaxDepthChanged(const Config::SpinItem &item, Thinker &th)
{
    if (!th.IsRootThinker())
        return;
    volatile int &maxLevel = th.SharedContext().maxLevel; // shorthand
    maxLevel = item.Value() - 1;
    if (maxLevel != Thinker::DepthNoLimit && th.Context().maxDepth > maxLevel)
        th.CmdMoveNow();
}
static void onMaxNodesChanged(const Config::SpinItem &item, Thinker &th)
{
    if (!th.IsRootThinker())
        return;
    th.SharedContext().maxNodes = item.Value();
    // The engine itself should shortly notice that it has exceeded
    //  maxNodes (if applicable), and return.
}
static void onRandomMovesChanged(const Config::CheckboxItem &item, Thinker &th)
{
    if (!th.IsRootThinker())
        return;
    th.SharedContext().randomMoves = item.Value();
}
static void onCanResignChanged(const Config::CheckboxItem &item, Thinker &th)
{
    if (!th.IsRootThinker())
        return;
    th.SharedContext().canResign = item.Value();
}
static void onHistoryWindowChanged(const Config::SpinItem &item, Thinker &th)
{
    if (!th.IsRootThinker())
        return;
    gHistoryWindow.SetWindow(item.Value());
}
void Thinker::onMaxMemoryChanged(const Config::SpinItem &item)
{
    if (!IsRootThinker())
        return;
    State origState = state;
    if (CompIsBusy())
        CmdBail();
    gTransTable.Reset(uint64(item.Value()) * 1024 * 1024);
    switch (origState) // continue where we were interrupted, if applicable.
    {
        case State::Pondering:
            CmdPonder(context.mvlist);
            break;
        case State::Thinking:
            CmdThink(context.clock, context.mvlist);
            break;
        case State::Searching:
            CmdSearch(searchArgs.alpha, searchArgs.beta, searchArgs.move);
            break;
        default:
            break;
    }
}

// ctor.
Thinker::Thinker()
{
    int err;
    int socks[2];

    err = socketpair(PF_UNIX, SOCK_STREAM, 0, socks);
    assert(err == 0);

    masterSock = socks[0];
    slaveSock = socks[1];
    moveNow = false;
    state = State::Idle;
    // 'searchArgs' and 'context' initialize themselves.

    // Assume the first Thinker created is the rootThinker.
    if (rootThinker == nullptr)
    {
        rootThinker = this;
        sharedContext = std::make_shared<SharedContextT>();
    }
    else
    {
        sharedContext = rootThinker->sharedContext;
    }
    
    // Register config callbacks.
    Config().Register(
        Config::SpinItem(Config::MaxDepthSpin, Config::MaxDepthDescription,
                         0, 0, INT_MAX,
                         std::bind(onMaxDepthChanged, std::placeholders::_1,
                                   std::ref(*this))));
    Config().Register(
        Config::SpinItem(Config::MaxNodesSpin, Config::MaxNodesDescription,
                         0, 0, INT_MAX,
                         std::bind(onMaxNodesChanged, std::placeholders::_1,
                                   std::ref(*this))));
    Config().Register(
        Config::CheckboxItem(Config::RandomMovesCheckbox,
                             Config::RandomMovesDescription,
                             false,
                             std::bind(onRandomMovesChanged,
                                       std::placeholders::_1,
                                       std::ref(*this))));
    Config().Register(
        Config::CheckboxItem(Config::CanResignCheckbox,
                             Config::CanResignDescription,
                             true,
                             std::bind(onCanResignChanged,
                                       std::placeholders::_1,
                                       std::ref(*this))));
    Config().Register(
        Config::SpinItem(Config::HistoryWindowSpin,
                         Config::HistoryWindowDescription,
                         0, gHistoryWindow.Window(), INT_MAX,
                         std::bind(onHistoryWindowChanged,
                                   std::placeholders::_1, std::ref(*this))));
    Config().Register(
        Config::SpinItem(Config::MaxMemorySpin, Config::MaxMemoryDescription,
                         0, gTransTable.DefaultSize() / (1024 * 1024),
                         gTransTable.MaxSize() / (1024 * 1024),
                         std::bind(&Thinker::onMaxMemoryChanged, this,
                                   std::placeholders::_1)));
    
    goalTime = CLOCK_TIME_INFINITE;

    thread = new std::thread(&Thinker::threadFunc, this);
    thread->detach();
}

// dtor
Thinker::~Thinker()
{
    close(masterSock);
    close(slaveSock);
    // We do not support Thinker destruction yet.  To do that, we will need
    //  to:
    // -- not detach this->thread
    // -- implement a CmdExit or something and use it here
    // thread->join()
    assert(0);
    delete thread;
}

static void sendBuf(int sock, const void *buf, int len)
{
    int sent;
    while (len > 0)
    {
        while ((sent = send(sock, buf, len, 0)) < 0 && errno == EINTR)
            ;
        assert(sent > 0);
        len -= sent;
        buf = (char *) buf + sent;
    }
}
static void recvBuf(int sock, void *buf, int len)
{
    int recvd;
    while (len > 0)
    {
        while ((recvd = recv(sock, buf, len, 0)) < 0 && errno == EINTR)
            ;
        assert(recvd > 0);
        len -= recvd;
        buf = (char *) buf + recvd;
    }
}

static void compSend(int sock, eThinkMsgT msg, const void *buffer, int bufLen)
{
    uint8 msgLen = sizeof(eThinkMsgT) + bufLen;
    assert(msgLen >= sizeof(eThinkMsgT) &&
           msgLen <= sizeof(eThinkMsgT) + kMaxBufLen);

    LOG_DEBUG("%s: sock %d msg %d len %d\n", __func__, sock, msg, msgLen);

    // issue msgLen, response, and buffer.
    sendBuf(sock, &msgLen, 1);
    LOG_DEBUG("%s: sock %d send msg\n", __func__, sock);
    sendBuf(sock, &msg, sizeof(eThinkMsgT));
    if (bufLen)
        sendBuf(sock, buffer, bufLen);
}

void Thinker::compSendCmd(eThinkMsgT cmd) const
{
    compSend(masterSock, cmd, nullptr, 0);
}

void Thinker::compSendRsp(eThinkMsgT rsp, const void *buffer, int bufLen) const
{
    compSend(slaveSock, rsp, buffer, bufLen);
}

static eThinkMsgT compRecv(int sock, void *buffer, int bufLen)
{
    uint8 msgLen;
    eThinkMsgT msg;
    char myBuf[kMaxBufLen];

    LOG_DEBUG("%s: sock %d start\n", __func__, sock);

    // Wait for the msgLen, response, and buffer (if any).
    recvBuf(sock, &msgLen, 1);
    assert(msgLen >= sizeof(eThinkMsgT) &&
           msgLen <= sizeof(eThinkMsgT) + kMaxBufLen);
    LOG_DEBUG("%s: sock %d len %d wait msg\n", __func__, sock, msgLen);

    recvBuf(sock, &msg, sizeof(eThinkMsgT));
    LOG_DEBUG("%s: sock %d msg %d len %d\n", __func__, sock, msg, msgLen);

    msgLen -= sizeof(eThinkMsgT);
    if (msgLen) // any buffer to recv?
    {
        // Yes, there is.
        if (msgLen <= bufLen) // Can we get the full message?
        {
            // Yes, just recv directly into the buffer.
            recvBuf(sock, buffer, msgLen);
        }
        else
        {
            // No -- get as much as we can.
            recvBuf(sock, myBuf, msgLen);
            if (buffer)
            {
                memcpy(buffer, myBuf, bufLen);
            }
        }
    }

    return msg;
}

eThinkMsgT Thinker::recvRsp(void *buffer, int bufLen)
{
    eThinkMsgT msg = compRecv(masterSock, buffer, bufLen);

    assert(msg == eRspDraw || msg == eRspMove || msg == eRspResign ||
           msg == eRspStats || msg == eRspPv || msg == eRspSearchDone);

    if (msg == eRspDraw || msg == eRspMove || msg == eRspResign ||
        msg == eRspSearchDone)
    {
        state = State::Idle;
        moveNow = false;
    }
    return msg;
}

eThinkMsgT Thinker::recvCmd(void *buffer, int bufLen) const
{
    eThinkMsgT msg = compRecv(slaveSock, buffer, bufLen);

    assert(msg == eCmdThink || msg == eCmdPonder || msg == eCmdSearch);
    return msg;
}

void Thinker::CmdNewGame()
{
    CmdBail();
    gTransTable.Reset();
    gHistoryWindow.Clear();
    sharedContext->pv.Clear();
    sharedContext->gameCount++;
    if (!context.board.SetPosition(Variant::Current()->StartingPosition()))
        assert(0);
}

void Thinker::CmdSetBoard(const Board &board)
{
    // If we were previously thinking, just start over.
    CmdBail();
    // Make a best effort at PV tracking (in case the boards are similar).  
    if (IsRootThinker())
    {
        sharedContext->pv.Rewind(context.board.Ply() - board.Ply());
        // If the ply happened to be the same, we still want to start the search
        //  over.
        sharedContext->pv.ResetSearchStartLevel();
    }
    context.board = board;
}

void Thinker::CmdMakeMove(MoveT move)
{
    CmdBail(); // If we were previously thinking, just start over.

    context.board.MakeMove(move);
    if (IsRootThinker())
        sharedContext->pv.Decrement(move);
}

void Thinker::CmdUnmakeMove()
{
    CmdBail(); // If we were previously thinking, just start over.
    context.board.UnmakeMove();
    if (IsRootThinker())
        sharedContext->pv.Rewind(1);
}

void Thinker::doThink(eThinkMsgT cmd, const MoveList *mvlist)
{
    // If we were previously thinking, just start over.
    CmdBail();

    // Copy over search args.
    if (mvlist != NULL)
        context.mvlist = *mvlist;
    else
        context.mvlist.DeleteAllMoves();
    
    if (cmd == eCmdThink)
    {
        state = State::Thinking;
    }
    else
    {
        assert(cmd == eCmdPonder);
        state = State::Pondering;
    }
    compSendCmd(cmd);
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
void Thinker::calcGoalTime(const Clock &myClock)
{
    const Board &board = context.board; // shorthand.
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

void Thinker::CmdThink(const Clock &myClock, const MoveList &mvlist)
{
    context.clock = myClock;
    context.clock.Start();
    calcGoalTime(myClock);
    doThink(eCmdThink, &mvlist);
}

void Thinker::CmdThink(const Clock &myClock)
{
    context.clock = myClock;
    context.clock.Start();
    calcGoalTime(myClock);
    doThink(eCmdThink, nullptr);
}

void Thinker::CmdPonder(const MoveList &mvlist)
{
    doThink(eCmdPonder, &mvlist);
}

void Thinker::CmdPonder()
{
    doThink(eCmdPonder, nullptr);
}

void Thinker::CmdSearch(int alpha, int beta, MoveT move)
{
    // If we were previously thinking, just start over.
    CmdBail();

    // Copy over search args.
    searchArgs.alpha = alpha;
    searchArgs.beta = beta;
    searchArgs.move = move;
    state = State::Searching;

    compSendCmd(eCmdSearch);
}

// Force the computer to move in the very near future.  This is asynchronous.
// For a synchronous analogue, see Game->MoveNow().
void Thinker::CmdMoveNow()
{
    if (CompIsBusy())
    {
        moveNow = true;
        // I do not think this is necessary until we try to support clustering.
        // compSendCmd(eCmdMoveNow);
    }
}


void Thinker::CmdBail()
{
    eThinkMsgT rsp;
    if (CompIsBusy())
    {
        CmdMoveNow();

        // Wait for, and discard, the computer's move.
        do
        {
            rsp = recvRsp(nullptr, 0);
        } while (rsp != eRspDraw && rsp != eRspMove && rsp != eRspResign &&
                 rsp != eRspSearchDone);
    }
    assert(!CompIsBusy());
}

void Thinker::SetRspHandler(const RspHandlerT &rspHandler)
{
    this->rspHandler = rspHandler;
}

void Thinker::ProcessOneRsp()
{
    alignas(sizeof(void *)) char rspBuf[MAX(MAX(sizeof(ThinkerStatsT), sizeof(RspPvArgsT)), MAX(sizeof(MoveT), sizeof(RspSearchDoneArgsT)))];
    ThinkerStatsT *stats = (ThinkerStatsT *)rspBuf;
    RspPvArgsT *pvArgs = (RspPvArgsT *)rspBuf;
    MoveT *move = (MoveT *)rspBuf;
    RspSearchDoneArgsT *searchDoneArgs = (RspSearchDoneArgsT *)rspBuf;

    static_assert(std::is_trivially_copyable<ThinkerStatsT>::value,
                  "ThinkerStatsT must be trivially copyable to copy it "
                  "through a socket!");
    static_assert(std::is_trivially_copyable<RspPvArgsT>::value,
                  "RspPvStatsT must be trivially copyable to copy it "
                  "through a socket!");
    static_assert(std::is_trivially_copyable<MoveT>::value,
                  "MoveT must be trivially copyable to copy it "
                  "through a socket!");
    static_assert(std::is_trivially_copyable<RspSearchDoneArgsT>::value,
                  "RspSearchDoneArgsT must be trivially copyable to copy it "
                  "through a socket!");

    
    eThinkMsgT rsp = recvRsp(&rspBuf, sizeof(rspBuf));

    switch (rsp)
    {
        case eRspDraw:
            rspHandler.Draw(*this, *move);
            break;
        case eRspMove:
            rspHandler.Move(*this, *move);
            break;
        case eRspResign:
            rspHandler.Resign(*this);
            break;
        case eRspStats:
            rspHandler.NotifyStats(*this, *stats);
            break;
        case eRspPv:
            rspHandler.NotifyPv(*this, *pvArgs);
            break;
        case eRspSearchDone:
            rspHandler.SearchDone(*this, *searchDoneArgs);
            break;
        default:
            LOG_EMERG("%s: unknown response type %d\n", __func__, rsp);
            assert(0);
            break;
    }
}

void Thinker::RspDraw(MoveT move) const
{
    compSendRsp(eRspDraw, &move, sizeof(MoveT));
}


void Thinker::RspMove(MoveT move) const
{
    compSendRsp(eRspMove, &move, sizeof(MoveT));
}


void Thinker::RspResign() const
{
    compSendRsp(eRspResign, nullptr, 0);
}


void Thinker::RspSearchDone(MoveT move, Eval eval, const SearchPv &pv) const
{
    RspSearchDoneArgsT args = {move, eval, pv};
    compSendRsp(eRspSearchDone, &args, sizeof(args));
}

void Thinker::RspNotifyStats(const ThinkerStatsT &stats) const
{
    compSendRsp(eRspStats, &stats, sizeof(ThinkerStatsT));
}

// This has the potential to fill the socket's buffer.
// ... Of course, it should block in that case.
void Thinker::RspNotifyPv(const ThinkerStatsT &stats, const DisplayPv &pv) const
{
    RspPvArgsT args = { stats, pv };
    if (!args.pv.Sanitize(context.board))
    {
        LogPrint(eLogNormal, "%s: game %d: note: illegal move detected, "
                 "probably zobrist collision\n",
                 __func__, sharedContext->gameCount);
    }
    compSendRsp(eRspPv, &args,
                // We (lazily) copy the full struct.  We could have dug into
                //  the pv and only sent the number of valid moves.
                sizeof(RspPvArgsT));
}

eThinkMsgT Thinker::CompWaitThinkOrPonder() const
{
    eThinkMsgT cmd;

    LOG_DEBUG("%s: start\n", __func__);
    do
    {
        cmd = recvCmd(nullptr, 0);
        LOG_DEBUG("%s: recvd cmd %d\n", __func__, cmd);
    } while (cmd != eCmdThink && cmd != eCmdPonder);

    return cmd;
}

void Thinker::CompWaitSearch() const
{
    eThinkMsgT cmd;

    LOG_DEBUG("%s: start\n", __func__);
    do
    {
        cmd = recvCmd(nullptr, 0);
        LOG_DEBUG("%s: recvd cmd %d\n", __func__, cmd);
    } while (cmd != eCmdSearch);
}

// Get an available searcher.
static Thinker *searcherGet()
{
    int i;
    for (i = 0; i < gSG.searchers.size(); i++)
    {
        if (!gSG.searchers[i]->CompIsBusy())
            return gSG.searchers[i];
    }
    assert(0);
    return nullptr;
}

// Returns (bool) whether we successfully delegated a move.
bool ThinkerSearcherGetAndSearch(int alpha, int beta, MoveT move)
{
    if (gSG.numSearching < gSG.searchers.size())
    {
        // Delegate a move.
        gSG.numSearching++;
        searcherGet()->CmdSearch(alpha, beta, move);
        return true;
    }
    return false;
}

// The purpose of ThinkerSearchersMove(Un)[Mm]ake() is to keep all
// search threads' boards in lock-step with the 'masterNode's board.  All moves
// but the PV are delegated to a searcher thread even on a uni-processor.
void ThinkerSearchersMakeMove(MoveT move)
{
    for (int i = 0; i < gSG.searchers.size(); i++)
    {
        gSG.searchers[i]->CmdMakeMove(move);
        gSG.searchers[i]->Context().depth++;
    }
}


void ThinkerSearchersUnmakeMove()
{
    for (int i = 0; i < gSG.searchers.size(); i++)
    {
        gSG.searchers[i]->CmdUnmakeMove();
        gSG.searchers[i]->Context().depth--;
    }
}

// Waits for a searcher to finish, then grabs the response from it.
static void searcherWaitOne()
{
    int res;
    int i;
    
    while ((res = poll(gSG.pfds.data(), gSG.pfds.size(), -1)) == -1 &&
           errno == EINTR)
    {
        continue;
    }
    assert(res > 0); // other errors should not happen
    for (i = 0; i < gSG.pfds.size(); i++)
    {
        assert(!(gSG.pfds[i].revents & (POLLERR | POLLHUP | POLLNVAL)));
        if (gSG.pfds[i].revents & POLLIN)
        {
            // Received response from slave.
            gSG.searchers[i]->ProcessOneRsp();
            gSG.numSearching--;
            return;
        }
    }
    assert(0);
}

Eval ThinkerSearchersWaitOne(MoveT &move, SearchPv &pv, Thinker &parent)
{
    RspSearchDoneArgsT &args = parent.Context().searchResult;

    searcherWaitOne();
    move = args.move;
    pv = args.pv;
    return args.eval;
}

void ThinkerSearchersBail()
{
    int i;
    for (i = 0; i < gSG.searchers.size() && gSG.numSearching > 0; i++)
    {
        if (gSG.searchers[i]->CompIsSearching())
        {
            gSG.searchers[i]->CmdBail();
            gSG.numSearching--;
        }
    }
    assert(i < gSG.searchers.size());
}

// Returns (bool) whether any searchers are searching.
int ThinkerSearchersAreSearching()
{
    return gSG.numSearching > 0;
}

void ThinkerSearchersSetBoard(const Board &board)
{
    for (int i = 0; i < gSG.searchers.size(); i++)
        gSG.searchers[i]->CmdSetBoard(board);
}

void ThinkerSearchersSetDepthAndLevel(int depth, int level)
{
    int i;
    for (i = 0; i < gSG.searchers.size(); i++)
    {
        gSG.searchers[i]->Context().depth = depth;
        gSG.searchers[i]->Context().maxDepth = level;
    }
}

static void onThinkerRspSearchDone(Thinker &searcher,
                                   const RspSearchDoneArgsT &args,
                                   Thinker &rootThinker)
{
    rootThinker.Context().searchResult = args;
}

void ThinkerSearchersCreate(int numThreads, Thinker &rootThinker)
{
    // Currently, we only support one 'master' thinker.
    assert(rootThinker.IsRootThinker());

    Thinker::RspHandlerT rspHandler;
    rspHandler.SearchDone =
        std::bind(onThinkerRspSearchDone,
                  std::placeholders::_1, std::placeholders::_2,
                  std::ref(rootThinker));
    
    for (int i = 0; i < numThreads; i++)
    {
        Thinker *th = new Thinker;
        th->SetRspHandler(rspHandler);
        gSG.searchers.push_back(th);

        struct pollfd pfd;
        pfd.fd = th->MasterSock();
        pfd.events = POLLIN;
        gSG.pfds.push_back(pfd);
    }
}
