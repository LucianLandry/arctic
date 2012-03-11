//--------------------------------------------------------------------------
//           uiUci.c - UCI (Universal Chess Interface) interface.
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
#include "log.h"
#include "moveList.h"
#include "playloop.h"
#include "ui.h"
#include "uiUtil.h"

// The UCI "position" command can be very large (polyglot likes to send
// the starting position + all the moves; fifty-move draws are claimed,
// not automatic; and arbitrary amounts of whitespace are also allowed.
// Still, if we exceed this, we probably have a problem, so bail.
#define MAXBUFLEN (1 * 1024 * 1024)


static struct {
    int bDebug;           // are we in debug mode or not?
    int bBadPosition;     // "invalid" position loaded (king in check?
                          // two kings? etc.) and cannot think on it.
    int bGotUciNewGame;   // got a "ucinewgame" command at least once, which
                          // lets us know the GUI supports it.
    int initialTime[2];   // Possible starting times on the w/b clock, in msec

    MoveT ponderMove;     // Move the UI wants us to ponder on.  (We ignore this
                          // advice but we use it to massage reported PVs etc.)

    int bSearching;       // Are we currently supposed to be searching a position?
                          // (may be true even if the computer technically has
                          // found mate or resigned, and is not actually
                          // searching)
    int bPonder;          // are we in pondering mode or not.
    int bInfinite;        // When true, we are supposed to not stop searching.
                          // What happens in reality is that if we do stop
                          // searching, we cache the search results and do not
                          // inform the UI until it directs us to stop.
    MoveListT searchList; // list of moves we are supposed to search on.

    // Cached results from the engine.
    struct {
	MoveT bestMove;
	MoveT ponderMove;
    } result;
} gUciState;

// Forward declarations.
static void uciNotifyMove(MoveT *move);

// Just a bit of syntactic sugar.
static int matches(char *str, char *needle)
{
    int len = strlen(needle);
    return
	str == NULL ? 0 :
	!strncmp(str, needle, len) &&
	(isspace(str[len]) || str[len] == '\0');
}

static void uciNotifyError(char *reason)
{
    printf("info string error: %s\n", reason);
}


static char *findNextNonWhiteSpace(char *pStr)
{
    if (pStr == NULL)
    {
	return NULL;
    }
    while (isspace(*pStr) && *pStr != '\0')
    {
	pStr++;
    }
    return *pStr != '\0' ? pStr : NULL;
}


static char *findNextWhiteSpace(char *pStr)
{
    if (pStr == NULL)
    {
	return NULL;
    }
    while (!isspace(*pStr) && *pStr != '\0')
    {
	pStr++;
    }
    return *pStr != '\0' ? pStr : NULL;
}

// Given that we are pointing at a token 'pStr',
// return a pointer to the next token.
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
	if (matches(pStr, "uci") ||
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

static void processUciCommand(void)
{
    // Respond appropriately to the "uci" command.
    // TODO: in the future we should support:
    // -- "Hash"
    printf("id name arctic %s.%s-%s\n"
	   "id author Lucian Landry\n"
	   "option name Ponder type check default true\n"
	   "option name RandomMoves type check default true\n"
	   "option name UCI_EngineAbout type string default arctic %s.%s-%s by"
	   " Lucian Landry\n"
	   "uciok\n",
	   VERSION_STRING_MAJOR, VERSION_STRING_MINOR, VERSION_STRING_PHASE,
	   VERSION_STRING_MAJOR, VERSION_STRING_MINOR, VERSION_STRING_PHASE);
}

// Helper function for processPositionCommand().  Process a board once it has
// started to diverge from the game proper.
// 'move', if !NULL, was the move that diverged.  Otherwise, the original
// position changed and we should probably blow away everything.
static void finishMoves(GameT *game, BoardT *fenBoard, MoveT *move, char *pToken, ThinkContextT *th)
{
    int lastPly, lastCommonPly;
    MoveT myMove;
    BoardT *board = &game->savedBoard;
    char tmpStr[6];

    printf("info string %s: diverged move was: %s\n",
	   __func__,
	   move && move->src != FLAG ? moveToStr(tmpStr, move) : "0000");
    if (move != NULL)
    {
	lastPly = GameLastPly(game);
	lastCommonPly = fenBoard->ply - 1;
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
	    gHashInit();
	    gHistInit();
	}
    }
    else
    {
	// First position was different.  We always have to blow the hash
	// etc. away because we do not know exactly how different things were.
	GameNewEx(game, th, fenBoard, 0, 1);
    }

    for (;
	 pToken != NULL;
	 pToken = findNextToken(pToken))
    {
	// As we find moves, play them on the board.
	if (!isLegalMove(pToken, &myMove, board))
	{
	    reportError(0, "%s: illegal move '%s', giving up",
			__func__, pToken);
	    gUciState.bBadPosition = 1;
	    return;
	}
	gUciState.ponderMove = myMove; // struct copy
	GameMoveMake(game, &myMove);
    }
}


static void processPositionCommand(ThinkContextT *th, GameT *game, char *pToken)
{
    BoardT fenBoard;
    int bFen;
    int i;
    MoveT myMove;
    BoardT *board = &game->savedBoard; // shorthand.

    if (gUciState.bSearching)
    {
	// See "Implementation Notes" for why we ignore this.
	reportError(0, "%s: received 'position' while searching, IGNORING",
		    __func__);
	return;
    }

    // Turn off computer control of all players so that we do not start
    // automatically thinking when we make moves.
    setForceMode(th, game);
    gVars.ponder = 0;

    gUciState.bBadPosition = 0; // assume the best case
    gUciState.ponderMove = gMoveNone;
    bFen = matches(pToken, "fen");

    if (!bFen && !matches(pToken, "startpos"))
    {
	// got an unknown token where we should have seen "fen" or "startpos".
	reportError(0, "%s: !fen and !startpos, giving up", __func__);
	gUciState.bBadPosition = 1;
	return;
    }

    pToken = findNextToken(pToken);

    if (fenToBoard(bFen ? pToken : FEN_STARTSTRING, &fenBoard) < 0)
    {
	reportError(0, "%s: fenToBoard failed, cannot build position",
		    __func__);
	gUciState.bBadPosition = 1;
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
	reportError(0, "%s: got unknown token where should have seen 'moves', "
		    "giving up", __func__);
	gUciState.bBadPosition = 1;	    
	return;
    }

    // skip past the 'moves' token (if any)
    pToken = findNextToken(pToken);

    // If the boards do not match at this ply or the current board does
    // not go back to this ply, blow everything away.
    if (SaveGameGotoPly(&game->sgame, fenBoard.ply, board, NULL) < 0 ||
	!BoardPositionsSame(board, &fenBoard))
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
	    reportError(0, "%s: illegal move '%s', giving up",
			__func__, pToken);
	    gUciState.bBadPosition = 1;
	    return;
	}
	gUciState.ponderMove = myMove; // struct copy
	BoardMoveMake(&fenBoard, &myMove, NULL);
	if (SaveGameGotoPly(&game->sgame, fenBoard.ply, board, NULL) < 0 ||
	    !BoardPositionsSame(board, &fenBoard))
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
			      char *context)
{
    *pToken = findNextToken(*pToken);
    if (*pToken == NULL ||
	sscanf(*pToken, "%d", result) < 1 ||
	(atLeast >= 0 && *result < atLeast))
    {
	reportError(0, "%s: failed converting arg for %s",
		    __func__, context);
	return 1;
    }
    return 0;
}

static void processGoCommand(ThinkContextT *th, GameT *game, char *pToken)
{
    MoveT myMove;
    BoardT *board = &game->savedBoard; // shorthand.
    MoveListT *searchList = &gUciState.searchList; // shorthand.
    int i;

    // Some temp state.  These are all processed at once after the entire
    // command is validated.
    // We take "infinite" to have a special meaning.  The actual search may
    // stop, but we will not report it until we receive a "stop" command.
    int bPonder = 0, bInfinite = 0;
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
	reportError(0, "%s: received 'go' while searching, IGNORING", __func__);
	return;
    }
    if (gUciState.bBadPosition)
    {
	reportError(0, "%s: cannot search on bad position, IGNORING", __func__);
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
		mlistMoveAdd(searchList, board, &myMove);
	    }
	}
	else if (matches(pToken, "ponder"))
	{
	    bPonder = 1;
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
	    bInfinite = 1;
	}
	else
	{
	    reportError(0, "%s: unknown token sequence '%s'", __func__, pToken);
	    return;
	}
    }

    // At this point we have a valid command.
    // Reset state.
    memset(searchList, 0, sizeof(MoveListT));
    gUciState.result.bestMove = gMoveNone;
    gUciState.result.ponderMove = gMoveNone;
    gUciState.bPonder = 0;
    gUciState.bInfinite = 0;

    if (bPonder)
    {
	// According to the spec "the last move sent in in (sic) the position
	// string is the ponder move".  Since we like to ponder on different
	// moves we need to start 1 move back.
	gUciState.bPonder = 1;
	GameRewind(game, 1, th);
    }

    // Setup the clocks.
    for (i = 0; i < NUM_PLAYERS; i++)
    {
	ClockInit(game->clocks[i]);
    }

    if (wtime != INT_MAX)
    {
        // (* 1000: msec -> usec)
	ClockSetTime(game->clocks[0], ((bigtime_t) wtime) * 1000);
	if (gUciState.bGotUciNewGame && gUciState.initialTime[0] == 0)
	{
	    gUciState.initialTime[0] = wtime;
	}
    }
    if (btime != INT_MAX)
    {
	ClockSetTime(game->clocks[1], ((bigtime_t) btime) * 1000);
	if (gUciState.bGotUciNewGame && gUciState.initialTime[1] == 0)
	{
	    gUciState.initialTime[1] = btime;
	}	
    }
    if (winc > 0)
    {
	ClockSetInc(game->clocks[0], ((bigtime_t) winc) * 1000);
    }
    if (binc > 0)
    {
	ClockSetInc(game->clocks[1], ((bigtime_t) binc) * 1000);
    }
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
	    ClockSetStartTime(game->clocks[i],
			      gUciState.initialTime[i] ?
			      ((bigtime_t) gUciState.initialTime) * 1000 :
			      ((bigtime_t) 60) * 60 * 1000000);
	    ClockSetNumMovesToNextTimeControl(game->clocks[i], movestogo);
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
	    ClockSetPerMoveLimit(game->clocks[i],
				 ((bigtime_t) movetime) * 1000);
	}
    }

    gUciState.bInfinite = bInfinite;

    if (bPonder)
    {
	gVars.ponder = 1;
	game->control[board->turn ^ 1] = 1;
	ClockStart(game->clocks[board->turn ^ 1]);
	ThinkerCmdPonderEx(th, board, searchList);
    }
    else
    {
	game->control[board->turn] = 1;
	ClockStart(game->clocks[board->turn]);
	GoaltimeCalc(game);
	ThinkerCmdThinkEx(th, board, searchList);
    }

    gUciState.bSearching = 1;
}

// This runs as a coroutine with the main thread, and can switch off to it at
// any time.  If it exits it will immediately be called again.

// One possibility if we set a bad position or otherwise get into a bad
// state is to just let the computer play null moves until a good position
// is set.
static void uciPlayerMove(ThinkContextT *th, GameT *game)
{
    char *inputStr, *pToken;
    BoardT *board = &game->savedBoard; // shorthand.

    // Skip past any unrecognized stuff.
    inputStr =
	findRecognizedToken(
	    ChopBeforeNewLine(getStdinLine(MAXBUFLEN, &game->sw)));

    if (matches(inputStr, "uci"))
    {
	// we should never get this command here (it should be invoked by
	// uiUciInit()) but if we do, respond to it as best as we can.
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
	// Process RandomMoves option if applicable.
	if (!strcmp((pToken = findNextToken(inputStr)), "name") &&
	    !strcmp((pToken = findNextToken(pToken)), "RandomMoves") &&
	    !strcmp((pToken = findNextToken(pToken)), "value") &&
	    (!strcmp((pToken = findNextToken(pToken)), "true") ||
	     !strcmp(pToken, "false")))
	{
	    gVars.randomMoves = !strcmp(pToken, "true");
	}
	else
	{
	    printf("info string %s: note: got option string \"%s\", ignoring\n",
		   __func__, inputStr);
	}
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
	    reportError(0, "%s: received 'ucinewgame' while searching, "
			"IGNORING", __func__);
	}
	else
	{
	    // Just setup a new game.
	    setForceMode(th, game);
	    gUciState.bBadPosition = 0;
	    gUciState.bGotUciNewGame = 1;
	    gUciState.initialTime[0] = 0;
	    gUciState.initialTime[1] = 0;
	    gUciState.ponderMove = gMoveNone;
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
	ThinkerCmdBail(th);
	// We preserve the rest of our state (bInfinite, mate, etc)
	gUciState.bPonder = 0;
	// GameFastForward(game, 1, th) does not work here, since the clock
	// is restored to infinite time.  So:
	if (gUciState.ponderMove.src != FLAG)
	{
	    GameMoveMake(game, &gUciState.ponderMove);
	}
	ClockStart(game->clocks[board->turn]);
	GoaltimeCalc(game);
	ThinkerCmdThinkEx(th, board, &gUciState.searchList);
    }
    else if (matches(inputStr, "stop") && gUciState.bSearching)
    {
	PlayloopCompMoveNowAndSync(game, th);
	gUciState.bPonder = 0;
	gUciState.bInfinite = 0;
	// Print the result that we just cached away.
	uciNotifyMove(&gUciState.result.bestMove);
    }
    else if (matches(inputStr, "quit"))
    {
	ThinkerCmdBail(th);
	exit(0);
    }

    // Wait for more input.
    SwitcherSwitch(&game->sw);
}


static void uciNotifyMove(MoveT *move)
{
    char tmpStr[6], tmpStr2[6];
    MoveT ponderMove =
	gUciState.result.ponderMove.src == FLAG ? gMoveNone :
	gUciState.result.ponderMove;
    int bShowPonderMove = move->src != FLAG && ponderMove.src != FLAG;

    if (gUciState.bPonder || gUciState.bInfinite)
    {
	// Store away the result for later.
	gUciState.result.bestMove = *move;
	return;
    }

    printf("bestmove %s%s%s\n",
	   move->src != FLAG ? moveToStr(tmpStr, move) : "0000",
	   bShowPonderMove ? " ponder " : "",
	   bShowPonderMove ? moveToStr(tmpStr2, &ponderMove) : "");
    gUciState.bSearching = 0;
}


static void uciNotifyDraw(char *reason, MoveT *move)
{
    // UCI seems to rely on a GUI arbiter to claim draws, simply because there
    // is no designated way for the engine to do it.  Nevertheless, when we
    // have an automatic draw we send no-move.  This seems better than picking
    // an actual move which may be losing (as all possible moves may lose).  But
    // mostly, we need an actual example where we need to do something else
    // in order to justify complicating the engine code to say "I cannot claim
    // a draw but my opponent can, what is my best move".
    printf("info string engine claims a draw (reason: %s)\n", reason);
    uciNotifyMove(move != NULL && move->src != FLAG ? move :
		  &gMoveNone);
}


static void uciNotifyResign(int turn)
{
    // This is just for the benefit of a human trying to understand our output.
    // Since our resignation threshold is so low, we normally do not "resign"
    // unless we are actually mated.
    printf("info string engine (turn %d) resigns\n", turn);
    uciNotifyMove(&gMoveNone);
}


// Assumes "result" is long enough to hold the actual result (say 80 chars to
// be safe).  Returns "result".
static char *buildStatsString(char *result, GameT *game, CompStatsT *stats)
{
    int nodes = stats->nodes;
    // (Convert bigtime to milliseconds)
    int timeTaken = ClockTimeTaken(game->clocks[game->savedBoard.turn]) / 1000;
    int nps = (int) (((uint64) nodes) * 1000 / (timeTaken ? timeTaken : 1));

    sprintf(result, "time %d nodes %d nps %d", timeTaken, nodes, nps);
    return result;
}

static void uciNotifyPV(GameT *game, PvRspArgsT *pvArgs)
{
    char lanString[65];
    char evalString[20];
    char statsString[80];
    int bDisplayPv = 1;
    PvT *pv = &pvArgs->pv; // shorthand

    // Save away a next move to ponder on, if possible.
    gUciState.result.ponderMove =
	pv->moves[0].src != FLAG && pv->moves[1].src != FLAG ?
	pv->moves[1] : gMoveNone;

    if (gUciState.bPonder)
    {
	// If we are not actually pondering on the move the UI wanted us to,
	// do not advertise the PV (to avoid confusing the UI).
	// (If the UI is mean and tries to make us ponder on the first move,
	// this just means we will never display the PV since ponderMove ==
	// gMoveNone)
	if (memcmp(&pv->moves[0], &gUciState.ponderMove, sizeof(MoveT)))
	{
	    bDisplayPv = 0;
	}
	// Chop off the first (ponder) move since the UI does not expect it.
	// This is not spelled out by the UCI spec, just observed from
	// interaction between Polyglot and (Fruit,Stockfish).  This does mean
	// we will actually ponder 1 ply deeper than advertised, and we may
	// get two "depth 1" searches (actually levels 0 and 1).
	PvFastForward(pv, 1);
	if (pv->moves[0].src == FLAG)
	{
	    bDisplayPv = 0;
	}
    }

    buildMoveString(lanString, sizeof(lanString), pv, NULL);

    if (abs(pv->eval) >= EVAL_WIN_THRESHOLD)
    {
	int mateInYMoves = (EVAL_WIN - abs(pv->eval) + 1) / 2;
	snprintf(evalString, sizeof(evalString), "mate %d",
		 pv->eval < 0 ? -mateInYMoves : mateInYMoves);
    }
    else
    {
	snprintf(evalString, sizeof(evalString), "cp %d", pv->eval);
    }

    // Sending a fairly basic string here.  hashfull would be nice to implement.
    printf("info depth %d score %s %s%s%s\n",
	   pv->level + 1, evalString,
	   buildStatsString(statsString, game, &pvArgs->stats),
	   bDisplayPv ? " pv " : "", bDisplayPv ? lanString : "");
}


static void uciNotifyComputerStats(GameT *game, CompStatsT *stats)
{
    char statsString[80];

    printf("info %s\n", buildStatsString(statsString, game, stats));
}

static int uciShouldNotCommitMoves(void)
{
    return 0;
}

static void uciBoardRefresh(const BoardT *board) { }
static void uciNoop(void) { }
static void uciStatusDraw(GameT *game) { }
static void uciNotifyTick(GameT *game) { }
static void uciNotifyCheckmated(int turn) { }

static UIFuncTableT uciUIFuncTable =
{
    .playerMove = uciPlayerMove,
    .boardRefresh = uciBoardRefresh,
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


UIFuncTableT *uiUciInit(void)
{
    // gUciState is already cleared (since it is global).
    // Initialize whatever fields we need to.
    gUciState.ponderMove = gMoveNone;

    // We arrive here when uiXboard has received "uci".
    // We assume uiXboard has already set unbuffered I/O (for input and
    // output).
    processUciCommand();

#if 1 // bldbg: goes out for debugging
    // use random moves.
    gVars.randomMoves = 1;
#endif
    // There is no standard way I am aware of for a UCI engine to resign
    // so until I figure out how Polyglot might interpret it, we have to
    // play until the bitter end.
    gVars.resignThreshold = EVAL_LOSS;

    return &uciUIFuncTable;
}
