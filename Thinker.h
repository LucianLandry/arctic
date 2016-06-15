//--------------------------------------------------------------------------
//           Thinker.h - chess-oriented message passing interface
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

#include <thread>

#include "aTypes.h"
#include "Board.h"
#include "Eval.h"
#include "MoveList.h"
#include "Pv.h"
#include "ref.h"
#include "saveGame.h"
#include "ThinkerTypes.h"

// FIXME, external code should not need to be aware of this.  That means
//  I need to do something about RecvRsp().
enum eThinkMsgT
{
    eCmdThink,   // full iterative-depth search
    eCmdPonder,  // ponder
    eCmdSearch,  // simple search on a sub-tree
    // eCmdMoveNow not needed until clustering

    eRspDraw,
    eRspMove,
    eRspResign,
    eRspStats,
    eRspPv,        // takes RspPvArgsT

    eRspSearchDone // takes RspSearchDoneArgsT
};

struct RspPvArgsT
{
    ThinkerStatsT stats;
    DisplayPv pv;
};

struct RspSearchDoneArgsT
{
    RspSearchDoneArgsT() : pv(0) {}
    RspSearchDoneArgsT(MoveT move, Eval eval, const SearchPv &pv) :
        move(move), eval(eval), pv(pv) {}
    MoveT move;
    Eval eval;
    SearchPv pv;
};

class Thinker
{
public:
    Thinker(); // ctor
    ~Thinker(); // dtor

    // ---------------------------------------------------------------------
    // Client/proxy-side methods:

    eThinkMsgT RecvRsp(void *buffer, int bufLen);
    void CmdNewGame();
    void CmdSetBoard(const Board &board);
    void CmdMakeMove(MoveT move);
    void CmdUnmakeMove();
    
    void CmdThink(const MoveList &mvlist);
    void CmdThink();
    void CmdPonder(const MoveList &mvlist);
    void CmdPonder();
    void CmdSearch(int alpha, int beta, MoveT move);
    
    void CmdMoveNow();
    void CmdBail();

    bool CompIsThinking() const;
    bool CompIsPondering() const;
    bool CompIsSearching() const;
    bool CompIsBusy() const;

    // return a poll()able object that says 'you can call RecvRsp()'
    int MasterSock() const;

    // ---------------------------------------------------------------------
    // Server/engine-side methods:
    // ---------------------------------------------------------------------
    void RspDraw(MoveT move) const;
    void RspMove(MoveT move) const;
    void RspResign() const;
    void RspSearchDone(MoveT move, Eval eval, const SearchPv &pv) const;
    void RspNotifyStats(const ThinkerStatsT &stats) const;
    void RspNotifyPv(const ThinkerStatsT &stats, const DisplayPv &pv) const;
    inline bool CompNeedsToMove() const;
    eThinkMsgT CompWaitThinkOrPonder() const;
    eThinkMsgT CompWaitSearch() const;
    inline bool IsRootThinker() const;

    struct ContextT
    {
        ContextT();      // ctor
        Board board;     // Internal board, used (and clobbered) by
                         //  think/ponder/search.  Set by CmdSetBoard().
        MoveList mvlist; // Limited list of moves we are allowed to think or
                         //  ponder on.  When empty (the usual state), we
                         //  think/ponder on all moves.  Set by CmdThink() and
                         //  CmdPonder().
        int maxDepth;    // Depth we are authorized to search at (can break this
                         //  w/quiescing).  "maxDepth == 0" implies that we can
                         //  make one half-move/ply, and then we must evaluate
                         //  (or quiesce).
        int depth;       // Depth we are currently searching at (searching from
                         //  root == 0).
    };
    // (used by engine to track/manipulate internal state)
    inline ContextT &Context();
    // ---------------------------------------------------------------------
    
private:
    // The masterSock sends commands and receives responses.
    // The slaveSock receives commands and sends responses.
    int masterSock, slaveSock;

    enum class State : uint8
    {
        Idle,
        Pondering,
        Thinking,
        Searching
    };

    State state;
    volatile bool moveNow;

    struct SearchArgsT
    {
        SearchArgsT(); // Need this since 'pv' has no default constructor

        // passed-in args.
        int alpha;          // (eCmdSearch)
        int beta;           // (eCmdSearch)

        // passed in and also out (but unaltered).
        MoveT move;         // (eCmdSearch)

        // passed-out args.
        SearchPv pv;        // (eCmdSearch ... not used for eRspPv)
        Eval eval;          // (eCmdSearch)
    } searchArgs;

    ContextT context;

    void threadFunc();
    std::thread *thread;

    void compSendCmd(eThinkMsgT cmd) const;
    void compSendRsp(eThinkMsgT rsp, const void *buffer, int bufLen) const;
    eThinkMsgT recvCmd(void *buffer, int bufLen) const;
    void doThink(eThinkMsgT cmd, const MoveList *mvlist);

    // There is (currently) one 'master' thinker that coordinates all of the
    //  other thinkers, which act as search threads.
    static Thinker *rootThinker;
};

inline bool Thinker::CompNeedsToMove() const
{
    return rootThinker->moveNow;
}

inline bool Thinker::CompIsThinking() const
{
    return state == State::Thinking;
}

inline bool Thinker::CompIsPondering() const
{
    return state == State::Pondering;
}

inline bool Thinker::CompIsSearching() const
{
    return state == State::Searching;
}

inline bool Thinker::CompIsBusy() const
{
    return state != State::Idle;
}

inline int Thinker::MasterSock() const
{
    return masterSock;
}

inline bool Thinker::IsRootThinker() const
{
    return this == rootThinker;
}

inline Thinker::ContextT &Thinker::Context()
{
    return context;
}

// Operations on slave threads.
bool ThinkerSearcherGetAndSearch(int alpha, int beta, MoveT move);
Eval ThinkerSearchersWaitOne(MoveT &move, SearchPv &pv);
void ThinkerSearchersBail();

void ThinkerSearchersMakeMove(MoveT move);
void ThinkerSearchersUnmakeMove();
int ThinkerSearchersAreSearching();
void ThinkerSearchersSetBoard(const Board &board);
void ThinkerSearchersSetDepthAndLevel(int depth, int level);
void ThinkerSearchersCreate(int numThreads); // Initialize searcher threads.

#endif // THINKER_H