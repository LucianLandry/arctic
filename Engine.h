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
    EventQueue rspQueue; // receives responses from Thinker.

    // Actual thinking happens on its own thread, and manipulates 'th'.
    std::unique_ptr<Thinker> th;

    Thinker::State state;

    enum class MoveNowState
    {
        IdleOrBusy,
        MoveNowRequested,
        BailRequested
    } moveNowState;
    class Config config;
    RspHandlerT rspHandler;

    void doThink(bool isPonder, const MoveList *mvlist);
    void restoreState(Thinker::State state);
    void onMaxDepthChanged(const Config::SpinItem &item);
    void onMaxNodesChanged(const Config::SpinItem &item);
    void onRandomMovesChanged(const Config::CheckboxItem &item);
    void onCanResignChanged(const Config::CheckboxItem &item);
    void onHistoryWindowChanged(const Config::SpinItem &item);
    void onMaxMemoryChanged(const Config::SpinItem &item);
    void onMaxThreadsChanged(const Config::SpinItem &item);

    void moveToIdleState();
    
    // non-final responses.
    void onRspNotifyStats(const EngineStatsT &stats);
    void onRspNotifyPv(const EnginePvArgsT &pv);
    // final responses.
    void onRspDraw(MoveT move);
    void onRspMove(MoveT move);
    void onRspResign();
    void onRspSearchDone(const EngineSearchDoneArgsT &args);
};

inline bool Engine::IsThinking() const
{
    return state == Thinker::State::Thinking;
}

inline bool Engine::IsPondering() const
{
    return state == Thinker::State::Pondering;
}

inline bool Engine::IsSearching() const
{
    return state == Thinker::State::Searching;
}

inline bool Engine::IsBusy() const
{
    return state != Thinker::State::Idle;
}

inline int Engine::MasterSock() const
{
    return rspQueue.PollableObject()->Fd();
}

inline class Config &Engine::Config()
{
    return config;
}

inline void Engine::ProcessOneRsp()
{
    rspQueue.RunOne();
}

inline void Engine::moveToIdleState()
{
    state = Thinker::State::Idle;
    moveNowState = MoveNowState::IdleOrBusy;
}

#endif // ENGINE_H
