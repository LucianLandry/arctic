#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h> /* exit() */
#include <assert.h>
#include "ref.h"


static char *searchlist(MoveListT *mvlist, char *comstr, int howmany)
{
    int i;
    for (i = 0; i < mvlist->lgh; i++)
	if (!memcmp(mvlist->list[i], comstr, howmany))
	    return mvlist->list[i];
    return NULL;
}


/* assumes computer won't be thinking on player's time.  For MS-DOS, I'm
thinking rerouting interrupts would be required.  For Unix, perhaps processes
could send signals to each other? */
/* this particular version also assumes it's player's turn. */
/* means:  you can't quit or restart when it's computer's turn... :( */

/* this function intended to get player input and adjust variables
 accordingly */
void playermove(BoardT *board, int *autopass, int control[])
{
    char *ptr;
    uint8 chr;
    MoveListT movelist;
    uint8 comstr[80];
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
    case 'W':     /* switch white control */
	control[0] = !control[0];
	return;
    case 'B':     /* black control */
	control[1] = !control[1];
	return;
    case 'A':
	control[0] = control[1] = 1;
	*autopass = 1;
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
    mlistGenerate(&movelist, board, 0);
    if (comstr[0] == 'M')	/* display moves */
    {
	UIMovelistShow(&movelist);
	UIBoardDraw();
	UITicksDraw();
	UIBoardUpdate(board);
     	return;
    }

    /* search movelist for comstr */
    ptr = searchlist(&movelist, comstr, 2);
    if (ptr != NULL)
    {
	/* do we need to promote? */
	if (ISPAWN(board->coord[comstr[0]]) &&
	    (comstr[1] > 55 || comstr[1] < 8))
	{
	    while ((chr = barf("Promote piece to (q, r, b, n)? >")) != 'q' &&
		   chr != 'r' && chr != 'b' && chr != 'n')
		; /* do nothing */
	    /* convert the answer to board representation. */
	    switch(chr)
	    {
	    case 'q': chr = QUEEN; break;
	    case 'r': chr = ROOK; break;
	    case 'b': chr = BISHOP; break;
	    case 'n': chr = NIGHT; break;
	    default: assert(0); break;
	    }
	    comstr[2] = chr | (board->ply & 1);

	    ptr = searchlist(&movelist, comstr, 3);
	    assert(ptr != NULL);
	}
	else
	{
	    comstr[2] = ptr[2];
	}
	comstr[3] = ptr[3];
	SavePosition(board);
	makemove(board, comstr, NULL);
    }
    else barf("Sorry, invalid move.");
    UIBoardDraw();
    UIBoardUpdate(board);
}
