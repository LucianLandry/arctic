#include <stdio.h>
#include <conio.h>
#include <ctype.h>	/* for isupper */
#include "ref.h"

#define UP	'H'	/* cursor directions */
#define DOWN	'P'
#define LEFT	'K'
#define RIGHT	'M'
#define ENTER	13

void printstatus(struct brd *board, int timetaken)
{    struct mlist mvlist;
	int turn = board->move & 1;
	gotoxy(26, 20);
	printf("time: %d   ", timetaken);
	gotoxy(26, 19);
	cprintf("%c%c %c%c ", File(board->ncheck[0]) + 'a',
		Rank(board->ncheck[0]) + '1', File(board->ncheck[1]) + 'a',
		Rank(board->ncheck[1]) + '1');
	genmlist(&mvlist, board, turn);
	if (!mvlist.lgh)
		if (board->ncheck[turn] != FLAG)
			if (!turn)
				barf("White is checkmated.");
			else barf("Black is checkmated.");
		else barf("Game is drawn (stalemate).");
	else
	{	gotoxy(26, 21);
		textcolor(SYSTEMCOL);
		cprintf("%s\'s move", turn ? "black" : "white");
		gotoxy(26, 22);
		cprintf(board->ncheck[turn] == FLAG ? "       " : "<check>");
	}
}

void printpv(char *moves, int howmany)
/* prints out expected move sequence at the bottom of the screen. */
{	gotoxy(1, 25);
	textcolor(SYSTEMCOL);
	cprintf("pv:");
	for (; howmany >= 0; howmany--)
	{	cprintf(" %c%c%c%c", File(moves[0]) + 'a', Rank(moves[0]) + '1',
				File(moves[1]) + 'a', Rank(moves[1]) + '1');
		moves += 2;
	}
	cprintf(".");
}

void getopt(char command[])
/* gets user input and translates it to valid command.  Returns: command, or
   two numbers signaling source and destination.  */
{    static int coord = 0;
	int c;
	int destcoord, srccoord;
	int gettingsrc = 1;
	drawoptions();
	gotoxy(26, 24);
	textcolor(LIGHTGREEN);
	cprintf("Ready   ");
	drawcurs(coord, 1, 0);
	while (1)
	{	c = getch();
		if (c == 'L' || c == 'W' || c == 'B' || c == 'Q' || c == 'P' ||
			c == 'H' || c == 'C' || c == 'S' || c == 'N' || c == 'M')
          {	command[0] = c; /* valid one-char command */
			return;
		}
		if (c && c != ENTER)
			continue;	/* otherwise better be escape sequence. */
		if (c == ENTER && gettingsrc)
		{	command[0] = coord;
			drawcurs(coord, 0, 0);
/*			cprintf("drew nonblinker"); */
			gettingsrc = 0;
			continue;
		}
		else if (c == ENTER)
		{	if (coord == command[0])	/* we want to unselect src spot */
			{	gettingsrc = 1;
				drawcurs(coord, 1, 0);
                	continue;
			}
			drawcurs(coord, 0, 1);
			drawcurs(command[0], 0, 1);
			command[1] = coord;	/* enter destination */
			return;
		}
		c = getch();	/* get direction. */
		if (c != UP && c != DOWN && c != LEFT && c != RIGHT)
			continue;
		/* valid direction. */
		if (gettingsrc || command[0] != coord)
			drawcurs(coord, 0, 1);	/* need to unmark current loc */
          switch(c)
		{	case UP: 		coord += 8;
						if (coord > 63)
							coord -= 64;
						break;
			case DOWN:	coord -= 8;
               			if (coord < 0)
							coord += 64;
						break;
			case LEFT:	coord--;
						if (File(coord) == 7)
							coord += 8;
						break;
			default:		coord++;
						if (!File(coord))
							coord -= 8;
						break;
		}
		if (gettingsrc || command[0] != coord)
			drawcurs(coord, 1, 0);	/* need to blink current loc */
	}	/* end while */
}



void drawcurs(int coord, int blink, int undo)
/* draws cursor at appropriate coordinate.  Blinks if spot hasn't been
   'selected', otherwise noblink.  'undo' 'undraws' the cursor at a spot. */
{    int x, y;
	/* translate coord to xy coords of upper left part of cursor. */
	x = 3 * File(coord) + 1;
	y = 3 * (7 - Rank(coord)) + 1;
     if ((Rank(coord) + File(coord)) & 1) /* we on a board-colored spot */
		textbackground(BOARDCOL);
	textcolor(BROWN + (blink ? BLINK : 0));
	gotoxy(x, y);
	cprintf("%c %c", undo ? ' ' : 'к', undo ? ' ' : 'П');
	gotoxy(x, y + 2);
	cprintf("%c %c", undo ? ' ' : 'Р', undo ? ' ' : 'й');
	gotoxy(31, 24);
	textbackground(BLACK);	/* get rid of that annoying blink */
}

void drawoptions()
{	gotoxy(26, 1);
	textcolor(SYSTEMCOL);
	cprintf("Options:");
	prettyprint("New game", 2);
	prettyprint("Level", 3);
	prettyprint("White control", 4);
	prettyprint("Black control", 5);
	prettyprint("Quit", 6);
	prettyprint("Hiswin", 8);
	prettyprint("Color", 9);
	prettyprint("Show", 10);
	prettyprint("Moves", 11);
	prettyprint("Pass", 12);
}

void prettyprint(char *option, int y)
/* spews user option to screen, highlighting the first char. */
{	gotoxy(26, y);
	textcolor(WHITE);
	cprintf("%c", option[0]);
	textcolor(LIGHTGRAY);
	cprintf("%s", &option[1]);
}

void drawticks()
{	int x;
	textcolor(TICKCOL);
	for (x = 0; x < 8;)
	{	gotoxy(25, 23-3*x);
		cprintf("%d", ++x);
	}
	gotoxy(1, 25);
	cprintf(" a  b  c  d  e  f  g  h                ");
}


void drawboard()
{	int x, y;
	textcolor(BOARDCOL);
	gotoxy(1,1);
	for (x = 0; x < 4; x++)
	{	for (y = 0; y < 3; y++)
			cprintf("ллл   ллл   ллл   ллл   \n");
			/* (If we're really a**l, we can take out the 3 spaces..) */
		for (y = 0; y < 3; y++)
			cprintf("   ллл   ллл   ллл   ллл\n");
	}
}

void update(char board[], int col[])
{	int x, y;
	int i = 0;
	for (y = 0; y < 8; y++)
		for (x = 0; x < 8; x++, i++)
		{	if ((x + y) % 2)
				textbackground(BOARDCOL);
			else
				textbackground(BLACK);
               gotoxy(2 + x * 3, 2 + (7 - y) * 3);
			/* note if we flip board, we switch the '7-' stuff... */
			if (!board[i])
				cprintf(" ");            /* erase any previous piece */
			else						/* 'draw' :) a piece */
			{	if (isupper(board[i]))	/* white */
                    	textcolor(col[0]);
                    else
					textcolor(col[1]);
				if (tolower(board[i]) == 'p')	/* pawn */
					cprintf("p");
				else putch(toupper(board[i]));
			}
		}
	textbackground(BLACK);
}

int barf(char *message)
{	int ack;
	gotoxy(20 - strlen(message) / 2, 25);
	textcolor(MAGENTA);
	cprintf(message);
	ack = getch();
	drawticks();
	if (ack == 27)	/* bail on ESC */
	{	textmode(C80);
		exit(0);
	}
	gotoxy(1, 1); /* justncase */
	return ack;
}
