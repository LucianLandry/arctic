//--------------------------------------------------------------------------
//          Thinker.cpp - chess-oriented message passing interface
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
#include <sys/socket.h>
#include <unistd.h>     // close(2)

#include "aSystem.h"    // SystemTotalProcessors()
#include "Engine.h"
#include "Thinker.h"

static const int kMaxBufLen = 160;

Thinker *Thinker::rootThinker = nullptr;

// An internal global resource.  Might be split later if we need sub-searchers.
struct SearcherGroupT
{
    std::vector<Engine *> searchers;
    std::vector<struct pollfd> pfds;
    int numSearching; // number of currently searching searchers

    // Normally this vector is empty, but if we lower the core count, the extra
    //  searchers are kept in a thread pool here.
    std::vector<Engine *> freePool;
};

static SearcherGroupT gSG;

Thinker::ContextT::ContextT() : maxDepth(0), depth(0), moveNow(false)
{
    searchArgs.alpha = Eval::Loss;
    searchArgs.beta = Eval::Win;
    searchArgs.move = MoveNone;
}

Thinker::SharedContextT::SharedContextT() :
    maxLevel(DepthNoLimit), maxNodes(0), randomMoves(false), canResign(true),
    maxThreads(SystemTotalProcessors()), gameCount(0) {}

Thinker::Thinker(int sock)
{
    slaveSock = sock;
    // 'context' initializes itself.

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
    close(slaveSock);
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

void Thinker::sendMessage(int sock, Thinker::Message msg, const void *args,
                          int argsLen)
{
    uint8 msgLen = sizeof(msg) + argsLen;
    assert(msgLen >= sizeof(msg) &&
           msgLen <= sizeof(msg) + kMaxBufLen);

    // issue msgLen, Message, and args.
    sendBuf(sock, &msgLen, 1);
    sendBuf(sock, &msg, sizeof(msg));
    if (argsLen)
        sendBuf(sock, args, argsLen);
}

Thinker::Message Thinker::recvMessage(int sock, void *args, int argsLen)
{
    uint8 msgLen;
    Message msg;
    char tmpBuf[kMaxBufLen];

    // Wait for the msgLen, response, and buffer (if any).
    recvBuf(sock, &msgLen, 1);
    assert(msgLen >= sizeof(msg) &&
           msgLen <= sizeof(msg) + kMaxBufLen);
    recvBuf(sock, &msg, sizeof(msg));
    msgLen -= sizeof(msg);
    if (msgLen) // any args to recv?
    {
        // Yup.
        if (msgLen <= argsLen) // Can we get the full message?
        {
            // Yes, just recv directly into the buffer.
            recvBuf(sock, args, msgLen);
        }
        else
        {
            // No -- get as much as we can.
            recvBuf(sock, tmpBuf, msgLen);
            if (args)
                memcpy(args, tmpBuf, argsLen);
        }
    }

    return msg;
}

void Thinker::sendRsp(Thinker::Message rsp, const void *args, int argsLen) const
{
    sendMessage(slaveSock, rsp, args, argsLen);
}

Thinker::Message Thinker::recvCmd(void *args, int argsLen) const
{
    Message msg = recvMessage(slaveSock, args, argsLen);

    assert(msg == Message::CmdThink || msg == Message::CmdPonder ||
           msg == Message::CmdSearch);
    return msg;
}

void Thinker::RspDraw(MoveT move) const
{
    sendRsp(Message::RspDraw, &move, sizeof(MoveT));
}

void Thinker::RspMove(MoveT move) const
{
    sendRsp(Message::RspMove, &move, sizeof(MoveT));
}

void Thinker::RspResign() const
{
    sendRsp(Message::RspResign, nullptr, 0);
}

void Thinker::RspSearchDone(MoveT move, Eval eval, const SearchPv &pv) const
{
    EngineSearchDoneArgsT args = {move, eval, pv};
    sendRsp(Message::RspSearchDone, &args, sizeof(args));
}

void Thinker::RspNotifyStats(const EngineStatsT &stats) const
{
    sendRsp(Message::RspStats, &stats, sizeof(stats));
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
    sendRsp(Message::RspPv, &args,
            // We (lazily) copy the full struct.  We could have dug into
            //  the pv and only sent the number of valid moves.
            sizeof(EnginePvArgsT));
}

Thinker::Message Thinker::WaitThinkOrPonder() const
{
    Message cmd;

    do
    {
        cmd = recvCmd(nullptr, 0);
    } while (cmd != Message::CmdThink && cmd != Message::CmdPonder);

    return cmd;
}

void Thinker::WaitSearch() const
{
    Message cmd;

    do
    {
        cmd = recvCmd(nullptr, 0);
    } while (cmd != Message::CmdSearch);
}

bool Thinker::IsFinalResponse(Message msg)
{
    return
        msg == Message::RspDraw || msg == Message::RspMove ||
        msg == Message::RspResign || msg == Message::RspSearchDone;
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
    for (i = 0; i < int(gSG.pfds.size()); i++)
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

Eval SearchersWaitOne(MoveT &move, SearchPv &pv, Thinker &parent)
{
    EngineSearchDoneArgsT &args = parent.Context().searchResult;

    searcherWaitOne();
    move = args.move;
    pv = args.pv;
    return args.eval;
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
