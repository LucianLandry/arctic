//--------------------------------------------------------------------------
//          Thinker.cpp - chess-oriented message passing interface
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
#include <errno.h>
#include <poll.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>   // close(2)

#include "gDynamic.h" // PvInit()
#include "log.h"
#include "Thinker.h"

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

#define MAXBUFLEN 160

Thinker *Thinker::rootThinker = nullptr;

// An internal global resource.  Might be split later if we need sub-searchers.
struct SearcherGroupT
{
    Thinker *th;
    struct pollfd *pfds;
    int count;        // number of valid searchers in the struct
    int numSearching; // number of currently searching searchers
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
        rootThinker = this;

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
           msgLen <= sizeof(eThinkMsgT) + MAXBUFLEN);

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
    char myBuf[MAXBUFLEN];

    LOG_DEBUG("%s: sock %d start\n", __func__, sock);

    /* Wait for the msgLen, response, and buffer (if any). */
    recvBuf(sock, &msgLen, 1);
    assert(msgLen >= sizeof(eThinkMsgT) &&
           msgLen <= sizeof(eThinkMsgT) + MAXBUFLEN);
    LOG_DEBUG("%s: sock %d len %d wait msg\n", __func__, sock, msgLen);

    recvBuf(sock, &msg, sizeof(eThinkMsgT));
    LOG_DEBUG("%s: sock %d msg %d len %d\n", __func__, sock, msg, msgLen);

    msgLen -= sizeof(eThinkMsgT);
    if (msgLen) /* any buffer to recv? */
    {
        /* Yes, there is. */
        if (msgLen <= bufLen) /* Can we get the full message? */
        {
            /* Yes, just recv directly into the buffer. */
            recvBuf(sock, buffer, msgLen);
        }
        else
        {
            /* No -- get as much as we can. */
            recvBuf(sock, myBuf, msgLen);
            if (buffer)
            {
                memcpy(buffer, myBuf, bufLen);
            }
        }
    }

    return msg;
}

eThinkMsgT Thinker::RecvRsp(void *buffer, int bufLen)
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

void Thinker::CmdSetBoard(const Board &board)
{
    // If we were previously thinking, just start over.
    CmdBail();

    context.board = board;
}

void Thinker::CmdMakeMove(MoveT move)
{
    // If we were previously thinking, just start over.
    CmdBail();

    context.board.MakeMove(move);
}

void Thinker::CmdUnmakeMove()
{
    // If we were previously thinking, just start over.
    CmdBail();

    context.board.UnmakeMove();
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

void Thinker::CmdThink(const MoveList &mvlist)
{
    return doThink(eCmdThink, &mvlist);
}

void Thinker::CmdThink()
{
    return doThink(eCmdThink, nullptr);
}

void Thinker::CmdPonder(const MoveList &mvlist)
{
    return doThink(eCmdPonder, &mvlist);
}

void Thinker::CmdPonder()
{
    return doThink(eCmdPonder, nullptr);
}

/* Force the computer to move in the very near future.
   This is asynchronous.  For a synchronous analogue, see
   PlayloopCompMoveNowAndSync().
 */
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
            rsp = RecvRsp(nullptr, 0);
        } while (rsp != eRspDraw && rsp != eRspMove && rsp != eRspResign &&
                 rsp != eRspSearchDone);
    }
    assert(!CompIsBusy());
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

eThinkMsgT Thinker::CompWaitSearch() const
{
    eThinkMsgT cmd;

    LOG_DEBUG("%s: start\n", __func__);
    do
    {
        cmd = recvCmd(nullptr, 0);
        LOG_DEBUG("%s: recvd cmd %d\n", __func__, cmd);
    } while (cmd != eCmdSearch);

    return cmd;
}

// Get an available searcher.
static Thinker *searcherGet(void)
{
    int i;
    for (i = 0; i < gSG.count; i++)
    {
        if (!gSG.th[i].CompIsBusy())
            return &gSG.th[i];
    }
    assert(0);
    return nullptr;
}

// Returns (bool) whether we successfully delegated a move.
bool ThinkerSearcherGetAndSearch(int alpha, int beta, MoveT move)
{
    if (gSG.numSearching < gSG.count)
    {
        // Delegate a move.
        searcherGet()->CmdSearch(alpha, beta, move);
        gSG.numSearching++;
        return true;
    }
    return false;
}

// The purpose of ThinkerSearchersMove(Un)[Mm]ake() is to keep all
// search threads' boards in lock-step with the 'masterNode's board.  All moves
// but the PV are delegated to a searcher thread even on a uni-processor.
void ThinkerSearchersMakeMove(MoveT move)
{
    for (int i = 0; i < gSG.count; i++)
    {
        gSG.th[i].CmdMakeMove(move);
        gSG.th[i].Context().depth++;
    }
}


void ThinkerSearchersUnmakeMove()
{
    for (int i = 0; i < gSG.count; i++)
    {
        gSG.th[i].CmdUnmakeMove();
        gSG.th[i].Context().depth--;
    }
}

// returns index of completed searcher.
static int searcherWaitOne(RspSearchDoneArgsT &args)
{
    int res;
    int i;
    eThinkMsgT rsp;
    
    while ((res = poll(gSG.pfds, gSG.count, -1)) == -1 &&
           errno == EINTR)
    {
        continue;
    }
    assert(res > 0); // other errors should not happen
    for (i = 0; i < gSG.count; i++)
    {
        assert(!(gSG.pfds[i].revents & (POLLERR | POLLHUP | POLLNVAL)));
        if (gSG.pfds[i].revents & POLLIN)
        {
            // Received response from slave.
            rsp = gSG.th[i].RecvRsp(&args, sizeof(args));
            assert(rsp == eRspSearchDone);
            gSG.numSearching--;
            return i;
        }
    }
    assert(0);
    return -1;
}

Eval ThinkerSearchersWaitOne(MoveT &move, SearchPv &pv)
{
    RspSearchDoneArgsT args; 

    searcherWaitOne(args);
    move = args.move;
    pv = args.pv;
    return args.eval;
}

void ThinkerSearchersBail(void)
{
    int i;
    for (i = 0; i < gSG.count && gSG.numSearching > 0; i++)
    {
        if (gSG.th[i].CompIsSearching())
        {
            gSG.th[i].CmdBail();
            gSG.numSearching--;
        }
    }
}

// Returns (bool) if any searchers are searching.
int ThinkerSearchersAreSearching(void)
{
    return gSG.numSearching > 0;
}

void ThinkerSearchersSetBoard(const Board &board)
{
    for (int i = 0; i < gSG.count; i++)
        gSG.th[i].CmdSetBoard(board);
}

void ThinkerSearchersSetDepthAndLevel(int depth, int level)
{
    int i;
    for (i = 0; i < gSG.count; i++)
    {
        gSG.th[i].Context().depth = depth;
        gSG.th[i].Context().maxDepth = level;
    }
}

void ThinkerSearchersCreate(int numThreads)
{
    int i;

    gSG.count = numThreads;

    gSG.pfds = new struct pollfd[numThreads];
    gSG.th = new Thinker[numThreads];

    for (i = 0; i < gSG.count; i++)
    {
        // (initialize the associated poll structures --
        //  it is global so is already zero.)
        gSG.pfds[i].fd = gSG.th[i].MasterSock();
        gSG.pfds[i].events = POLLIN;
    }
}
