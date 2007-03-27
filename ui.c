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

#define SQUARE_WIDTH 5   /* width of chess square, in characters */
#define OPTIONS_X (SQUARE_WIDTH * 8 + 2) /* 1 since one-based, 1 for ticks) */
#define OPTIONS_X2 (OPTIONS_X + 15)
#define SCREEN_WIDTH 80

static struct {
    int col[2];    // player colors.
    int flipped;   // bool, is the board inverted (black on the bottom)
    int cursCoord; // coordinate cursor is at.
} gBoardIf;


static int nativeToBoardAscii(uint8 piece)
{
    int ascii = nativeToAscii(piece);
    return ISPAWN(piece) ? tolower(ascii) :
	(piece & 1) ? toupper(ascii) : ascii;
}


static void UIPrintBoardStatus(BoardT *board)
{
    /* all shorthand. */
    int ncheck = board->ncheck[board->ply & 1];
    int cbyte = board->cbyte;
    int ebyte = board->ebyte;

    textcolor(LIGHTGRAY);

    /* print castle status. */
    gotoxy(OPTIONS_X, 15);
    cprintf("castle QKqk: %c%c%c%c",
	    cbyte & WHITEQCASTLE ? 'y' : 'n',
	    cbyte & WHITEKCASTLE ? 'y' : 'n',
	    cbyte & BLACKQCASTLE ? 'y' : 'n',
	    cbyte & BLACKKCASTLE ? 'y' : 'n');

    /* print en passant status. */
    gotoxy(OPTIONS_X, 16);
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
    gotoxy(OPTIONS_X, 17);
    cprintf("chk: ");
    if (ncheck == FLAG)
	cprintf("   ");
    else if (ncheck == DISCHKFLAG)
    {
	cprintf("dis");
    }
    else /* normal check */
    {
	cprintf("%c%c ", File(ncheck) + 'a', Rank(ncheck) + '1');
    }
}


void UIStatusDraw(BoardT *board, int timeTaken)
{
    int turn = board->ply & 1;

    UIPrintBoardStatus(board);
    textcolor(LIGHTGRAY);
    gotoxy(OPTIONS_X, 19);
    cprintf("time: %d   ", timeTaken);
    gotoxy(OPTIONS_X, 20);
    cprintf("move: %d   ", (board->ply >> 1) + 1);
    gotoxy(OPTIONS_X, 21);
    textcolor(SYSTEMCOL);
    cprintf("%s\'s turn", turn ? "black" : "white");
    gotoxy(OPTIONS_X, 22);
    cprintf(board->ncheck[turn] == FLAG ? "       " : "<check>");
}


void UINotifyPV(PvT *pv)
/* prints out expected move sequence at the bottom of the screen. */
{
    static int lasthowmany = 0;
    /* pv->depth includes quiescing, whereas pv->level does not. */
    int howmany = pv->depth + 1;
    uint8 *moves = pv->pv;
    char spaces[80];
    int numspaces;
    int i;

    gotoxy(1, 25);
    textcolor(SYSTEMCOL);

    // blank out the last pv.
    numspaces = lasthowmany * 5 + 14;
    assert(numspaces < 80);
    for (i = 0; i < numspaces; i++)
    {
	sprintf(&spaces[i], " ");
    }
    cprintf(spaces);

    lasthowmany = howmany;

    // print the new pv.
    gotoxy(1, 25);
    cprintf("pv: d%d %+d", pv->level, pv->eval / 100);
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
    textcolor(/* BROWN */ YELLOW + (blink ? BLINK : 0));
    gotoxy(x, y);
    cprintf("%s %s", undo ? "  " : "\\ ", undo ? "  " : " /");
    gotoxy(x, y + 2);
    cprintf("%s %s", undo ? "  " : "/ ", undo ? "  " : " \\");

    textbackground(BLACK);	/* get rid of that annoying blink */
    textcolor(BLACK);
    gotoxy(SQUARE_WIDTH * 8 + 7, 24);
}


static void prettyprint(int y, char *option, char *option2)
/* spews user option to screen, highlighting the first char. */
{
    int i, j, didHighlight = 0;
    char *myopt;

    for (i = 0; i < 2; i++)
    {
	if ((myopt = i ? option2 : option) == NULL)
	    return;
	gotoxy(i ? OPTIONS_X2 : OPTIONS_X, y);
	textcolor(LIGHTGRAY);

	for (j = 0, didHighlight = 0;
	     j < strlen(myopt);
	     j++)
	{
	    if (!didHighlight && isupper(myopt[j]))
	    {
		textcolor(WHITE);
		didHighlight = 1;
		cprintf("%c", myopt[j]);
		textcolor(LIGHTGRAY);
		continue;
	    }

	    cprintf("%c", myopt[j]);
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


static void UIOptionsDraw(void)
{
    UIWindowClear(OPTIONS_X, 1, SCREEN_WIDTH - OPTIONS_X, 12);
    gotoxy(OPTIONS_X, 1);
    textcolor(SYSTEMCOL);
    cprintf("Options:");
    prettyprint(2,  "New game",       "Level");
    prettyprint(3,  "Save game",      "White control");
    prettyprint(4,  "Restore game",   "Black control");
    prettyprint(5,  "Edit position",  "Move now");
    prettyprint(6,  "Quit",           NULL);

    prettyprint(8,  "Flip board",     NULL);
    prettyprint(9,  "Color",          NULL);
    prettyprint(10, "Debug logging",  NULL);
    prettyprint(11, "History window", NULL);
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
    prettyprint(5, "Turn",           NULL);
    prettyprint(6, "Done",           NULL);
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

    /* Now, blank the entire message. */
    gotoxy((SCREEN_WIDTH / 2) - len / 2, 25);
    for (i = 0; i < len; i++)
    {
	cprintf(" ");
    }

    UITicksDraw();
    if (chr == ESC)	/* bail on ESC */
    {
	UIExit();
	exit(0);
    }
    gotoxy(1, 1); /* justncase */
    return chr;
}


static void UINotifyError(char *reason)
{
    UIBarf(reason);
}


/* Edits a board.  ('ply' is set to 0, or 1, at the end of the turn.) */
static void UIEditPosition(BoardT *board)
{   
    int c, chr, i, myply;
    int *coord = &gBoardIf.cursCoord; // shorthand.
    char validChars[] = "WwEeCcDdTt PpRrNnBbQqKk";

    UIEditOptionsDraw();
    UICursorDraw(*coord, 1, 0);
    while (1)
    {
	UIPrintBoardStatus(board);
	gotoxy(OPTIONS_X, 21);
	textcolor(SYSTEMCOL);
	cprintf("%s\'s turn", board->ply & 1 ? "black" : "white");
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
			delpiece(board, board->coord[i], i);
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
		    ((board->ply & 1) && board->coord[*coord] == PAWN &&
		     *coord >= 24 && *coord < 32) ||
		    (!(board->ply & 1) && board->coord[*coord] == BPAWN &&
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

	    case 'T':
	    case 't':
		chr = UIBarf("Whose move is it (W/b)? ");
		myply = (chr == 'B' || chr == 'b');
		if ((myply & 1) != (board->ply & 1)) 
		    board->ebyte = FLAG; /* because it can't be valid, now */
		board->ply = myply;
		break;

	    case 'D':
	    case 'd':
		/* bail from editing mode. */
		board->ply &= 0x1;
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
		    delpiece(board, board->coord[*coord], *coord);
		    board->coord[*coord] = 0;
		    if (board->ebyte == *coord)
		    {
			board->ebyte = FLAG;
		    }
		}
		if (chr)
		{
		    addpiece(board, chr, *coord);
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


/* Gets user input and translates it to valid command.  Returns: command, or
   two numbers signaling source and destination.  */
static void UIGetCommand(uint8 command[], SwitcherContextT *sw,
			 GameStateT *gameState)
{   
    int c;
    int gettingsrc = 1;
    int *coord = &gBoardIf.cursCoord; // shorthand.
    char validChars[] = "NSRLWBFQDHCMEG";

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
}


static void UIPlayerColorChange(void)
{
    while ((gBoardIf.col[0] = UIBarf("White color? >") - '0') < 1 ||
	   gBoardIf.col[0] > 15)
	;
    while ((gBoardIf.col[1] = UIBarf("Black color? >") - '0') < 1 ||
	   gBoardIf.col[1] > 15 || gBoardIf.col[1] == gBoardIf.col[0])
	;
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
    cprintf("%d %d %d ",
	    stats->funcCallCount, stats->moveCount, stats->hashHitGood);
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
    UIBarf("possible moves.");
}


/* this function intended to get player input and adjust variables
   accordingly */
static void UIPlayerMove(BoardT *board, ThinkContextT *th,
			 SwitcherContextT *sw, GameStateT *gameState)
{
    char *ptr;
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
	ThinkerBail(th);
	UIExit();
	printf("bye.\n");
	exit(0);
	break;
    case 'N':     /* new game */
	ThinkerBail(th);
	newgame(board);
	commitmove(board, NULL, th, gameState, 0);
	return;
    case 'L':     /* switch computer level */
	while ((myLevel = UIBarf("Set to what level? >") - '0') < 0 ||
	       myLevel > 9)
	    ;	/* do nothing */
	if (board->level > (board->maxLevel = myLevel))
	{
	    ThinkerMoveNow(th);
	}
	return;
    case 'H':     /* change history window */
	while ((myHiswin = UIBarf("Set to x moves (0-9)? >") - '0') < 0 ||
	       myHiswin > 9)
	    ;	/* do nothing */
	board->hiswin = myHiswin << 1;	/* convert moves to plies. */
	return;
    case 'W':     /* toggle computer control */
    case 'B':
	player = (comstr[0] == 'B');
	on = (gameState->control[player] ^= 1);
	/* Are we affecting the control of the side to move? */
	if ((gameState->boardCopy.ply & 1) == player)
	{
	    /* Yes. */
	    if (on)
		ThinkerThink(th);
	    else
	    {
		ThinkerBail(th);
		UINotifyReady();
	    }
	}
	return;
    case 'M':
	ThinkerMoveNow(th);
	return;
    case 'C':     /* change w/b colors */
	UIPlayerColorChange();
	UIBoardRefresh(&gameState->boardCopy);
	return;
    case 'F':     /* flip board. */
	UIBoardFlip(&gameState->boardCopy);
	return;
    case 'D':     /* change debug logging level. */
	UISetDebugLoggingLevel();
	return;
    case 'S':
	UIBarf(GameSave(&gameState->boardCopy) < 0 ?
	       "Game save failed." :
	       "Game save succeeded.");
	return;
    case 'R':
	/* restore to copy at first, so computer does not need to re-think if
	   the restore fails. */
	if (GameRestore(&gameState->boardCopy) < 0)
	{
	    UIBarf("Game restore failed.");
	}
	else
	{
	    ThinkerBail(th);
	    UIBarf("Game restore succeeded.");
	    CopyBoard(board, &gameState->boardCopy);
	    commitmove(board, NULL, th, gameState, 0);
	}
	return;
    case 'E':
	ThinkerBail(th);
	do {
	    UIEditPosition(board);
	} while (BoardSanityCheck(board));
	UIOptionsDraw();

	memcpy(myPieces, board->coord, sizeof(myPieces));
	newgameEx(board, myPieces, board->cbyte, board->ebyte, board->ply);
	commitmove(board, NULL, th, gameState, 0);
	return;
    default:
	break;
    }

    /* at this point must be a move or request for moves. */
    /* get valid moves. */
    mlistGenerate(&movelist, &gameState->boardCopy, 0);
    if (comstr[0] == 'G')	/* display moves */
    {
	UIMovelistShow(&movelist);
	UIBoardDraw();
	UITicksDraw();
	UIOptionsDraw();
	UIBoardRefresh(&gameState->boardCopy);
	UIStatusDraw(board, 0);
     	return;
    }

    /* search movelist for comstr */
    ptr = searchlist(&movelist, comstr, 2);
    if (ptr == NULL)
    {
	UIBarf("Sorry, invalid move.");
	UITicksDraw();
	return;
    }

    /* At this point, we must have a valid move. */
    ThinkerBail(th);

    /* Do we need to promote? */
    if (ISPAWN(gameState->boardCopy.coord[comstr[0]]) &&
	(comstr[1] > 55 || comstr[1] < 8))
    {
	while ((chr = UIBarf("Promote piece to (q, r, b, n)? >")) != 'q' &&
	       chr != 'r' && chr != 'b' && chr != 'n')
	    ; /* do nothing */
	chr = asciiToNative(chr);
	comstr[2] = (chr & ~1) | (gameState->boardCopy.ply & 1);

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
UIFuncTableT *UIInit(void)
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
    UIOptionsDraw();    

    return &myUIFuncTable;
}
