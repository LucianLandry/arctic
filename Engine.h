//--------------------------------------------------------------------------
//                 Engine.h - a top-level engine control API
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#ifndef ENGINE_H
#define ENGINE_H

#include <functional>    // std::function
#include <memory>        // std::unique_ptr

#include "Config.h"
#include "EngineTypes.h"
#include "Thinker.h"

class Engine
{
public:
    Engine(); // ctor
    ~Engine(); // dtor

    void CmdNewGame();
    void CmdSetBoard(const Board &board);
    void CmdMakeMove(MoveT move);
    void CmdUnmakeMove();
    
    void CmdThink(const Clock &myClock, const MoveList &mvlist);
    void CmdThink(const Clock &myClock);
    void CmdPonder(const MoveList &mvlist);
    void CmdPonder();
    // 'curDepth': how many plies away from the root node we are at (before
    //             'move')
    // 'maxDepth': depth we are authorized to search at.  "curDepth == maxDepth"
    //             implies we can make one half-move/ply, and then we must
    //             evaluate (or quiesce).
    // Explicitly passing 'curDepth' facilitates using the hintPv, while
    //  explicitly passing 'maxDepth' gives the potential of passing a
    //  fractional depth in the future.
    void CmdSearch(int alpha, int beta, MoveT move, int curDepth, int maxDepth);
    void CmdMoveNow();
    void CmdBail();

    bool IsThinking() const;
    bool IsPondering() const;
    bool IsSearching() const;
    bool IsBusy() const;

    // Return *clock* time we want to move at.  For instance if == 30000000, we
    //  want to move when there is 30 seconds left on our clock.  When
    //  CLOCK_TIME_INFINITE, we should rely on the Thinker to move itself.
    // This is a bit bizarre compared to just returning the absolute time we
    //  want to move at, but it helps us with displaying ticks, and time
    //  management should be internal in the future anyway.
    inline bigtime_t GoalTime() const;
    
    // Returns a poll()able object that alerts you to call ProcessOneRsp().
    int MasterSock() const;

    using RspDrawFunc = std::function<void(Engine &, MoveT)>;
    using RspMoveFunc = std::function<void(Engine &, MoveT)>;
    using RspResignFunc = std::function<void(Engine &)>;
    using RspNotifyStatsFunc =
        std::function<void(Engine &, const EngineStatsT &)>;
    using RspNotifyPvFunc =
        std::function<void(Engine &, const EnginePvArgsT &)>;
    using RspSearchDoneFunc =
        std::function<void(Engine &, const EngineSearchDoneArgsT &)>;

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
    
    inline class Config &Config();
    
private:
    int masterSock; // Sends commands and receives responses.

    // Actual thinking happens on its own thread, and manipulates 'th'.
    std::unique_ptr<Thinker> th;

    enum class State : uint8
    {
        Idle,
        Pondering,
        Thinking,
        Searching
    };
    State state;
    bigtime_t goalTime;
    class Config config;
    RspHandlerT rspHandler;

    void calcGoalTime(const Clock &myClock);
    void sendCmd(Thinker::Message cmd) const;
    Thinker::Message recvRsp(void *buffer, int bufLen);
    void doThink(Thinker::Message cmd, const MoveList *mvlist);
    void restoreState(State state);
    void onMaxDepthChanged(const Config::SpinItem &item);
    void onMaxNodesChanged(const Config::SpinItem &item);
    void onRandomMovesChanged(const Config::CheckboxItem &item);
    void onCanResignChanged(const Config::CheckboxItem &item);
    void onHistoryWindowChanged(const Config::SpinItem &item);
    void onMaxMemoryChanged(const Config::SpinItem &item);
    void onMaxThreadsChanged(const Config::SpinItem &item);
};

inline bool Engine::IsThinking() const
{
    return state == State::Thinking;
}

inline bool Engine::IsPondering() const
{
    return state == State::Pondering;
}

inline bool Engine::IsSearching() const
{
    return state == State::Searching;
}

inline bool Engine::IsBusy() const
{
    return state != State::Idle;
}

inline int Engine::MasterSock() const
{
    return masterSock;
}

inline bigtime_t Engine::GoalTime() const
{
    return IsThinking() ? goalTime : CLOCK_TIME_INFINITE;
}

inline class Config &Engine::Config()
{
    return config;
}

#endif // ENGINE_H
