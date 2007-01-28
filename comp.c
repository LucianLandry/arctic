#include <stddef.h> /* NULL */
#include <string.h>
#include "ref.h"


int drawInsufficientMaterial(BoardT *board)
{
    int b1, b2;

    return
	/* K vs k */
	board->totalStrgh == 0 ||

	/* (KN or KB) vs k */
	(board->totalStrgh == 3 &&
	 board->playlist[PAWN].lgh + board->playlist[BPAWN].lgh == 0) ||

	/* KB vs kb, bishops on same color */
	(board->totalStrgh == 6 && board->playlist[BISHOP].lgh == 1 &&
	 board->playlist[BBISHOP].lgh == 1 && 
	 !((Rank(b1 = board->playlist[BISHOP].list[0]) + File(b1) +
	    Rank(b2 = board->playlist[BBISHOP].list[0]) + File(b2)) & 1));
}


/* With minor modification, we could also detect 1 repeat, but it would be
   more expensive. */
int drawThreefoldRepetition(BoardT *board)
{
    int repeats, ncpPlies, ply;

    /* 4th ply would be first possible repeat, 8th ply is 2nd and final
       repeat. */
    if (board->ncpPlies >= 8)
    {
	repeats = 0;
	for (ncpPlies = board->ncpPlies - 4, ply = board->ply - 4;
	     ncpPlies >= 4 || (repeats == 1 && ncpPlies >= 0);
	     ncpPlies -= 2, ply -= 2)
	{
	    if (board->zobrist == board->positions[ply & 127].zobrist &&
		memcmp(board->hashcoord,
		       board->positions[ply & 127].hashcoord,
		       34 /* 32 for hashcoord + cbyte + ebyte */) == 0 &&
		/* At this point we have a full match. */
		++repeats == 2)
	    {
		return 1;
	    }
	}
    }
    return 0;
}


/* Calculates (roughly) how 'valuable' a move is. */
int calcCapWorth(BoardT *board, uint8 *comstr)
{
    int cappiece = board->coord[comstr[1]];
    int capWorth = WORTH(cappiece);

    if (capWorth == -1) /* captured king, cannot happen */
    {
	UIMoveShow(board, comstr, "diagnostic");
    }

    if (comstr[2])
    {
	/* add in extra value for promotion or en passant. */
	capWorth += WORTH(comstr[2]);
    }

    return capWorth;
}


static int eval(BoardT *board)
{
    uint8 *coord = board->coord;
    int turn = board->ply & 1;
    int i, result;
    result = 0;
    for (i = 0; i < 64; i++)
	if (!CHECK(coord[i], turn))	/* friend */
	    result += WORTH(coord[i]);
	else if (CHECK(coord[i], turn) == 1) /* enemy */
	    result -= WORTH(coord[i]);
    return result;
}


#define QUIESCING(board) ((board)->depth > (board)->level)

static int minimax(BoardT *board, uint8 *comstr, int alpha,
		   int strgh, CompStatsT *stats, PvT *goodPv)
/* evaluates a given board position from {board->ply}'s point of view. */
{
    int bestVal, newval, i, besti, capWorth;
    PvT newPv;
    int oldval, turn, ncheck;
    UnMakeT unmake;
    MoveListT mvlist;

    goodPv->depth = 0;

    if (drawFiftyMove(board) ||
	drawInsufficientMaterial(board) ||
	drawThreefoldRepetition(board))
    {
	return 0;
    }

    /* I'm trying to use lazy initialization for this function. */
    turn = board->ply & 1;
    ncheck = board->ncheck[turn];

    /* When quiescing (ncheck is a special case because we attempt to detect
       checkmate even during quiesce) we assume that we can at least preserve
       our current strgh, by picking some theoretical move that wasn't
       generated.  This actually functions as our node-level evaluation
       function, cleverly hidden. */
    bestVal = QUIESCING(board) && ncheck == FLAG ? strgh : -1000;

    if (bestVal >= alpha) /* ie, will leave other side just as bad off
			     (if not worse) */
    {
	/* It is possible to hit this while quiescing.  If this is the case,
	   we would rather return before we even generate a movelist. */
	return bestVal;
    }

    mlistGenerate(&mvlist, board,
		  QUIESCING(board) && ncheck == FLAG);
    LogMoveList(eLogDebug, &mvlist);

    if (comstr && comstr[0] != FLAG)
    {
	/* incremental search.  Make sure the move from the previous search is
	   evaluated first. */
	mlistFirstMove(&mvlist, board, comstr);
    }

    stats->funcCallCount++;
    /* Note:  ncheck for any side guaranteed to be correct *only* after
       the other side has made its move. */
    if (!(mvlist.lgh))
    {
	return QUIESCING(board) || ncheck != FLAG ?
	    bestVal : /* checkmate, or nothing to quiesce */
	    0; /* stalemate */
    }

    if (QUIESCING(board))
    {
        /* once we know we're not mated, bestVal always == strgh. */
	if ((bestVal = strgh) >= alpha)
	{
	    return bestVal;
	}
	mlistSortByCap(&mvlist, board);
	LogMoveList(eLogDebug, &mvlist);
    }

    /* Save board position for later draw detection, if applicable. */
    /* I use 3 instead of 4 because the first quiesce depth may be a repeated
       position.
       Actually, in certain very rare cases, even the 2nd (or perhaps
       more?) quiesce depth might be a repeated position due to the way we
       handle check in quiesce, but I think that is not worth the computational
       cost of detecting. */
    if ((board->ncpPlies >= 4 /* possible 1 repeat */ &&
	 board->depth <= board->level - 3) ||
	board->depth <= board->level - (7 - board->ncpPlies))
    {
	SavePosition(board);
    }

    for (i = 0, besti = 0, oldval = -1000;
	 i < mvlist.lgh;
	 i++)
    {
	capWorth = calcCapWorth(board, mvlist.list[i]);

	if (board->depth >= board->level &&
	    capWorth + strgh <= bestVal &&
	    mvlist.list[i] [3] == FLAG)
	{
	    /* Last level, or quiescing;
	       The capture/promo/en passant is not good enough;
	       And there is no check.
	       So, this particular move will not improve things...
	       and we can skip it. */
	    if (QUIESCING(board) || i >= mvlist.insrt - 1)
	    {
		/* ... in this case, the other moves will not help either,
		   so... */
		break;
	    }
	    continue;
	}

	if (board->depth > board->level + 2 &&
	    capWorth + strgh <= board->qstrgh[turn] &&
	    mvlist.list[i] [3] == FLAG)
	{
	    /* quiescing, and we have already done so (on our turn) at least
	       once.  If we cannot improve over this side's previous bestVal,
	       this line is not worth investigating.  (We may miss a few
	       check(mate)s, but we need to keep the tree pruned.) */
	    break;
	}


	board->qstrgh[turn] = bestVal;

	LogMove(eLogDebug, board, mvlist.list[i]);
	SavePosition(board);
	makemove(board, mvlist.list[i], &unmake); /* switches sides */
	stats->moveCount++;

	strgh += capWorth;

	board->depth++;
	newval = -minimax(board, NULL, -bestVal, -strgh, stats, &newPv);
	board->depth--;

	if (newval >= bestVal)
	{
	    oldval = bestVal;  /* record 2ndbest val for history table. */
	    if (newval > bestVal)
	    {
		besti = i;
		bestVal = newval;

		if (board->depth < PV_DEPTH)
		{
		    /* this be a good move. */
		    memcpy(goodPv->pv, mvlist.list[i], 2);
		    goodPv->depth = MAX(newPv.depth, board->depth);

		    if (newPv.depth) /* copy the expected sequence. */
			memcpy(&goodPv->pv[2], newPv.pv, goodPv->depth << 1);

		    if (!board->depth) /* searching at root level, so let user
					  know the updated line. */
		    {
			UIPVDraw(goodPv, bestVal);
		    }
		}
	    }
	}

	/* restore the current board position. */
	unmakemove(board, mvlist.list[i], &unmake);
	strgh -= capWorth;

	if (bestVal >= alpha) /* ie, will leave other side just as bad off
				 (if not worse) */
	{
	    break;	/* why bother checking had bad the other moves are? */
	}
    }

    if (!QUIESCING(board) && bestVal > oldval)
    {
	/* move is at least one point better than others. */
	board->hist[turn]
	    [mvlist.list[besti] [0]] [mvlist.list[besti] [1]] = board->ply;
    }

    if (!board->depth) /* root level */
	memcpy(comstr, mvlist.list[besti], 4); /* copy off the best move */

    return bestVal;
}


static int doOptionalDraw(BoardT *board)
{
    /* Draw, if possible. */
    if (drawFiftyMove(board))
    {
	barf("Game is drawn (fifty-move rule).");
	return 1;
    }
    else if (drawThreefoldRepetition(board))
    {
	barf("Game is drawn (threefold repetition).");
	return 1;
    }
    return 0;
}


void computermove(BoardT *board)
{
    int value, strngth;
    CompStatsT stats = {0, 0};
    PvT pv;
    int turn = board->ply & 1;
    /* save off ncheck, minimax() clobbers it. */
    int ncheck = board->ncheck[turn];
    int savedLevel = board->level;
    int resigned = 0;

    uint8 comstr[4];

    comstr[0] = FLAG;
    board->depth = 0;	/* start search from root depth. */
    UINotifyThinking();

    if (doOptionalDraw(board))
    {
	return;
    }

    /* get starting strength, though it doesn't really matter */
    strngth = eval(board);

    for (board->level = 0; board->level <= savedLevel; board->level++)
    {
	/* perform incremental search. */
	value = minimax(board, comstr, 1000, strngth, &stats, &pv);
	board->ncheck[turn] = ncheck; /* restore ncheck */
	if (value < -500) /* we're in a really bad situation */
	{
	    resigned = 1;
	    break;
	}
    }
    board->level = savedLevel; /* restore board->level */

    UINotifyComputerStats(&stats);

    if (resigned)
    {
	barf("Computer resigns.");
    }
    else if (comstr[0] != FLAG) /* check for valid move; may be stalemate */
    {
	makemove(board, comstr, NULL);
	UIBoardUpdate(board);
	doOptionalDraw(board);
    }
}
