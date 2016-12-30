//--------------------------------------------------------------------------
//                   uiNcurses.cpp - ncurses UI for Arctic
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#include <assert.h>
#include <ctype.h>  // isupper()
#include <curses.h> // curs_set()
#include <locale.h> // setlocale()
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h> // exit()
#include <string.h> // strlen()

#include "Clock.h"
#include "clockUtil.h"
#include "conio.h"
#include "gPreCalc.h"
#include "log.h"
#include "Pv.h"
#include "ui.h"
#include "uiUtil.h"
#include "Variant.h"

using arctic::File;
using arctic::Rank;

#define SYSTEMCOL       GREEN
#define TICKCOL         BLUE
#define BOARDCOL        BLUE

#define ENTER 13
#define ESC   27
#define BACKSPACE 263

#define SQUARE_WIDTH 5   // width of chess square, in characters
#define OPTIONS_X (SQUARE_WIDTH * 8 + 2) // 1 since one-based, + 1 for ticks
#define OPTIONS_X2 (OPTIONS_X + 15)
#define SCREEN_WIDTH 80

static struct {
    int col[2];    // player colors.
    int flipped;   // bool, is the board inverted (black on the bottom)
    int cursCoord; // coordinate cursor is at.
    Game *game;
    Switcher *sw;
} gBoardIf;


static void UIPrintPositionStatus(const Position &position)
{
    // all shorthand.
    cell_t ncheck = position.CheckingCoord();
    cell_t ebyte = position.EnPassantCoord();

    textcolor(LIGHTGRAY);

    // print castle status.
    gotoxy(OPTIONS_X, 14);
    cprintf("castle QKqk: %c%c%c%c",
            position.CanCastleOOO(0) ? 'y' : 'n',
            position.CanCastleOO(0)  ? 'y' : 'n',
            position.CanCastleOOO(1) ? 'y' : 'n',
            position.CanCastleOO(1)  ? 'y' : 'n');

    // print en passant status.
    gotoxy(OPTIONS_X, 15);
    cprintf("enpass: ");
    if (ebyte == FLAG)
        cprintf("  ");
    else
        cprintf("%c%c", AsciiFile(ebyte), AsciiRank(ebyte));

    // print check status.
    gotoxy(OPTIONS_X, 16);
    cprintf("chk: ");
    if (ncheck == FLAG)
        cprintf("   ");
    else if (ncheck == DOUBLE_CHECK)
        cprintf("dis");
    else // normal check
        cprintf("%c%c ", AsciiFile(ncheck), AsciiRank(ncheck));
}

static void UINotifyTick()
{
    char timeStr[CLOCK_TIME_STR_LEN];
    int i;
    bigtime_t myTime, perMoveTime;
    int bytesWritten = 0;
    char spaces[] = "                               ";

    // Display clocks.
    gotoxy(OPTIONS_X, 18);
    for (i = 0; i < NUM_PLAYERS; i++)
    {
        const Clock &myClock = gBoardIf.game->Clock(i);
        myTime = myClock.Time();

        // The clock goes red even when the time supposedly reaches 0, probably
        // because TimeStringFromBigTime() is rounding up.  FIXME: need to
        // rethink that.
        textcolor(myTime >= 0 ? LIGHTGRAY : RED);
        bytesWritten += cprintf("%s", TimeStringFromBigTime(timeStr, myTime));
        if (myClock.PerMoveLimit() < CLOCK_TIME_INFINITE)
        {
            perMoveTime = myClock.PerMoveTime();
            textcolor(perMoveTime >= 0 ? LIGHTGRAY : RED);
            bytesWritten += cprintf("(%s)", TimeStringFromBigTime(timeStr,
                                                                  perMoveTime));
        }
        textcolor(LIGHTGRAY);
        bytesWritten += cprintf("%s ", myClock.IsRunning() ? "r" : "s");
    }

    // Prevent old longer clock-line strings from sticking around.
    if ((uint) bytesWritten < sizeof(spaces))
    {
        cprintf("%s", &spaces[bytesWritten]);
    }
}

static void UIStatusDraw()
{
    bigtime_t timeTaken;
    Game *game = gBoardIf.game;        // shorthand
    const Board &board = game->Board(); // shorthand.
    int turn = board.Turn();

    UIPrintPositionStatus(board.Position());
    UINotifyTick();

    gotoxy(OPTIONS_X, 20);
    timeTaken = game->Clock(turn ^ 1).TimeTaken();
    cprintf("move: %d (%.2f sec)     ",
            (board.Ply() >> 1) + 1,
            ((double) timeTaken) / 1000000);
    gotoxy(OPTIONS_X, 21);
    textcolor(SYSTEMCOL);
    cprintf("%s\'s turn", turn ? "black" : "white");
    gotoxy(OPTIONS_X, 22);
    cprintf(board.IsInCheck() ? "<check>" : "       ");
}

// prints out expected move sequence at the bottom of the screen.
static void UINotifyPV(const EnginePvArgsT *pvArgs)
{
    char spaces[80];
    char mySanString[79 - 18];
    char evalString[20];
    int len;
    const DisplayPv &pv = pvArgs->pv; // shorthand.
    MoveStyleT pvStyle = {mnSAN, csOO, true};

    // Get a suitable string of moves to print.
    if (pv.BuildMoveString(mySanString, sizeof(mySanString), pvStyle,
                           gBoardIf.game->Board()) < 1)
    {
        return;
    }

    // blank out the last pv.
    memset(spaces, ' ', 79);
    spaces[79] = '\0';
    gotoxy(1, 25);
    textcolor(SYSTEMCOL);
    cprintf("%s", spaces);

    if (pv.Eval().DetectedWinOrLoss())
    {
        int movesUntilMate = pv.Eval().MovesToWinOrLoss();
        len = snprintf(evalString, sizeof(evalString), "%smate",
                       pv.Eval().DetectedLoss() ? "-" : "");
        if (movesUntilMate > 0)
        {
            snprintf(&evalString[len], sizeof(evalString) - len, "%d",
                     movesUntilMate);
        }
    }
    else
    {
        snprintf(evalString, sizeof(evalString), "%+.2f",
                 ((double) pv.Eval().LowBound()) / Eval::Pawn);
    }

    // print the new pv.
    gotoxy(1, 25);
    cprintf("pv: d%d %s %s.", pv.Level() + 1, evalString, mySanString);
}

#define CURSOR_NOBLINK 0
#define CURSOR_BLINK 1
#define CURSOR_HIDE 2

#if 0
// Use UTF-8 characters.  These work but I am already used to the slashes.
#define CURSOR_NW "\xe2\x94\x8c"
#define CURSOR_NE "\xe2\x94\x90"
#define CURSOR_SW "\xe2\x94\x94"
#define CURSOR_SE "\xe2\x94\x98"
#else
// Non-UTF8 fallback.
#define CURSOR_NW "\\"
#define CURSOR_NE "/"
#define CURSOR_SW "/"
#define CURSOR_SE "\\"
#endif

/* draws cursor at appropriate coordinate, according to mode 'mode'. */
static void UICursorDraw(int coord, int mode)
{
    int x, y;
    /* translate coord to xy coords of upper left part of cursor. */
    x = SQUARE_WIDTH * (gBoardIf.flipped ? 7 - File(coord): File(coord)) + 1;
    y = 3 * (gBoardIf.flipped ? Rank(coord) : 7 - Rank(coord)) + 1;

    if ((Rank(coord) + File(coord)) & 1) /* we on a board-colored spot */
        textbackground(BOARDCOL);
    textcolor(/* BROWN */ YELLOW + (mode == CURSOR_BLINK ? BLINK : 0));
    gotoxy(x, y);

    // Printing 1 char at a time is necessary because of the way the linux
    // console handles blinking characters (read: not well)
    cprintf("%s", mode == CURSOR_HIDE ? " " : CURSOR_NW);
    gotoxy(x + 4, y);
    cprintf("%s", mode == CURSOR_HIDE ? " " : CURSOR_NE);
    gotoxy(x, y + 2);
    cprintf("%s", mode == CURSOR_HIDE ? " " : CURSOR_SW);
    gotoxy(x + 4, y + 2);
    cprintf("%s", mode == CURSOR_HIDE ? " " : CURSOR_SE);
    textbackground(BLACK);      /* get rid of that annoying blink */
    textcolor(BLACK);
    gotoxy(SQUARE_WIDTH * 8 + 7, 24);
}

// Spews user option to screen, highlighting the first char.
static void prettyprint(int y, const char *option, const char *option2, ...)
{
    int i, j, didHighlight = 0;
    const char *myopt;

    va_list ap;
    char myBuf[80];

    for (i = 0; i < 2; i++)
    {
        if ((myopt = i ? option2 : option) == NULL)
            return;

        /* Hacky; assumes only one option has any arguments. */
        va_start(ap, option2);
        vsprintf(myBuf, myopt, ap);
        va_end(ap);

        gotoxy(i ? OPTIONS_X2 : OPTIONS_X, y);
        textcolor(LIGHTGRAY);

        for (j = 0, didHighlight = 0;
             (uint) j < strlen(myBuf);
             j++)
        {
            if (!didHighlight && isupper(myBuf[j]))
            {
                textcolor(WHITE);
                didHighlight = 1;
                cprintf("%c", myBuf[j]);
                textcolor(LIGHTGRAY);
                continue;
            }

            cprintf("%c", myBuf[j]);
        }
    }
}

static void UIWindowClear(int startx, int starty, int width, int height)
{
    int y, i;
    char spaces[81];

    assert(width <= 80);
    for (i = 0; i < width; i++)
    {
        sprintf(&spaces[i], " ");
    }

    textbackground(BLACK);

    for (y = starty; y < starty + height; y++)
    {
        gotoxy(startx, y);
        cprintf("%s", spaces);
    }
}

static void UIOptionsDraw(Game *game)
{
    const Config::SpinItem *sItem =
        game->EngineConfig().SpinItemAt(Config::MaxDepthSpin);
    int level = sItem == nullptr ? -1 : sItem->Value();
    const Config::CheckboxItem *cbItem =
        game->EngineConfig().CheckboxItemAt(Config::RandomMovesCheckbox);
    bool randomMoves = cbItem == nullptr ? false : cbItem->Value();

    sItem = game->EngineConfig().SpinItemAt(Config::HistoryWindowSpin);
    int histWindow = sItem == nullptr ? -1 : sItem->Value();
    
    UIWindowClear(OPTIONS_X, 1, SCREEN_WIDTH - OPTIONS_X, 12);
    gotoxy(OPTIONS_X, 1);
    textcolor(SYSTEMCOL);
    cprintf("Options:");
    prettyprint(2,  "New game",       "Level (%d)", level);
    prettyprint(3,  "Save game",      "White control (%s)",
                game->EngineControl(0) ? "C" : "P");
    prettyprint(4,  "Restore game",   "Black control (%s)",
                game->EngineControl(1) ? "C" : "P");
    prettyprint(5,  "Edit position",  "rAndom moves (%s)",
                randomMoves ? "On" : "Off");
    prettyprint(6,  "Quit",           "Ponder (%s)",
                game->Ponder() ? "On" : "Off");

    prettyprint(8,  "Generate moves", "History window (%d)", histWindow);
    prettyprint(9,  "Move now",       "Time control");
    prettyprint(10, "Flip board",     "Undo");
    prettyprint(11, "Color",          "redO");
#ifdef ENABLE_DEBUG_LOGGING
    prettyprint(12, "Debug logging",  NULL);
#endif
}

static void UIEditOptionsDraw()
{
    UIWindowClear(OPTIONS_X, 1, SCREEN_WIDTH - OPTIONS_X, 12);
    gotoxy(OPTIONS_X, 1);
    textcolor(SYSTEMCOL);
    cprintf("Options:");
    prettyprint(2, "Wipe board",     NULL);
    prettyprint(3, "Enpassant mark", NULL);
    prettyprint(4, "Castle mark",    NULL);
    prettyprint(5, "Switch turn",    NULL);
    prettyprint(6, "Done",           NULL);
}

static void UITimeOptionsDraw(Game *game, int applyToggle)
{
    char t1[CLOCK_TIME_STR_LEN], t2[CLOCK_TIME_STR_LEN];
    UIWindowClear(OPTIONS_X, 1, SCREEN_WIDTH - OPTIONS_X, 12);
    gotoxy(OPTIONS_X, 1);
    textcolor(SYSTEMCOL);

    Clock clocks[NUM_PLAYERS];
    for (int i = 0; i < NUM_PLAYERS; i++)
        clocks[i] = game->InitialClock(i);
    
    cprintf("Options:");
    prettyprint(2, "Start time(s) (%s %s)", NULL,
                TimeStringFromBigTime(t1, clocks[0].Time()),
                TimeStringFromBigTime(t2, clocks[1].Time()));
    prettyprint(3, "Increment(s) (%s %s)",  NULL,
                TimeStringFromBigTime(t1, clocks[0].Increment()),
                TimeStringFromBigTime(t2, clocks[1].Increment()));
    prettyprint(4, "Time control period(s) (%d %d)", NULL,
                clocks[0].TimeControlPeriod(),
                clocks[1].TimeControlPeriod());
    prettyprint(5, "Per-move limit (%s %s)", NULL,
                TimeStringFromBigTime
                (t1, clocks[0].PerMoveLimit()),
                TimeStringFromBigTime
                (t2, clocks[1].PerMoveLimit()));
    prettyprint(7, "Apply to current game", NULL);
    prettyprint(8, "Changes: (%s)", NULL,
                applyToggle == 0 ? "white" :
                applyToggle == 1 ? "black" :
                "both");
    prettyprint(10, "Done",           NULL);
}

static void UICursorMove(int key, int *coord)
{
    if ((key == KEY_UP && !gBoardIf.flipped) ||
        (key == KEY_DOWN && gBoardIf.flipped))
    {
        if ((*coord += 8) > 63)
            *coord -= NUM_SQUARES;
    }
    else if ((key == KEY_DOWN && !gBoardIf.flipped) ||
             (key == KEY_UP && gBoardIf.flipped))
    {
        if ((*coord -= 8) < 0)
            *coord += NUM_SQUARES;
    }
    else if ((key == KEY_LEFT && !gBoardIf.flipped) ||
             (key == KEY_RIGHT && gBoardIf.flipped))
    {
        if (File(--(*coord)) == 7)
            *coord += 8;
    }
    else
    {
        // assume KEY_RIGHT, or KEY_LEFT && flipped
        if (!File(++(*coord)))
            *coord -= 8;
    }
}

static void UIPositionRefresh(const Position &position)
{
    int x, y;
    int i = 0;

    for (y = 0; y < 8; y++)
    {
        for (x = 0; x < 8; x++, i++)
        {
            if ((x + y) % 2)
                textbackground(BOARDCOL);
            else
                textbackground(BLACK);
            gotoxy((gBoardIf.flipped ? 7 - x : x) * SQUARE_WIDTH +
                   SQUARE_WIDTH / 2 + 1,
                   2 + (gBoardIf.flipped ? y : 7 - y) * 3);

            Piece piece(position.PieceAt(i));
            
            if (!piece.IsEmpty())
            {
                // Use the appropriate color to draw white/black pieces.
                textcolor(gBoardIf.col[piece.Player()]);
            }

            // Draw a piece (or lack thereof).
            putch(nativeToBoardAscii(piece));
        }
    }
    textbackground(BLACK);
}

static void UITicksDraw()
{
    int x;
    textcolor(TICKCOL);
    for (x = 0; x < 8; x++)
    {
        /* Clear any garbage from 'Generate Moves' dump. */
        gotoxy(OPTIONS_X - 1, 23-3*x - 1);
        cprintf(" ");
        gotoxy(OPTIONS_X - 1, 23-3*x + 1);
        cprintf(" ");

        gotoxy(OPTIONS_X - 1, 23-3*x);
        cprintf("%d", gBoardIf.flipped ? 8 - x : x + 1);
    }
    gotoxy(1, 25);
    cprintf(gBoardIf.flipped ? 
            "  h    g    f    e    d    c    b    a                 " :
            "  a    b    c    d    e    f    g    h                 ");
}

static void UIExit()
{
    doneconio();
}

// Prompts the user, and waits for a single char as input.
// FIXME: modal() and modalString() both assume the user will respond promptly;
//  in the meantime other events (like engine responses) may be delayed!  One
//  solution would be to stop and restart game processing.  A slightly nicer
//  solution would be to make modals interruptible.  OTOH, such a solution would
//  need to take PV notifications into account.
static int modal(const char *format, ...)
{
    int chr, i;
    int len;
    char message[80];
    va_list ap;

    va_start(ap, format);
    len = vsnprintf(message, sizeof(message), format, ap);
    va_end(ap);
    assert(len < int(sizeof(message))); // check for truncation

    // Display the message.
    gotoxy((SCREEN_WIDTH / 2) - len / 2, 25);
    textcolor(MAGENTA);
    cprintf("%s", message);

    // Wait for input.
    chr = getch();
    if (chr == ESC) // bail on ESC (is this really a good idea?)
    {
        UIExit();
        exit(0);
    }

    // Now, blank the entire message.
    gotoxy((SCREEN_WIDTH / 2) - len / 2, 25);
    for (i = 0; i < len; i++)
    {
        cprintf(" ");
    }

    UITicksDraw();
    gotoxy(1, 1); // (just in case)
    return chr;
}

// 'myStrLen' == sizeof(myStr) (including terminator) and must be at least 2
// bytes long.
// Returns 'myStr'.
static char *modalString(char *myStr, int myStrLen,
                         const char *validChars, const char *format, ...)
{
    int chr, i;
    int len;
    char message[80];
    int curStrLen;
    int x;
    int y = 25;
    va_list ap;

    assert(myStrLen >= 2);
    memset(myStr, 0, myStrLen);

    va_start(ap, format);
    len = vsnprintf(message, 80, format, ap);
    va_end(ap);
    assert(len < 80);

    // Display the message.
    x = (SCREEN_WIDTH / 2) - len / 2;
    gotoxy(x, y);
    textcolor(MAGENTA);
    x += cprintf("%s", message);

    // Get input.
    for (curStrLen = 0; (chr = getch()) != ENTER;)
    {
        if (chr == ESC) // bail on ESC
        {
            UIExit();
            exit(0);
        }
        else if (curStrLen < myStrLen - 1 &&
                 (validChars ? strchr(validChars, chr) != NULL : isprint(chr)))
        {
            myStr[curStrLen++] = chr;
            x += cprintf("%c", chr);
        }
        else if (chr == BACKSPACE && curStrLen > 0) // backspace
        {
            myStr[--curStrLen] = '\0';
            gotoxy(--x, y);
            cprintf(" ");
            gotoxy(x, y);
        }
    }

    // Now, blank the entire message.
    len += curStrLen;
    gotoxy((SCREEN_WIDTH / 2) - len / 2, 25);
    for (i = 0; i < len; i++)
    {
        cprintf(" ");
    }

    UITicksDraw();
    gotoxy(1, 1); /* justncase */
    return myStr;
}

static void UINotifyError(char *reason)
{
    modal("Error: %s", reason);
}

// Edits a board.
static void UIEditPosition(Position &position)
{   
    int c, i;
    int *coord = &gBoardIf.cursCoord; // shorthand.
    char validChars[] = "WwEeCcDdSs PpRrNnBbQqKk";

    UIEditOptionsDraw();
    UICursorDraw(*coord, CURSOR_BLINK);

    position.SetPly(0);
    position.SetNcpPlies(0);

    while (true)
    {
        UIPrintPositionStatus(position);
        gotoxy(OPTIONS_X, 21);
        textcolor(SYSTEMCOL);
        cprintf("%s\'s turn", position.Turn() ? "black" : "white");
        // I do this here just so the cursor ends up in an aesthetically
        //  pleasing spot.
        gotoxy(OPTIONS_X, 24);
        textcolor(LIGHTCYAN);
        cprintf("Edit             ");

        c = getch();
        if (strchr(validChars, c) != NULL)
        {
            switch(c)
            {
            case 'W':
            case 'w':
                // Wipe position.
                for (i = 0; i < NUM_SQUARES; i++)
                    position.SetPiece(i, Piece());
                UIPositionRefresh(position);
                break;

            case 'E':
            case 'e':
                // (possibly) set an enpassant square.
                position.SetEnPassantCoord(*coord);
                position.Sanitize();
                break;

            case 'C':
            case 'c':
                // (possibly) set castling.
                for (i = 0; i < NUM_PLAYERS; i++)
                {
                    CastleStartCoordsT start =
                        Variant::Current()->Castling(i).start;
                    if (*coord == start.king)
                        position.EnableCastling(i);
                    else if (*coord == start.rookOO) 
                        position.EnableCastlingOO(i);
                    else if (*coord == start.rookOOO) 
                        position.EnableCastlingOOO(i);
                    else
                        position.ClearCastling();
                }
                position.Sanitize();
                break;

            case 'S':
            case 's':
                // Switch turn.
                position.SetTurn(position.Turn() ^ 1);
                break;

            case 'D':
            case 'd':
                // bail from editing mode.
                return;

            default:
                // At this point, it must be a piece, or nothing.
                Piece piece(asciiToNative(c));

                position.SetPiece(*coord, piece);
                position.Sanitize();
                UIPositionRefresh(position);
                break;
            }
        }
        if (c != KEY_UP && c != KEY_DOWN && c != KEY_LEFT && c != KEY_RIGHT)
        {
            continue;
        }
            
        // At this point we have a valid direction.
        UICursorDraw(*coord, CURSOR_HIDE);  /* Unmark current loc */
        UICursorMove(c, coord);
        UICursorDraw(*coord, CURSOR_BLINK); /* Blink new loc */
    } // end while (true)
}

#define APPLY_BOTH (NUM_PLAYERS)

// Adjusts time.
static void UITimeMenu(Game *game)
{   
    int c, i;
    int *coord = &gBoardIf.cursCoord; // shorthand.
    char validChars[] = "sitpacd";
    char timeStr[CLOCK_TIME_STR_LEN];
    int timeControlPeriod;
    Clock clock;
    
    // 0 -> white
    // 1 -> black
    // 2 -> both
    static int applyToggle = APPLY_BOTH;

    UICursorDraw(*coord, CURSOR_BLINK);

    while (true)
    {
        UITimeOptionsDraw(game, applyToggle);
        // I do this here just so the cursor ends up in an aesthetically
        //  pleasing spot.
        gotoxy(OPTIONS_X, 24);
        textcolor(LIGHTCYAN);
        cprintf("Time             ");

        c = tolower(getch());
        if (strchr(validChars, c) != NULL)
        {
            switch(c)
            {
            case 's':
                do
                {
                    modalString(timeStr, 9, /* xx:yy:zz\0 */
                                "0123456789:inf", "Set start time to? >");
                } while (!TimeStringIsValid(timeStr));

                for (i = 0; i < NUM_PLAYERS; i++)
                {
                    if (applyToggle == i || applyToggle == APPLY_BOTH)
                    {
                        clock = game->InitialClock(i);
                        clock.SetStartTime(TimeStringToBigTime(timeStr))
                            .Reset();
                        game->SetInitialClock(i, clock);
                    }
                }
                break;
            case 'i':
                do
                {
                    modalString(timeStr, 9, /* xx:yy:zz\0 */
                                "0123456789:", "Set increment to? >");
                } while (!TimeStringIsValid(timeStr));

                for (i = 0; i < NUM_PLAYERS; i++)
                {
                    if (applyToggle == i || applyToggle == APPLY_BOTH)
                    {
                        clock = game->InitialClock(i);
                        clock.SetIncrement(TimeStringToBigTime(timeStr));
                        game->SetInitialClock(i, clock);
                    }
                }
                break;
            case 't':
                do
                {
                    modalString(timeStr, 9, /* xx:yy:zz\0 */
                                "0123456789", "Set time control period to? >");
                } while (sscanf(timeStr, "%d", &timeControlPeriod) < 1);

                for (i = 0; i < NUM_PLAYERS; i++)
                {
                    if (applyToggle == i || applyToggle == APPLY_BOTH)
                    {
                        clock = game->InitialClock(i);
                        clock.SetTimeControlPeriod(timeControlPeriod);
                        game->SetInitialClock(i, clock);
                    }
                }
                break;
            case 'p':
                do
                {
                    modalString(timeStr, 9, /* xx:yy:zz\0 */
                                "0123456789:inf", "Set per-move limit to? >");
                } while (!TimeStringIsValid(timeStr));

                for (i = 0; i < NUM_PLAYERS; i++)
                {
                    if (applyToggle == i || applyToggle == APPLY_BOTH)
                    {
                        clock = game->InitialClock(i);
                        clock.SetPerMoveLimit(TimeStringToBigTime(timeStr));
                        game->SetInitialClock(i, clock);
                    }
                }
                break;
            case 'a':
                game->ResetClocks();
                UIStatusDraw();
                break;
            case 'c':
                if (++applyToggle > APPLY_BOTH)
                    applyToggle = 0;
                break;
            case 'd':
                // bail from time menu.
                return;
            default:
                break;
            }
        }
    } // end while
}

// Gets user input and translates it to valid command.
// Returns: command, or two numbers signaling source and destination.
static void UIGetCommand(uint8 command[], Game *game)
{   
    int c;
    bool gettingsrc = true;
    int *coord = &gBoardIf.cursCoord; // shorthand.
    const Board &board = game->Board(); // shorthand
    // It would be easy to use tolower() up front, but I guess I'm paranoid that
    //  KEY_UP etc. are interpreted as alphabetic in some odd locales.
    char validChars[] = "NnSsRrLlWwBbFfQqHhCcMmEeGgAaTtUuOoPp"
#ifdef ENABLE_DEBUG_LOGGING
        "Dd"
#endif
        ; // terminates 'validChars[]'

    while (true)
    {
        // Wait for actual input.
        while (!kbhit())
            gBoardIf.sw->Switch();
        c = getch();
        if (strchr(validChars, c) != NULL)
        {
            if (!gettingsrc)
                UICursorDraw(command[0], CURSOR_HIDE);
            command[0] = tolower(c); // valid one-char command
            return;
        }
        if (c == ENTER && gettingsrc)
        {
            // (ignore attempts to set a blank src)
            if (!board.PieceAt(*coord).IsEmpty())
            {
                command[0] = *coord;
                UICursorDraw(*coord, CURSOR_NOBLINK);
                gettingsrc = false;
            }
            continue;
        }
        else if (c == ENTER)
        {
            if (*coord == command[0]) // we want to unselect src spot
            {
                gettingsrc = true;
                UICursorDraw(*coord, CURSOR_BLINK);
                continue;
            }
            UICursorDraw(command[0], CURSOR_HIDE);
            command[1] = *coord;      // enter destination
            return;
        }
        if (c != KEY_UP && c != KEY_DOWN && c != KEY_LEFT && c != KEY_RIGHT)
            continue;

        // At this point we have a valid direction.
        if (gettingsrc || command[0] != *coord)
            UICursorDraw(*coord, CURSOR_HIDE);  // need to unmark current loc
        UICursorMove(c, coord);
        if (gettingsrc || command[0] != *coord)
            UICursorDraw(*coord, CURSOR_BLINK); // need to blink current loc
    } // end while
}

static void UIBoardDraw()
{
    int i, j;

    // note: a carriage return after drawing the board could clobber the ticks.
    for (i = 0; i < NUM_SQUARES; i++)
    {
        for (j = 0; j < 3; j++) // three rows per 'checker'
        {
            gotoxy(File(i) * SQUARE_WIDTH + 1,
                   22 - (Rank(i) * 3) + j);
            if (((File(i) + Rank(i)) & 1))
            {
                textcolor(BLACK);
                textbackground(BOARDCOL);               
            }
            else
            {
                textcolor(BOARDCOL);
                textbackground(BLACK);
            }
            cprintf("     ");
        }
    }
}

static void UIBoardFlip(const Board &board)
{
    UICursorDraw(gBoardIf.cursCoord, CURSOR_HIDE); // hide old cursor
    gBoardIf.flipped ^= 1;
    UITicksDraw();   // update ticks
    UIPositionRefresh(board.Position()); // update player positions
    UICursorDraw(gBoardIf.cursCoord, CURSOR_BLINK);
}

static void UIPlayerColorChange()
{
    const char *colors[NUM_PLAYERS] = {"White", "Black"};
    int i;

    char myStr[3];
    int myColor;

    for (i = 0; i < NUM_PLAYERS; i++)
    {
        do
        {
            modalString(myStr, 3, "0123456789", "%s color? >", colors[i]);
        } while (sscanf(myStr, "%d", &myColor) < 1 ||
                 myColor < 1 || myColor > 15 ||
                 (i == 1 && myColor == gBoardIf.col[0]));
        gBoardIf.col[i] = myColor;
    }
}

static void UISetDebugLoggingLevel()
{
    int i;
    while ((i = modal("Set debug level to (0-2) (higher -> more verbose)? >") - '0') < 0 ||
           i > 2)
        ;
    LogSetLevel(LogLevelT(i));
}

static void UINotifyThinking()
{
    gotoxy(OPTIONS_X, 24);
    textcolor(RED);
    cprintf("Thinking         ");
}

static void UINotifyPonder()
{
    gotoxy(OPTIONS_X, 24);
    textcolor(LIGHTGREEN);
    cprintf("Ready (pondering)");
    UICursorDraw(gBoardIf.cursCoord, CURSOR_BLINK);
}

static void UINotifyReady()
{
    gotoxy(OPTIONS_X, 24);
    textcolor(LIGHTGREEN);
    cprintf("Ready            ");
    UICursorDraw(gBoardIf.cursCoord, CURSOR_BLINK);
}

static void UINotifyComputerStats(const EngineStatsT *stats)
{
    gotoxy(1, 1);
    textcolor(SYSTEMCOL);
    cprintf("%d %d %d %d ",
            stats->nodes, stats->nonQNodes, stats->moveGenNodes,
            stats->hashHitGood);
}

static void UINotifyDraw(const char *reason, MoveT *move)
{
    modal("Game is drawn (%s).", reason);
}

static void UINotifyCheckmated(int turn)
{
    modal("%s is checkmated.", turn ? "Black" : "White");
}

static void UINotifyResign(int turn)
{
    modal("%s resigns.", turn ? "Black" : "White");
}

static void UIMovelistShow(const MoveList &mvlist, const Board &board)
{
    int i;
    char result[MOVE_STRING_MAX];
    MoveStyleT msUI = {mnSAN, csOO, true};

    textcolor(SYSTEMCOL);
    gotoxy(1, 1);

    for (i = 0; i < mvlist.NumMoves(); i++)
        cprintf("%s ", mvlist.Moves(i).ToString(result, &msUI, &board));
    modal("possible moves.");
}

// Do any UI-specific initialization.
static void UIInit(Game *game, Switcher *sw)
{
    setlocale(LC_CTYPE, ""); // necessary for a UTF-8 console cursor
    initconio();
    if (curs_set(0) == ERR) // set cursor invisible (ncurses).
        assert(0);
    clrscr();
    gBoardIf.col[0] = LIGHTCYAN;
    gBoardIf.col[1] = LIGHTGRAY;
    gBoardIf.flipped = 0;
    gBoardIf.cursCoord = 0;
    gBoardIf.game = game;
    gBoardIf.sw = sw;
    
    UIBoardDraw();
    UITicksDraw();
    UIOptionsDraw(game);    
    uiPrepareEngines(game);
    game->Go(); // start white's clock
}

// This function intended to get player input and adjust variables accordingly.
static void UIPlayerMove()
{
    const MoveT *foundMove;
    uint8 chr;
    MoveList movelist;
    uint8 comstr[2] = {FLAG, FLAG};
    MoveT myMove = MoveNone;
    int myLevel;
    char myStr[3];
    Game *game = gBoardIf.game; // shorthand
    const Board &board = game->Board(); // shorthand

    UIGetCommand(comstr, game);
    
    switch (comstr[0])
    {
        case 'q':    // bail
            game->Stop();
            UIExit();
            printf("bye.\n");
            exit(0);
            break;
        case 'n':    // new game
            game->NewGame();
            return;
        case 'l':    // switch computer level
            do
            {
                modalString(myStr, 3, "0123456789", "Set level to? >");
            } while (sscanf(myStr, "%d", &myLevel) < 1);

            game->EngineConfig().SetSpinClamped(Config::MaxDepthSpin, myLevel);
            UIOptionsDraw(game);
            return;
        case 'h':    // change history window
        {
            int myHiswin;
            while ((myHiswin = modal("Set to x moves (0-9)? >") - '0') < 0 ||
                   myHiswin > 9)
            {
                ; // noop
            }
            game->EngineConfig().SetSpinClamped(Config::HistoryWindowSpin,
                                                myHiswin);
            UIOptionsDraw(game);
            return;
        }
        case 'w':    // toggle computer control
        case 'b':
            game->ToggleEngineControl(comstr[0] == 'b');
            UIOptionsDraw(game);
            return;
        case 'p':    // toggle pondering.
            game->TogglePonder();
            UIOptionsDraw(game);
            return;
        case 'm':
            game->MoveNow();
            return;
        case 'c':    // change w/b colors
            UIPlayerColorChange();
            UIPositionRefresh(board.Position());
            return;
        case 'f':    // flip board.
            UIBoardFlip(board);
            return;
        case 'd':    // change debug logging level.
            UISetDebugLoggingLevel();
            return;
        case 's':
            modal(game->Save() < 0 ?
                  "Game save failed." :
                  "Game save succeeded.");
            return;
        case 'r':
            modal(game->Restore() < 0 ?
                  "Game restore failed." :
                  "Game restore succeeded.");
            return;
        case 'u':
            if (game->Rewind(1) < 0)
                modal("Start of game.");
            return;
        case 'o':
            if (game->FastForward(1) < 0)
                modal("End of redo information.");
            return;
        case 'e':
        {
            game->Stop();

            Position position = board.Position();
            std::string errString;
            Board tmpBoard = board;

            do
            {
                UIEditPosition(position);
                if (!position.IsLegal(errString))
                    reportError(false, "%s", errString.c_str());
            } while (!tmpBoard.SetPosition(position));

            UIOptionsDraw(game);
            game->NewGame(tmpBoard, false);
            game->Go();
            return;
        }
        case 'a': // toggle randomization of moves.
        {
            game->EngineConfig().ToggleCheckbox(Config::RandomMovesCheckbox);
            UIOptionsDraw(game);
            return;
        }
        case 't':
            // I'm pretty sure I want the computer to stop thinking, if I'm
            //  (maybe) swiping the time out from under it.
            game->Stop();
            UITimeMenu(game);
            UIOptionsDraw(game);
            UINotifyReady();
            game->Go();
            return;
        default:
            break;
    }

    // At this point, must be a move or request for moves, so get valid moves.
    board.GenerateLegalMoves(movelist, false);

    if (comstr[0] == 'g')       // display moves
    {
        UIMovelistShow(movelist, board);
        UIBoardDraw();
        UITicksDraw();
        UIOptionsDraw(game);
        UIPositionRefresh(board.Position());
        UIStatusDraw();
        UICursorDraw(gBoardIf.cursCoord, CURSOR_BLINK);
        return;
    }

    // Suppose we have a valid move.  Can we find it in the movelist?
    myMove.src = comstr[0];
    myMove.dst = comstr[1];
    myMove.UnmangleCastle(board);

    // Search movelist for move.
    if ((foundMove = movelist.SearchSrcDst(myMove)) == NULL)
    {
        modal("Sorry, invalid move.");
        UITicksDraw();
        return;
    }

    // At this point, we must have a valid move.
    // Do we need to promote?
    if (foundMove->IsPromote())
    {
        while ((chr = tolower(modal("Promote piece to (q, r, b, n)? >")))
               != 'q' && chr != 'r' && chr != 'b' && chr != 'n')
        {
            ; // no-op
        }
        Piece piece = asciiToNative(chr);
        myMove.promote = piece.Type();

        foundMove = movelist.SearchSrcDstPromote(myMove);
        assert(foundMove != NULL);
    }

    myMove = *foundMove;
    game->MakeMove(myMove);
}

static void UINotifyMove(MoveT move) { }

static UIFuncTableT myUIFuncTable =
{
    .init = UIInit,
    .playerMove = UIPlayerMove,
    .positionRefresh = UIPositionRefresh,
    .exit = UIExit,
    .statusDraw = UIStatusDraw,
    .notifyTick = UINotifyTick,
    .notifyMove = UINotifyMove,
    .notifyError = UINotifyError,
    .notifyPV = UINotifyPV,
    .notifyThinking = UINotifyThinking,
    .notifyPonder = UINotifyPonder,
    .notifyReady = UINotifyReady,
    .notifyComputerStats = UINotifyComputerStats,
    .notifyDraw = UINotifyDraw,
    .notifyCheckmated = UINotifyCheckmated,
    .notifyResign = UINotifyResign,
};

UIFuncTableT *uiNcursesOps()
{
    return &myUIFuncTable;
}
