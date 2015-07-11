//--------------------------------------------------------------------------
//           thinker.h - chess-oriented message passing interface
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

#ifndef THINKER_H
#define THINKER_H

#include "aThread.h"
#include "aTypes.h"
#include "board.h"
#include "moveList.h"
#include "ref.h"
#include "saveGame.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    // passed-in args.
    SaveGameT sgame;    // (eCmdThink, eCmdPonder)
    BoardT localBoard;  // (eCmdThink, eCmdPonder, eCmdSearch)
    int alpha;          // (eCmdSearch)
    int beta;           // (eCmdSearch)
    MoveListT mvlist;   // (eCmdThink, eCmdPonder)

    // passed in and also out (but unaltered).
    MoveT move;         // (eCmdSearch)

    // passed-out args.
    PvT pv;             // (eCmdSearch ... not used for eRspPv)
    PositionEvalT eval; // (eCmdSearch)
} SearchArgsT;

typedef struct {
    // The masterSock sends commands and receives responses.
    // The slaveSock receives commands and sends responses.
    int masterSock, slaveSock;
    volatile uint8 moveNow;
    uint8 isThinking;   // bool.
    uint8 isPondering;  // bool.
    uint8 isSearching;  // bool

    SearchArgsT searchArgs;
} ThinkContextT;

typedef enum {
    eCmdThink,   // full iterative-depth search
    eCmdPonder,  // ponder
    eCmdSearch,  // simple search on a sub-tree
    // eCmdMoveNow not needed until clustering

    eRspDraw,
    eRspMove,
    eRspResign,
    eRspStats,
    eRspPv,

    eRspSearchDone
} eThinkMsgT;

typedef struct {
    CompStatsT stats;
    PvT pv;
} PvRspArgsT;

void ThinkerInit(ThinkContextT *th);
eThinkMsgT ThinkerRecvRsp(ThinkContextT *th, void *buffer, int bufLen);

void ThinkerCmdSearch(ThinkContextT *th, int alpha, int beta, MoveT move);

// 'board' is (optional) position to set before thinking.
void ThinkerCmdThinkEx(ThinkContextT *th, BoardT *board, SaveGameT *sgame,
		       MoveListT *mvlist);
void ThinkerCmdThink(ThinkContextT *th, BoardT *board, SaveGameT *sgame);
void ThinkerCmdPonderEx(ThinkContextT *th, BoardT *board, SaveGameT *sgame,
			MoveListT *mvlist);
void ThinkerCmdPonder(ThinkContextT *th, BoardT *board, SaveGameT *sgame);
void ThinkerCmdMoveNow(ThinkContextT *th);
void ThinkerCmdBail(ThinkContextT *th);

static inline int ThinkerCompNeedsToMove(ThinkContextT *th)
{
    return th->moveNow;
}
static inline int ThinkerCompIsThinking(ThinkContextT *th)
{
    return th->isThinking;
}
static inline int ThinkerCompIsPondering(ThinkContextT *th)
{
    return th->isPondering;
}
static inline int ThinkerCompIsSearching(ThinkContextT *th)
{
    return th->isSearching;
}
static inline int ThinkerCompIsBusy(ThinkContextT *th)
{
    return
	ThinkerCompIsSearching(th) ||
	ThinkerCompIsThinking(th) ||
	ThinkerCompIsPondering(th);
}
void ThinkerRspDraw(ThinkContextT *th, MoveT *move);
void ThinkerRspMove(ThinkContextT *th, MoveT *move);
void ThinkerRspResign(ThinkContextT *th);
void ThinkerRspSearchDone(ThinkContextT *th);
void ThinkerRspNotifyStats(ThinkContextT *th, CompStatsT *stats);
void ThinkerRspNotifyPv(ThinkContextT *th, PvRspArgsT *pvArgs);
eThinkMsgT ThinkerCompWaitThinkOrPonder(ThinkContextT *th);
eThinkMsgT ThinkerCompWaitSearch(ThinkContextT *th);

// Operations on slave threads.
int ThinkerSearcherGetAndSearch(int alpha, int beta, MoveT *move);
PositionEvalT ThinkerSearchersWaitOne(MoveT **move, PvT *pv);
void ThinkerSearchersBail(void);

void ThinkerSearchersMoveMake(MoveT *move, UnMakeT *unmake, int mightDraw);
void ThinkerSearchersMoveUnmake(UnMakeT *unmake);
int ThinkerSearchersSearching(void);
void ThinkerSearchersBoardSet(BoardT *board);
void ThinkerSearchersSetDepthAndLevel(int depth, int level);

// Passed as an arg to 'threadFunc'.
typedef struct {
    ThreadArgsT args;
    ThinkContextT *th;
} SearcherArgsT;
void ThinkerSearchersCreate(int numThreads, THREAD_FUNC threadFunc);

#ifdef __cplusplus
}
#endif

#endif // THINKER_H
