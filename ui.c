#include <stdio.h>
#include <ctype.h>	/* islower() */
#include <string.h>     /* strlen() */
#include <stdlib.h>     /* exit() */
#include "conio.h"
#include "ref.h"

#define SYSTEMCOL       GREEN
#define TICKCOL         BLUE
#define BOARDCOL        BLUE

#define ENTER 13
#define ESC   27

#define SQUARE_WIDTH 5   /* width of chess square, in characters */
#define OPTIONS_X (SQUARE_WIDTH * 8 + 2) /* 1 since one-based, 1 for ticks) */

static struct {
    int col[2];    // player colors.
    int flipped;   // bool, is the board inverted (black on the bottom)
    int cursCoord; // coordinate cursor is at.
} gBoardIf;


/* (one extra space for \0.) */
static uint8 gPieceUITable[BQUEEN + 2] = "  KKppNNBBRRQQ";

void printstatus(BoardT *board, int timetaken)
{
    MoveListT mvlist;
    int turn = board->ply & 1;
    textcolor(LIGHTGRAY);
    gotoxy(OPTIONS_X, 19);
    cprintf("time: %d   ", timetaken);
    gotoxy(OPTIONS_X, 18);
    cprintf("%c%c %c%c ", File(board->ncheck[0]) + 'a',
	    Rank(board->ncheck[0]) + '1', File(board->ncheck[1]) + 'a',
	    Rank(board->ncheck[1]) + '1');
    gotoxy(OPTIONS_X, 20);
    cprintf("move: %d   ", (board->ply >> 1) + 1);
    gotoxy(OPTIONS_X, 21);
    textcolor(SYSTEMCOL);
    cprintf("%s\'s turn", turn ? "black" : "white");
    gotoxy(OPTIONS_X, 22);
    cprintf(board->ncheck[turn] == FLAG ? "       " : "<check>");

    mlistGenerate(&mvlist, board, 0);
#if 1
    if (drawThreefoldRepetition(board))
    {
	barf("Game is drawn (threefold repetition).");
    }
#endif
    else if (drawInsufficientMaterial(board))
    {
	barf("Game is drawn (insufficient material).");
    }
    else if (!mvlist.lgh)
    {
	barf(board->ncheck[turn] == FLAG ? "Game is drawn (stalemate)." :
	     turn ? "Black is checkmated." :
	     "White is checkmated.");
    }
}


void UIPVDraw(PvT *pv, int eval)
/* prints out expected move sequence at the bottom of the screen. */
{
    static int lasthowmany = 0;
    int howmany = pv->depth + 1;
    uint8 *moves = pv->pv;
    int i;

    gotoxy(1, 25);
    textcolor(SYSTEMCOL);

    // blank out the last pv.
    for (i = 0; i < lasthowmany * 5 + 10; i++)
    {
	cprintf(" ");
    }
    lasthowmany = howmany;

    // print the new pv.
    gotoxy(1, 25);
    cprintf("pv: %+d", eval);
    for (; howmany > 0; howmany--)
    {
	cprintf(" %c%c%c%c", File(moves[0]) + 'a', Rank(moves[0]) + '1',
		File(moves[1]) + 'a', Rank(moves[1]) + '1');
	moves += 2;
    }
    cprintf(".");
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
    textcolor(BROWN + (blink ? BLINK : 0));
    gotoxy(x, y);
    cprintf("%s %s", undo ? "  " : "\\ ", undo ? "  " : " /");
    gotoxy(x, y + 2);
    cprintf("%s %s", undo ? "  " : "/ ", undo ? "  " : " \\");
    gotoxy(SQUARE_WIDTH * 8 + 7, 24);
    textbackground(BLACK);	/* get rid of that annoying blink */
}


static void prettyprint(char *option, int y)
/* spews user option to screen, highlighting the first char. */
{
    int i, didHighlight = 0;
    gotoxy(OPTIONS_X, y);
    textcolor(LIGHTGRAY);

    for (i = 0; i < strlen(option); i++)
    {
	if (!didHighlight && isupper(option[i]))
	{
	    textcolor(WHITE);
	    didHighlight = 1;
	    cprintf("%c", option[i]);
	    textcolor(LIGHTGRAY);
	    continue;
	}

	cprintf("%c", option[i]);
    }
}


static void drawoptions(void)
{
    gotoxy(OPTIONS_X, 1);
    textcolor(SYSTEMCOL);
    cprintf("Options:");
    prettyprint("New game", 2);
    prettyprint("Save game", 3);
    prettyprint("Restore game", 4);
    prettyprint("Level", 5);
    prettyprint("White control", 6);
    prettyprint("Black control", 7);
    prettyprint("Flip board", 8);
    prettyprint("Quit", 9);

    prettyprint("Debug logging", 11);
    prettyprint("History window", 12);
    prettyprint("Color", 13);
    // prettyprint("shOw", 14); still can do, just not visible
    prettyprint("Moves", 14);
    prettyprint("Pass", 15);
    prettyprint("Autopass", 16);
}


void getopt(uint8 command[])
/* gets user input and translates it to valid command.  Returns: command, or
   two numbers signaling source and destination.  */
{   
    int c;
    int gettingsrc = 1;
    int *coord = &gBoardIf.cursCoord; // shorthand.
    drawoptions();
    gotoxy(OPTIONS_X, 24);
    textcolor(LIGHTGREEN);
    cprintf("Ready   ");
    UICursorDraw(*coord, 1, 0);
    while (1)
    {
	if (isalpha((c = getch())))
	    c = toupper(c); // accept lower-case commands, too.
	if (c == 'L' || c == 'W' || c == 'B' || c == 'Q' || c == 'P' ||
	    c == 'F' || c == 'H' || c == 'C' || c == 'S' || c == 'N' ||
	    c == 'M' || c == 'D' || c == 'R' || c == 'A')
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
	    if (*coord == command[0])	/* we want to unselect src spot */
	    {
		gettingsrc = 1;
		UICursorDraw(*coord, 1, 0);
		continue;
	    }
	    UICursorDraw(command[0], 0, 1);
	    command[1] = *coord;	/* enter destination */
	    return;
	}
	if (c != KEY_UP && c != KEY_DOWN && c != KEY_LEFT && c != KEY_RIGHT)
	    continue;
	/* valid direction. */
	if (gettingsrc || command[0] != *coord)
	    UICursorDraw(*coord, 0, 1);	/* need to unmark current loc */
	if ((c == KEY_UP && !gBoardIf.flipped) ||
	    (c == KEY_DOWN && gBoardIf.flipped))
	{
	    if ((*coord += 8) > 63)
		*coord -= 64;
	}
	else if ((c == KEY_DOWN && !gBoardIf.flipped) ||
		 (c == KEY_UP && gBoardIf.flipped))
	{
	    if ((*coord -= 8) < 0)
		*coord += 64;
	}
	else if ((c == KEY_LEFT && !gBoardIf.flipped) ||
		 (c == KEY_RIGHT && gBoardIf.flipped))
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

	if (gettingsrc || command[0] != *coord)
	    UICursorDraw(*coord, 1, 0);	/* need to blink current loc */
    }	/* end while */
}


void UITicksDraw(void)
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


void UIBoardDraw(void)
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


// Do any UI-specific initialization.
void UIInit(void)
{
    initconio();
    clrscr();
    gBoardIf.col[0] = LIGHTCYAN;
    gBoardIf.col[1] = LIGHTGRAY;
    gBoardIf.flipped = 0;
    gBoardIf.cursCoord = 0;
}


void UIBoardFlip(BoardT *board)
{
    UICursorDraw(gBoardIf.cursCoord, 0, 1); // hide old cursor
    gBoardIf.flipped ^= 1;
    UITicksDraw();   // update ticks
    UIBoardUpdate(board); // update player positions
}


void UIBoardUpdate(BoardT *board)
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
	    putch(gPieceUITable[bcoord[i]]);
	}
    textbackground(BLACK);
}


void UIPlayerColorChange(void)
{
    while ((gBoardIf.col[0] = barf("White color? >") - '0') < 1 ||
	   gBoardIf.col[0] > 15)
	;
    while ((gBoardIf.col[1] = barf("Black color? >") - '0') < 1 ||
	   gBoardIf.col[1] > 15 || gBoardIf.col[1] == gBoardIf.col[0])
	;
}


void UISetDebugLoggingLevel(void)
{
    int i;
    while ((i = barf("Set debug level to (0-2)? >") - '0') < 0 ||
	   i > 2)
	;
    LogSetLevel(i);
}


void UIMoveShow(BoardT *board, uint8 *comstr, char *caption)
{
    UIBoardUpdate(board);
    gotoxy(1,1);
    cprintf("move was %c%c%c%c", File(comstr[0]) + 'a',
	    Rank(comstr[0]) + '1',
	    File(comstr[1]) + 'a',
	    Rank(comstr[1]) + '1');
    barf(caption);
}


void UINotifyThinking(void)
{
    gotoxy(OPTIONS_X, 24);
    textcolor(RED);
    cprintf("Thinking");
}


void UINotifyComputerStats(CompStatsT *stats)
{
    gotoxy(1, 1);
    textcolor(SYSTEMCOL);
    cprintf("%d %d ", stats->funcCallCount, stats->moveCount);
}


void UIMovelistShow(MoveListT *mvlist)
{
    int x;
    textcolor(SYSTEMCOL);
    gotoxy(1, 1);
    for (x = 0; x < mvlist->lgh; x++)
	cprintf("%c%c%c%c%c%c%c ",
		File(mvlist->list[x] [0]) + 'a',
		Rank(mvlist->list[x] [0]) + '1',
		File(mvlist->list[x] [1]) + 'a',
		Rank(mvlist->list[x] [1]) + '1',
		mvlist->list[x] [2] ? mvlist->list[x] [2] : '0',
		File(mvlist->list[x] [3]) + 'a',
		Rank(mvlist->list[x] [3]) + '1');
    barf("possible moves.");
}


void UIExit(void)
{
    doneconio();
}


int barf(char *message)
{
    int chr, i;
    int len = strlen(message);

    gotoxy(40 - len / 2, 25);
    textcolor(MAGENTA);
    cprintf(message);
    chr = getch();

    /* Now, blank the entire message. */
    gotoxy(40 - len / 2, 25);
    for (i = 0; i < len; i++)
    {
	cprintf(" ");
    }

    UITicksDraw();
    if (chr == ESC)	/* bail on ESC */
    {
	doneconio();
	exit(0);
    }
    gotoxy(1, 1); /* justncase */
    return chr;
}
