//--------------------------------------------------------------------------
//          thinker.cpp - chess-oriented message passing interface
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

#include "gDynamic.h" // PvInit()
#include "log.h"
#include "thinker.h"

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
   eRspDraw<move, FLAG if none>
   eRspMove<move>
   eRspResign
   eRspStats<ThinkerStatsT>
   eRspPv<PvRspArgsT>

   Extended this for multiple computer threads:
   eRspSearchDone
*/   

#define MAXBUFLEN 160

// An internal global resource.  Might be split later if we need sub-searchers.
typedef struct {
    ThinkContextT *th;
    struct pollfd *pfds;
    int count;        // number of valid searchers in the struct
    int numSearching; // number of currently searching searchers
} SearcherGroupT;

static SearcherGroupT gSG;

SearchArgsT::SearchArgsT() :
    alpha(Eval::Loss),
    beta(Eval::Win),
    move(MoveNone),
    pv(0),
    eval(Eval::Loss, Eval::Win)
{
}

// Initialize a given ThinkContextT.
void ThinkerInit(ThinkContextT *th)
{
    int err;
    int socks[2];

    err = socketpair(PF_UNIX, SOCK_STREAM, 0, socks);
    assert(err == 0);

    th->masterSock = socks[0];
    th->slaveSock = socks[1];
    th->moveNow = false;
    th->isThinking = false;
    th->isPondering = false;
    th->isSearching = false;
    // searchArgs initializes itself.
    th->maxDepth = 0;
    th->depth = 0;
}


static void sendBuf(int sock, void *buf, int len)
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


static void compSend(int sock, eThinkMsgT msg, ThinkContextT *th,
                     void *buffer, int bufLen)
{
    uint8 msgLen = sizeof(eThinkMsgT) + bufLen;
    assert(msgLen >= sizeof(eThinkMsgT) &&
           msgLen <= sizeof(eThinkMsgT) + MAXBUFLEN);

    LOG_DEBUG("compSend: sock %d msg %d len %d\n", sock, msg, msgLen);

    /* issue msgLen, response, and buffer. */
    sendBuf(sock, &msgLen, 1);
    LOG_DEBUG("compSend: sock %d send msg\n", sock);
    sendBuf(sock, &msg, sizeof(eThinkMsgT));
    if (bufLen)
    {
        sendBuf(sock, buffer, bufLen);
    }
}


static void compSendCmd(ThinkContextT *th, eThinkMsgT cmd)
{
    compSend(th->masterSock, cmd, th, NULL, 0);
}


static void compSendRsp(ThinkContextT *th, eThinkMsgT rsp,
                        void *buffer, int bufLen)
{
    compSend(th->slaveSock, rsp, th, buffer, bufLen);
}


static eThinkMsgT compRecv(int sock, ThinkContextT *th,
                           void *buffer, int bufLen)
{
    uint8 msgLen;
    eThinkMsgT msg;
    char myBuf[MAXBUFLEN];

    LOG_DEBUG("compRecv: sock %d start\n", sock);

    /* Wait for the msgLen, response, and buffer (if any). */
    recvBuf(sock, &msgLen, 1);
    assert(msgLen >= sizeof(eThinkMsgT) &&
           msgLen <= sizeof(eThinkMsgT) + MAXBUFLEN);
    LOG_DEBUG("compRecv: sock %d len %d wait msg\n", sock, msgLen);

    recvBuf(sock, &msg, sizeof(eThinkMsgT));
    LOG_DEBUG("compRecv: sock %d msg %d len %d\n", sock, msg, msgLen);

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

    if (msg == eRspDraw || msg == eRspMove || msg == eRspResign ||
        msg == eRspSearchDone)
    {
        th->isThinking = false;
        th->isPondering = false;
        th->isSearching = false;
        th->moveNow = false;
    }
    return msg;
}


eThinkMsgT ThinkerRecvRsp(ThinkContextT *th, void *buffer, int bufLen)
{
    return compRecv(th->masterSock, th, buffer, bufLen);
}


eThinkMsgT ThinkerRecvCmd(ThinkContextT *th, void *buffer, int bufLen)
{
    return compRecv(th->slaveSock, th, buffer, bufLen);
}


void ThinkerCmdSearch(ThinkContextT *th, int alpha, int beta, MoveT move)
{
    // If we were previously thinking, just start over.
    ThinkerCmdBail(th);

    // Copy over search args.
    th->searchArgs.alpha = alpha;
    th->searchArgs.beta = beta;
    th->searchArgs.move = move;
    th->isSearching = true;

    compSendCmd(th, eCmdSearch);
}

void ThinkerCmdBoardSet(ThinkContextT *th, Board *board)
{
    // If we were previously thinking, just start over.
    ThinkerCmdBail(th);

    if (board != NULL)
    {
        th->searchArgs.localBoard = *board;
    }
}


static void doThink(ThinkContextT *th, eThinkMsgT cmd, Board *board,
                    MoveList *mvlist)
{
    // If we were previously thinking, just start over.
    ThinkerCmdBail(th);

    // Copy over search args.
    assert(board != NULL);

    th->searchArgs.localBoard = *board;

    if (mvlist != NULL)
        th->searchArgs.mvlist = *mvlist;
    else
        th->searchArgs.mvlist.DeleteAllMoves();
    
    if (cmd == eCmdThink)
    {
        th->isThinking = true;
    }
    else
    {
        assert(cmd == eCmdPonder);
        th->isPondering = true;
    }
    compSendCmd(th, cmd);
}

void ThinkerCmdThinkEx(ThinkContextT *th, Board *board, MoveList *mvlist)
{
    return doThink(th, eCmdThink, board, mvlist);
}

void ThinkerCmdThink(ThinkContextT *th, Board *board)
{
    return doThink(th, eCmdThink, board, NULL);
}

void ThinkerCmdPonderEx(ThinkContextT *th, Board *board, MoveList *mvlist)
{
    return doThink(th, eCmdPonder, board, mvlist);
}

void ThinkerCmdPonder(ThinkContextT *th, Board *board)
{
    return doThink(th, eCmdPonder, board, NULL);
}


/* Force the computer to move in the very near future.
   This is asynchronous.  For a synchronous analogue, see
   PlayloopCompMoveNowAndSync().
 */
void ThinkerCmdMoveNow(ThinkContextT *th)
{
    if (ThinkerCompIsBusy(th))
    {
        th->moveNow = true;
        // I do not think this is necessary until we try to support clustering.
        // compSendCmd(th, eCmdMoveNow);
    }
}


void ThinkerCmdBail(ThinkContextT *th)
{
    eThinkMsgT rsp;
    if (ThinkerCompIsBusy(th))
    {
        ThinkerCmdMoveNow(th);

        // Wait for, and discard, the computer's move.
        do
        {
            rsp = compRecv(th->masterSock, th, NULL, 0);
        } while (rsp != eRspDraw && rsp != eRspMove && rsp != eRspResign &&
                 rsp != eRspSearchDone);
    }
    assert(!ThinkerCompIsBusy(th));
}


void ThinkerRspDraw(ThinkContextT *th, MoveT move)
{
    compSendRsp(th, eRspDraw, &move, sizeof(MoveT));
}


void ThinkerRspMove(ThinkContextT *th, MoveT move)
{
    compSendRsp(th, eRspMove, &move, sizeof(MoveT));
}


void ThinkerRspResign(ThinkContextT *th)
{
    compSendRsp(th, eRspResign, NULL, 0);
}


void ThinkerRspSearchDone(ThinkContextT *th)
{
    compSendRsp(th, eRspSearchDone, NULL, 0);
}


void ThinkerRspNotifyStats(ThinkContextT *th, ThinkerStatsT *stats)
{
    compSendRsp(th, eRspStats, stats, sizeof(ThinkerStatsT));
}


// Warning: this has the potential to fill the socket's buffer.
// ... Of course, it should block in that case.
void ThinkerRspNotifyPv(ThinkContextT *th, PvRspArgsT *pvArgs)
{
    compSendRsp(th, eRspPv, pvArgs,
                // We (lazily) copy the full struct.  We could have dug into
                //  the pv and only sent the number of valid moves.
                sizeof(PvRspArgsT));
}


eThinkMsgT ThinkerCompWaitThinkOrPonder(ThinkContextT *th)
{
    eThinkMsgT cmd;

    LOG_DEBUG("ThinkerCompWaitThinkOrPonder: start\n");
    do
    {
        cmd = ThinkerRecvCmd(th, NULL, 0);
        LOG_DEBUG("ThinkerCompWaitThinkOrPonder: recvd cmd %d\n", cmd);
    } while (cmd != eCmdThink && cmd != eCmdPonder);

    return cmd;
}

eThinkMsgT ThinkerCompWaitSearch(ThinkContextT *th)
{
    eThinkMsgT cmd;

    LOG_DEBUG("ThinkerCompWaitSearch: start\n");
    do
    {
        cmd = ThinkerRecvCmd(th, NULL, 0);
        LOG_DEBUG("ThinkerCompWaitSearch: recvd cmd %d\n", cmd);
    } while (cmd != eCmdSearch);

    return cmd;
}

// Get an available searcher.
static ThinkContextT *searcherGet(void)
{
    int i;
    for (i = 0; i < gSG.count; i++)
    {
        if (!ThinkerCompIsBusy(&gSG.th[i]))
        {
            return &gSG.th[i];
        }
    }
    assert(0);
    return NULL;
}

// Returns (bool) if we successfully delegated a move.
int ThinkerSearcherGetAndSearch(int alpha, int beta, MoveT move)
{
    if (gSG.numSearching < gSG.count)
    {
        // Delegate a move.
        ThinkerCmdSearch(searcherGet(), alpha, beta, move);
        gSG.numSearching++;
        return 1;
    }
    return 0;
}

// The purpose of ThinkerSearchersMove(Un)[Mm]ake() is to keep all
// search threads' boards in lock-step with the 'masterNode's board.  All moves
// but the PV are delegated to a searcher thread even on a uni-processor.
void ThinkerSearchersMoveMake(MoveT move)
{
    int i;
    Board *localBoard;
    for (i = 0; i < gSG.count; i++)
    {
        localBoard = &gSG.th[i].searchArgs.localBoard;
        localBoard->MakeMove(move);
        gSG.th[i].depth++;
    }
}


void ThinkerSearchersMoveUnmake()
{
    int i;
    Board *localBoard;
    for (i = 0; i < gSG.count; i++)
    {
        localBoard = &gSG.th[i].searchArgs.localBoard;
        gSG.th[i].depth--;
        localBoard->UnmakeMove();
    }
}

// returns index of completed searcher.
static int searcherWaitOne(void)
{
    int res;
    int i;
    eThinkMsgT rsp;

    while ((res = poll(gSG.pfds, gSG.count, -1)) == -1
           && errno == EINTR)
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
            rsp = ThinkerRecvRsp(&gSG.th[i], NULL, 0);
            assert(rsp == eRspSearchDone);
            gSG.numSearching--;
            return i;
        }
    }
    assert(0);
    return -1;
}

Eval ThinkerSearchersWaitOne(MoveT *move, SearchPv *pv)
{
    SearchArgsT *sa = &gSG.th[searcherWaitOne()].searchArgs;
    *move = sa->move;
    *pv = sa->pv;
    return sa->eval;
}

void ThinkerSearchersBail(void)
{
    int i;
    for (i = 0; i < gSG.count && gSG.numSearching > 0; i++)
    {
        if (ThinkerCompIsSearching(&gSG.th[i]))
        {
            ThinkerCmdBail(&gSG.th[i]);
            gSG.numSearching--;
        }
    }
}

// Returns (bool) if any searchers are searching.
int ThinkerSearchersSearching(void)
{
    return gSG.numSearching > 0;
}

void ThinkerSearchersBoardSet(Board *board)
{
    int i;
    for (i = 0; i < gSG.count; i++)
    {
        ThinkerCmdBoardSet(&gSG.th[i], board);
    }
}

void ThinkerSearchersSetDepthAndLevel(int depth, int level)
{
    int i;
    for (i = 0; i < gSG.count; i++)
    {
        gSG.th[i].depth = depth;
        gSG.th[i].maxDepth = level;
    }
}

void ThinkerSearchersCreate(int numThreads, THREAD_FUNC threadFunc)
{
    int i;
    SearcherArgsT sargs;

    gSG.count = numThreads;

    gSG.pfds = new struct pollfd[numThreads];
    gSG.th = new ThinkContextT[numThreads];

    for (i = 0; i < gSG.count; i++)
    {
        sargs.th = &gSG.th[i];
        ThinkerInit(sargs.th);
        ThreadCreate(threadFunc, (ThreadArgsT *) &sargs);

        // (also initialize the associated poll structures --
        //  it is global so is already zero.)
        gSG.pfds[i].fd = sargs.th->masterSock;
        gSG.pfds[i].events = POLLIN;
    }
}
