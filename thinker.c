/***************************************************************************
                thinker.c - interactive computer functionality
                            (see comp.c for actual 'AI')
                             -------------------
    copyright            : (C) 2007 by Lucian Landry
    email                : lucian_b_landry@yahoo.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 ***************************************************************************/

#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "ref.h"


/* Okay, here's the lowdown.
   Communication between the main program and computer is done via 2 sockets,
   via messages.

   Message format is:
   msglen (1 byte)
   msg ('msglen' bytes)

   Possible messages the UI layer can send:
   eCmdThink
   eCmdMoveNow # also used for bail, where the move may be discarded.

   Possible messages the computer can send:
   eRspDraw<move, FLAG if none>
   eRspMove<move>
   eRspResign
   eRspStats<CompStatsT>
   eRspPv<PvT>
*/   

#define MAXBUFLEN 80

/* Initialize a given ThinkContentT. */
void ThinkerInit(ThinkContextT *th)
{
    int err;
    int socks[2];

    memset(th, 0, sizeof(ThinkContextT));

    err = socketpair(PF_UNIX, SOCK_STREAM, 0, socks);
    assert(err == 0);

    th->mainSock = socks[0];
    th->compSock = socks[1];
}


static void compSend(int sock, eThinkMsgT msg, ThinkContextT *th,
		     void *buffer, int bufLen)
{
    int count;
    uint8 msgLen = sizeof(eThinkMsgT) + bufLen;

    LOG_DEBUG("compSend: sock %d msg %d len %d\n", sock, msg, msgLen);

    /* issue msgLen, response, and buffer. */
    count = send(sock, &msgLen, 1, 0);
    assert(count == 1);
    LOG_DEBUG("compSend: sock %d send msg\n", sock);
    count = send(sock, &msg, sizeof(eThinkMsgT), 0);
    assert(count == sizeof(eThinkMsgT));    
    if (bufLen)
    {
	count = send(sock, buffer, bufLen, 0);
	assert(count == bufLen);
    }
}


static void compSendCmd(ThinkContextT *th, eThinkMsgT cmd)
{
    compSend(th->mainSock, cmd, th, NULL, 0);
}


static void compSendRsp(eThinkMsgT rsp, ThinkContextT *th,
			void *buffer, int bufLen)
{
    compSend(th->compSock, rsp, th, buffer, bufLen);
}


static eThinkMsgT compRecv(int sock, ThinkContextT *th,
			   void *buffer, int bufLen)
{
    int count;
    uint8 msgLen;
    eThinkMsgT msg;
    char myBuf[MAXBUFLEN];

    LOG_DEBUG("compRecv: sock %d start\n", sock);

    /* Wait for the msgLen, response, and buffer (if any). */
    count = recv(sock, &msgLen, 1, 0);
    assert(count == 1 &&
	   msgLen >= sizeof(eThinkMsgT) &&
	   msgLen <= sizeof(eThinkMsgT) + MAXBUFLEN);
    LOG_DEBUG("compRecv: sock %d len %d wait msg\n", sock, msgLen);
    count = recv(sock, &msg, sizeof(eThinkMsgT), 0);
    assert(count == sizeof(eThinkMsgT));

    LOG_DEBUG("compRecv: sock %d msg %d len %d\n", sock, msg, msgLen);

    msgLen -= sizeof(eThinkMsgT);
    if (msgLen) /* any buffer to recv? */
    {
	/* Yes, there is. */
	if (msgLen <= bufLen) /* Can we get the full message? */
	{
	    /* Yes, just recv directly into the buffer. */
	    count = recv(sock, buffer, msgLen, 0);
	    assert(count == msgLen);    
	}
	else
	{
	    /* No -- get as much as we can. */
	    count = recv(sock, myBuf, msgLen, 0);
	    assert(count == msgLen);
	    if (buffer)
	    {
		memcpy(buffer, myBuf, bufLen);
	    }
	}
    }

    if (msg == eRspDraw || msg == eRspMove || msg == eRspResign)
	th->isThinking = th->moveNow = 0;
    return msg;
}


eThinkMsgT ThinkerRecvRsp(ThinkContextT *th, void *buffer, int bufLen)
{
    return compRecv(th->mainSock, th, buffer, bufLen);
}


/* only to be used when we know the machine is idle. */
void ThinkerThink(ThinkContextT *th)
{
    assert(!th->isThinking);
    th->isThinking = 1;
    gUI->notifyThinking();
    compSendCmd(th, eCmdThink);
}


/* Force the computer to move in the very near future. */
void ThinkerMoveNow(ThinkContextT *th)
{
    if (th->isThinking)
    {
	th->moveNow = 1;
	compSendCmd(th, eCmdMoveNow);
    }
}


void ThinkerBail(ThinkContextT *th)
{
    eThinkMsgT rsp;
    if (th->isThinking)
    {
	ThinkerMoveNow(th);

	/* Wait for, and discard, the computer's move. */
	do
	{
	    rsp = compRecv(th->mainSock, th, NULL, 0);
	} while (rsp != eRspDraw && rsp != eRspMove && rsp != eRspResign);
    }
}


void ThinkerCompDraw(ThinkContextT *th, uint8 *comstr)
{
    compSendRsp(eRspDraw, th, comstr, 4);
}


void ThinkerCompMove(ThinkContextT *th, uint8 *comstr)
{
    compSendRsp(eRspMove, th, comstr, 4);
}


void ThinkerCompResign(ThinkContextT *th)
{
    compSendRsp(eRspResign, th, NULL, 0);
}


void ThinkerCompNotifyStats(ThinkContextT *th, CompStatsT *stats)
{
    compSendRsp(eRspStats, th, stats, sizeof(CompStatsT));
}


/* Warning: this has the potential to overflow the buffer. ... Of course,
   it should block in that case. */
void ThinkerCompNotifyPv(ThinkContextT *th, PvT *Pv)
{
    compSendRsp(eRspPv, th, Pv, sizeof(PvT));
}


void ThinkerCompWaitThink(ThinkContextT *th)
{
    eThinkMsgT cmd;

    LOG_DEBUG("ThinkerCompWaitThink: start\n");
    do
    {
	cmd = compRecv(th->compSock, th, NULL, 0);
	LOG_DEBUG("ThinkerCompWaitThink: recvd cmd %d\n", cmd);
    } while (cmd != eCmdThink);
}
