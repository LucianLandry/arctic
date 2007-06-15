/***************************************************************************
                        ui.c - ncurses UI for Arctic
                             -------------------
    copyright            : (C) 2007 by Lucian Landry
    email                : lucian_b_landry@yahoo.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 ***************************************************************************/


#include <stdio.h>
#include <ctype.h>	/* islower() */
#include <string.h>     /* strlen() */
#include <stdlib.h>     /* exit() */
#include <assert.h>
#include <curses.h>
#include <stdarg.h>
#include "conio.h"
#include "ref.h"


#define SYSTEMCOL       GREEN
#define TICKCOL         BLUE
#define BOARDCOL        BLUE

#define ENTER 13
#define ESC   27
#define BACKSPACE 263

#define SQUARE_WIDTH 5   /* width of chess square, in characters */
#define OPTIONS_X (SQUARE_WIDTH * 8 + 2) /* 1 since one-based, 1 for ticks) */
#define OPTIONS_X2 (OPTIONS_X + 15)
#define SCREEN_WIDTH 80

static struct {
    int col[2];    // player colors.
    int flipped;   // bool, is the board inverted (black on the bottom)
    int cursCoord; // coordinate cursor is at.
} gBoardIf;


static void UIPrintBoardStatus(BoardT *board)
{
    /* all shorthand. */
    int ncheck = board->ncheck[board->ply & 1];
    int cbyte = board->cbyte;
    int ebyte = board->ebyte;

    textcolor(LIGHTGRAY);

    /* print castle status. */
    gotoxy(OPTIONS_X, 14);
    cprintf("castle QKqk: %c%c%c%c",
	    cbyte & WHITEQCASTLE ? 'y' : 'n',
	    cbyte & WHITEKCASTLE ? 'y' : 'n',
	    cbyte & BLACKQCASTLE ? 'y' : 'n',
	    cbyte & BLACKKCASTLE ? 'y' : 'n');

    /* print en passant status. */
    gotoxy(OPTIONS_X, 15);
    cprintf("enpass: ");
    if (ebyte == FLAG)
    {
	cprintf("  ");
    }
    else
    {
	cprintf("%c%c", File(ebyte) + 'a', Rank(ebyte) + '1');
    }

    /* print check status. */
    gotoxy(OPTIONS_X, 16);
    cprintf("chk: ");
    if (ncheck == FLAG)
	cprintf("   ");
    else if (ncheck == DOUBLE_CHECK)
    {
	cprintf("dis");
    }
    else /* normal check */
    {
	cprintf("%c%c ", File(ncheck) + 'a', Rank(ncheck) + '1');
    }
}


void UINotifyTick(GameStateT *gameState)
{
    ClockT *myClock;
    char timeStr[TIME_STR_LEN];
    int i;

    // Display clocks.
    textcolor(LIGHTGRAY);
    gotoxy(OPTIONS_X, 18);
    for (i = 0; i < 2; i++)
    {
	myClock = gameState->clocks[i];

	cprintf("%s%s ",
		TimeStringFromBigtime(timeStr, ClockCurrentTime(myClock)),
		ClockIsRunning(myClock) ? "r" : "s");
    }
    cprintf("        ");
}


void UIStatusDraw(BoardT *board, GameStateT *gameState)
{
    int turn = board->ply & 1;
    bigtime_t timeTaken;

    UIPrintBoardStatus(board);
    UINotifyTick(gameState);

    gotoxy(OPTIONS_X, 20);
    timeTaken = ClockTimeTaken(gameState->clocks[turn ^ 1]);
    cprintf("move: %d (%.2f sec)     ",
	    (board->ply >> 1) + 1,
	    ((double) timeTaken) / 1000000);
    gotoxy(OPTIONS_X, 21);
    textcolor(SYSTEMCOL);
    cprintf("%s\'s turn", turn ? "black" : "white");
    gotoxy(OPTIONS_X, 22);
    cprintf(board->ncheck[turn] == FLAG ? "       " : "<check>");
}


void UINotifyPV(BoardT *board, PvT *pv)
/* prints out expected move sequence at the bottom of the screen. */
{
    char spaces[80];
    char mySanString[79 - 14];

    // Get a suitable string of moves to print.
    buildSanString(board, mySanString, sizeof(mySanString), pv);

    // blank out the last pv.
    memset(spaces, ' ', 79);
    spaces[79] = '\0';
    gotoxy(1, 25);
    textcolor(SYSTEMCOL);
    cprintf(spaces);

    // print the new pv.
    gotoxy(1, 25);
    cprintf("pv: d%d %+d%s.", pv->level, pv->eval / 100, mySanString);
}


static void UICursorDraw(int coord, int blink, int undo)
/* draws cursor at appropriate coordinate.  Blinks if spot hasn't been
   'selected', otherwise noblink.  'undo' 'undraws' the cursor at a spot. */
{
    int x, y;
    /* translate coord to xy coords of upper left part of cursor. */
    x = SQUARE_WIDTH * (gBoardIf.flipped ? 7 - File(coord): File(coord)) + 1;
    y = 3 * (gBoardIf.flipped ? Rank(coord) : 7 - Rank(coord)) + 1;

    if ((Rank(coord) + File(coord)) & 1) /* we on a board-colored spot */
	textbackground(BOARDCOL);
    textcolor(/* BROWN */ YELLOW + (blink ? BLINK : 0));
    gotoxy(x, y);
    cprintf("%s %s", undo ? "  " : "\\ ", undo ? "  " : " /");
    gotoxy(x, y + 2);
    cprintf("%s %s", undo ? "  " : "/ ", undo ? "  " : " \\");

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
	cprintf(spaces);
    }
}


static void UIOptionsDraw(GameStateT *gameState)
{
    UIWindowClear(OPTIONS_X, 1, SCREEN_WIDTH - OPTIONS_X, 12);
    gotoxy(OPTIONS_X, 1);
    textcolor(SYSTEMCOL);
    cprintf("Options:");
    prettyprint(2,  "New game",       "Level (%d)", gVars.maxLevel);
    prettyprint(3,  "Save game",      "White control (%s)",
		gameState->control[0] ? "C" : "P");
    prettyprint(4,  "Restore game",   "Black control (%s)",
		gameState->control[1] ? "C" : "P");
    prettyprint(5,  "Edit position",  "Move now");
    prettyprint(6,  "Quit",           "rAndom moves (%s)",
		gVars.randomMoves ? "On" : "Off");

    prettyprint(8,  "Flip board",     "Time control");
    prettyprint(9,  "Color",          NULL);
    prettyprint(10, "Debug logging",  NULL);
    prettyprint(11, "History window (%d)", NULL, gVars.hiswin >> 1);
    prettyprint(12, "Generate Moves", NULL);
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


static void UITimeOptionsDraw(GameStateT *gameState, int applyToggle)
{
    char t1[TIME_STR_LEN], t2[TIME_STR_LEN];

    UIWindowClear(OPTIONS_X, 1, SCREEN_WIDTH - OPTIONS_X, 12);
    gotoxy(OPTIONS_X, 1);
    textcolor(SYSTEMCOL);
    cprintf("Options:");
    prettyprint(2, "Start time(s) (%s %s)", NULL,
		TimeStringFromBigtime(t1, ClockCurrentTime
				      (&gameState->origClocks[0])),
		TimeStringFromBigtime(t2, ClockCurrentTime
				      (&gameState->origClocks[1])));
    prettyprint(3, "Increment(s) (%s %s)",  NULL,
		TimeStringFromBigtime(t1, ClockGetInc
				      (&gameState->origClocks[0])),
		TimeStringFromBigtime(t2, ClockGetInc
				      (&gameState->origClocks[1])));
    prettyprint(4, "inc Period(s) (%d %d)", NULL,
		ClockGetIncPeriod(&gameState->origClocks[0]),
		ClockGetIncPeriod(&gameState->origClocks[1]));
    prettyprint(6, "Apply to current",      NULL);
    prettyprint(7, "Changes: (%s)", NULL,
		applyToggle == 0 ? "white" :
		applyToggle == 1 ? "black" :
		"both");
    prettyprint(9, "Done",           NULL);
}


static void UICursorMove(int key, int *coord)
{
    if ((key == KEY_UP && !gBoardIf.flipped) ||
	(key == KEY_DOWN && gBoardIf.flipped))
    {
	if ((*coord += 8) > 63)
	    *coord -= 64;
    }
    else if ((key == KEY_DOWN && !gBoardIf.flipped) ||
	     (key == KEY_UP && gBoardIf.flipped))
    {
	if ((*coord -= 8) < 0)
	    *coord += 64;
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


static void UIBoardRefresh(BoardT *board)
{
    int x, y;
    int i = 0;
    uint8 *bcoord = board->coord;

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
    cprintf(message);

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
    x += cprintf(message);

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


/* Edits a board.  ('ply' is set to 0, or 1, at the end of the turn.) */
static void UIEditPosition(BoardT *board)
{   
    int c, chr, i;
    int *coord = &gBoardIf.cursCoord; // shorthand.
    char validChars[] = "WwEeCcDdSs PpRrNnBbQqKk";

    UIEditOptionsDraw();
    UICursorDraw(*coord, 1, 0);
    board->ply &= 1; /* erase move history. */

    while (1)
    {
	UIPrintBoardStatus(board);
	gotoxy(OPTIONS_X, 21);
	textcolor(SYSTEMCOL);
	cprintf("%s\'s turn", board->ply ? "black" : "white");
	/* I do this here just so the cursor ends up in an aesthetically
	   pleasing spot. */
	gotoxy(OPTIONS_X, 24);
	textcolor(LIGHTCYAN);
	cprintf("Edit    ");

	c = getch();
	if (strchr(validChars, c) != NULL)
	{
	    switch(c)
	    {
	    case 'W':
	    case 'w':
		/* wipe board. */
		for (i = 0; i < 64; i++)
		{
		    if (board->coord[i])
		    {
			delpieceSmart(board, board->coord[i], i);
			board->coord[i] = 0;
		    }
		}
		board->ebyte = FLAG;
		newcbyte(board);
		UIBoardRefresh(board);
		break;

	    case 'E':
	    case 'e':
		/* (possibly) set an enpassant square. */
		board->ebyte =
		    (board->ply && board->coord[*coord] == PAWN &&
		     *coord >= 24 && *coord < 32) ||
		    (!board->ply && board->coord[*coord] == BPAWN &&
		     *coord >= 32 && *coord < 40) ?
		    *coord : FLAG;
		break;

	    case 'C':
	    case 'c':
		/* (possibly) set cbyte. */
		switch(*coord)
		{
		case 0:
		    board->cbyte |= WHITEQCASTLE; break;
		case 4:
		    board->cbyte |= (WHITEQCASTLE | WHITEKCASTLE); break;
		case 7:
		    board->cbyte |= WHITEKCASTLE; break;
		case 56:
		    board->cbyte |= BLACKQCASTLE; break;
		case 60:
		    board->cbyte |= (BLACKQCASTLE | BLACKKCASTLE); break;
		case 63:
		    board->cbyte |= BLACKKCASTLE; break;
		default:
		    board->cbyte = 0; break;
		}
		newcbyte(board);
		break;

	    case 'S':
	    case 's':
		/* Switch turn. */
		board->ply ^= 1;
		board->ebyte = FLAG; /* because it can't be valid, now */
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

		if (board->coord[*coord])
		{
		    delpieceSmart(board, board->coord[*coord], *coord);
		    board->coord[*coord] = 0;
		    if (board->ebyte == *coord)
		    {
			board->ebyte = FLAG;
		    }
		}
		if (chr)
		{
		    addpieceSmart(board, chr, *coord);
		    board->coord[*coord] = chr;
		}
		newcbyte(board);
		UIBoardRefresh(board);
		break;
	    }
	}
	if (c != KEY_UP && c != KEY_DOWN && c != KEY_LEFT && c != KEY_RIGHT)
	    continue;

	/* At this point we have a valid direction. */
	UICursorDraw(*coord, 0, 1);	/* Unmark current loc */
	UICursorMove(c, coord);
	UICursorDraw(*coord, 1, 0);	/* Blink new loc */
    }	/* end while */
}



/* Adjusts time. */
static void UITimeMenu(BoardT *board, GameStateT *gameState)
{   
    int c, i;
    int *coord = &gBoardIf.cursCoord; // shorthand.
    char validChars[] = "SsIiPpAaCcDd";
    char timeStr[TIME_STR_LEN];
    int incPeriod;

    // 0 -> white
    // 1 -> black
    // 2 -> both
    static int applyToggle = 2;

    UICursorDraw(*coord, 1, 0);

    while (1)
    {
	UITimeOptionsDraw(gameState, applyToggle);
	/* I do this here just so the cursor ends up in an aesthetically
	   pleasing spot. */
	gotoxy(OPTIONS_X, 24);
	textcolor(LIGHTCYAN);
	cprintf("Time    ");

	c = getch();
	if (strchr(validChars, c) != NULL)
	{
	    /* FIXME: implement time options. */
	    switch(c)
	    {
	    case 'S':
	    case 's':
		do
		{
		    UIBarfString(timeStr, 9, /* xx:yy:zz\0 */
				 "0123456789:inf", "Set start time to? >");
		} while (!TimeStringIsValid(timeStr));

		for (i = 0; i < 2; i++)
		{
		    if (applyToggle == i || applyToggle == 2)
		    {
			ClockSetTime(&gameState->origClocks[i],
				     TimeStringToBigtime(timeStr));
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

		for (i = 0; i < 2; i++)
		{
		    if (applyToggle == i || applyToggle == 2)
		    {
			ClockSetInc(&gameState->origClocks[i],
				    TimeStringToBigtime(timeStr));
		    }
		}
		break;
	    case 'P':
	    case 'p':
		do
		{
		    UIBarfString(timeStr, 9, /* xx:yy:zz\0 */
				 "0123456789", "Set increment period to? >");
		} while (sscanf(timeStr, "%d", &incPeriod) < 1);

		for (i = 0; i < 2; i++)
		{
		    if (applyToggle == i || applyToggle == 2)
		    {
			ClockSetIncPeriod(&gameState->origClocks[i],
					  incPeriod);
		    }
		}
		break;
	    case 'A':
	    case 'a':
		ClocksReset(gameState);
		UIStatusDraw(board, gameState);
		break;
	    case 'C':
	    case 'c':
		if (++applyToggle == 3)
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
static void UIGetCommand(uint8 command[], SwitcherContextT *sw,
			 GameStateT *gameState)
{   
    int c;
    int gettingsrc = 1;
    int *coord = &gBoardIf.cursCoord; // shorthand.
    char validChars[] = "NSRLWBFQDHCMEGAT";

    while (1)
    {
	// Wait for actual input.
	while(!kbhit())
	{
	    SwitcherSwitch(sw, gameState->playCookie);
	}
	c = getch();
	if (isalpha(c))
	    c = toupper(c); // accept lower-case commands, too.
	if (strchr(validChars, c) != NULL)
	{
	    if (!gettingsrc)
	    {
		UICursorDraw(command[0], 0, 1);
	    }
	    command[0] = c; /* valid one-char command */
	    return;
	}
	if (c == ENTER && gettingsrc)
	{
	    command[0] = *coord;
	    UICursorDraw(*coord, 0, 0);
	    gettingsrc = 0;
	    continue;
	}
	else if (c == ENTER)
	{
	    if (*coord == command[0]) /* we want to unselect src spot */
	    {
		gettingsrc = 1;
		UICursorDraw(*coord, 1, 0);
		continue;
	    }
	    UICursorDraw(command[0], 0, 1);
	    command[1] = *coord;	/* enter destination */
	    return;
	}
	if (c != KEY_UP && c != KEY_DOWN && c != KEY_LEFT &&
	    c != KEY_RIGHT)
	    continue;

	/* At this point we have a valid direction. */
	if (gettingsrc || command[0] != *coord)
	    UICursorDraw(*coord, 0, 1); /* need to unmark current loc */
	UICursorMove(c, coord);
	if (gettingsrc || command[0] != *coord)
	    UICursorDraw(*coord, 1, 0); /* need to blink current loc */
    }	/* end while */
}


static void UIBoardDraw(void)
{
    int i, j;

    /* note: a carriage return after drawing the board could clobber the
       ticks. */
    for (i = 0; i < 64; i++)
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
    UICursorDraw(gBoardIf.cursCoord, 0, 1); // hide old cursor
    gBoardIf.flipped ^= 1;
    UITicksDraw();   // update ticks
    UIBoardRefresh(board); // update player positions
    UICursorDraw(gBoardIf.cursCoord, 1, 0);
}


static void UIPlayerColorChange(void)
{
    char *colors[2] = {"White", "Black"};
    int i;

    char myStr[3];
    int myColor;

    for (i = 0; i < 2; i++)
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
    while ((i = UIBarf("Set debug level to (0-2)? >") - '0') < 0 ||
	   i > 2)
	;
    LogSetLevel(i);
}


void UINotifyThinking(void)
{
    gotoxy(OPTIONS_X, 24);
    textcolor(RED);
    cprintf("Thinking");
}


void UINotifyReady(void)
{
    gotoxy(OPTIONS_X, 24);
    textcolor(LIGHTGREEN);
    cprintf("Ready   ");
    UICursorDraw(gBoardIf.cursCoord, 1, 0);
}


void UINotifyComputerStats(CompStatsT *stats)
{
    gotoxy(1, 1);
    textcolor(SYSTEMCOL);
    cprintf("%d %d %d %d %d ",
	    stats->funcCallCount, stats->moveCount,
	    stats->hashHitGood, stats->hashHitPartial,
	    stats->filler);
}


void UINotifyDraw(char *reason)
{
    UIBarf("Game is drawn (%s).", reason);
}


void UINotifyCheckmated(int turn)
{
    UIBarf("%s is checkmated.", turn ? "Black" : "White");
}


void UINotifyResign(int turn)
{
    UIBarf("%s resigns.", turn ? "Black" : "White");
}


static void UIMovelistShow(MoveListT *mvlist)
{
    int i;
    textcolor(SYSTEMCOL);
    gotoxy(1, 1);
    uint8 *comstr;
    for (i = 0; i < mvlist->lgh; i++)
    {
	comstr = mvlist->list[i];
	cprintf("%c%c%c%c.%d.%c%c ",
		File(comstr[0]) + 'a',
		Rank(comstr[0]) + '1',
		File(comstr[1]) + 'a',
		Rank(comstr[1]) + '1',
		comstr[2],
		(comstr[3] == FLAG ? 'F' :
		 comstr[3] == DOUBLE_CHECK ? 'D' :
		 File(comstr[3]) + 'a'),
		(comstr[3] == FLAG ? 'F' :
		 comstr[3] == DOUBLE_CHECK ? 'D' :
		 Rank(comstr[3]) + '1'));
    }
    UIBarf("possible moves.");
}


/* this function intended to get player input and adjust variables
   accordingly */
static void UIPlayerMove(BoardT *board, ThinkContextT *th,
			 SwitcherContextT *sw, GameStateT *gameState)
{
    uint8 *ptr;
    uint8 chr;
    MoveListT movelist;
    uint8 myPieces[64];
    uint8 comstr[80];
    int myLevel;
    int myHiswin;
    int on, player;

    UIGetCommand(comstr, sw, gameState);
    
    switch(comstr[0])
    {
    case 'Q':    /* bail */
	ThinkerCmdBail(th);
	UIExit();
	printf("bye.\n");
	exit(0);
	break;
    case 'N':     /* new game */
	ThinkerCmdBail(th);
	ClocksReset(gameState);
	newgame(board);
	GoaltimeCalc(gameState, board);
	commitmove(board, NULL, th, gameState, 0);
	return;
    case 'L':     /* switch computer level */
	while ((myLevel = UIBarf("Set to what level? >") - '0') < 0 ||
	       myLevel > 9)
	    ;	/* do nothing */
	if (board->level > (gVars.maxLevel = myLevel))
	{
	    ThinkerCmdMoveNow(th);
	}
	UIOptionsDraw(gameState);
	return;
    case 'H':     /* change history window */
	while ((myHiswin = UIBarf("Set to x moves (0-9)? >") - '0') < 0 ||
	       myHiswin > 9)
	    ;	/* do nothing */
	gVars.hiswin = myHiswin << 1;	/* convert moves to plies. */
	UIOptionsDraw(gameState);
	return;
    case 'W':     /* toggle computer control */
    case 'B':
	player = (comstr[0] == 'B');
	on = (gameState->control[player] ^= 1);
	/* Are we affecting the control of the side to move? */
	if ((gameState->savedBoard.ply & 1) == player)
	{
	    /* Yes. */
	    if (on)
	    {
		GoaltimeCalc(gameState, board);
		gUI->notifyThinking();
		ThinkerCmdThink(th);
	    }
	    else
	    {
		ThinkerCmdBail(th);
		GoaltimeCalc(gameState, board);
		UINotifyReady();
	    }
	}
	UIOptionsDraw(gameState);
	return;
    case 'M':
	ThinkerCmdMoveNow(th);
	return;
    case 'C':     /* change w/b colors */
	UIPlayerColorChange();
	UIBoardRefresh(&gameState->savedBoard);
	return;
    case 'F':     /* flip board. */
	UIBoardFlip(&gameState->savedBoard);
	return;
    case 'D':     /* change debug logging level. */
	UISetDebugLoggingLevel();
	return;
    case 'S':
	UIBarf(GameSave(&gameState->savedBoard) < 0 ?
	       "Game save failed." :
	       "Game save succeeded.");
	return;
    case 'R':
	/* restore to copy at first, so computer does not need to re-think if
	   the restore fails. */
	if (GameRestore(&gameState->savedBoard) < 0)
	{
	    UIBarf("Game restore failed.");
	}
	else
	{
	    ThinkerCmdBail(th);
	    UIBarf("Game restore succeeded.");
	    hashInit();
	    histInit();
	    BoardCopy(board, &gameState->savedBoard);
	    commitmove(board, NULL, th, gameState, 0);
	}
	return;
    case 'E':
	ThinkerCmdBail(th);
	ClocksStop(gameState);
	do {
	    UIEditPosition(board);
	} while (BoardSanityCheck(board));
	UIOptionsDraw(gameState);

	memcpy(myPieces, board->coord, sizeof(myPieces));
	ClocksReset(gameState);
	newgameEx(board, myPieces, board->cbyte, board->ebyte, board->ply);
	GoaltimeCalc(gameState, board);
	commitmove(board, NULL, th, gameState, 0);
	return;
    case 'A': /* toggle randomize moves. */
	gVars.randomMoves ^= 1;
	UIOptionsDraw(gameState);
	return;
    case 'T':
        // I'm pretty sure I want the computer to stop thinking, if I'm swiping
	// the time out from under it.
	ThinkerCmdBail(th);
	ClocksStop(gameState);
	UITimeMenu(board, gameState);
	UIOptionsDraw(gameState);
	UINotifyReady();
	commitmove(board, NULL, th, gameState, 0);
	return;
    default:
	break;
    }

    /* at this point must be a move or request for moves. */
    /* get valid moves. */
    mlistGenerate(&movelist, &gameState->savedBoard, 0);
    if (comstr[0] == 'G')	/* display moves */
    {
	UIMovelistShow(&movelist);
	UIBoardDraw();
	UITicksDraw();
	UIOptionsDraw(gameState);
	UIBoardRefresh(&gameState->savedBoard);
	UIStatusDraw(board, gameState);
     	return;
    }

    /* search movelist for comstr */
    if ((ptr = searchlist(&movelist, comstr, 2)) == NULL)
    {
	UIBarf("Sorry, invalid move.");
	UITicksDraw();
	return;
    }

    /* At this point, we must have a valid move. */
    ThinkerCmdBail(th);

    /* Do we need to promote? */
    if (ISPAWN(gameState->savedBoard.coord[comstr[0]]) &&
	(comstr[1] > 55 || comstr[1] < 8))
    {
	while ((chr = UIBarf("Promote piece to (q, r, b, n)? >")) != 'q' &&
	       chr != 'r' && chr != 'b' && chr != 'n')
	    ; /* do nothing */
	chr = asciiToNative(chr);
	comstr[2] = (chr & ~1) | (gameState->savedBoard.ply & 1);

	ptr = searchlist(&movelist, comstr, 3);
	assert(ptr != NULL);
    }
    else
    {
	comstr[2] = ptr[2];
    }
    comstr[3] = ptr[3];
    commitmove(board, comstr, th, gameState, 0);
}


static void UINotifyMove(uint8 *comstr) { }


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
    .notifyReady = UINotifyReady,
    .notifyComputerStats = UINotifyComputerStats,
    .notifyDraw = UINotifyDraw,
    .notifyCheckmated = UINotifyCheckmated,
    .notifyResign = UINotifyResign
};


/* Do any UI-specific initialization. */
UIFuncTableT *UIInit(GameStateT *gameState)
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
    UIOptionsDraw(gameState);    

    return &myUIFuncTable;
}
