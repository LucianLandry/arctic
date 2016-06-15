//--------------------------------------------------------------------------
//          uiUci.cpp - UCI (Universal Chess Interface) interface.
//                           -------------------
//  copyright            : (C) 2009 by Lucian Landry
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

// Implementation notes:
//
// The UCI spec is not completely precise about what "GUI to Engine" commands
// are allowed when an engine is in active search.  I am specifically concerned
// about the "ucinewgame" and "position" commands.
//
// IMO, when a sane GUI wanted to setup a new position during an active search,
// it would send "stop", wait for a "bestmove" response (and possibly ignore
// it), then setup a new position/game and "go" from there.
//
// However, if "ucinewgame" and/or "position" are allowed during active search,
// it would imply that when the engine gets these crazy commands, it should
// continue calculating on its current position while a new position is setup
// (unless the search terminated due to depth/time limits etc.)
// Then a new "go" command would force the engine to send a bestmove for the
// original position before it started searching the new position.  (because
// "for every "go" command a "bestmove" command is needed!")
//
// My first instinct in this situation was to force an implicit "stop" command
// through to the engine, but if such commands are really not supposed to
// come, then according to the spec we should do the less GUI-friendly thing
// and ignore them completely.
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

// If I was even more of a language lawyer I could claim that two back-to-back
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

#include "clockUtil.h"
#include "gDynamic.h"
#include "gPreCalc.h"
#include "log.h"
#include "MoveList.h"
#include "playloop.h"
#include "TransTable.h"
#include "ui.h"
#include "uiUtil.h"

// This should change on-the-fly to csKxR if we ever implement chess960.
static const MoveStyleT gMoveStyleUCI = {mnCAN, csK2, false};

// The UCI "position" command can be very large (polyglot likes to send
// the starting position + all the moves; fifty-move draws are claimed,
// not automatic; and arbitrary amounts of whitespace are also allowed.
// Still, if we exceed this, we probably have a problem, so bail.
#define MAXBUFLEN (1 * 1024 * 1024)


static struct {
    bool bDebug;          // are we in debug mode or not?
    bool bBadPosition;    // "invalid" position loaded (king in check?
                          // two kings? etc.) and cannot think on it.
    bool bGotUciNewGame;  // got a "ucinewgame" command at least once, which
                          // lets us know the GUI supports it.
    int initialTime[2];   // Possible starting times on the w/b clock, in msec

    MoveT ponderMove;     // Move the UI wants us to ponder on.  (We ignore this
                          // advice but we use it to massage reported PVs etc.)

    bool bSearching;      // Are we currently supposed to be searching a position?
                          // (may be true even if the computer technically has
                          // found mate or resigned, and is not actually
                          // searching)
    bool bPonder;         // are we in pondering mode or not.
    bool bInfinite;       // When true, we are supposed to not stop searching.
                          // What happens in reality is that if we do stop
                          // searching, we cache the search results and do not
                          // inform the UI until it directs us to stop.
    MoveList searchList;  // list of moves we are supposed to search on.

    // Cached results from the engine.
    struct {
        MoveT bestMove;
        MoveT ponderMove;
    } result;
} gUciState;

// Forward declarations.
static void uciNotifyMove(MoveT move);

static void uciNotifyError(char *reason)
{
    printf("info string error: %s\n", reason);
}

// Given that we are pointing at a token 'pStr',
//  return a pointer to the token after it (or NULL, if none).
static char *findNextToken(char *pStr)
{
    return
        pStr == NULL ? NULL :
        findNextNonWhiteSpace(isspace(*pStr) ? pStr :
                              findNextWhiteSpace(pStr));
}

static char *findRecognizedToken(char *pStr)
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

static void uciInit(GameT *ignore)
{
    static bool initialized;

    if (initialized)
    {
        return;
    }

    // Set unbuffered I/O (obviously necessary for output, also necessary for
    // input if we want to poll() correctly.)
    setbuf(stdout, NULL);
    setbuf(stdin, NULL);

    // gUciState is already cleared (since it is global).
    // Initialize whatever fields we need to.
    gUciState.ponderMove = MoveNone;

#if 1 // bldbg: goes out for debugging
    // Use random moves by default.
    gVars.randomMoves = true;
#endif
    // There is no standard way I am aware of for a UCI engine to resign
    // so until I figure out how Polyglot might interpret it, we have to
    // play until the bitter end.  Even then, we probably want to work w/all
    // UCI interfaces.
    gVars.canResign = false;
    initialized = true;
}

void processUciCommand(void)
{
    char hashString[160];
    int rv;

    uciInit(NULL);
    rv = snprintf(hashString, sizeof(hashString),
                  "option name Hash type spin default %" PRId64
                  " min 0 max %" PRId64 "\n",
                  (int64) gTransTable.DefaultSize() / (1024 * 1024),
                  (int64) gTransTable.MaxSize() / (1024 * 1024));
    assert(rv >= 0 && (uint) rv < sizeof(hashString)); // bail on truncated string

    // Respond appropriately to the "uci" command.
    printf("id name arctic %s.%s-%s\n"
           "id author Lucian Landry\n"
           "%s"
           // Though we do not care what "Ponder" is set to, we must
           // provide it as an option to signal (according to UCI) that the
           // engine can ponder at all.
           "option name Ponder type check default true\n"
           "option name RandomMoves type check default true\n"
           "option name UCI_EngineAbout type string default arctic %s.%s-%s by"
           " Lucian Landry\n"
           "uciok\n",
           VERSION_STRING_MAJOR, VERSION_STRING_MINOR, VERSION_STRING_PHASE,
           // Do not advertise a hash option if user overrode it.
           gPreCalc.userSpecifiedHashSize ? "" : hashString,
           VERSION_STRING_MAJOR, VERSION_STRING_MINOR, VERSION_STRING_PHASE);

    // switch to uiUci if we have not already.
    gUI = uiUciOps();
}

// Helper function for processPositionCommand().  Process a board once it has
// started to diverge from the game proper.
// 'move', if !NULL, was the move that diverged.  Otherwise, the original
// position changed and we should probably blow away everything.
static void finishMoves(GameT *game, Board *fenBoard, MoveT *move, char *pToken, Thinker *th)
{
    int lastPly, lastCommonPly;
    MoveT myMove;
    Board *board = &game->savedBoard;
    char tmpStr[MOVE_STRING_MAX];

    printf("info string %s: diverged move was: %s\n",
           __func__,
           (move && *move != MoveNone ?
            move->ToString(tmpStr, &gMoveStyleUCI, NULL) :
            "0000"));
    if (move != NULL)
    {
        lastPly = GameLastPly(game);
        lastCommonPly = fenBoard->Ply() - 1;
        assert(lastCommonPly >= 0);

        // I cannot GameNew(Ex)() because it would blow away the savegame.
        // Goto the ply just before the board started to diverge.

        // Note: this is a little fragile, as GameGotoPly() calls
        // GameMoveCommit() which would dump bad (for UCI) output for automatic
        // draws.  We rely on this function not being called when we are
        // handling such a case.  Fortunately all such (legal) draws must happen
        // on the last ply and can therefore be handled entirely by
        // processPositionCommand().
        GameGotoPly(game, lastCommonPly, th);
        GameMoveMake(game, move);
        if (abs(lastPly - lastCommonPly) > 2)
        {
            printf("info string %s: plydiff %d, blowing away hash/hist\n",
                   __func__, abs(lastPly - lastCommonPly));
            // Assume the boards have diverged too much to preserve the hash
            // table, history window, etc.  This should be large enough to
            // not trigger in a normal case (ponder miss etc.)
            th->CmdNewGame();
        }
    }
    else
    {
        // First position was different.  We always have to blow the hash
        // etc. away because we do not know exactly how different things were.
        GameNewEx(game, th, fenBoard, false);
    }

    for (;
         pToken != NULL;
         pToken = findNextToken(pToken))
    {
        // As we find moves, play them on the board.
        if (!isLegalMove(pToken, &myMove, board))
        {
            reportError(false, "%s: illegal move '%s', giving up",
                        __func__, pToken);
            gUciState.bBadPosition = true;
            return;
        }
        gUciState.ponderMove = myMove;
        GameMoveMake(game, &myMove);
    }
}

static void processPositionCommand(Thinker *th, GameT *game, char *pToken)
{
    Board fenBoard;
    bool bFen;
    int i;
    MoveT myMove;
    Board *board = &game->savedBoard; // shorthand.

    if (gUciState.bSearching)
    {
        // See "Implementation Notes" for why we ignore this.
        reportError(false, "%s: received 'position' while searching, IGNORING",
                    __func__);
        return;
    }

    // Turn off computer control of all players so that we do not start
    // automatically thinking when we make moves.
    setForceMode(th, game);
    gVars.ponder = false;

    gUciState.bBadPosition = false; // assume the best case
    gUciState.ponderMove = MoveNone;
    bFen = matches(pToken, "fen");

    if (!bFen && !matches(pToken, "startpos"))
    {
        // got an unknown token where we should have seen "fen" or "startpos".
        reportError(false, "%s: !fen and !startpos, giving up", __func__);
        gUciState.bBadPosition = true;
        return;
    }

    pToken = findNextToken(pToken);

    if (fenToBoard(bFen ? pToken : FEN_STARTSTRING, &fenBoard) < 0)
    {
        reportError(false, "%s: fenToBoard failed, cannot build position",
                    __func__);
        gUciState.bBadPosition = true;
        return;
    }    
    if (bFen)
    {
        // skip past the fenstring
        for (i = 0; i < 6; i++)
        {
            pToken = findNextToken(pToken);
        }
    }

    // Now we expect a 'moves' token.
    // The document makes it slightly ambiguous whether no
    // 'moves' token is okay, so we allow it.
    if (!matches(pToken, "moves") && pToken != NULL)
    {
        reportError(false, "%s: got unknown token where should have seen "
                    "'moves', giving up", __func__);
        gUciState.bBadPosition = true;
        return;
    }

    // skip past the 'moves' token (if any)
    pToken = findNextToken(pToken);

    // If the boards do not match at this ply or the current board does
    // not go back to this ply, blow everything away.
    if (SaveGameGotoPly(&game->sgame, fenBoard.Ply(), board, NULL) < 0 ||
        board->Position() != fenBoard.Position())
    {
        finishMoves(game, &fenBoard, NULL, pToken, th);
        return;
    }

    for(;
        pToken != NULL;
        pToken = findNextToken(pToken))
    {
        // As we find moves, play them on the board.
        if (!isLegalMove(pToken, &myMove, &fenBoard))
        {
            reportError(false, "%s: illegal move '%s', giving up",
                        __func__, pToken);
            gUciState.bBadPosition = true;
            return;
        }
        gUciState.ponderMove = myMove;
        fenBoard.MakeMove(myMove);
        if (SaveGameGotoPly(&game->sgame, fenBoard.Ply(), board, NULL) < 0 ||
            board->Position() != fenBoard.Position())
        {
            // Positions diverged.
            finishMoves(game, &fenBoard, &myMove, findNextToken(pToken), th);
            return;
        }
    }

    // If we backed up some moves, take that into account (manipulate the
    // PV for instance).  Otherwise, try not to touch anything.
    if (GameCurrentPly(game) != GameLastPly(game))
    {
        finishMoves(game, &fenBoard, &myMove, NULL, th);
    }
}

// Helper function.  Attempt to convert the next token after this one
// (presumed to be an argument) to an integer.
// "atLeast" is a sanity check which is disabled if < 0.
// Return -1 if we failed, 0 otherwise
static int convertNextInteger(char **pToken, int *result, int atLeast,
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
static int convertNextInteger64(char **pToken, int64 *result, int64 atLeast,
                                const char *context)
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

static void processSetOptionCommand(char *inputStr)
{
    int64 hashSizeMB;
    char *pToken;

    if (gUciState.bSearching)
    {
        // The UCI spec says this command "will only be sent when the engine
        // is waiting".
        reportError(false, "%s: received 'setoption' while searching, IGNORING",
                    __func__);
        return;
    }

    // Process RandomMoves option if applicable.
    // (UCI spec says option names and values "should not be case sensitive".)
    if (matches(inputStr, "name") &&
        matchesNoCase((pToken = findNextToken(inputStr)), "RandomMoves") &&
        matches((pToken = findNextToken(pToken)), "value") &&
        (matchesNoCase((pToken = findNextToken(pToken)), "true") ||
         matchesNoCase(pToken, "false")))
    {
        gVars.randomMoves = !strcasecmp(pToken, "true");
    }
    else if (!gPreCalc.userSpecifiedHashSize &&
             matches(inputStr, "name") &&
             matchesNoCase((pToken = findNextToken(inputStr)), "Hash") &&
             matches((pToken = findNextToken(pToken)), "value") &&
             convertNextInteger64(&pToken, &hashSizeMB, 0, "Hash") == 0 &&
             hashSizeMB <= (int64) gTransTable.MaxSize() / (1024 * 1024))
    {
        gTransTable.Reset((int64) hashSizeMB * 1024 * 1024);
    }
    else if (matches(inputStr, "name") &&
             matchesNoCase((pToken = findNextToken(inputStr)), "Ponder") &&
             matches((pToken = findNextToken(pToken)), "value") &&
             (matchesNoCase((pToken = findNextToken(pToken)), "true") ||
              matchesNoCase(pToken, "false")))
    {
        // do nothing.  This is just a hint to the engine; UCI controls
        // when we ponder.
        ; 
    }
    else
    {
        printf("info string %s: note: got option string \"%s\", ignoring\n",
               __func__, inputStr);
    }
}

static void processGoCommand(Thinker *th, GameT *game, char *pToken)
{
    MoveT myMove;
    Board *board = &game->savedBoard; // shorthand.
    MoveList searchList;
    int i;

    // Some temp state.  These are all processed at once after the entire
    // command is validated.
    // We take "infinite" to have a special meaning.  The actual search may
    // stop, but we will not report it until we receive a "stop" command.
    int bPonder = false, bInfinite = false;
    // (I am hesitant to use 0 or negative numbers here as special values
    //  since in theory a clock could go negative.)
    int wtime = INT_MAX, btime = INT_MAX;
    int winc = 0, binc = 0;
    int movestogo = -1;
    int depth = NO_LIMIT;  // okay, these better be at least zero
    int nodes = NO_LIMIT;
    int movetime = -1;
    int mate = -1;

    if (gUciState.bSearching)
    {
        // See "Implementation Notes" for why we ignore this.
        reportError(false, "%s: received 'go' while searching, IGNORING",
                    __func__);
        return;
    }
    if (gUciState.bBadPosition)
    {
        reportError(false, "%s: cannot search on bad position, IGNORING",
                    __func__);
        return;
    }

    for (;
         pToken != NULL;
         pToken = findNextToken(pToken))
    {
        // I do not expect these sub-tokens to come in in any particular order.
        if (matches(pToken, "searchmoves"))
        {
            for (;
                 // (we do this check-next-token-instead-of-current-one
                 //  goofiness just to make the outer loop work correctly.)
                 isLegalMove(findNextToken(pToken), &myMove, board);
                 pToken = findNextToken(pToken))
            {
                searchList.AddMove(myMove, *board);
            }
        }
        else if (matches(pToken, "ponder"))
        {
            bPonder = true;
        }
        else if (matches(pToken, "wtime"))
        {
            if (convertNextInteger(&pToken, &wtime, -1, "wtime") < 0)
            {
                return;
            }
        }
        else if (matches(pToken, "btime"))
        {
            if (convertNextInteger(&pToken, &btime, -1, "btime") < 0)
            {
                return;
            }
        }
        else if (matches(pToken, "winc"))
        {
            if (convertNextInteger(&pToken, &winc, -1, "winc") < 0)
            {
                return;
            }
            if (winc < 0) // UCI does not strictly forbid this
            {
                winc = 0;
            }
        }
        else if (matches(pToken, "binc"))
        {
            if (convertNextInteger(&pToken, &binc, -1, "binc") < 0)
            {
                return;
            }
            if (binc < 0) // UCI does not strictly forbid this
            {
                binc = 0;
            }
        }
        else if (matches(pToken, "movestogo"))
        {
            if (convertNextInteger(&pToken, &movestogo, 1, "movestogo") < 0)
            {
                return;
            }
        }
        else if (matches(pToken, "depth"))
        {
            if (convertNextInteger(&pToken, &depth, 1, "depth") < 0)
            {
                return;
            }
        }
        else if (matches(pToken, "nodes"))
        {
            if (convertNextInteger(&pToken, &nodes, 0, "nodes") < 0)
            {
                return;
            }
        }
        else if (matches(pToken, "mate"))
        {
            if (convertNextInteger(&pToken, &mate, 0, "mate") < 0)
            {
                return;
            }
        }
        else if (matches(pToken, "movetime"))
        {
            if (convertNextInteger(&pToken, &movetime, 0, "movetime") < 0)
            {
                return;
            }
        }
        else if (matches(pToken, "infinite"))
        {
            bInfinite = true;
        }
        else
        {
            reportError(false, "%s: unknown token sequence '%s'",
                        __func__, pToken);
            return;
        }
    }

    // At this point we have a valid command.
    // Reset and/or transfer state.
    gUciState.searchList = searchList;
    gUciState.result.bestMove = MoveNone;
    gUciState.result.ponderMove = MoveNone;
    gUciState.bPonder = bPonder;
    gUciState.bInfinite = bInfinite;

    if (bPonder)
    {
        // According to the spec "the last move sent in in (sic) the position
        // string is the ponder move".  Since we like to ponder on different
        // moves we need to start 1 move back.
        GameRewind(game, 1, th);
    }

    // Setup the clocks.
    for (i = 0; i < NUM_PLAYERS; i++)
        game->clocks[i]->ReInit();

    if (wtime != INT_MAX)
    {
        // (* 1000: msec -> usec)
        game->clocks[0]->SetTime(bigtime_t(wtime) * 1000);
        if (gUciState.bGotUciNewGame && gUciState.initialTime[0] == 0)
        {
            gUciState.initialTime[0] = wtime;
        }
    }
    if (btime != INT_MAX)
    {
        game->clocks[1]->SetTime(bigtime_t(btime) * 1000);
        if (gUciState.bGotUciNewGame && gUciState.initialTime[1] == 0)
        {
            gUciState.initialTime[1] = btime;
        }       
    }

    if (winc > 0)
        game->clocks[0]->SetIncrement(bigtime_t(winc) * 1000);
    if (binc > 0)
        game->clocks[1]->SetIncrement(bigtime_t(binc) * 1000);

    if (movestogo > 0)
    {
        // "movestogo" is tricky (and kind of dumb) since there is not
        // necessarily an indication of how much time we will add when the next
        // time control begins.  But, in keeping w/biasing more time for
        // earlier moves, we assume we can (almost) run out the clock and the
        // next time control will replenish it.
        //
        // So for starters, assume a 60-minute time control.  That should be
        // long enough.
        //
        // What happens when we are playing white and we get a ponderhit?
        // ... Well, we only bump timecontrol after *our* move.
        for (i = 0; i < NUM_PLAYERS; i++)
        {
            game->clocks[i]->
                SetStartTime(gUciState.initialTime[i] ?
                             ((bigtime_t) gUciState.initialTime) * 1000 :
                             ((bigtime_t) 60) * 60 * 1000000)
                .SetNumMovesToNextTimeControl(movestogo);
        }
    }

    gVars.maxLevel = depth - (depth != NO_LIMIT);
    gVars.maxNodes = nodes;  // (maxNodes is best-effort only.)

    if (mate > 0)
    {
        // We interpret the 'mate' command as
        // ('we are getting checkmated' || 'we are checkmating') in x moves.
        // With our current (almost-)full search window, we should not need
        // any further customization for mates, but if we change that we would.
        gVars.maxLevel =
            (gVars.maxLevel == NO_LIMIT ?
             mate * 2 :
             MIN(gVars.maxLevel, mate * 2));
    }

    if (movetime >= 0)
    {
        for (i = 0; i < NUM_PLAYERS; i++)
        {
            game->clocks[i]->SetPerMoveLimit(bigtime_t(movetime) * 1000);
        }
    }

    if (bPonder)
    {
        gVars.ponder = true;
        game->control[board->Turn() ^ 1] = 1;
        game->clocks[board->Turn() ^ 1]->Start();
        th->CmdSetBoard(*board);
        th->CmdPonder(searchList);
    }
    else
    {
        game->control[board->Turn()] = 1;
        game->clocks[board->Turn()]->Start();
        GoaltimeCalc(game);
        th->CmdSetBoard(*board);
        th->CmdThink(searchList);
    }

    gUciState.bSearching = true;
}

// This runs as a coroutine with the main thread, and can switch off to it at
// any time.  If it exits it will immediately be called again.

// One possibility if we set a bad position or otherwise get into a bad
// state is to just let the computer play null moves until a good position
// is set.
static void uciPlayerMove(Thinker *th, GameT *game)
{
    char *inputStr;
    Board *board = &game->savedBoard; // shorthand.

    // Skip past any unrecognized stuff.
    inputStr =
        findRecognizedToken(
            ChopBeforeNewLine(getStdinLine(MAXBUFLEN, &game->sw)));

    if (matches(inputStr, "xboard"))
    {
        // Special case: switch to xboard interface.
        return processXboardCommand();
    }
    else if (matches(inputStr, "uci"))
    {
        processUciCommand();
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
        processSetOptionCommand(findNextToken(inputStr));
    }
    else if (matches(inputStr, "register"))
    {
        // We always allow attempts to register this engine.
        printf("registration checking\n"
               "registration ok\n");
    }
    else if (matches(inputStr, "ucinewgame"))
    {
        if (gUciState.bSearching)
        {
            // See "Implementation Notes" for why we ignore this.
            reportError(false, "%s: received 'ucinewgame' while searching, "
                        "IGNORING", __func__);
        }
        else
        {
            // Just setup a new game.
            gVars.gameCount++;
            setForceMode(th, game);
            gUciState.bBadPosition = false;
            gUciState.bGotUciNewGame = true;
            gUciState.initialTime[0] = 0;
            gUciState.initialTime[1] = 0;
            gUciState.ponderMove = MoveNone;
            GameNew(game, th);
        }
    }
    else if (matches(inputStr, "position"))
    {
        processPositionCommand(th, game, findNextToken(inputStr));
    }
    else if (matches(inputStr, "go"))
    {
        processGoCommand(th, game, findNextToken(inputStr));
    }
    else if (matches(inputStr, "ponderhit") && gUciState.bPonder)
    {
        th->CmdBail();
        // We preserve the rest of our state (bInfinite, mate, etc)
        gUciState.bPonder = false;
        // GameFastForward(game, 1, th) does not work here, since the clock
        // is restored to infinite time.  So:
        if (gUciState.ponderMove != MoveNone)
        {
            GameMoveMake(game, &gUciState.ponderMove);
        }
        game->clocks[board->Turn()]->Start();
        GoaltimeCalc(game);
        th->CmdSetBoard(*board);
        th->CmdThink(gUciState.searchList);
    }
    else if (matches(inputStr, "stop") && gUciState.bSearching)
    {
        PlayloopCompMoveNowAndSync(game, th);
        gUciState.bPonder = false;
        gUciState.bInfinite = false;
        // Print the result that we just cached away.
        uciNotifyMove(gUciState.result.bestMove);
    }
    else if (matches(inputStr, "quit"))
    {
        th->CmdBail();
        exit(0);
    }

    // Wait for more input.
    SwitcherSwitch(&game->sw);
}


static void uciNotifyMove(MoveT move)
{
    char tmpStr[MOVE_STRING_MAX], tmpStr2[MOVE_STRING_MAX];
    MoveT ponderMove = gUciState.result.ponderMove;
    bool bShowPonderMove = move != MoveNone && ponderMove != MoveNone;

    if (gUciState.bPonder || gUciState.bInfinite)
    {
        // Store away the result for later.
        gUciState.result.bestMove = move;
        return;
    }

    printf("bestmove %s%s%s\n",
           (move != MoveNone ?
            move.ToString(tmpStr, &gMoveStyleUCI, NULL) :
            "0000"),
           bShowPonderMove ? " ponder " : "",
           (bShowPonderMove ?
            ponderMove.ToString(tmpStr2, &gMoveStyleUCI, NULL) : ""));
    gUciState.bSearching = false;
}


static void uciNotifyDraw(const char *reason, MoveT *move)
{
    // UCI seems to rely on a GUI arbiter to claim draws, simply because there
    // is no designated way for the engine to do it.  Nevertheless, when we
    // have an automatic draw we send no-move.  This seems better than picking
    // an actual move which may be losing (as all possible moves may lose).  But
    // mostly, we need an actual example where we need to do something else
    // in order to justify complicating the engine code to say "I cannot claim
    // a draw but my opponent can, what is my best move".
    printf("info string engine claims a draw (reason: %s)\n", reason);
    uciNotifyMove(move != NULL ? *move : MoveNone);
}


static void uciNotifyResign(int turn)
{
    // This is just for the benefit of a human trying to understand our output.
    // Since our resignation threshold is so low, we normally do not "resign"
    // unless we are actually mated.
    printf("info string engine (turn %d) resigns\n", turn);
    uciNotifyMove(MoveNone);
}


// Assumes "result" is long enough to hold the actual result (say 80 chars to
// be safe).  Returns "result".
static char *buildStatsString(char *result, GameT *game, ThinkerStatsT *stats)
{
    int nodes = stats->nodes;
    // (Convert bigtime_t to milliseconds)
    int timeTaken =
        game->clocks[game->savedBoard.Turn()]->TimeTaken() / 1000;
    int nps = (int) (((uint64) nodes) * 1000 / (timeTaken ? timeTaken : 1));
    int charsWritten;

    charsWritten = sprintf(result, "time %d nodes %d nps %d",
                           timeTaken, nodes, nps);
    if (gTransTable.NumEntries())
    {
        sprintf(&result[charsWritten], " hashfull %d",
                (int) (((uint64) stats->hashWroteNew) * 1000 /
                       gTransTable.NumEntries()));
    }
    return result;
}

// Returns whether a move was chopped or not.
static bool chopFirstMove(char *moveString)
{
    char *space = strchr(moveString, ' ');
    if (space)
    {
        strcpy(moveString, space + 1);
        return true;
    }
    return false;
}

static void uciNotifyPV(GameT *game, RspPvArgsT *pvArgs)
{
    char lanString[65];
    char evalString[20];
    char statsString[80];
    bool bDisplayPv = true;
    const DisplayPv &pv = pvArgs->pv; // shorthand
    bool chopFirst = false;
    int moveCount;

    // Save away a next move to ponder on, if possible.
    gUciState.result.ponderMove = pv.Moves(1);

    if (gUciState.bPonder)
    {
        // If we are not actually pondering on the move the UI wanted us to,
        // do not advertise the PV (to avoid confusing the UI).
        // (If the UI is mean and tries to make us ponder on the first move,
        // this just means we will never display the PV since ponderMove ==
        // MoveNone)
        if (pv.Moves(0) != gUciState.ponderMove)
        {
            bDisplayPv = false;
        }
        // Chop off the first (ponder) move since the UI does not expect it.
        // This is not spelled out by the UCI spec, just observed from
        // interaction between Polyglot and (Fruit,Stockfish).  This does mean
        // we will actually ponder 1 ply deeper than advertised, and we may
        // get two "depth 1" searches (actually levels 0 and 1).
        chopFirst = true;
    }

    moveCount = pv.BuildMoveString(lanString, sizeof(lanString), gMoveStyleUCI,
                                   game->savedBoard);
    if (chopFirst && chopFirstMove(lanString))
    {
        moveCount--;
    }
    if (moveCount < 1)
    {
        bDisplayPv = false;
    }

    if (pv.Eval().DetectedWinOrLoss())
    {
        int movesToMate = pv.Eval().MovesToWinOrLoss();
        snprintf(evalString, sizeof(evalString), "mate %d",
                 pv.Eval().DetectedLoss() ? -movesToMate : movesToMate);

    }
    else
    {
        snprintf(evalString, sizeof(evalString), "cp %d", pv.Eval().LowBound());
    }

    // Sending a fairly basic string here.
    printf("info depth %d score %s %s%s%s\n",
           pv.Level() + 1, evalString,
           buildStatsString(statsString, game, &pvArgs->stats),
           bDisplayPv ? " pv " : "", bDisplayPv ? lanString : "");
}


static void uciNotifyComputerStats(GameT *game, ThinkerStatsT *stats)
{
    char statsString[80];

    printf("info %s\n", buildStatsString(statsString, game, stats));
}

static bool uciShouldNotCommitMoves(void)
{
    return false;
}

static void uciPositionRefresh(const Position &position) { }
static void uciNoop(void) { }
static void uciStatusDraw(GameT *game) { }
static void uciNotifyTick(GameT *game) { }
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
        .shouldCommitMoves = uciShouldNotCommitMoves
    };

    return &uciUIFuncTable;
}
