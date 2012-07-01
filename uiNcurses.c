//--------------------------------------------------------------------------
//                    uiNcurses.c - ncurses UI for Arctic
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
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

#include <stdio.h>
#include <ctype.h>  // isupper()
#include <string.h> // strlen()
#include <stdlib.h> // exit()
#include <assert.h>
#include <curses.h>
#include <stdarg.h>

#include "clock.h"
#include "clockUtil.h"
#include "comp.h" // CompCurrentLevel()
#include "conio.h"
#include "gDynamic.h"
#include "log.h"
#include "saveGame.h"
#include "transTable.h"
#include "ui.h"
#include "uiUtil.h"


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
} gBoardIf;


static void UIPrintBoardStatus(BoardT *board)
{
    // all shorthand.
    int ncheck = board->ncheck[board->turn];
    int cbyte = board->cbyte;
    int ebyte = board->ebyte;

    textcolor(LIGHTGRAY);

    // print castle status.
    gotoxy(OPTIONS_X, 14);
    cprintf("castle QKqk: %c%c%c%c",
	    cbyte & WHITEQCASTLE ? 'y' : 'n',
	    cbyte & WHITEKCASTLE ? 'y' : 'n',
	    cbyte & BLACKQCASTLE ? 'y' : 'n',
	    cbyte & BLACKKCASTLE ? 'y' : 'n');

    // print en passant status.
    gotoxy(OPTIONS_X, 15);
    cprintf("enpass: ");
    if (ebyte == FLAG)
    {
	cprintf("  ");
    }
    else
    {
	cprintf("%c%c", AsciiFile(ebyte), AsciiRank(ebyte));
    }

    // print check status.
    gotoxy(OPTIONS_X, 16);
    cprintf("chk: ");
    if (ncheck == FLAG)
	cprintf("   ");
    else if (ncheck == DOUBLE_CHECK)
    {
	cprintf("dis");
    }
    else // normal check
    {
	cprintf("%c%c ", AsciiFile(ncheck), AsciiRank(ncheck));
    }
}


static void UINotifyTick(GameT *game)
{
    ClockT *myClock;
    char timeStr[CLOCK_TIME_STR_LEN];
    int i;
    bigtime_t myTime, perMoveTime;
    int bytesWritten = 0;
    char spaces[] = "                               ";

    // Display clocks.
    gotoxy(OPTIONS_X, 18);
    for (i = 0; i < NUM_PLAYERS; i++)
    {
	myClock = game->clocks[i];
	myTime = ClockGetTime(myClock);
	perMoveTime = ClockGetPerMoveTime(myClock);

	// The clock goes red even when the time supposedly reaches 0, probably
	// because TimeStringFromBigTime() is rounding up.  FIXME: need to
	// rethink that.
	textcolor(myTime >= 0 ? LIGHTGRAY : RED);
	bytesWritten += cprintf("%s", TimeStringFromBigTime(timeStr, myTime));
	if (perMoveTime < CLOCK_TIME_INFINITE)
	{
	    textcolor(perMoveTime >= 0 ? LIGHTGRAY : RED);
	    bytesWritten += cprintf("(%s)", TimeStringFromBigTime(timeStr,
								  perMoveTime));
	}
	textcolor(LIGHTGRAY);
	bytesWritten += cprintf("%s ", ClockIsRunning(myClock) ? "r" : "s");
    }

    // Prevent old longer clock-line strings from sticking around.
    if (bytesWritten < sizeof(spaces))
    {
	cprintf("%s", &spaces[bytesWritten]);
    }
}


static void UIStatusDraw(GameT *game)
{
    bigtime_t timeTaken;
    BoardT *board = &game->savedBoard; // shorthand.
    int turn = board->turn;

    UIPrintBoardStatus(board);
    UINotifyTick(game);

    gotoxy(OPTIONS_X, 20);
    timeTaken = ClockTimeTaken(game->clocks[turn ^ 1]);
    cprintf("move: %d (%.2f sec)     ",
	    (board->ply >> 1) + 1,
	    ((double) timeTaken) / 1000000);
    gotoxy(OPTIONS_X, 21);
    textcolor(SYSTEMCOL);
    cprintf("%s\'s turn", turn ? "black" : "white");
    gotoxy(OPTIONS_X, 22);
    cprintf(board->ncheck[turn] == FLAG ? "       " : "<check>");
}


// prints out expected move sequence at the bottom of the screen.
static void UINotifyPV(GameT *game, PvRspArgsT *pvArgs)
{
    char spaces[80];
    char mySanString[79 - 18];
    char evalString[20];
    int len;
    PvT *pv = &pvArgs->pv; // shorthand.

    // Get a suitable string of moves to print.
    if (buildMoveString(mySanString, sizeof(mySanString), pv, &game->savedBoard,
			true, false) < 1)
    {
	return;
    }

    // blank out the last pv.
    memset(spaces, ' ', 79);
    spaces[79] = '\0';
    gotoxy(1, 25);
    textcolor(SYSTEMCOL);
    cprintf("%s", spaces);

    if (abs(pv->eval) >= EVAL_WIN_THRESHOLD)
    {
	len = snprintf(evalString, sizeof(evalString), "%smate",
		       pv->eval < 0 ? "-" : "");
	if (abs(pv->eval) < EVAL_WIN)
	{
	    snprintf(&evalString[len], sizeof(evalString) - len, "%d",
		     (EVAL_WIN - abs(pv->eval) + 1) / 2);
	}
    }
    else
    {
	snprintf(evalString, sizeof(evalString), "%+.2f",
		 ((double) pv->eval) / EVAL_PAWN);
    }

    // print the new pv.
    gotoxy(1, 25);
    cprintf("pv: d%d %s %s.",
	    pv->level, evalString, mySanString);
}


#define CURSOR_NOBLINK 0
#define CURSOR_BLINK 1
#define CURSOR_HIDE 2
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
    cprintf("%s %s",
	    mode == CURSOR_HIDE ? "  " : "\\ ",
	    mode == CURSOR_HIDE ? "  " : " /");
    gotoxy(x, y + 2);
    cprintf("%s %s",
	    mode == CURSOR_HIDE ? "  " : "/ ",
	    mode == CURSOR_HIDE ? "  " : " \\");

    textbackground(BLACK);	/* get rid of that annoying blink */
    textcolor(BLACK);
    gotoxy(SQUARE_WIDTH * 8 + 7, 24);
}


static void prettyprint(int y, char *option, char *option2, ...)
/* spews user option to screen, highlighting the first char. */
{
    int i, j, didHighlight = 0;
    char *myopt;

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
	     j < strlen(myBuf);
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


static void UIOptionsDraw(GameT *game)
{
    UIWindowClear(OPTIONS_X, 1, SCREEN_WIDTH - OPTIONS_X, 12);
    gotoxy(OPTIONS_X, 1);
    textcolor(SYSTEMCOL);
    cprintf("Options:");
    prettyprint(2,  "New game",       "Level (%d)", gVars.maxLevel);
    prettyprint(3,  "Save game",      "White control (%s)",
		game->control[0] ? "C" : "P");
    prettyprint(4,  "Restore game",   "Black control (%s)",
		game->control[1] ? "C" : "P");
    prettyprint(5,  "Edit position",  "rAndom moves (%s)",
		gVars.randomMoves ? "On" : "Off");
    prettyprint(6,  "Quit",           "Ponder (%s)",
		gVars.ponder ? "On" : "Off");

    prettyprint(8,  "Generate moves", "History window (%d)",
		gVars.hiswin >> 1);
    prettyprint(9,  "Move now",       "Time control");
    prettyprint(10, "Flip board",     "Undo");
    prettyprint(11, "Color",          "redO");
#ifdef ENABLE_DEBUG_LOGGING
    prettyprint(12, "Debug logging",  NULL);
#endif
}


static void UIEditOptionsDraw(void)
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


static void UITimeOptionsDraw(GameT *game, int applyToggle)
{
    char t1[CLOCK_TIME_STR_LEN], t2[CLOCK_TIME_STR_LEN];

    UIWindowClear(OPTIONS_X, 1, SCREEN_WIDTH - OPTIONS_X, 12);
    gotoxy(OPTIONS_X, 1);
    textcolor(SYSTEMCOL);
    cprintf("Options:");
    prettyprint(2, "Start time(s) (%s %s)", NULL,
		TimeStringFromBigTime(t1, ClockGetTime(&game->origClocks[0])),
		TimeStringFromBigTime(t2, ClockGetTime(&game->origClocks[1])));
    prettyprint(3, "Increment(s) (%s %s)",  NULL,
		TimeStringFromBigTime(t1, ClockGetInc(&game->origClocks[0])),
		TimeStringFromBigTime(t2, ClockGetInc(&game->origClocks[1])));
    prettyprint(4, "Time control period(s) (%d %d)", NULL,
		ClockGetTimeControlPeriod(&game->origClocks[0]),
		ClockGetTimeControlPeriod(&game->origClocks[1]));
    prettyprint(5, "Per-move limit (%s %s)", NULL,
		TimeStringFromBigTime
		(t1, ClockGetPerMoveLimit(&game->origClocks[0])),
		TimeStringFromBigTime
		(t2, ClockGetPerMoveLimit(&game->origClocks[1])));
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


static void UIBoardRefresh(const BoardT *board)
{
    int x, y;
    int i = 0;
    const uint8 *bcoord = board->coord;

    for (y = 0; y < 8; y++)
	for (x = 0; x < 8; x++, i++)
	{
	    if ((x + y) % 2)
		textbackground(BOARDCOL);
	    else
		textbackground(BLACK);
	    gotoxy((gBoardIf.flipped ? 7 - x : x) * SQUARE_WIDTH +
		   SQUARE_WIDTH / 2 + 1,
		   2 + (gBoardIf.flipped ? y : 7 - y) * 3);
	    /* note if we flip board, we switch the '7-' stuff... */

	    if (bcoord[i])
	    {
		/* Use the appropriate color to draw white/black pieces. */
		textcolor(gBoardIf.col[bcoord[i] & 1]);
	    }

	    /* Draw a piece (or lack thereof). */
	    putch(nativeToBoardAscii(bcoord[i]));
	}
    textbackground(BLACK);
}


static void UITicksDraw(void)
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


static void UIExit(void)
{
    doneconio();
}


static int UIBarf(char *format, ...)
{
    int chr, i;
    int len;
    char message[80];
    va_list ap;

    va_start(ap, format);
    len = vsnprintf(message, 80, format, ap);
    va_end(ap);
    assert(len < 80);

    /* Display the message. */
    gotoxy((SCREEN_WIDTH / 2) - len / 2, 25);
    textcolor(MAGENTA);
    cprintf("%s", message);

    /* Wait for input. */
    chr = getch();
    if (chr == ESC)	/* bail on ESC */
    {
	UIExit();
	exit(0);
    }

    /* Now, blank the entire message. */
    gotoxy((SCREEN_WIDTH / 2) - len / 2, 25);
    for (i = 0; i < len; i++)
    {
	cprintf(" ");
    }

    UITicksDraw();
    gotoxy(1, 1); /* justncase */
    return chr;
}


// 'myLen' == sizeof(myStr) (including terminator) and must be at least 2 bytes
// long.
// Returns 'myStr'.
static char *UIBarfString(char *myStr, int myStrLen,
			  char *validChars, char *format, ...)
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
	if (chr == ESC)	// bail on ESC
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
	    myStr[curStrLen--] = '\0';
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
    UIBarf(reason);
}


/* Edits a board. */
static void UIEditPosition(BoardT *board)
{   
    int c, chr, i;
    int *coord = &gBoardIf.cursCoord; // shorthand.
    char validChars[] = "WwEeCcDdSs PpRrNnBbQqKk";
    int cbyte; // tmpvars

    UIEditOptionsDraw();
    UICursorDraw(*coord, CURSOR_BLINK);

    board->ply = 0;
    board->ncpPlies = 0;

    while (1)
    {
	UIPrintBoardStatus(board);
	gotoxy(OPTIONS_X, 21);
	textcolor(SYSTEMCOL);
	cprintf("%s\'s turn", board->turn ? "black" : "white");
	/* I do this here just so the cursor ends up in an aesthetically
	   pleasing spot. */
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
		// Wipe board.
		for (i = 0; i < NUM_SQUARES; i++)
		{
		    BoardPieceSet(board, i, 0);
		}
		UIBoardRefresh(board);
		break;

	    case 'E':
	    case 'e':
		/* (possibly) set an enpassant square. */
		BoardEbyteSet(board, *coord);
		break;

	    case 'C':
	    case 'c':
		cbyte = board->cbyte;
		/* (possibly) set cbyte. */
		switch(*coord)
		{
		case 0:
		    cbyte |= WHITEQCASTLE; break;
		case 4:
		    cbyte |= (WHITEQCASTLE | WHITEKCASTLE); break;
		case 7:
		    cbyte |= WHITEKCASTLE; break;
		case 56:
		    cbyte |= BLACKQCASTLE; break;
		case 60:
		    cbyte |= (BLACKQCASTLE | BLACKKCASTLE); break;
		case 63:
		    cbyte |= BLACKKCASTLE; break;
		default:
		    cbyte = 0; break;
		}
		BoardCbyteSet(board, cbyte);
		break;

	    case 'S':
	    case 's':
		/* Switch turn. */
		BoardTurnSet(board, board->turn ^ 1);
		break;

	    case 'D':
	    case 'd':
		/* bail from editing mode. */
		return;

	    default:
		/* At this point, it must be a piece, or nothing. */
		chr = asciiToNative(c);
		/* disallow pawns on first or eighth ranks. */
		if (ISPAWN(chr) && (*coord < 8 || *coord >= 56))
		{
		    break;
		}

		BoardPieceSet(board, *coord, chr);
		UIBoardRefresh(board);
		break;
	    }
	}
	if (c != KEY_UP && c != KEY_DOWN && c != KEY_LEFT && c != KEY_RIGHT)
	    continue;

	/* At this point we have a valid direction. */
	UICursorDraw(*coord, CURSOR_HIDE);  /* Unmark current loc */
	UICursorMove(c, coord);
	UICursorDraw(*coord, CURSOR_BLINK); /* Blink new loc */
    }	/* end while */
}

#define APPLY_BOTH (NUM_PLAYERS)

/* Adjusts time. */
static void UITimeMenu(GameT *game)
{   
    int c, i;
    int *coord = &gBoardIf.cursCoord; // shorthand.
    char validChars[] = "SsIiTtPpAaCcDd";
    char timeStr[CLOCK_TIME_STR_LEN];
    int timeControlPeriod;

    // 0 -> white
    // 1 -> black
    // 2 -> both
    static int applyToggle = APPLY_BOTH;

    UICursorDraw(*coord, CURSOR_BLINK);

    while (1)
    {
	UITimeOptionsDraw(game, applyToggle);
	/* I do this here just so the cursor ends up in an aesthetically
	   pleasing spot. */
	gotoxy(OPTIONS_X, 24);
	textcolor(LIGHTCYAN);
	cprintf("Time             ");

	c = getch();
	if (strchr(validChars, c) != NULL)
	{
	    switch(c)
	    {
	    case 'S':
	    case 's':
		do
		{
		    UIBarfString(timeStr, 9, /* xx:yy:zz\0 */
				 "0123456789:inf", "Set start time to? >");
		} while (!TimeStringIsValid(timeStr));

		for (i = 0; i < NUM_PLAYERS; i++)
		{
		    if (applyToggle == i || applyToggle == APPLY_BOTH)
		    {
			ClockSetStartTime(&game->origClocks[i],
					  TimeStringToBigTime(timeStr));
			ClockReset(&game->origClocks[i]);
		    }
		}
		break;
	    case 'I':
	    case 'i':
		do
		{
		    UIBarfString(timeStr, 9, /* xx:yy:zz\0 */
				 "0123456789:", "Set increment to? >");
		} while (!TimeStringIsValid(timeStr));

		for (i = 0; i < NUM_PLAYERS; i++)
		{
		    if (applyToggle == i || applyToggle == APPLY_BOTH)
		    {
			ClockSetInc(&game->origClocks[i],
				    TimeStringToBigTime(timeStr));
		    }
		}
		break;
	    case 'T':
	    case 't':
		do
		{
		    UIBarfString(timeStr, 9, /* xx:yy:zz\0 */
				 "0123456789", "Set time control period to? >");
		} while (sscanf(timeStr, "%d", &timeControlPeriod) < 1);

		for (i = 0; i < NUM_PLAYERS; i++)
		{
		    if (applyToggle == i || applyToggle == APPLY_BOTH)
		    {
			ClockSetTimeControlPeriod(&game->origClocks[i],
						  timeControlPeriod);
		    }
		}
		break;
	    case 'P':
	    case 'p':
		do
		{
		    UIBarfString(timeStr, 9, /* xx:yy:zz\0 */
				 "0123456789:inf", "Set per-move limit to? >");
		} while (!TimeStringIsValid(timeStr));

		for (i = 0; i < NUM_PLAYERS; i++)
		{
		    if (applyToggle == i || applyToggle == APPLY_BOTH)
		    {
			ClockSetPerMoveLimit(&game->origClocks[i],
					     TimeStringToBigTime(timeStr));
		    }
		}
		break;
	    case 'A':
	    case 'a':
		ClocksReset(game);
		UIStatusDraw(game);
		break;
	    case 'C':
	    case 'c':
		if (++applyToggle > APPLY_BOTH)
		{
		    applyToggle = 0;
		}
		break;
	    case 'D':
	    case 'd':
		/* bail from time menu. */
		return;
	    default:
		break;
	    }
	}
    }	/* end while */
}


/* Gets user input and translates it to valid command.  Returns: command, or
   two numbers signaling source and destination.  */
static void UIGetCommand(uint8 command[], GameT *game)
{   
    int c;
    int gettingsrc = 1;
    int *coord = &gBoardIf.cursCoord; // shorthand.
    BoardT *board = &game->savedBoard; // shorthand
    char validChars[] = "NSRLWBFQHCMEGATUOP"
#ifdef ENABLE_DEBUG_LOGGING
	"D"
#endif
	;

    while (1)
    {
	// Wait for actual input.
	while(!kbhit())
	{
	    SwitcherSwitch(&game->sw);
	}
	c = getch();
	if (isalpha(c))
	    c = toupper(c); // accept lower-case commands, too.
	if (strchr(validChars, c) != NULL)
	{
	    if (!gettingsrc)
	    {
		UICursorDraw(command[0], CURSOR_HIDE);
	    }
	    command[0] = c; /* valid one-char command */
	    return;
	}
	if (c == ENTER && gettingsrc)
	{
            // ignore attempts to set a blank src
	    if (board->coord[*coord])
	    {
		command[0] = *coord;
		UICursorDraw(*coord, CURSOR_NOBLINK);
		gettingsrc = 0;
	    }
	    continue;
	}
	else if (c == ENTER)
	{
	    if (*coord == command[0]) /* we want to unselect src spot */
	    {
		gettingsrc = 1;
		UICursorDraw(*coord, CURSOR_BLINK);
		continue;
	    }
	    UICursorDraw(command[0], CURSOR_HIDE);
	    command[1] = *coord;	/* enter destination */
	    return;
	}
	if (c != KEY_UP && c != KEY_DOWN && c != KEY_LEFT &&
	    c != KEY_RIGHT)
	    continue;

	/* At this point we have a valid direction. */
	if (gettingsrc || command[0] != *coord)
	    UICursorDraw(*coord, CURSOR_HIDE); /* need to unmark current loc */
	UICursorMove(c, coord);
	if (gettingsrc || command[0] != *coord)
	    UICursorDraw(*coord, CURSOR_BLINK); /* need to blink current loc */
    }	/* end while */
}


static void UIBoardDraw(void)
{
    int i, j;

    /* note: a carriage return after drawing the board could clobber the
       ticks. */
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


static void UIBoardFlip(BoardT *board)
{
    UICursorDraw(gBoardIf.cursCoord, CURSOR_HIDE); // hide old cursor
    gBoardIf.flipped ^= 1;
    UITicksDraw();   // update ticks
    UIBoardRefresh(board); // update player positions
    UICursorDraw(gBoardIf.cursCoord, CURSOR_BLINK);
}


static void UIPlayerColorChange(void)
{
    char *colors[NUM_PLAYERS] = {"White", "Black"};
    int i;

    char myStr[3];
    int myColor;

    for (i = 0; i < NUM_PLAYERS; i++)
    {
	do
	{
	    UIBarfString(myStr, 3, "0123456789", "%s color? >", colors[i]);
	    
	} while (sscanf(myStr, "%d", &myColor) < 1 ||
		 myColor < 1 || myColor > 15 ||
		 (i == 1 && myColor == gBoardIf.col[0]));
	gBoardIf.col[i] = myColor;
    }
}


static void UISetDebugLoggingLevel(void)
{
    int i;
    while ((i = UIBarf("Set debug level to (0-2) (higher -> more verbose)? >") - '0') < 0 ||
	   i > 2)
	;
    LogSetLevel(i);
}


static void UINotifyThinking(void)
{
    gotoxy(OPTIONS_X, 24);
    textcolor(RED);
    cprintf("Thinking         ");
}


static void UINotifyPonder(void)
{
    gotoxy(OPTIONS_X, 24);
    textcolor(LIGHTGREEN);
    cprintf("Ready (pondering)");
    UICursorDraw(gBoardIf.cursCoord, CURSOR_BLINK);
}


static void UINotifyReady(void)
{
    gotoxy(OPTIONS_X, 24);
    textcolor(LIGHTGREEN);
    cprintf("Ready            ");
    UICursorDraw(gBoardIf.cursCoord, CURSOR_BLINK);
}


static void UINotifyComputerStats(GameT *game, CompStatsT *stats)
{
    gotoxy(1, 1);
    textcolor(SYSTEMCOL);
    cprintf("%d %d %d %d ",
	    stats->nodes, stats->nonQNodes, stats->moveGenNodes,
	    stats->hashHitGood);
}


static void UINotifyDraw(char *reason, MoveT *move)
{
    UIBarf("Game is drawn (%s).", reason);
}


static void UINotifyCheckmated(int turn)
{
    UIBarf("%s is checkmated.", turn ? "Black" : "White");
}


static void UINotifyResign(int turn)
{
    UIBarf("%s resigns.", turn ? "Black" : "White");
}


static void UIMovelistShow(MoveListT *mvlist)
{
    int i;
    char result[15];

    textcolor(SYSTEMCOL);
    gotoxy(1, 1);

    for (i = 0; i < mvlist->lgh; i++)
    {
	cprintf("%s ", moveToFullStr(result, &mvlist->moves[i]));
    }
    UIBarf("possible moves.");
}


/* this function intended to get player input and adjust variables
   accordingly */
static void UIPlayerMove(ThinkContextT *th, GameT *game)
{
    MoveT *foundMove;
    uint8 chr;
    MoveListT movelist;
    uint8 comstr[2] = {FLAG, FLAG};
    MoveT myMove;
    int myLevel;
    int myHiswin;
    int player, turn;
    char myStr[3];

    BoardT *board = &game->savedBoard; // shorthand
    turn = game->savedBoard.turn; // shorthand

    UIGetCommand(comstr, game);
    
    switch(comstr[0])
    {
    case 'Q':    /* bail */
	ThinkerCmdBail(th);
	UIExit();
	printf("bye.\n");
	exit(0);
	break;
    case 'N':     /* new game */
	gVars.gameCount++;
	ThinkerCmdBail(th);
	GameNew(game, th);
	return;
    case 'L':     /* switch computer level */
	do
	{
	    UIBarfString(myStr, 3, "0123456789", "Set level to? >");
	} while (sscanf(myStr, "%d", &myLevel) < 1);

	if (CompCurrentLevel() > (gVars.maxLevel = myLevel))
	{
	    ThinkerCmdMoveNow(th);
	}
	UIOptionsDraw(game);
	return;
    case 'H':     /* change history window */
	while ((myHiswin = UIBarf("Set to x moves (0-9)? >") - '0') < 0 ||
	       myHiswin > 9)
	    ;	/* do nothing */
	gVars.hiswin = myHiswin << 1;	/* convert moves to plies. */
	UIOptionsDraw(game);
	return;
    case 'W':     /* toggle computer control */
    case 'B':
	player = (comstr[0] == 'B');
	game->control[player] ^= 1;

	GameCompRefresh(game, th);
	UIOptionsDraw(game);
	return;
    case 'P': // toggle pondering.
	gVars.ponder ^= 1;

	GameCompRefresh(game, th);
	UIOptionsDraw(game);
	return;
    case 'M':
	ThinkerCmdMoveNow(th);
	return;
    case 'C':     /* change w/b colors */
	UIPlayerColorChange();
	UIBoardRefresh(board);
	return;
    case 'F':     /* flip board. */
	UIBoardFlip(board);
	return;
    case 'D':     /* change debug logging level. */
	UISetDebugLoggingLevel();
	return;
    case 'S':
	UIBarf(SaveGameSave(&game->sgame) < 0 ?
	       "Game save failed." :
	       "Game save succeeded.");
	return;
    case 'R':
	if (SaveGameRestore(&game->sgame) < 0)
	{
	    UIBarf("Game restore failed.");
	}
	else
	{
	    ThinkerCmdBail(th);
	    UIBarf("Game restore succeeded.");
	    TransTableReset();
	    gHistInit();
	    // Could goto current ply instead of numPlies.  I'm assuming
	    // here the user is absent-minded and might forget (or might not
	    // know) the current ply is persistent.
	    GameGotoPly(game, GameLastPly(game), th);
	}
	return;
    case 'U':
	if (GameRewind(game, 1, th) < 0)
	{
	    UIBarf("Start of game.");
	}
	return;
    case 'O':
	if (GameFastForward(game, 1, th) < 0)
	{
	    UIBarf("End of redo information.");
	}
	return;
    case 'E':
	ThinkerCmdBail(th);
	ClocksStop(game);
	do {
	    UIEditPosition(board);
	} while (BoardSanityCheck(board, 0));
	UIOptionsDraw(game);

	GameNewEx(game, th, board, 0, 1);
	return;
    case 'A': /* toggle randomize moves. */
	gVars.randomMoves ^= 1;
	UIOptionsDraw(game);
	return;
    case 'T':
        // I'm pretty sure I want the computer to stop thinking, if I'm swiping
	// the time out from under it.
	ThinkerCmdBail(th);
	ClocksStop(game);
	UITimeMenu(game);
	UIOptionsDraw(game);
	UINotifyReady();
	GameMoveCommit(game, NULL, th, 0);
	return;
    default:
	break;
    }

    /* at this point must be a move or request for moves. */
    /* get valid moves. */
    mlistGenerate(&movelist, board, 0);
    if (comstr[0] == 'G')	/* display moves */
    {
	UIMovelistShow(&movelist);
	UIBoardDraw();
	UITicksDraw();
	UIOptionsDraw(game);
	UIBoardRefresh(board);
	UIStatusDraw(game);
	UICursorDraw(gBoardIf.cursCoord, CURSOR_BLINK);
     	return;
    }

    /* Suppose we have a valid move.  Can we find it in the movelist? */
    myMove.src = comstr[0];
    myMove.dst = comstr[1];

    /* search movelist for comstr */
    if ((foundMove = mlistSearch(&movelist, &myMove, 2)) == NULL)
    {
	UIBarf("Sorry, invalid move.");
	UITicksDraw();
	return;
    }

    /* At this point, we must have a valid move. */
    ThinkerCmdBail(th);

    /* Do we need to promote? */
    if (ISPAWN(board->coord[myMove.src]) &&
	(myMove.dst > 55 || myMove.dst < 8))
    {
	while ((chr = UIBarf("Promote piece to (q, r, b, n)? >")) != 'q' &&
	       chr != 'r' && chr != 'b' && chr != 'n')
	    ; /* do nothing */
	chr = asciiToNative(chr);
	myMove.promote = (chr & ~1) | (board->turn);

	foundMove = mlistSearch(&movelist, &myMove, 3);
	assert(foundMove != NULL);
    }
    else
    {
	myMove.promote = foundMove->promote;
    }
    myMove.chk = foundMove->chk;
    GameMoveCommit(game, &myMove, th, 0);
}

static int UIShouldCommitMoves(void)
{
    return 1;
}

static void UINotifyMove(MoveT *move) { }


static UIFuncTableT myUIFuncTable =
{
    .playerMove = UIPlayerMove,
    .boardRefresh = UIBoardRefresh,
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
    .shouldCommitMoves = UIShouldCommitMoves
};


/* Do any UI-specific initialization. */
UIFuncTableT *uiNcursesInit(GameT *game)
{
    initconio();
    /* set cursor invisible (ncurses).  Hacky, but geez. */
    if (curs_set(0) == ERR)
    {
	assert(0);
    }
    clrscr();
    gBoardIf.col[0] = LIGHTCYAN;
    gBoardIf.col[1] = LIGHTGRAY;
    gBoardIf.flipped = 0;
    gBoardIf.cursCoord = 0;

    UIBoardDraw();
    UITicksDraw();
    UIOptionsDraw(game);    

    return &myUIFuncTable;
}
