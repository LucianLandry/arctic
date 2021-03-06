//--------------------------------------------------------------------------
//                Game.h - current game and associated state.
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#ifndef GAME_H
#define GAME_H

#include "aTypes.h"
#include "Board.h"
#include "Clock.h"
#include "Engine.h"
#include "SaveGame.h"

class Game
{
public:
    explicit Game(Engine *eng); // ctor

    void MakeMove(MoveT move);
    void NewGame(const Board &board, bool resetClocks);
    void NewGame();
    
    // These return -1 and do nothing if the ply was out of range.  On
    //  success, they return 0.
    int GotoPly(int ply);
    int Rewind(int numPlies);
    int FastForward(int numPlies);
    
    void ResetClocks();
    void LogClocks(const char *context) const;

    // Both of these return: "was there a state change".
    bool Stop(); // Enters force mode.  All engines bail.  Clocks are stopped.
    bool StopAndForce(); // like Stop(), but also resets all engine control.
                         // (ponder setting is still not affected, though)
    bool Go(); // Leave force mode.
    // Leave force mode; engines think only on moves in 'searchList' (for 1
    //  ply) unless searchList is empty, in which case the behavior is the same
    //  as Go().
    bool Go(const MoveList &searchList);
    
    void SetBoard(const Board &other);
    void SetInitialClock(int player, const Clock &other);
    void SetClock(int player, const Clock &other);
    void SetEngineControl(int player, bool value);
    void ToggleEngineControl(int player);
    void SetPonder(bool value);
    void TogglePonder();
    
    // FIXME: add an 'engineId' argument or something once we support multiple
    //  engines.
    Config &EngineConfig();

    void SetAutoPlayEngineMoves(bool value);

    // Force any engine playing the current side to move.  Synchronous.
    void MoveNow();
    // A synchronous way to wait for the active engine to stop thinking.
    void WaitForEngineIdle();

    // Save/restore functionality.
    int Save();
    int Restore();

    // Getters.
    int CurrentPly() const;
    int FirstPly() const;
    int LastPly() const;
    const class Board &Board() const; // return current board.
    const class Clock &Clock(int player) const; // return current clock
    // Returns the initial clock for any new game.
    const class Clock &InitialClock(int player) const;
    bool EngineControl(int player) const; // Is an engine playing this player?
    bool Ponder() const; // Is engine pondering enabled? (default: false)
    
private:
    enum class State
    {
        Stopped, // engines stopped, and should not alter state until 'Go()'
        Running
    } state;

    bool ponder; // should engine(s) be allowed to ponder or not
    bool autoPlayEngineMoves; // 'true' iff controlling UI desires that engine
                              //   moves be automatically played.
    // 'true' iff Game.has ended (draw/mate), or computer resigned the position.
    bool done;
    bool engineControl[NUM_PLAYERS]; // true iff engine plays for that side
    
    SaveGame sgame;

    // Clocks are reset to these values at beginning of new game.  They can be
    //  set w/out affecting saveGame's start clocks.
    class Clock initialClocks[NUM_PLAYERS];
    class Clock clocks[NUM_PLAYERS];    // Time control for both sides.

    class Board savedBoard;

    // Associated engine.  For now there is only one, but we will probably want
    //  to expand this if we start supporting multiple engines.
    Engine *eng;
    Position lastRefreshedPosition; // tracked for gUI->positionRefresh()

    MoveList searchList; // any particular moves we want to search on?
    
    void makeMove(MoveT move, bool moveEngines);
    void refresh();
    void stopClocks();

    // engine-response-handler methods.
    void sanityCheckBadRsp(const char *context) const;
    void onEngineRspDraw(Engine &eng, MoveT move);
    void onEngineRspMove(Engine &eng, MoveT move);
    void onEngineRspResign(Engine &eng);
    void onEngineRspNotifyStats(Engine &eng, const EngineStatsT &stats);
    void onEngineRspNotifyPv(Engine &eng, const EnginePvArgsT &pvArgs);
    void setEngineRspHandler(Engine &eng);
};

// These are wrappers for SaveGame.
inline int Game::CurrentPly() const
{
    return sgame.CurrentPly();
}
inline int Game::FirstPly() const
{
    return sgame.FirstPly();
}
inline int Game::LastPly() const
{
    return sgame.LastPly();
}

inline const class Board &Game::Board() const
{
    return savedBoard;
}

inline bool Game::Ponder() const
{
    return ponder;
}

#endif // GAME_H
