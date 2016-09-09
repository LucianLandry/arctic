//--------------------------------------------------------------------------
//           Thinker.h - chess-oriented message passing interface
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

#ifndef THINKER_H
#define THINKER_H

#include <thread>

#include "aTypes.h"
#include "Board.h"
#include "Clock.h"
#include "Config.h"
#include "Eval.h"
#include "MoveList.h"
#include "Pv.h"
#include "ref.h"
#include "ThinkerTypes.h"

// FIXME, external code should not need to be aware of this.
enum eThinkMsgT
{
    eCmdThink,   // full iterative-depth search
    eCmdPonder,  // ponder
    eCmdSearch,  // simple search on a sub-tree
    // eCmdMoveNow not needed until clustering

    eRspDraw,      // takes MoveT
    eRspMove,      // takes MoveT
    eRspResign,    // takes (void)
    eRspStats,     // takes ThinkerStatsT
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

    void CmdNewGame();
    void CmdSetBoard(const Board &board);
    void CmdMakeMove(MoveT move);
    void CmdUnmakeMove();
    
    void CmdThink(const Clock &myClock, const MoveList &mvlist);
    void CmdThink(const Clock &myClock);
    void CmdPonder(const MoveList &mvlist);
    void CmdPonder();
    void CmdSearch(int alpha, int beta, MoveT move);
    
    void CmdMoveNow();
    void CmdBail();

    bool CompIsThinking() const;
    bool CompIsPondering() const;
    bool CompIsSearching() const;
    bool CompIsBusy() const;

    // Return *clock* time we want to move at.  For instance if == 30000000, we
    //  want to move when there is 30 seconds left on our clock.  When
    //  CLOCK_TIME_INFINITE, we should rely on the Thinker to move itself.
    // This is a bit bizarre compared to just returning the absolute time we
    //  want to move at, but it helps us with displaying ticks, and time
    //  management should be internal in the future anyway.
    inline bigtime_t GoalTime() const;
    
    // return a poll()able object that says 'you can call RecvRsp()'
    int MasterSock() const;

    using RspDrawFunc = std::function<void(Thinker &, MoveT)>;
    using RspMoveFunc = std::function<void(Thinker &, MoveT)>;
    using RspResignFunc = std::function<void(Thinker &)>;
    using RspNotifyStatsFunc =
        std::function<void(Thinker &, const ThinkerStatsT &)>;
    using RspNotifyPvFunc =
        std::function<void(Thinker &, const RspPvArgsT &)>;
    using RspSearchDoneFunc =
        std::function<void(Thinker &, const RspSearchDoneArgsT &)>;

    struct RspHandlerT
    {
        RspDrawFunc Draw;
        RspMoveFunc Move;
        RspResignFunc Resign;
        RspNotifyStatsFunc NotifyStats;
        RspNotifyPvFunc NotifyPv;
        RspSearchDoneFunc SearchDone;
    };
    void SetRspHandler(const RspHandlerT &rspHandler);
    void ProcessOneRsp();
    
    // ---------------------------------------------------------------------
    // Server/engine-side methods:

    // Currently, only claimed draws use RspDraw().  Automatic draws use
    // RspMove().
    void RspDraw(MoveT move) const;
    void RspMove(MoveT move) const;
    void RspResign() const;
    void RspNotifyStats(const ThinkerStatsT &stats) const;
    void RspNotifyPv(const ThinkerStatsT &stats, const DisplayPv &pv) const;
    void RspSearchDone(MoveT move, Eval eval, const SearchPv &pv) const;
    inline bool CompNeedsToMove() const;
    eThinkMsgT CompWaitThinkOrPonder() const;
    void CompWaitSearch() const;
    inline bool IsRootThinker() const;
    inline Thinker &RootThinker() const;

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

        // Searchers dump their results into here.
        RspSearchDoneArgsT searchResult;
    };

    // (used by engine to track/manipulate internal state)
    inline ContextT &Context();

    struct SharedContextT
    {
        SharedContextT();      // ctor
        // The following are currently only meaningful for the root thinker:
        // Config variable.  -1 == no limit.  Like 'maxDepth', except 'maxDepth'
        //  is manipulated by the engine.
        volatile int maxLevel;
        // Config variable.  0 == no limit.  Max nodes we are authorized to
        //  search.
        volatile int maxNodes;
        volatile bool randomMoves;
        volatile bool canResign;
        HintPv pv; // Attempts to track the principal variation.
    };
    // (used by engine to track/manipulate internal state shared between
    //  threads)
    inline SharedContextT &SharedContext();
    
    // ---------------------------------------------------------------------
    // methods common to both client and engine:
    inline class Config &Config();
    
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
    bigtime_t goalTime;
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
    std::shared_ptr<SharedContextT> sharedContext;
    class Config config;
    
    void threadFunc();
    std::thread *thread;

    RspHandlerT rspHandler;

    void calcGoalTime(const Clock &myClock);
    void compSendCmd(eThinkMsgT cmd) const;
    void compSendRsp(eThinkMsgT rsp, const void *buffer, int bufLen) const;
    eThinkMsgT recvCmd(void *buffer, int bufLen) const;
    eThinkMsgT recvRsp(void *buffer, int bufLen);
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

inline Thinker &Thinker::RootThinker() const
{
    return *rootThinker;
}

inline Thinker::ContextT &Thinker::Context()
{
    return context;
}

inline Thinker::SharedContextT &Thinker::SharedContext()
{
    return *sharedContext;
}

inline bigtime_t Thinker::GoalTime() const
{
    return CompIsThinking() ? goalTime : CLOCK_TIME_INFINITE;
}

inline class Config &Thinker::Config()
{
    return config;
}

// Operations on slave threads.
bool ThinkerSearcherGetAndSearch(int alpha, int beta, MoveT move);
Eval ThinkerSearchersWaitOne(MoveT &move, SearchPv &pv, Thinker &parent);
void ThinkerSearchersBail();

void ThinkerSearchersMakeMove(MoveT move);
void ThinkerSearchersUnmakeMove();
int ThinkerSearchersAreSearching();
void ThinkerSearchersSetBoard(const Board &board);
void ThinkerSearchersSetDepthAndLevel(int depth, int level);
// Initialize searcher threads.
void ThinkerSearchersCreate(int numThreads, Thinker &rootThinker);

#endif // THINKER_H
