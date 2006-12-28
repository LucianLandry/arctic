#include <stddef.h> /* NULL */
#include <ctype.h>  /* tolower() */
#include <string.h>
#include "ref.h"


static int worth(char a)
/* in absolute terms, from white's point o' view. */
/* Hmm, if there is any compute here, I could probably replace this w/a
   lookup table. */
{
    switch(tolower(a))
    {
    case 'p': return 1;
    case 'b':
    case 'n': return 3;
    case 'r': return 5;
    case 'q': return 9;
    case 'k': return -1; /*error condition. */
    default:  return 0;
    }
}


static int eval(char coord[], int turn)
{
    int i, result;
    result = 0;
    for (i = 0; i < 64; i++)
	if (!check(coord[i], turn))	/* friend */
	    result += worth(coord[i]);
	else if (check(coord[i], turn) == 1) /* enemy */
	    result -= worth(coord[i]);
    return result;
}


static int minimax(BoardT *board, uint8 comstr[], int turn, int alpha,
		   int strgh, CompStatsT *stats, int *show, char *goodpv)
/* evaluates a given board position from {turn}'s point of view. */
{
    int val, newval, i, besti, cap;
    char newpv[20];
    int oldval = -1000;
    char ocbyte, oebyte;
    char newstr[4], cappiece;
    struct mlist mvlist;
    comstr[0] = FLAG;

    genmlist(&mvlist, board, turn);
    LogMoveList(eLogDebug, &mvlist);

    stats->funcCallCount++;
    /* Note:  ncheck for any side guaranteed to be correct *only* after
       the other side has made its move. */
    if (!(mvlist.lgh))
	return (board->ncheck[turn] != FLAG) ?
	    -1000 /* checkmate */ : 0; /* stalemate */
    val = -1000;
    turn ^= 1;		/* switch sides */
    for (i = 0; i < mvlist.lgh; i++)
    {
	ocbyte = board->cbyte;
	oebyte = board->ebyte;
	cappiece = board->coord[mvlist.list[i] [1]];

	LogMove(eLogDebug, board->depth, mvlist.list[i]);
	makemove(board, mvlist.list[i], cappiece);
	stats->moveCount++;

	strgh += (cap = worth(cappiece));
	if (cap == -1) /* captured king */
	{
	    UIMoveShow(board, mvlist.list[i], "diagnostic");
	}

	if (*show)
	{
	    if (concheck(board))
	    {
		if (barf("show results.") == 's')
		    *show = 0;
		UIMoveShow(board, mvlist.list[i], "board pos.");
	    }
	}

	if (board->depth >= board->level)
	    newval = strgh;
	else
	{
	    board->depth++;
	    newval = -minimax(board, newstr, turn, -val, -strgh,
			      stats, show, newpv);
	    board->depth--;
	}
	if (newval >= val)
	{
	    oldval = val;  /* record 2ndbest val for history table. */
	    if (newval > val)
	    {
		besti = i;
		val = newval;
		memcpy(goodpv, mvlist.list[i], 2); /* this be a good move. */
		if (board->level - board->depth)  /* copy the expected
						     sequence. */
		    memcpy(&goodpv[2], newpv,
			  (board->level - board->depth) << 1);
		if (!board->depth) /* searching at root level, so let user
				      know the updated line. */
		{
		    /* FIXME: when the search depth varies (when we implement
		       extensions, or mating scenarios), always passing in
		       'level' isn't correct. */
		    UIPVDraw(goodpv, val, board->level);
		}
	    }
	}
	board->cbyte = ocbyte;
	board->ebyte = oebyte;
	unmakemove(board, mvlist.list[i], cappiece);
	strgh -= cap;
	if (val >= alpha) /* ie, will leave other side just as bad off
			     (if not worse) */
	    break;	/* why bother checking had bad the alternatives are? */

	if ((board->depth == board->level &&
	     i >= mvlist.insrt - 1) ||
	    (board->depth == board->level - 1 &&
	     i >= mvlist.insrt - 1 &&
	     val >= strgh))
	    /* ran out of cool moves.
	       Don't search further. */
	    break;
    }
    memcpy(comstr, mvlist.list[besti], 4);
    if (val > oldval) /* move is at least one point better than others. */
	board->hist[turn ^ 1] [comstr[0]] [comstr[1]] = board->ply;
    return val;
}


void computermove(BoardT *board, int *show)
{
    int value, strngth;
    CompStatsT stats = {0, 0};
    char pv[20];
    int turn = board->ply & 1;
    /* save off ncheck, minimax() clobbers it. */
    int ncheck = board->ncheck[turn];
    uint8 comstr[4];
    board->depth = 0;	/* start search from root depth. */
    UINotifyThinking();

    /* get starting strength, though it doesn't really matter */
    strngth = eval(board->coord, turn);
    value = minimax(board, comstr, turn, 1000, strngth, &stats, show, pv);
    board->ncheck[turn] = ncheck;

    UINotifyComputerStats(&stats);
    if (value < -500) /* we're in a really bad situation */
	barf("Computer resigns.");
    else if (comstr[0] != FLAG) /* check for valid move; may be stalemate */
    {
	makemove(board, comstr, board->coord[comstr[1]]);
	UIBoardUpdate(board);
    }
}
