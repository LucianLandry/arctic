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

#include <thread>     // std::thread

#include "Board.h"
#include "Clock.h"
#include "EngineTypes.h"
#include "MoveList.h"

class Thinker
{
public:
    Thinker(int sock); // ctor
    ~Thinker(); // dtor

    // Currently, only claimed draws use RspDraw().  Automatic draws use
    // RspMove().
    void RspDraw(MoveT move) const;
    void RspMove(MoveT move) const;
    void RspResign() const;
    void RspNotifyStats(const EngineStatsT &stats) const;
    void RspNotifyPv(const EngineStatsT &stats, const DisplayPv &pv) const;
    void RspSearchDone(MoveT move, Eval eval, const SearchPv &pv) const;
    inline bool NeedsToMove() const;

    // Messages passed between Engine and Thinker threads.
    enum class Message
    {
        CmdThink,   // full iterative-depth search
        CmdPonder,  // ponder
        CmdSearch,  // simple search on a sub-tree
        // CmdMoveNow not needed until clustering
        
        RspDraw,      // takes MoveT
        RspMove,      // takes MoveT
        RspResign,    // takes (void)
        RspStats,     // takes EngineStatsT
        RspPv,        // takes EnginePvArgsT
        RspSearchDone // takes EngineSearchDoneArgsT
    };
    
    Message WaitThinkOrPonder() const;
    void WaitSearch() const;
    inline bool IsRootThinker() const;
    static inline Thinker &RootThinker();

    struct ContextT
    {
        ContextT();      // ctor
        Board board;     // Internal board, used (and clobbered) by
                         //  think/ponder/search.  Set by CmdSetBoard().
        Clock clock;     // Time we started thinking.  Set by CmdThink().
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

        // Signals that we should move; set by CmdMoveNow().
        volatile bool moveNow;

        struct
        {
            int alpha, beta;
            MoveT move;
            int curDepth, maxDepth;
        } searchArgs; // These args are set by CmdSearch().
        
        // Sub-searchers dump their results into here.
        EngineSearchDoneArgsT searchResult;
    };

    // (used by engine to track/manipulate internal state)
    inline ContextT &Context();

    static const int DepthNoLimit = -1;
    struct SharedContextT
    {
        SharedContextT(); // ctor

        // The following are currently only meaningful for the root thinker:
        // Config variable.  DepthNoLimit == no limit.  Like 'maxDepth', except
        //  'maxDepth' is manipulated by the engine.
        volatile int maxLevel;
        // Config variable.  0 == no limit.  Max nodes we are authorized to
        //  search.
        volatile int maxNodes;
        volatile bool randomMoves;
        volatile bool canResign;
        int maxThreads; // max searcher threads.

        // State that is shared between local thinkers because it would be
        //  a bad idea to not do so.
        HintPv pv; // Attempts to track the principal variation.
        EngineStatsT stats;
        int gameCount; // for debugging.
    };
    // (used by engine to track/manipulate internal state shared between
    //  threads)
    inline SharedContextT &SharedContext();

    static void sendMessage(int sock, Thinker::Message msg, const void *args,
                            int argsLen);
    static Message recvMessage(int sock, void *args, int argsLen);
    // Returns 'true' iff the computer went idle after sending 'msg'.
    static bool IsFinalResponse(Message msg);
private:
    int slaveSock; // Receives commands and sends responses.

    ContextT context;
    std::shared_ptr<SharedContextT> sharedContext;
    
    void threadFunc();
    std::thread *thread;

    void sendRsp(Message rsp, const void *args, int argsLen) const;
    Message recvCmd(void *args, int argsLen) const;
    
    // There is (currently) one 'master' thinker that coordinates all of the
    //  other thinkers, which act as search threads.
    static Thinker *rootThinker;
};

inline bool Thinker::NeedsToMove() const
{
    return rootThinker->context.moveNow;
}

inline bool Thinker::IsRootThinker() const
{
    return this == rootThinker;
}

inline Thinker &Thinker::RootThinker()
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

// Operations on searcher threads:
bool SearchersDelegateSearch(int alpha, int beta, MoveT move, int curDepth,
                             int maxDepth);
Eval SearchersWaitOne(MoveT &move, SearchPv &pv, Thinker &parent);
void SearchersBail();
void SearchersMakeMove(MoveT move);
void SearchersUnmakeMove();
bool SearchersAreSearching();
void SearchersSetBoard(const Board &board);
// Initialize searcher threads on the fly.  Should be called only when the
//  engine is idle.
void SearchersSetNumThreads(int numThreads);

#endif // THINKER_H
