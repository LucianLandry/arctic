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
#include "gDynamic.h"
#include "gPreCalc.h"
#include "log.h"
#include "MoveList.h"
#include "playloop.h"
#include "ui.h"
#include "uiUtil.h"
#include "TransTable.h"

#define MAXBUFLEN 160

static struct {
    bool post;        // controls display of PV
    bool badPosition; // can be triggered by editing a bad position
    bool newgame;     // turned on every "new", turned off every "go"
    bool ponder;
} gXboardState;

#define OPPONENT_CLOCK (&game->actualClocks[0])
#define ENGINE_CLOCK (&game->actualClocks[1])


#if 0 // not needed, presently
// bool, and hopefully self-explanatory.  The "force mode" definition is
// more or less given in the documentation for the "force" command.
static int inForceMode(GameT *game)
{
    return !game->control[0] && !game->control[1];
}
#endif


static void xboardNotifyError(char *reason)
{
    printf("tellusererror Illegal position: %s\n", reason);
}


static void xboardEditPosition(Position &position, SwitcherContextT *sw)
{
    char *inputStr;

    int i = 0;
    int turn = 0;
    int coord;
    Piece piece;

    position.SetEnPassantCoord(FLAG); // assumed, for 'edit' command.
    position.SetPly(0);
    position.SetNcpPlies(0);

    while(1)
    {
        inputStr = getStdinLine(MAXBUFLEN, sw);

        if (matches(inputStr, "#"))
        {
            // Wipe board.
            for (i = 0; i < NUM_SQUARES; i++)
            {
                position.SetPiece(i, Piece());
            }
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
                printf("Error (edit: %c: bad coord): %s",
                       inputStr[0], &inputStr[1]);
                break;
            }

            // Set the new piece.
            piece = asciiToNative(inputStr[0]);
            if (!piece.IsEmpty())
            {
                piece = Piece(turn, piece.Type());
            }
            position.SetPiece(coord, piece);
            break;
        default:
            printf("Error (edit: unknown command): %s", inputStr);
            break;
        }
    }
}

static void xboardInit(GameT *game)
{
    static bool initialized;
    if (initialized)
        return;

    // Set unbuffered I/O (obviously necessary for output, also necessary for
    // input if we want to poll() correctly.)
    setbuf(stdout, NULL);
    setbuf(stdin, NULL);

    initialized = true;
}

void processXboardCommand(void)
{
    struct sigaction ignoreSig;
    int err;

    // We are definitely doing xboard.  So do some xboard-specific
    // stuff. ... such as ignoring SIGINT.  Also switch to uiXboard
    // if we have not already.
    xboardInit(NULL);
    memset(&ignoreSig, 0, sizeof(struct sigaction));
    ignoreSig.sa_flags = SA_RESTART;
    ignoreSig.sa_handler = SIG_IGN;
    err = sigaction(SIGINT, &ignoreSig, NULL);
    assert(err == 0);
    gUI = uiXboardOps();
}

// This runs as a coroutine with the main thread, and can switch off to it
// at any time.  If it simply exits, it will immediately be called again.
static void xboardPlayerMove(Thinker *th, GameT *game)
{
    char *inputStr;
    int protoVersion, myLevel, turn;
    int i, err;
    int64 i64;
    Board *board = &game->savedBoard; // shorthand.
    Board tmpBoard;

    // Move-related stuff.
    MoveT myMove;

    // These are for time controls.
    int centiSeconds, mps;
    char baseStr[20];

    // Other various commands' vars.
    char icsStr[255];
    int ourRating, oppRating;

    int base;
    bigtime_t baseTime = 0;
    int inc, perMoveLimit;

    inputStr = getStdinLine(MAXBUFLEN, &game->sw);

    // I tried (when practical) to handle commands in the order they are
    // documented in engine-intf.html, in other words, fairly random...

    if (matches(inputStr, "uci"))
    {
        // Special case.  Switch to UCI interface.
        return processUciCommand();
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

             // (bughouse commands.)
             matches(inputStr, "partner") ||
             matches(inputStr, "ptell") ||
             matches(inputStr, "holding"))
    {
        printf("Error (unimplemented command): %s", inputStr);
    }

    else if (matches(inputStr, "xboard"))
    {
        processXboardCommand();
    }

    else if (sscanf(inputStr, "protover %d", &protoVersion) == 1 &&
             protoVersion >= 2)
    {
        // Note: we do not care if these features are accepted or rejected.
        // We try to handle all input as well as possible.
        printf("feature analyze=0 myname=arctic%s.%s-%s variants=normal "
               "colors=0 ping=1 setboard=1 memory=%d done=1 debug=1 ics=1\n",
               VERSION_STRING_MAJOR, VERSION_STRING_MINOR,
               VERSION_STRING_PHASE, !gPreCalc.userSpecifiedHashSize);
    }

    else if (matches(inputStr, "new"))
    {
        gVars.gameCount++;

        // New game, computer is Black.
        gXboardState.badPosition = false; // hope for the best.
        setForceMode(th, game);

        // associate clocks correctly.
        game->clocks[0] = OPPONENT_CLOCK;
        game->clocks[1] = ENGINE_CLOCK;

        gVars.ponder = false; // disable pondering on first ply.
        th->Config().SetSpin(Config::MaxDepthSpin, 0);
        game->control[1] = 1;

        GameNew(game);

        th->Config().SetCheckbox(Config::RandomMovesCheckbox, false);
        gXboardState.newgame = true;
    }

    else if (matches(inputStr, "quit"))
    {
        th->CmdBail();
        exit(0);
    }

    else if (matches(inputStr, "random"))
    {
        const Config::CheckboxItem *cbItem =
            th->Config().CheckboxItemAt(Config::RandomMovesCheckbox);
        bool randomMoves = cbItem ? cbItem->Value() : false;
#if 1 // bldbg: goes out for debugging
        th->Config().SetCheckbox(Config::RandomMovesCheckbox,
                                 !randomMoves); // toggle random moves.
#endif
    }

    else if (matches(inputStr, "force"))
    {
        // Stop everything.
        setForceMode(th, game);
    }

    else if (matches(inputStr, "white") ||
             matches(inputStr, "black"))
    {
        // Stop everything.  Engine plays the other color.  This is not
        // exactly as specified.  Too bad, I'm not going to change whose
        // turn it is to move!
        setForceMode(th, game);
        turn = matches(inputStr, "white");
        game->control[turn ^ 1] = 1;
    }

    else if (sscanf(inputStr, "level %d %20s %d", &mps, baseStr, &inc) == 3)
    {
        err = 0;
        if ((strchr(baseStr, ':') && !TimeStringIsValid(baseStr)) ||
            (!strchr(baseStr, ':') && sscanf(baseStr, "%d", &base) != 1))
        {
            printf("Error (bad parameter '%s'): %s", baseStr, inputStr);
            err = 1;
        }
        else if (!strchr(baseStr, ':'))
        {
            // 'base' is in minutes (and is already filled out).
            baseTime = ((bigtime_t) base) * 60 * 1000000;
        }
        else
        {
            // base is in more-or-less standard time format.
            baseTime = TimeStringToBigTime(baseStr);
        }

        for (i = 0; !err && i < NUM_PLAYERS; i++)
        {
            game->origClocks[i].ReInit()
                .SetStartTime(baseTime)
                .Reset()
                .SetTimeControlPeriod(mps)
                // Incremental time control.
                .SetIncrement(bigtime_t(inc) * 1000000);
        }
        if (gXboardState.newgame)
        {
            // Game has not started yet.  Under xboard this means "set
            // clocks in addition to time controls"
            ClocksReset(game);
        }
        // ClocksPrint(game, "level");
    }
    else if (sscanf(inputStr, "st %d", &perMoveLimit) == 1)
    {
        // Note: 'st' and 'level' are mutually exclusive, according to the
        // documentation.
        if (perMoveLimit < 0)
        {
            perMoveLimit = 0;
        }
        for (i = 0; i < NUM_PLAYERS; i++)
        {
            game->origClocks[i].ReInit()
                .SetPerMoveLimit(bigtime_t(perMoveLimit) * 1000000);
        }
        if (gXboardState.newgame)
        {
            // Game has not started yet.  Under xboard this means "set
            // clocks in addition to time controls"
            ClocksReset(game);
        }
    }

    else if (sscanf(inputStr, "sd %d", &myLevel) == 1)
    {
        // Set depth.  I took out an upper limit check.  If you want
        // depth 5000, okay ...
        if (myLevel > 0)
        {
            if (th->Config().SetSpinClamped(Config::MaxDepthSpin, myLevel) !=
                Config::Error::None)
            {
                printf("Error (cannot set maxDepth for this engine): %s",
                       inputStr);
            }
        }
        else
        {
            printf("Error (bad parameter %d): %s", myLevel, inputStr);
        }
    }

    else if (sscanf(inputStr, "time %d", &centiSeconds) == 1)
    {
        // Set engine clock.
        ENGINE_CLOCK->SetTime(bigtime_t(centiSeconds) * 10000);

        // I interpret the xboard doc's "idioms" section as saying the computer
        // will not be on move when this command comes in.  But just in case
        // it is ...
        GoaltimeCalc(game);
        // ClocksPrint(game, "time");
    }

    else if (sscanf(inputStr, "otim %d", &centiSeconds) == 1)
    {
        // Set opponent clock.
        OPPONENT_CLOCK->SetTime(bigtime_t(centiSeconds) * 10000);
        // ClocksPrint(game, "otim");
    }

    else if (matches(inputStr, "?"))
    {
        // Move now.
        PlayloopCompMoveNowAndSync(*th);
    }

    else if (sscanf(inputStr, "ping %d", &i) == 1)
    {
        // xboard docs imply it is very unlikely we will see a ping while
        //  moving.  "However, if you do..." then we still try to do the right
        //  thing.  Still, reading the protocol strictly, it is possible we
        //  might hang here forever if there is no search limit at all and no
        //  preceeding "move now".
        if (th->CompIsThinking())
            PlayloopCompProcessRspUntilIdle(*th);

        printf("pong %d\n", i);
    }

    else if (matches(inputStr, "result"))
    {
        // We don't care if we won, lost, or drew.  Just stop thinking.
        th->CmdBail();
    }

    else if (matches(inputStr, "setboard"))
    {
        gXboardState.badPosition = false; // hope for the best.
        th->CmdBail();

        if (fenToBoard(inputStr + strlen("setboard "), &tmpBoard) < 0)
        {
            gXboardState.badPosition = true;
        }
        else
        {
            // We could attempt to detect if we are more or less in the
            // same game and not clear the hash, like we do w/uci "position"
            // command.
            GameNewEx(game, &tmpBoard, false);
        }
    }

    else if (matches(inputStr, "edit"))
    {
        gXboardState.badPosition = false; // hope for the best.
        th->CmdBail();

        Position tmpPosition = board->Position();
        std::string errString;
        xboardEditPosition(tmpPosition, &game->sw);

        if (tmpPosition.IsLegal(errString))
        {
            tmpBoard.SetPosition(tmpPosition);
            GameNewEx(game, &tmpBoard, false);
        }
        else
        {
            reportError(false, "Error: illegal position: %s",
                        errString.c_str());
            gXboardState.badPosition = true;
        }
    }

    else if (matches(inputStr, "undo"))
    {
        if (GameRewind(game, 1) < 0)
        {
            printf("Error (undo: start of game): %s", inputStr);
        }
    }

    else if (matches(inputStr, "remove"))
    {
        if (GameRewind(game, 2) < 0)
        {
            printf("Error (remove: ply %d): %s",
                   GameCurrentPly(game), inputStr);
        }
    }

    else if (matches(inputStr, "hard"))
    {
        gXboardState.ponder = true;
        if (!gXboardState.newgame)
        {
            // Activate pondering, if necessary.
            gVars.ponder = true;
            GameCompRefresh(game);
        }
    }

    else if (matches(inputStr, "easy"))
    {
        gXboardState.ponder = false;
        gVars.ponder = false;
        GameCompRefresh(game);
    }

    else if (matches(inputStr, "post"))
    {
        gXboardState.post = true;
    }

    else if (matches(inputStr, "nopost"))
    {
        gXboardState.post = false;
    }

    else if (sscanf(inputStr, "rating %d %d", &ourRating, &oppRating) == 2)
    {
        // 'rating' could be useful to implement when determining how
        // to evaluate a draw.  However, right now I only use it to force
        // ICS mode as a backup for when the xboard UI does not understand the
        // "ics" command.
        game->icsClocks = 1;
    }

    else if (sscanf(inputStr, "ics %20s", icsStr) == 1)
    {
        // Turn this on iff not playing against a local opponent.
        // Assuming for now that every ICS server we care about (namely, FICS)
        // does the funky "clocks do not start ticking on the first move, and
        // no increment is applied after the first move" thing.  If some servers
        // differ (ICC?) then we'll just have to adjust.
        game->icsClocks = strcmp(icsStr, "-");
    }

    else if (sscanf(inputStr, "memory %" PRId64, &i64) == 1)
    {
        // If user overrode, it cannot be set here.
        if (gPreCalc.userSpecifiedHashSize)
        {
            printf("Error (unimplemented command): %s", inputStr);
        }
        else if (th->CompIsThinking())
        {
            printf("Error (command received while thinking): %s", inputStr);
        }
        else
        {
            gTransTable.Reset(i64 * 1024 * 1024); // MB -> bytes
        }
    }

    // (Anything below this case needs a decent position.)
    else if (gXboardState.badPosition)
    {
        printf("Illegal move (bad position): %s", inputStr);
        return;
    }

    else if (matches(inputStr, "go"))
    {
        // Play the color on move, and start thinking.
        gXboardState.newgame = false;
        gVars.ponder = gXboardState.ponder;
        setForceMode(th, game); // Just in case.

        turn = board->Turn();
        game->control[turn] = 1;
        game->clocks[turn] = ENGINE_CLOCK;
        game->clocks[turn ^ 1] = OPPONENT_CLOCK;
        ENGINE_CLOCK->Start();
        // ClocksPrint(game, "go");
        GoaltimeCalc(game);
        th->CmdSetBoard(*board);
        th->CmdThink();
    }

    else if (isMove(inputStr, &myMove, board))
    {
        if (!isLegalMove(inputStr, &myMove, board))
        {
            printf("Illegal move: %s", inputStr);
            return;
        }

        // At this point, we must have a valid move.
        th->CmdBail();

        gXboardState.newgame = false;
        gVars.ponder = gXboardState.ponder;
#if 0 // I think GameMoveCommit takes care of this, and in any case,
      // would override it.
        if (!inForceMode(game))
        {
            ClockStop(OPPONENT_CLOCK);
            ClockStart(ENGINE_CLOCK);
        }
#endif
        GameMoveCommit(game, &myMove, false);
    }

    else
    {
        /* Default case. */
        printf("Error (unknown command): %s", inputStr);
    }

    /* Wait for more input. */
    SwitcherSwitch(&game->sw);
}


static void xboardNotifyMove(MoveT move)
{
    char tmpStr[MOVE_STRING_MAX];
    // This should switch on the fly to csOO if we ever implement chess960.
    static const MoveStyleT ms = { mnCAN, csK2, false };

    printf("move %s\n", move.ToString(tmpStr, &ms, NULL));
}


void xboardNotifyDraw(const char *reason, MoveT *move)
{
    /* I do not know of a way to claim a draw w/move atomically with Xboard
       (for instance, we know this next move will get us draw by repetition
       or draw by fifty-move rule).  So, there is a race where the opponent
       can make a move before we can claim the draw.  This only matters
       when playing on a chess server.  FIXME.
    */
    if (move != NULL && *move != MoveNone)
    {
        xboardNotifyMove(*move);
    }
    printf("1/2-1/2 {%s}\n", reason);
}


static void xboardNotifyResign(int turn)
{
    printf("%d-%d {%s resigns}\n",
           turn, turn ^ 1, turn ? "Black" : "White");
}


static void xboardNotifyCheckmated(int turn)
{
    printf("%d-%d {%s mates}\n",
           turn, turn ^ 1, turn ? "White" : "Black");
}


static void xboardNotifyPV(GameT *game, const RspPvArgsT *pvArgs)
{
    char mySanString[65];
    const DisplayPv &pv = pvArgs->pv; // shorthand
    Board &board = game->savedBoard; // shorthand
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
           uint32(game->clocks[board.Turn()]->TimeTaken() / 10000),
           pvArgs->stats.nodes, mySanString);
}

static bool xboardShouldCommitMoves(void)
{
    return true;
}

static void xboardNotifyComputerStats(GameT *game,
                                      const ThinkerStatsT *stats) { }
static void xboardPositionRefresh(const Position &position) { }
static void xboardNoop(void) { }
static void xboardStatusDraw(GameT *game) { }
static void xboardNotifyTick(GameT *game) { }

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
        .shouldCommitMoves = xboardShouldCommitMoves
    };

    return &xboardUIFuncTable;
}
