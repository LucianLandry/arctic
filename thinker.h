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
#include "Board.h"
#include "MoveList.h"
#include "ref.h"
#include "saveGame.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    // passed-in args.
    Board localBoard;   // (eCmdThink, eCmdPonder, eCmdSearch)
    int alpha;          // (eCmdSearch)
    int beta;           // (eCmdSearch)
    MoveList mvlist;    // (eCmdThink, eCmdPonder)

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
    volatile bool moveNow;
    bool isThinking;
    bool isPondering;
    bool isSearching;

    SearchArgsT searchArgs;

    int maxDepth; // Depth we are authorized to search at (can break this
                  //  w/quiescing).  "maxDepth == 0" implies that we can make
                  //  one half-move/ply, and then we must evaluate (or quiesce).
    int depth;    // Depth we are currently searching at (searching from root
                  //  == 0).
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
void ThinkerCmdThinkEx(ThinkContextT *th, Board *board, MoveList *mvlist);
void ThinkerCmdThink(ThinkContextT *th, Board *board);
void ThinkerCmdPonderEx(ThinkContextT *th, Board *board, MoveList *mvlist);
void ThinkerCmdPonder(ThinkContextT *th, Board *board);
void ThinkerCmdMoveNow(ThinkContextT *th);
void ThinkerCmdBail(ThinkContextT *th);

static inline bool ThinkerCompNeedsToMove(ThinkContextT *th)
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
void ThinkerRspDraw(ThinkContextT *th, MoveT move);
void ThinkerRspMove(ThinkContextT *th, MoveT move);
void ThinkerRspResign(ThinkContextT *th);
void ThinkerRspSearchDone(ThinkContextT *th);
void ThinkerRspNotifyStats(ThinkContextT *th, CompStatsT *stats);
void ThinkerRspNotifyPv(ThinkContextT *th, PvRspArgsT *pvArgs);
eThinkMsgT ThinkerCompWaitThinkOrPonder(ThinkContextT *th);
eThinkMsgT ThinkerCompWaitSearch(ThinkContextT *th);

// Operations on slave threads.
int ThinkerSearcherGetAndSearch(int alpha, int beta, MoveT move);
PositionEvalT ThinkerSearchersWaitOne(MoveT *move, PvT *pv);
void ThinkerSearchersBail(void);

void ThinkerSearchersMoveMake(MoveT move);
void ThinkerSearchersMoveUnmake();
int ThinkerSearchersSearching(void);
void ThinkerSearchersBoardSet(Board *board);
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
