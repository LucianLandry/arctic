//--------------------------------------------------------------------------
//                      uiXboard.cpp - Xboard interface.
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

// Note: I am deliberately trying to avoid reliance on version 2 of the xboard
// protocol, in order to interop w/other chess GUIs that might only utilize
// version 1.
//
// I will not try to fully document here what every xboard command does (unless
// we deviate from the spec).  Basically, I read Tim Mann's engine-intf.html
// (v 2.1 2003/10/27 19:21:00), and if the code does something different ...
// unless we're talking about the "black" and "white" commands ... it's
// wrong.

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clockUtil.h"
#include "gPreCalc.h"
#include "log.h"
#include "MoveList.h"
#include "ui.h"
#include "uiUtil.h"

#define MAXBUFLEN 160

static struct {
    bool post;        // controls display of PV
    bool badPosition; // can be triggered by editing a bad position
    bool newgame;     // turned on every "new", turned off every "go"
    bool ponder;
    Switcher *sw;
    int engineLastPlayed; // player engine last played for (0 -> white,
                          //  1 -> black).  The engine might not be currently
                          //  playing for either side, but that is irrelevant.
    bool icsClocks;
} gXboardState;

static void xboardNotifyError(char *reason)
{
    // Generally, we should only communicate carefully crafted errors to the
    //  GUI so that it's not interpreted specially.  We may have arbitrary
    //  errors here, so, we just forward everything as a comment.
    printf("# Error: %s\n", reason);
}

static void xboardEditPosition(Position &position)
{
    char *inputStr;

    int i = 0;
    int turn = 0;
    int coord;
    Piece piece;

    position.SetEnPassantCoord(FLAG); // assumed, for 'edit' command.
    position.SetPly(0);
    position.SetNcpPlies(0);

    while (1)
    {
        inputStr = getStdinLine(MAXBUFLEN, gXboardState.sw);

        if (matches(inputStr, "#"))
        {
            // Wipe board.
            for (i = 0; i < NUM_SQUARES; i++)
                position.SetPiece(i, Piece());
        }
        else if (matches(inputStr, "c"))
        {
            // Change current color.
            turn ^= 1;
        }
        else if (matches(inputStr, "."))
        {
            // Leave edit mode.
            // (edit mode is optimistic about castling.)
            position.EnableCastling();
            position.Sanitize();
            return;
        }
        else switch(inputStr[0])
        {
            case 'x': // delete the piece @ inputStr[1].
            case 'P': // Add one of these pieces @ inputStr[1].
            case 'R':
            case 'N':
            case 'B':
            case 'Q':
            case 'K':
                if ((coord = asciiToCoord(&inputStr[1])) == FLAG)
                {
                    printf("Error (edit: %c: bad coord): %s\n",
                           inputStr[0], &inputStr[1]);
                    break;
                }

                // Set the new piece.
                piece = asciiToNative(inputStr[0]);
                if (!piece.IsEmpty())
                    piece = Piece(turn, piece.Type());
                position.SetPiece(coord, piece);
                break;
            default:
                printf("Error (edit: unknown command): %s\n", inputStr);
                break;
        }
    }
}

static void xboardInit(Game *game, Switcher *sw)
{
    static bool initialized;
    if (initialized)
        return;

    // Set unbuffered I/O (obviously necessary for output, also necessary for
    // input if we want to poll() correctly.)
    setbuf(stdout, NULL);
    setbuf(stdin, NULL);
    gXboardState.sw = sw;
    // UCI may have clobbered autoplay, so reset
    game->SetAutoPlayEngineMoves(true);
    // The spec does not mention that a "new" must come in before anything else,
    //  so playing it safe and doing some initialization here.
    uiPrepareEngines(game);
    initialized = true;
}

static void setIcsClocks(Game *game, bool enabled)
{
    gXboardState.icsClocks = enabled;
    for (int i = 0; i < NUM_PLAYERS; i++)
    {
        Clock myClock = game->InitialClock(i);
        game->SetInitialClock(i, myClock.SetIsFirstMoveFree(enabled));
        myClock = game->Clock(i);
        game->SetClock(i, myClock.SetIsFirstMoveFree(enabled));
    }
}

void processXboardCommand(Game *game, Switcher *sw)
{
    struct sigaction ignoreSig;
    int err;

    // We are definitely doing xboard.  So do some xboard-specific
    // stuff. ... such as ignoring SIGINT.  Also switch to uiXboard
    // if we have not already.
    xboardInit(game, sw);
    memset(&ignoreSig, 0, sizeof(struct sigaction));
    ignoreSig.sa_flags = SA_RESTART;
    ignoreSig.sa_handler = SIG_IGN;
    err = sigaction(SIGINT, &ignoreSig, NULL);
    assert(err == 0);
    gUI = uiXboardOps();
}

void processProtoverCommand(Game *game, const char *inputStr)
{
    int protoVersion;
    
    if (sscanf(inputStr, "protover %d", &protoVersion) < 1)
    {
        printf("Error (bad args): %s\n", inputStr);
        return;
    }
    if (protoVersion >= 2)
    {
        // We currently do not care if these features are accepted or
        //  rejected.  We try to handle all input as well as possible.
        printf("feature analyze=0 myname=arctic%s.%s-%s variants=normal "
               "colors=0 ping=1 setboard=1 memory=%d smp=%d done=1 debug=1 "
               "ics=1\n",
               VERSION_STRING_MAJOR, VERSION_STRING_MINOR,
               VERSION_STRING_PHASE, gPreCalc.userSpecifiedHashSize == -1,
               gPreCalc.userSpecifiedNumThreads == -1);
    }
}

void processLevelCommand(Game *game, const char *inputStr)
{
    int inc, mptc; // moves per time control
    char baseStr[20];
    
    if (sscanf(inputStr, "level %d %20s %d", &mptc, baseStr, &inc) < 3 ||
        mptc < 0 || inc < 0)
    {
        printf("Error (bad args): %s\n", inputStr);
        return;
    }

    int base;
    
    // The spec states that future time parameters might be in the form
    //  '40 25+5 0' (to specify additional time control periods).  We do not
    //  always handle extra characters for now, but we could if necessary.
    // Hopefully any change like that would be negotiated as a feature.
    if ((strchr(baseStr, ':') && !TimeStringIsValid(baseStr)) ||
        (!strchr(baseStr, ':') && sscanf(baseStr, "%d", &base) != 1))
    {
        printf("Error (bad parameter '%s'): %s\n", baseStr, inputStr);
        return;
    }
    bigtime_t baseTime =
        !strchr(baseStr, ':') ?
        // 'base' is in minutes (and is already filled out).            
        ((bigtime_t) base) * 60 * 1000000 :
        // base is in more-or-less standard time format.
        TimeStringToBigTime(baseStr);

    Clock myClock;
    myClock.SetStartTime(baseTime)
        .Reset()
        .SetIsFirstMoveFree(gXboardState.icsClocks)
        .SetTimeControlPeriod(mptc)
        // Incremental time control.
        .SetIncrement(bigtime_t(inc) * 1000000);            
    for (int i = 0; i < NUM_PLAYERS; i++)
        game->SetInitialClock(i, myClock);
    // FIXME: read the documents again and figure out if 'level' in a game
    //  implies we should always set new time controls.  It looks like we
    //  should.
    if (gXboardState.newgame)
    {
        // Game has not started yet.  Under xboard this means "set clocks in
        //  addition to time controls"
        game->ResetClocks();
    }
    // game->LogClocks("level");
}

void processStCommand(Game *game, const char *inputStr)
{
    int perMoveLimit;
    
    if (sscanf(inputStr, "st %d", &perMoveLimit) < 1 || perMoveLimit < 1)
    {
        printf("Error (bad args): %s\n", inputStr);
        return;
    }
    
    // Note: 'st' and 'level' are "not used together", per the spec.  FIXME:
    //  the latest spec makes no mention of that.
    perMoveLimit = MAX(perMoveLimit, 0);
    Clock myClock;
    myClock.SetIsFirstMoveFree(gXboardState.icsClocks)
        .SetPerMoveLimit(bigtime_t(perMoveLimit) * 1000000);
    for (int i = 0; i < NUM_PLAYERS; i++)
        game->SetInitialClock(i, myClock);
    if (gXboardState.newgame)
    {
        // Game has not started yet.  Under xboard this means "set
        // clocks in addition to time controls"
        game->ResetClocks();
    }
}

void processSdCommand(Game *game, const char *inputStr)
{
    int myLevel;

    if (sscanf(inputStr, "sd %d", &myLevel) < 1 || myLevel <= 0)
    {
        printf("Error (bad args): %s\n", inputStr);
        return;
    }
    
    // Set depth.  I took out an upper limit check.  If you want
    // depth 5000, okay ...
    if (game->EngineConfig().SetSpinClamped(Config::MaxDepthSpin,
                                            myLevel) != Config::Error::None)
    {
        printf("Error (cannot set maxDepth for this engine): %s\n", inputStr);
    }
}

void processTimeCommand(Game *game, const char *inputStr, bool opponent)
{
    int centiSeconds;

    // Clocks might go negative, so we allow centiSeconds < 0.
    if (sscanf(inputStr,
               opponent ? "otim %d" : "time %d",
               &centiSeconds) < 1)
    {
        printf("Error (bad args): %s\n", inputStr);
        return;
    }

    // Set the engine clock (or the opponent clock, if opponent).
    Clock myClock = game->Clock(gXboardState.engineLastPlayed ^ opponent);
    game->SetClock(gXboardState.engineLastPlayed ^ opponent,
                   myClock.SetTime(bigtime_t(centiSeconds) * 10000));
    // game->LogClocks("time");
}

void processPingCommand(Game *game, const char *inputStr)
{
    int i;
    
    if (sscanf(inputStr, "ping %d", &i) < 1)
    {
        printf("Error (bad args): %s\n", inputStr);
        return;
    }

    // Reading the spec strictly, it is possible we might hang here forever if
    //  there is no search limit at all and no preceeding "move now".  xboard
    //  documentation for "ping" implies this should never happen.
    if (game->EngineControl(game->Board().Turn())) // assume we are thinking
        game->WaitForEngineIdle();

    printf("pong %d\n", i);
}

void processRatingCommand(Game *game, const char *inputStr)
{
    int ourRating, oppRating;

    if (sscanf(inputStr, "rating %d %d", &ourRating, &oppRating) < 2)
    {
        printf("Error (bad args): %s\n", inputStr);
        return;
    }

    // 'rating' could be useful to implement when determining how to evaluate a
    //  draw.  However, right now I only use it to force ICS mode as a backup
    //  for when the GUI does not understand the "ics" command.
    // FIXME: the spec says in the future, this might not be sent only for ICS
    //  games.  Ignore this if we get 'accepted ics'.
    setIcsClocks(game, true);
}

void processIcsCommand(Game *game, const char *inputStr)
{
    char icsStr[255];

    if (sscanf(inputStr, "ics %255s", icsStr) < 1)
    {
        printf("Error (bad args): %s\n", inputStr);
        return;
    }

    // Turn this on iff not playing against a local opponent.
    // Assuming for now that every ICS server we care about (namely, FICS)
    // does the funky "clocks do not start ticking on the first move, and
    // no increment is applied after the first move" thing.  If some servers
    // differ (ICC?) then we'll just have to adjust.
    setIcsClocks(game, strcmp(icsStr, "-"));
}

void processMemoryCommand(Game *game, const char *inputStr)
{
    // If user overrode, it cannot be set here.
    if (gPreCalc.userSpecifiedHashSize != -1)
    {
        printf("Error (unimplemented command): %s\n", inputStr);
        return;
    }

    int64 memMiB;
    if (sscanf(inputStr, "memory %" PRId64, &memMiB) < 1 || memMiB < 0)
    {
        printf("Error (bad args): %s\n", inputStr);
        return;
    }

    game->EngineConfig().SetSpinClamped(Config::MaxMemorySpin, memMiB);
}

void processCoresCommand(Game *game, const char *inputStr)
{
    // If user overrode, it cannot be set here.
    if (gPreCalc.userSpecifiedNumThreads != -1)
    {
        printf("Error (unimplemented command): %s\n", inputStr);
        return;
    }

    int numCores;
    if (sscanf(inputStr, "cores %d", &numCores) < 1 || numCores < 0)
    {
        printf("Error (bad args): %s\n", inputStr);
        return;
    }

    game->EngineConfig().SetSpinClamped(Config::MaxThreadsSpin, numCores);
}

// This runs as a coroutine with the main thread, and can switch off to it
// at any time.  If it simply exits, it will immediately be called again.
static void xboardPlayerMove(Game *game)
{
    char *inputStr;
    int turn;
    Board tmpBoard;
    MoveT myMove;

    // Other various commands' vars.
    const Board &board = game->Board(); // shorthand
    
    inputStr = getStdinLine(MAXBUFLEN, gXboardState.sw);

    // I tried (when practical) to handle commands in the order they are
    // documented in engine-intf.html, in other words, fairly random...

    if (matches(inputStr, "uci"))
    {
        // Special case.  Switch to UCI interface.
        return processUciCommand(game, gXboardState.sw);
    }
    // Ignore certain commands...
    else if (matches(inputStr, "accepted") ||
             matches(inputStr, "rejected") ||
             // (We do not accept draw offers yet.)
             matches(inputStr, "draw") ||
             matches(inputStr, "hint") ||
             matches(inputStr, "name") ||
             matches(inputStr, "computer"))
    {
        LOG_DEBUG("ignoring cmd: %s", inputStr);
    }
    // Return others as unimplemented...
    else if (matches(inputStr, "variant") ||
             matches(inputStr, "playother") ||
             matches(inputStr, "usermove") ||
             matches(inputStr, "bk") ||
             matches(inputStr, "analyze") ||
             matches(inputStr, "pause") ||
             matches(inputStr, "resume") ||
             // (bughouse commands:)
             matches(inputStr, "partner") ||
             matches(inputStr, "ptell") ||
             matches(inputStr, "holding"))
    {
        printf("Error (unimplemented command): %s\n", inputStr);
    }
    else if (matches(inputStr, "xboard"))
    {
        processXboardCommand(game, gXboardState.sw);
    }
    else if (matches(inputStr, "protover"))
    {
        processProtoverCommand(game, inputStr);
    }
    else if (matches(inputStr, "new"))
    {
        // New game, computer is Black.
        game->StopAndForce();
        game->EngineConfig().SetSpin(Config::MaxDepthSpin, 0);
        game->EngineConfig().SetCheckbox(Config::RandomMovesCheckbox, false);
        game->SetEngineControl(1, true);
        game->NewGame();
        gXboardState.engineLastPlayed = 1;
        gXboardState.badPosition = false; // hope for the best.
        gXboardState.newgame = true;
    }
    else if (matches(inputStr, "quit"))
    {
        game->StopAndForce();
        exit(0);
    }
    else if (matches(inputStr, "random"))
    {
#if 1 // bldbg: goes out for debugging
        // toggle random moves.
        game->EngineConfig().ToggleCheckbox(Config::RandomMovesCheckbox);
#endif
    }
    else if (matches(inputStr, "force"))
    {
        // Stop everything.
        game->StopAndForce();
    }
    else if (matches(inputStr, "white") ||
             matches(inputStr, "black"))
    {
        // Stop everything.  Engine plays the other color.  This is not
        // exactly as specified.  Too bad, I'm not going to change whose
        // turn it is to move!
        game->StopAndForce();
        turn = matches(inputStr, "white");
        game->SetEngineControl(turn ^ 1, true);
        gXboardState.engineLastPlayed = turn ^ 1;
    }
    else if (matches(inputStr, "level"))
    {
        processLevelCommand(game, inputStr);
    }
    else if (matches(inputStr, "st"))
    {
        processStCommand(game, inputStr);
    }
    else if (matches(inputStr, "sd"))
    {
        processSdCommand(game, inputStr);
    }
    else if (matches(inputStr, "time"))
    {
        processTimeCommand(game, inputStr, false);
    }
    else if (matches(inputStr, "otim"))
    {
        processTimeCommand(game, inputStr, true);
    }
    else if (matches(inputStr, "?"))
    {
        game->MoveNow(); // Move now.
    }
    else if (matches(inputStr, "ping"))
    {
        processPingCommand(game, inputStr);
    }
    else if (matches(inputStr, "result"))
    {
        // We don't care if we won, lost, or drew.  Just stop thinking.
        game->StopAndForce();
    }
    else if (matches(inputStr, "setboard"))
    {
        bool wasRunning = game->Stop();
        gXboardState.badPosition = false; // hope for the best.
        if (fenToBoard(inputStr + strlen("setboard "), &tmpBoard) < 0)
        {
            gXboardState.badPosition = true;
        }
        else
        {
            // The documentation implies the user sets up positions with this
            //  command.  Therefore we force a new game, but we could instead
            //  attempt to detect if we are more or less in the same game and
            //  not clear the hash, like we do w/uci "position" command.
            game->NewGame(tmpBoard, false);
            if (wasRunning)
                game->Go();
        }
    }
    else if (matches(inputStr, "edit"))
    {
        bool wasRunning = game->Stop();
        gXboardState.badPosition = false; // hope for the best.
        Position tmpPosition = board.Position();
        std::string errString;
        xboardEditPosition(tmpPosition);

        if (tmpPosition.IsLegal(errString))
        {
            tmpBoard.SetPosition(tmpPosition);
            game->NewGame(tmpBoard, false);
            if (wasRunning)
                game->Go();
        }
        else
        {
            printf("tellusererror Illegal position: %s\n", errString.c_str());
            gXboardState.badPosition = true;
        }
    }
    else if (matches(inputStr, "undo"))
    {
        if (game->Rewind(1) < 0)
            printf("Error (start of game): %s\n", inputStr);
    }
    else if (matches(inputStr, "remove"))
    {
        // We assume here that we want to back up the clocks as well (since
        //  that is only fair).  However, xboard mentions nothing about that :|
        if (game->Rewind(2) < 0)
            printf("Error (ply %d): %s\n", game->CurrentPly(), inputStr);
    }
    else if (matches(inputStr, "hard"))
    {
        gXboardState.ponder = true;
        game->SetPonder(true); // Activate pondering, if necessary.
    }
    else if (matches(inputStr, "easy"))
    {
        gXboardState.ponder = false;
        game->SetPonder(false);
    }
    else if (matches(inputStr, "post"))
    {
        gXboardState.post = true;
    }
    else if (matches(inputStr, "nopost"))
    {
        gXboardState.post = false;
    }
    else if (matches(inputStr, "rating"))
    {
        processRatingCommand(game, inputStr);
    }
    else if (matches(inputStr, "ics"))
    {
        processIcsCommand(game, inputStr);
    }
    else if (matches(inputStr, "memory"))
    {
        processMemoryCommand(game, inputStr);
    }
    else if (matches(inputStr, "cores"))
    {
        processCoresCommand(game, inputStr);
    }
    // (Anything below this case needs a decent position.)
    else if (gXboardState.badPosition)
    {
        printf("Illegal move (bad position): %s\n", inputStr);
        return;
    }
    else if (matches(inputStr, "go"))
    {
        // Play the color on move, and start thinking.
        gXboardState.newgame = false;
        game->StopAndForce(); // Just in case.
        turn = board.Turn();
        game->SetEngineControl(turn, true);
        gXboardState.engineLastPlayed = turn;
        // game->LogClocks("go");
        game->Go();
    }
    else if (isMove(inputStr, &myMove, &board))
    {
        if (!isLegalMove(inputStr, &myMove, &board))
        {
            printf("Illegal move: %s\n", inputStr);
            return;
        }

        // At this point, we must have a valid move.
        game->MakeMove(myMove);
        gXboardState.newgame = false;
        // We may already be Go()ing, but this is necessary for newgame and
        //  other situations where we are Stop()ped.
        if (game->EngineControl(0) || game->EngineControl(1))
            game->Go();
    }
    else
    {
        // Default case.
        printf("Error (unknown command): %s\n", inputStr);
    }

    gXboardState.sw->Switch(); // Wait for more input.
}


static void xboardNotifyMove(Game *game, MoveT move)
{
    char tmpStr[MOVE_STRING_MAX];
    // This should switch on the fly to csOO if we ever implement chess960.
    static const MoveStyleT ms = { mnCAN, csK2, false };

    printf("move %s\n", move.ToString(tmpStr, &ms, NULL));
}


void xboardNotifyDraw(Game *game, const char *reason, MoveT *move)
{
    // I do not know of a way to claim a draw w/move atomically with Xboard
    //  (for instance, we know this next move will get us draw by repetition
    //  or draw by fifty-move rule).  So, there is a race where the opponent
    //  can make a move before we can claim the draw.  This only matters
    //  when playing on a chess server.  FIXME.
    if (move != NULL && *move != MoveNone)
    {
        xboardNotifyMove(game, *move);
    }
    printf("1/2-1/2 {%s}\n", reason);
}

static void xboardNotifyResign(Game *game, int turn)
{
    printf("%d-%d {%s resigns}\n",
           turn, turn ^ 1, turn ? "Black" : "White");
}

static void xboardNotifyCheckmated(int turn)
{
    printf("%d-%d {%s mates}\n",
           turn, turn ^ 1, turn ? "White" : "Black");
}

static void xboardNotifyPV(Game *game, const RspPvArgsT *pvArgs)
{
    char mySanString[kMaxPvStringLen];
    const DisplayPv &pv = pvArgs->pv; // shorthand
    const Board &board = game->Board(); // shorthand
    MoveStyleT pvStyle = {mnSAN, csOO, true};

    if (!gXboardState.post ||
        pv.BuildMoveString(mySanString, sizeof(mySanString), pvStyle,
                           board) < 1)
    {
        return;
    }

    printf("%d %d %u %d %s.\n",
           pv.Level() + 1, pv.Eval().LowBound(),
           // (Convert bigtime to centiseconds)
           uint32(game->Clock(board.Turn()).TimeTaken() / 10000),
           pvArgs->stats.nodes, mySanString);
}

static void xboardNotifyComputerStats(Game *game,
                                      const ThinkerStatsT *stats) { }
static void xboardPositionRefresh(const Position &position) { }
static void xboardNoop(void) { }
static void xboardStatusDraw(Game *game) { }
static void xboardNotifyTick(Game *game) { }

UIFuncTableT *uiXboardOps(void)
{
    static UIFuncTableT xboardUIFuncTable =
    {
        .init = xboardInit,
        .playerMove = xboardPlayerMove,
        .positionRefresh = xboardPositionRefresh,
        .exit = xboardNoop,
        .statusDraw = xboardStatusDraw,
        .notifyTick = xboardNotifyTick,
        .notifyMove = xboardNotifyMove,
        .notifyError = xboardNotifyError,
        .notifyPV = xboardNotifyPV,
        .notifyThinking = xboardNoop,
        .notifyPonder = xboardNoop,
        .notifyReady = xboardNoop,
        .notifyComputerStats = xboardNotifyComputerStats,
        .notifyDraw = xboardNotifyDraw,
        .notifyCheckmated = xboardNotifyCheckmated,
        .notifyResign = xboardNotifyResign,
    };

    return &xboardUIFuncTable;
}
