#include <stdio.h>
#include <conio.h>
#include <string.h>
#include <ctype.h>
#include "ref.h"

/* assumes computer won't be thinking on player's time.  For MS-DOS, I'm
thinking rerouting interrupts would be required.  For Unix, perhaps processes
could send signals to each other? */
/* this particular version also assumes it's player's turn. */
/* means:  you can't quit or restart when it's computer's turn... :( */

/* this function intended to get player input and adjust variables
 accordingly */
void playermove(struct brd *board, int *show, int control[],
	int col[])
{	char *ptr;
	struct mlist movelist;
	char comstr[80], cappiece;
	getopt(comstr);
	if (comstr[0] == 'Q')	/* bail */
	{	textmode(C80);
		printf("bye.");
		exit(0);
	}
	if (comstr[0] == 'P')	/* nada.  Useful for pitting comp */
		return;				/* against itself. */
	if (comstr[0] == 'N')	/* new game */
	{	new(board, col);
      	return;
	}
	if (comstr[0] == 'L')	/* switch computer level */
	{	while ((board->level = barf("Set to what level? >") - '0') < 0 ||
				board->level > 9)
			;	/* do nothing */
		return;
	}
	if (comstr[0] == 'H')	/* change history window */
	{	while ((board->hiswin = barf("Set to x moves (0-9)? >") - '0') < 0 ||
				board->hiswin > 9)
			;	/* do nothing */
		board->hiswin <<= 1;	/* convert moves to plies. */
		return;
	}
	if (comstr[0] == 'S')	/* debug mode.  Show thinking. */
	{	*show = !(*show);
		return;
	}
	if (comstr[0] == 'W')	/* switch white control */
	{	control[0] = !control[0];
		return;
	}
	if (comstr[0] == 'B')	/* black control */
	{	control[1] = !control[1];
		return;
	}
	if (comstr[0] == 'C')	/* change w/b colors */
	{	while ((col[0] = barf("White color? >") - '0') < 1 ||
				col[0] > 15)
			;
		while ((col[1] = barf("Black color? >") - '0') < 1 ||
				col[1] > 15 || col[1] == col[0])
			;
		return;
	}

	/* at this point must be a move or request for moves. */
	/* get valid moves. */
	genmlist(&movelist, board, board->move & 1);

	if (comstr[0] == 'M')	/* display moves */
     {	printlist(&movelist);
		drawboard();
		update(board->coord, col);
     	return;
	}

	/* search movelist for comstr */
	ptr = searchlist(&movelist, comstr);
	if (ptr != NULL)
	{    /* do we need to promote? */
		if (tolower(board->coord[comstr[0]]) == 'p' &&
			(comstr[1] > 55 || comstr[1] < 8))
		{	while ((comstr[2] = barf("Promote piece to (q, r, b, n)? >"))
					!= 'q' &&	comstr[2] != 'r' && comstr[2] != 'b' &&
					comstr[2] != 'n')
				; /* do nothing */
			ptr += comstr[2] == 'b' ? 4 :
				  comstr[2] == 'r' ? 8 :
				  comstr[2] == 'q' ? 12 :
				  0;
		/* note: if we change size o' mvlist, better change size of this! */
          	comstr[2] = toupper(comstr[2]) | ((board->move & 1) << 5);
			/* ascii-dependent statement converts to proper case. */
		}
		else comstr[2] = 0;
		comstr[3] = ptr[3];
		cappiece = board->coord[comstr[1]];
/*		gotoxy(1, 25);
		cprintf("%x %c%c", board->cbyte, File(board->ebyte) + 'a',
			Rank(board->ebyte) + '1'); */
		makemove(board, comstr, cappiece);
	}
	else barf("Sorry, invalid move.");
	drawboard();
	update(board->coord, col);
}

char *searchlist(struct mlist *mvlist, char *comstr)
{	int x;
	for (x = 0; x < mvlist->lgh; x++)
		if (mvlist->list[x] [0] == comstr[0] &&
			mvlist->list[x] [1] == comstr[1])
			return mvlist->list[x];
     return NULL;
}
