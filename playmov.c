#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h> /* exit() */
#include <assert.h>
#include "ref.h"


static char *searchlist(struct mlist *mvlist, char *comstr)
{
    int x;
    for (x = 0; x < mvlist->lgh; x++)
	if (mvlist->list[x] [0] == comstr[0] &&
	    mvlist->list[x] [1] == comstr[1])
	    return mvlist->list[x];
    return NULL;
}


/* assumes computer won't be thinking on player's time.  For MS-DOS, I'm
thinking rerouting interrupts would be required.  For Unix, perhaps processes
could send signals to each other? */
/* this particular version also assumes it's player's turn. */
/* means:  you can't quit or restart when it's computer's turn... :( */

/* this function intended to get player input and adjust variables
 accordingly */
void playermove(BoardT *board, int *show, int control[])
{
    char *ptr;
    struct mlist movelist;
    uint8 comstr[80], cappiece;
    getopt(comstr);
    switch(comstr[0])
    {
    case 'Q':    /* bail */
	UIExit();
	printf("bye.\n");
	exit(0);
	break;
    case 'P':     /* nada.  Useful for pitting comp against itself. */
	return;
    case 'N':     /* new game */
	newgame(board);
	return;
    case 'L':     /* switch computer level */
	while ((board->level = barf("Set to what level? >") - '0') < 0 ||
	       board->level > 9)
	    ;	/* do nothing */
	return;
    case 'H':     /* change history window */
	while ((board->hiswin = barf("Set to x moves (0-9)? >") - '0') < 0 ||
	       board->hiswin > 9)
	    ;	/* do nothing */
	board->hiswin <<= 1;	/* convert moves to plies. */
	return;
    case 'O':     /* debug mode.  Show thinking. */
	*show = !(*show);
	return;
    case 'W':     /* switch white control */
	control[0] = !control[0];
	return;
    case 'B':     /* black control */
	control[1] = !control[1];
	return;
    case 'C':     /* change w/b colors */
	UIPlayerColorChange();
	return;
    case 'F':     /* flip board. */
	UIBoardFlip(board);
	return;
    case 'D':     /* change debug logging level. */
	UISetDebugLoggingLevel();
	return;
    case 'S':
	barf(GameSave(board) < 0 ?
	     "Game save failed." :
	     "Game save succeeded.");
	return;
    case 'R':
	if (GameRestore(board) < 0)
	{
	    barf("Game restore failed.");
	}
	else
	{
	    barf("Game restore succeeded.");
	    UIBoardUpdate(board);
	}
	return;
    default:
	break;
    }

    /* at this point must be a move or request for moves. */
    /* get valid moves. */
    genmlist(&movelist, board, board->ply & 1);
    if (comstr[0] == 'M')	/* display moves */
    {
	UIMovelistShow(&movelist);
	UIBoardDraw();
	UITicksDraw();
	UIBoardUpdate(board);
     	return;
    }

    /* search movelist for comstr */
    ptr = searchlist(&movelist, comstr);
    if (ptr != NULL)
    {
	/* do we need to promote? */
	if (tolower(board->coord[comstr[0]]) == 'p' &&
	    (comstr[1] > 55 || comstr[1] < 8))
	{
	    while ((comstr[2] = barf("Promote piece to (q, r, b, n)? >"))
		   != 'q' &&	comstr[2] != 'r' && comstr[2] != 'b' &&
		   comstr[2] != 'n')
		; /* do nothing */
	    ptr += comstr[2] == 'b' ? 4 :
		comstr[2] == 'r' ? 8 :
		comstr[2] == 'q' ? 12 :
		0;
	    /* note: if we change size o' mvlist, better change size of this!*/
	    comstr[2] = toupper(comstr[2]) | ((board->ply & 1) << 5);
	    /* ascii-dependent statement converts to proper case. */
	}
	else comstr[2] = 0;
	comstr[3] = ptr[3];
	cappiece = board->coord[comstr[1]];
	makemove(board, comstr, cappiece);
    }
    else barf("Sorry, invalid move.");
    UIBoardDraw();
    UIBoardUpdate(board);
}
