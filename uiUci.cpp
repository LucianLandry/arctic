//--------------------------------------------------------------------------
//          uiUci.cpp - UCI (Universal Chess Interface) interface.
//                           -------------------
//  copyright            : (C) 2009 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

// Implementation notes:
//
// The UCI spec is not completely precise about what "GUI to Engine" commands
//  are allowed when an engine is in active search.  I am specifically concerned
//  about the "ucinewgame" and "position" commands.
// IMO, when a sane GUI wanted to setup a new position during an active search,
//  it would send "stop", wait for a "bestmove" response (and possibly ignore
//  it), then setup a new position/game and "go" from there.
// However, if "ucinewgame" and/or "position" are allowed during active search,
//  it would imply that when the engine gets these commands, it should continue
//  calculating on its current position while a new position is setup (unless
//  the search terminated due to depth/time limits etc.)
// Then a new "go" command would force the engine to send a bestmove for the
//  original position before it started searching the new position.  (because
//  "for every "go" command a "bestmove" command is needed!")
// My first instinct in this situation was to force an implicit "stop" command
//  through to the engine, but if such commands are really not supposed to
//  come, then according to the spec we should do the less GUI-friendly thing
//  and ignore them completely.
//
// Since:
// -- I am not familiar w/any UCI GUIs' behavior yet
// -- I think trying to clear a hashtable (or more likely delaying it) due to
//    "ucinewgame" while an active search is going on is scary
// -- the infrastructure is not really built to setup new positions while
//    searching old ones
// -- and it complicates the code to add a corner case like this that perhaps
//    we will never see
//
// ... the current code goes for the "ignore" route (although we will print out
// big fat warnings about it).  If this turns out to be the wrong decision, it
// will need to be revisited.

// If I was even more of a language lawyer I might claim that two back-to-back
// "go" commands might be acceptable since the language from the spec:
//
// 'Before the engine is asked to search on a position, there will always be a
// position command to tell the engine about the current position.'
//
// ... could be interpreted as "you need to send the position command at least
// once at the start of game, but after that you may "go" with impunity".  But I
// think the word "always" makes the real meaning sufficiently unambiguous.

// An unrelated issue is, the spec does not mention when it is allowed for
// the engine to send a nullmove ("0000").  Currently, we choose to send a
// nullmove when we would normally resign or claim a draw.  Presumably we could
// also send it as part of a PV if we implemented null-move pruning.

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gPreCalc.h"
#include "log.h"
#include "MoveList.h"
#include "stringUtil.h"
#include "ui.h"
#include "uiUtil.h"

// This should change on-the-fly to csKxR if we ever implement chess960.
static const MoveStyleT gMoveStyleUCI = {mnCAN, csK2, false};

// The UCI "position" command can be very large (polyglot likes to send
// the starting position + all the moves; fifty-move draws are claimed,
// not automatic; and arbitrary amounts of whitespace are also allowed.
// Still, if we exceed this, we probably have a problem, so bail.
#define MAXBUFLEN (1 * 1024 * 1024)

// Forward declarations.
static void uciNotifyMove(Game *game, MoveT move);

enum class UciState
{
    Idle,          // initial state
    HasPosition,   // received *valid* "position" command (ready to go)
    // Note: we may be in the below states even if the computer is not
    //  not technically searching (ex. found mate or draw, or hit depth limit).
    // In that case we are just waiting for a stop cmd.
    PonderOneMove, // pondering the pondermove UCI wanted us to ponder
    PonderAll,     // pondering all moves
    Thinking       // actually searching (not pondering)
};

static struct
{
    bool bDebug;          // Are we in debug mode or not?
    bool bGotUciNewGame;  // Got a "ucinewgame" command at least once, which
                          //  lets us know the GUI supports it.
    int initialTime[2];   // Possible starting times on the w/b clock, in msec
    MoveT ponderMove;     // Move the GUI wants us to ponder on.  (We sometimes
                          //  ignore this advice, but we use it to massage
                          //  reported PVs etc.)
    UciState state;       // What state are we in?

    // This is preserved state from the "go" command.  We may refer back to this
    //  when processing 'ponderhit' or responses.
    struct
    {
        Clock clocks[2];
        MoveList searchList; // list of moves we are supposed to search on.
        // When true, we are supposed to not stop searching.  In reality, if we
        //  do stop searching, we cache the search results and do not inform
        //  the GUI until it directs us to stop.
        bool isInfinite;
    } goState;
    
    // Cached results from the engine.
    struct
    {
        MoveT bestMove;
        MoveT ponderMove;
    } result;

    Switcher *sw;
} gUciState;

static const char *uciStateString()
{
    switch (gUciState.state)
    {
        case UciState::Idle:          return "idle";
        case UciState::HasPosition:   return "hasPosition";
        case UciState::PonderOneMove: return "ponderOneMove";
        case UciState::PonderAll:    return "ponderAll";
        case UciState::Thinking:      return "thinking";
        default: break;
    }
    return "(unknown!)";
}

static bool isSearching()
{
    switch (gUciState.state)
    {
        case UciState::Idle:
        case UciState::HasPosition:
            return false;
        default:
            break;
    }
    return true;
}

static void moveToIdleState(Game *game)
{
    if (gUciState.state == UciState::Idle)
        return;
    game->StopAndForce();
    game->SetPonder(false);
    gUciState.ponderMove = MoveNone;
    gUciState.result.bestMove = MoveNone;
    gUciState.result.ponderMove = MoveNone;
    gUciState.state = UciState::Idle;
}

static void uciNotifyError(char *reason)
{
    printf("info string error: %s\n", reason);
}

static const char *findRecognizedToken(const char *pStr)
{
    for (; pStr != NULL; pStr = findNextToken(pStr))
    {
        // Check all engine-to-GUI commands.
        if (matches(pStr, "xboard") ||
            matches(pStr, "uci") ||
            matches(pStr, "debug") ||
            matches(pStr, "isready") ||
            matches(pStr, "setoption") ||
            matches(pStr, "register") ||
            matches(pStr, "ucinewgame") ||
            matches(pStr, "position") ||
            matches(pStr, "go") ||
            matches(pStr, "stop") ||
            matches(pStr, "ponderhit") ||
            matches(pStr, "quit"))
        {
            return pStr;
        }
    }

    // hit newline w/out finding a good token.
    return NULL;
}

static void uciInit(Game *game, Switcher *sw)
{
    static bool initialized;

    if (initialized)
        return;

    // Set unbuffered I/O (obviously necessary for output, also necessary for
    // input if we want to poll() correctly.)
    setbuf(stdout, NULL);
    setbuf(stdin, NULL);

    // gUciState is already cleared (since it is global).
    // Initialize whatever fields we need to.
    gUciState.ponderMove = MoveNone;
    gUciState.result.bestMove = MoveNone;
    gUciState.result.ponderMove = MoveNone;
    gUciState.sw = sw;
    
#if 1 // bldbg: goes out for debugging
    // Use random moves by default.
    game->EngineConfig().SetCheckbox(Config::RandomMovesCheckbox, true);
#endif
    // There is no standard way I am aware of for a UCI engine to resign
    //  so until I figure out how Polyglot might interpret it, we must play
    //  until the bitter end.  Even then, we probably want to work w/all UCI
    //  interfaces.
    game->EngineConfig().SetCheckbox(Config::CanResignCheckbox, false);
    game->SetAutoPlayEngineMoves(false);
    // FIXME we don't want to do this until later (because initializing the
    //  transposition table can take some time), but for now it's okay.
    uiPrepareEngines(game);
    initialized = true;
}

void processUciCommand(Game *game, Switcher *sw)
{
    char hashString[100] = "";
    char threadsString[100] = "";
    int rv;

    uciInit(game, sw);
    const Config::SpinItem *sItem =
        game->EngineConfig().SpinItemAt(Config::MaxMemorySpin);
    if (gPreCalc.userSpecifiedHashSize == -1 && sItem != nullptr)
    {
        rv = snprintf(hashString, sizeof(hashString),
                      "option name Hash type spin default %d min 0 max %d\n",
                      sItem->Value(), sItem->Max());
        // bail on truncated string.
        assert(rv >= 0 && (uint) rv < sizeof(hashString));
    }
    sItem = game->EngineConfig().SpinItemAt(Config::MaxThreadsSpin);
    if (gPreCalc.userSpecifiedNumThreads == -1 && sItem != nullptr)
    {
        rv = snprintf(threadsString, sizeof(threadsString),
                      "option name Threads type spin default %d min 1 max %d\n",
                      sItem->Value(), sItem->Max());
        // bail on truncated string.
        assert(rv >= 0 && (uint) rv < sizeof(threadsString));
    }
    
    // Respond appropriately to the "uci" command.
    printf("id name arctic %s.%s-%s\n"
           "id author Lucian Landry\n"
           "%s%s"
           // Though we do not care what "Ponder" is set to, we must
           // provide it as an option to signal (according to UCI) that the
           // engine can ponder at all.
           "option name Ponder type check default true\n"
           "option name RandomMoves type check default true\n"
           "option name UCI_EngineAbout type string default arctic %s.%s-%s by"
           " Lucian Landry\n"
           "uciok\n",
           VERSION_STRING_MAJOR, VERSION_STRING_MINOR, VERSION_STRING_PHASE,
           hashString, threadsString,
           VERSION_STRING_MAJOR, VERSION_STRING_MINOR, VERSION_STRING_PHASE);

    // switch to uiUci if we have not already.
    gUI = uiUciOps();
}

// Note: if we receive what we think is a bad 'position' command, we currently
//  ignore it.  This does mean that if we received a previous 'position' cmd,
//  we might think on a position the GUI didn't mean for us to.  (By the spec,
//  I believe the behavior is undefined.)
static void processPositionCommand(Game *game, const char *pToken)
{
    if (isSearching())
    {
        // See "Implementation Notes" for why we ignore this.
        reportError(false, "%s: received 'position' in state %s, ignoring",
                    __func__, uciStateString());
        return;
    }

    bool isFen = matches(pToken, "fen");
    if (!isFen && !matches(pToken, "startpos"))
    {
        // got an unknown token where we should have seen "fen" or "startpos".
        reportError(false, "%s: !fen and !startpos, giving up", __func__);
        return;
    }

    pToken = findNextToken(pToken);

    Board fenBoard;
    if (fenToBoard(isFen ? pToken : FEN_STARTSTRING, &fenBoard) < 0)
    {
        reportError(false, "%s: fenToBoard failed, cannot build position",
                    __func__);
        return;
    }    
    if (isFen)
    {
        // skip past the fenstring
        for (int i = 0; i < 6; i++)
            pToken = findNextToken(pToken);
    }

    // Now we expect a 'moves' token.
    // The document makes it slightly ambiguous whether no 'moves' token is
    //  okay if there are no actual moves, so we allow it.
    if (!matches(pToken, "moves") && pToken != NULL)
    {
        reportError(false, "%s: got unknown token where should have seen "
                    "'moves', giving up", __func__);
        return;
    }

    pToken = findNextToken(pToken); // skip past 'moves' token (if any)

    // 'tmpBoard' represents the starting position of the current game.
    Board tmpBoard = game->Board();
    for (int i = tmpBoard.Ply() - tmpBoard.BasePly(); i > 0; i--)
        tmpBoard.UnmakeMove();

    bool needNewGame = false;
    if (!gUciState.bGotUciNewGame &&
        fenBoard.Position() != tmpBoard.Position())
    {
        // If the new starting position is different, and we haven't received
        //  "ucinewgame", assume we need a newgame.
        needNewGame = true;
    }

    MoveT myMove = MoveNone;
    for(; pToken != NULL; pToken = findNextToken(pToken))
    {
        // As we find moves, play them on the board.
        if (!isLegalMove(pToken, &myMove, &fenBoard))
        {
            reportError(false, "%s: illegal move '%s', giving up",
                        __func__, pToken);
            return;
        }
        fenBoard.MakeMove(myMove);
    }

    // At this point we know the 'position' command is good.  Set everything up.
    if (needNewGame)
        game->NewGame(fenBoard, true);
    else
        game->SetBoard(fenBoard);
    gUciState.ponderMove = myMove;
    gUciState.state = UciState::HasPosition;
}

// Helper function.  Attempt to convert the next token after this one
// (presumed to be an argument) to an integer.
// "atLeast" is a sanity check which is disabled if < 0.
// Return -1 if we failed, 0 otherwise
static int convertNextInteger(const char **pToken, int *result, int atLeast,
                              const char *context)
{
    *pToken = findNextToken(*pToken);
    if (*pToken == NULL ||
        sscanf(*pToken, "%d", result) < 1 ||
        (atLeast >= 0 && *result < atLeast))
    {
        reportError(false, "%s: failed converting arg for %s",
                    __func__, context);
        return 1;
    }
    return 0;
}

// As above, but converts a 64-bit integer.
static int convertNextInteger64(const char **pToken, int64 *result,
                                int64 atLeast, const char *context)
{
    *pToken = findNextToken(*pToken);
    if (*pToken == NULL ||
        sscanf(*pToken, "%" PRId64, result) < 1 ||
        (atLeast >= 0 && *result < atLeast))
    {
        reportError(false, "%s: failed converting arg for %s",
                    __func__, context);
        return 1;
    }
    return 0;
}

static void processSetOptionCommand(Game *game, const char *inputStr)
{
    int64 hashSizeMiB;
    int numThreads;
    const char *pToken;

    if (isSearching())
    {
        // The UCI spec says this command "will only be sent when the engine
        //  is waiting".
        reportError(false, "%s: received 'setoption' in state %s, ignoring",
                    __func__, uciStateString());
        return;
    }
    if (!matches(inputStr, "name"))
    {
        reportError(false, "%s: expected 'name' token is missing, ignoring",
                    __func__);
        return;
    }
    pToken = findNextToken(inputStr);

    // Process RandomMoves option if applicable.
    // (UCI spec says option names and values "should not be case sensitive".)
    if (matchesNoCase(pToken, "RandomMoves") &&
        matches((pToken = findNextToken(pToken)), "value") &&
        (matchesNoCase((pToken = findNextToken(pToken)), "true") ||
         matchesNoCase(pToken, "false")))
    {
        game->EngineConfig().SetCheckbox(Config::RandomMovesCheckbox,
                                         !strcasecmp(pToken, "true"));
    }
    else if (gPreCalc.userSpecifiedHashSize == -1 &&
             matchesNoCase(pToken, "Hash") &&
             matches((pToken = findNextToken(pToken)), "value") &&
             convertNextInteger64(&pToken, &hashSizeMiB, 0, "Hash") == 0)
    {
        game->EngineConfig().SetSpinClamped(Config::MaxMemorySpin, hashSizeMiB);
    }
    else if (gPreCalc.userSpecifiedNumThreads == -1 &&
             matchesNoCase(pToken, "Threads") &&
             matches((pToken = findNextToken(pToken)), "value") &&
             convertNextInteger(&pToken, &numThreads, 1, "Threads") == 0)
    {
        game->EngineConfig().SetSpinClamped(Config::MaxThreadsSpin, numThreads);
    }
    else if (matchesNoCase(pToken, "Ponder") &&
             matches((pToken = findNextToken(pToken)), "value") &&
             (matchesNoCase((pToken = findNextToken(pToken)), "true") ||
              matchesNoCase(pToken, "false")))
    {
        // No-op.  This is just a hint to the engine; UCI controls when we
        //  ponder.
        ; 
    }
    else
    {
        printf("info string %s: ignoring unknown option string \"%s\"\n",
               __func__, inputStr);
    }
}

static void processUciNewGameCommand(Game *game)
{
    if (isSearching())
    {
        // See "Implementation Notes" for why we ignore this.
        reportError(false, "%s: received 'ucinewgame' in state %s, ignoring",
                    __func__, uciStateString());
        return;
    }

    // Just setup a new game.
    moveToIdleState(game);
    game->NewGame();
    gUciState.bGotUciNewGame = true;
    gUciState.initialTime[0] = 0;
    gUciState.initialTime[1] = 0;
}

static void processGoCommand(Game *game, const char *pToken)
{
    if (gUciState.state != UciState::HasPosition)
    {
        // See "Implementation Notes" for why we ignore some other states.
        reportError(false, "%s: received 'go' in state %s, ignoring",
                    __func__, uciStateString());
        return;
    }

    const Board &board = game->Board(); // shorthand.

    // Some temp state.  These are all processed at once after the entire
    // command is validated.
    // We take "infinite" to have a special meaning.  The actual search may
    // stop, but we will not report it until we receive a "stop" command.
    bool bPonder = false, bInfinite = false;
    // (I am hesitant to use 0 or negative numbers here as special values
    //  since in theory a clock could go negative.)
    int times[2] = {INT_MAX, INT_MAX};
    int incs[2] = {0, 0};
    int movestogo = -1;
    int depth = 0;
    int nodes = 0;
    int movetime = -1;
    int mate = -1;
    MoveList searchList;

    for (;
         pToken != NULL;
         pToken = findNextToken(pToken))
    {
        // I do not expect these sub-tokens to come in in any particular order.
        //  Thus, an "illegal" move for 'searchmoves' might actually be
        //  something like 'ponder' or 'wtime' (etc.).
        if (matches(pToken, "searchmoves"))
        {
            MoveT myMove;
            for (;
                 // (we do this check-next-token-instead-of-current-one
                 //  goofiness just to make the outer loop work correctly.)
                 isMove(findNextToken(pToken));
                 pToken = findNextToken(pToken))
            {
                if (!isLegalMove(findNextToken(pToken), &myMove, &board))
                {
                    char moveStr[MOVE_STRING_MAX];
                    reportError(false, "%s: illegal move '%s', ignoring "
                                "entire 'go' command",
                                __func__,
                                myMove.ToString(moveStr, &gMoveStyleUCI, NULL));
                    return;
                }
                searchList.AddMove(myMove, board);
            }
        }
        else if (matches(pToken, "ponder"))
        {
            // The UCI document doesn't specify what to do with this, so I
            //  suspect it is nonsensical.
            if (gUciState.ponderMove == MoveNone)
            {
                reportError(false, "%s: cannot ponder when no pondermove; "
                            "ignoring \"go\" command\n", __func__);
                return;
            }
            bPonder = true;
        }
        else if (matches(pToken, "wtime"))
        {
            if (convertNextInteger(&pToken, &times[0], -1, "wtime") < 0)
                return;
        }
        else if (matches(pToken, "btime"))
        {
            if (convertNextInteger(&pToken, &times[1], -1, "btime") < 0)
                return;
        }
        else if (matches(pToken, "winc"))
        {
            if (convertNextInteger(&pToken, &incs[0], -1, "winc") < 0)
                return;
        }
        else if (matches(pToken, "binc"))
        {
            if (convertNextInteger(&pToken, &incs[1], -1, "binc") < 0)
                return;
        }
        else if (matches(pToken, "movestogo"))
        {
            if (convertNextInteger(&pToken, &movestogo, 1, "movestogo") < 0)
                return;
        }
        else if (matches(pToken, "depth"))
        {
            if (convertNextInteger(&pToken, &depth, 1, "depth") < 0)
                return;
        }
        else if (matches(pToken, "nodes"))
        {
            if (convertNextInteger(&pToken, &nodes, 1, "nodes") < 0)
                return;
        }
        else if (matches(pToken, "mate"))
        {
            if (convertNextInteger(&pToken, &mate, 0, "mate") < 0)
                return;
        }
        else if (matches(pToken, "movetime"))
        {
            if (convertNextInteger(&pToken, &movetime, 0, "movetime") < 0)
                return;
        }
        else if (matches(pToken, "infinite"))
        {
            bInfinite = true;
        }
        else
        {
            reportError(false, "%s: unknown token sequence '%s', ignoring "
                        "entire 'go' command", __func__, pToken);
            return;
        }
    }

    // At this point, we know we have a valid command.
    game->EngineConfig().SetSpin(Config::MaxDepthSpin, depth);
    game->EngineConfig().SetSpin(Config::MaxNodesSpin, nodes);

    if (mate > 0)
    {
        // We interpret the 'mate' command as
        //  ('we are getting checkmated' || 'we are checkmating') in x moves.
        // With our current (almost-)full search window, we should not need
        //  any further customization for mates, but if we change that we would.
        // In the future this should be set via a specialized config variable.
        game->EngineConfig()
            .SetSpin(Config::MaxDepthSpin,
                     depth == 0 ? mate * 2 : MIN(depth, mate * 2));
    }

    // Setup our clocks.
    Clock *clocks = gUciState.goState.clocks; // shorthand

    for (int i = 0; i < NUM_PLAYERS; i++)
    {
        if (times[i] != INT_MAX)
        {
            // (* 1000: msec -> usec)
            clocks[i].SetTime(bigtime_t(times[i]) * 1000);
            if (gUciState.bGotUciNewGame && gUciState.initialTime[i] == 0)
                gUciState.initialTime[i] = times[i];
        }
        // UCI does not strictly forbid negative increment (that would be
        //  interesting) ... but we can't handle it.
        incs[i] = MAX(incs[i], 0);
        clocks[i].SetIncrement(bigtime_t(incs[i]) * 1000);

        if (movestogo > 0)
        {
            // "movestogo" is tricky (and kind of dumb) since there is not
            //  necessarily an indication of how much time we will add when the
            //  next time control begins.  But, in keeping w/biasing more time
            //  for earlier moves, we assume we can (almost) run out the clock
            //  and the next time control will replenish it.
            // So for starters, assume a 60-minute time control.  That should be
            //  long enough.
            // What happens when we are playing white and we get a ponderhit?
            //  ... Well, we only bump timecontrol after *our* move.
            clocks[i]
                .SetStartTime(gUciState.initialTime[i] ?
                              ((bigtime_t) gUciState.initialTime) * 1000 :
                              ((bigtime_t) 60) * 60 * 1000000)
                .SetNumMovesToNextTimeControl(movestogo);
        }
        if (movetime >= 0)
            clocks[i].SetPerMoveLimit(bigtime_t(movetime) * 1000);
        game->SetClock(i, clocks[i]);
    }
    gUciState.goState.searchList = searchList;
    gUciState.goState.isInfinite = bInfinite;
    
    gUciState.state =
        bPonder && searchList.NumMoves() ? UciState::PonderOneMove :
        bPonder ? UciState::PonderAll :
        UciState::Thinking;

    if (gUciState.state == UciState::Thinking)
    {
        game->SetEngineControl(board.Turn(), true);
    }
    else // we are pondering
    {
        if (gUciState.state == UciState::PonderAll)
        {
            // According to the spec "the last move sent in in (sic) the
            //  position string is the ponder move".  Since we want to ponder on
            //  different moves, we need to start 1 move back.
            game->Rewind(1);
        }
        game->SetPonder(true);
        game->SetEngineControl(board.Turn() ^ 1, true);
    }
    game->Go(searchList);
}

static void processPonderHitCommand(Game *game)
{
    if (gUciState.state != UciState::PonderOneMove &&
        gUciState.state != UciState::PonderAll)
    {
        reportError(false, "%s: received 'ponderhit' in state %s, ignoring",
                    __func__, uciStateString());
        return;
    }

    if (gUciState.state == UciState::PonderAll)
    {
        // Assumes searchList is empty.  Just let the engine proceed.
        game->MakeMove(gUciState.ponderMove);
    }
    else // We were pondering on one move
    {
        game->StopAndForce();
        int turn = game->Board().Turn(); // shorthand
        // Our own clock was run down; we should restore it.
        game->SetClock(turn, gUciState.goState.clocks[turn]);
        game->SetEngineControl(turn, true);
    }
    game->Go(gUciState.goState.searchList);
    // We preserve the rest of our state (infinite, mate, etc)
    gUciState.state = UciState::Thinking;
}

static void processStopCommand(Game *game)
{
    if (!isSearching())
    {
        reportError(false, "%s: received 'stop' in state %s, ignoring",
                    __func__, uciStateString());
        return;
    }

    game->MoveNow();
    if (gUciState.goState.isInfinite || gUciState.state != UciState::Thinking)
    {
        gUciState.goState.isInfinite = false;
        // Kludgy, just get us out of pondering state so we can actually send
        //  the bestmove.
        gUciState.state = UciState::Thinking;
        // Could not notify of the move while infinite; so try again now.
        uciNotifyMove(game, gUciState.result.bestMove);
    }
}

// This runs as a coroutine with the main thread, and can switch off to it at
// any time.  If it exits it will immediately be called again.

// One possibility if we set a bad position or otherwise get into a bad
// state is to just let the computer play null moves until a good position
// is set.
static void uciPlayerMove(Game *game)
{
    const char *inputStr;

    // Skip past any unrecognized stuff.
    inputStr =
        findRecognizedToken(
            ChopBeforeNewLine(getStdinLine(MAXBUFLEN, gUciState.sw)));

    if (matches(inputStr, "xboard"))
    {
        // Special case: switch to xboard interface.
        return processXboardCommand(game, gUciState.sw);
    }
    else if (matches(inputStr, "uci"))
    {
        processUciCommand(game, gUciState.sw);
    }
    else if (matches(inputStr, "debug"))
    {
        // Force debugging on if we get a bad arg, under the theory that we
        // would like to debug the problem :P
        gUciState.bDebug = !matches(findNextToken(inputStr), "off");
    }
    else if (matches(inputStr, "isready"))
    {
        // If we can process the command, I suppose we are ready...
        printf("readyok\n");
    }
    else if (matches(inputStr, "setoption"))
    {
        processSetOptionCommand(game, findNextToken(inputStr));
    }
    else if (matches(inputStr, "register"))
    {
        // We always allow attempts to register this engine.
        printf("registration checking\n"
               "registration ok\n");
    }
    else if (matches(inputStr, "ucinewgame"))
    {
        processUciNewGameCommand(game);
    }
    else if (matches(inputStr, "position"))
    {
        processPositionCommand(game, findNextToken(inputStr));
    }
    else if (matches(inputStr, "go"))
    {
        processGoCommand(game, findNextToken(inputStr));
    }
    else if (matches(inputStr, "ponderhit"))
    {
        processPonderHitCommand(game);
    }
    else if (matches(inputStr, "stop"))
    {
        processStopCommand(game);
    }
    else if (matches(inputStr, "quit"))
    {
        game->StopAndForce();
        exit(0);
    }

    gUciState.sw->Switch(); // Wait for more input.
}

static void uciNotifyMove(Game *game, MoveT move)
{
    char tmpStr[MOVE_STRING_MAX], tmpStr2[MOVE_STRING_MAX];

    MoveT ponderMove = gUciState.result.ponderMove;
    bool bShowPonderMove = move != MoveNone;
    
    // When ponderAll, cannot actually show this as bestmove.
    // Hopefully we recorded something in the PV, though.
    if (gUciState.state != UciState::PonderAll)
        gUciState.result.bestMove = move;
    if (gUciState.goState.isInfinite ||
        // We are not supposed to return a move when pondering, either..
        gUciState.state != UciState::Thinking)
    {
        return;
    }
        
    move = gUciState.result.bestMove;
    printf("bestmove %s%s%s\n",
           (move != MoveNone ?
            move.ToString(tmpStr, &gMoveStyleUCI, NULL) :
            "0000"),
           bShowPonderMove ? " ponder " : "",
           (bShowPonderMove ?
            ponderMove.ToString(tmpStr2, &gMoveStyleUCI, NULL) : ""));
    moveToIdleState(game);
}

static void uciNotifyDraw(Game *game, const char *reason, MoveT *move)
{
    // UCI seems to rely on a GUI arbiter to claim draws, simply because there
    // is no designated way for the engine to do it.  Nevertheless, when we
    // have an automatic draw we send no-move.  This seems better than picking
    // an actual move which may be losing (as all possible moves may lose).  But
    // mostly, we need an actual example where we need to do something else
    // in order to justify complicating the engine code to say "I cannot claim
    // a draw but my opponent can, what is my best move".
    printf("info string engine claims a draw (reason: %s)\n", reason);
    uciNotifyMove(game, move != NULL ? *move : MoveNone);
}

static void uciNotifyResign(Game *game, int turn)
{
    // The info string is just for the benefit of a human trying to understand
    //  our output.  Since our resignation threshold is so low, we normally do
    //  not "resign" unless we are actually mated.
    printf("info string engine (turn %d) resigns\n", turn);
    uciNotifyMove(game, MoveNone);
}

// Assumes "result" is long enough to hold the actual result (say 80 chars to
// be safe).  Returns "result".
static char *buildStatsString(char *result, Game *game,
                              const EngineStatsT *stats)
{
    int nodes = stats->nodes;
    // (Convert bigtime_t to milliseconds)
    int timeTaken =
        game->Clock(game->Board().Turn()).TimeTaken() / 1000;
    int nps = (int) (((uint64) nodes) * 1000 / (timeTaken ? timeTaken : 1));
    int charsWritten;

    charsWritten = sprintf(result, "time %d nodes %d nps %d",
                           timeTaken, nodes, nps);
    const Config::SpinItem *sItem =
        game->EngineConfig().SpinItemAt(Config::MaxMemorySpin);
    if (sItem != nullptr && sItem->Value()) // non-empty hash?
    {
        sprintf(&result[charsWritten], " hashfull %d", stats->hashFullPerMille);
    }
    return result;
}

static void uciNotifyPV(Game *game, const EnginePvArgsT *pvArgs)
{
    bool bDisplayPv = true, bDisplayEval = true;
    DisplayPv pv = pvArgs->pv; // copy for modification
    Board board = game->Board(); // copy for modification
    
    if (gUciState.state == UciState::PonderAll)
    {
        // If we are not actually pondering on the suggested GUI move,
        // do not advertise the PV or eval (to avoid confusing the GUI).
        if (pv.Moves(0) != gUciState.ponderMove)
        {
            bDisplayEval = false;
            bDisplayPv = false;
        }
        else
        {
            // should always be legal, since it is the pondermove
            board.MakeMove(pv.Moves(0));
        }
        pv.Decrement(); // always do this since we want to display pv.level
    }

    // Save away a next move to ponder on, if possible.
    // (we may not be able to record bestMove from uciNotifyMove when
    // PonderAll).
    if (bDisplayEval && pv.Moves(0) != MoveNone)
    {
        gUciState.result.bestMove = pv.Moves(0);
        if (pv.Moves(1) != MoveNone)
            gUciState.result.ponderMove = pv.Moves(1);
    }
    
    char lanString[kMaxPvStringLen];
    if (bDisplayPv)
    {
        int moveCount = pv.BuildMoveString(lanString, sizeof(lanString),
                                           gMoveStyleUCI, board);
        if (moveCount < 1)
            bDisplayPv = false;
    }

    char evalString[20];
    if (bDisplayEval)
    {
        if (pv.Eval().DetectedWinOrLoss())
        {
            int movesToMate = pv.Eval().MovesToWinOrLoss();
            snprintf(evalString, sizeof(evalString), "mate %d",
                     pv.Eval().DetectedLoss() ? -movesToMate : movesToMate);
        }
        else
        {
            snprintf(evalString, sizeof(evalString), "cp %d",
                     pv.Eval().LowBound());
        }
    }

    char statsString[80];
    // Sending a fairly basic string here.
    printf("info depth %d %s%s%s%s%s%s\n",
           pv.Level() + 1,
           bDisplayEval ? "score " : "", bDisplayEval ? evalString : "",
               bDisplayEval ? " " : "",
           buildStatsString(statsString, game, &pvArgs->stats),
           bDisplayPv ? " pv " : "", bDisplayPv ? lanString : "");
}


static void uciNotifyComputerStats(Game *game, const EngineStatsT *stats)
{
    char statsString[80];

    printf("info %s\n", buildStatsString(statsString, game, stats));
}

static void uciPositionRefresh(const Position &position) { }
static void uciNoop(void) { }
static void uciStatusDraw(Game *game) { }
static void uciNotifyTick(Game *game) { }
static void uciNotifyCheckmated(int turn) { }

UIFuncTableT *uiUciOps(void)
{
    static UIFuncTableT uciUIFuncTable =
    {
        .init = uciInit,
        .playerMove = uciPlayerMove,
        .positionRefresh = uciPositionRefresh,
        .exit = uciNoop,
        .statusDraw = uciStatusDraw,
        .notifyTick = uciNotifyTick,
        .notifyMove = uciNotifyMove,
        .notifyError = uciNotifyError,
        .notifyPV = uciNotifyPV,
        .notifyThinking = uciNoop,
        .notifyPonder = uciNoop,
        .notifyReady = uciNoop,
        .notifyComputerStats = uciNotifyComputerStats,
        .notifyDraw = uciNotifyDraw,
        .notifyCheckmated = uciNotifyCheckmated,
        .notifyResign = uciNotifyResign,
    };

    return &uciUIFuncTable;
}
