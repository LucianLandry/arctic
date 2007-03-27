/***************************************************************************
                     comp.c - computer 'AI' functionality.
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

#include <stddef.h> /* NULL */
#include <string.h>
#include <pthread.h> /* pthread_create() */
#include <assert.h>
#include "ref.h"


/* FIXME things which probably shouldn't be globals: gHash.
   And also maybe 'gThinker' ...
   but I'm trying to avoid passing around extra args to minimax(). */
static ThinkContextT *gThinker;

static inline int PositionHit(BoardT *board, PositionT *position)
{
    /* a simple memcmp would suffice, but the inline zobrist check
       is liable to be faster. */
    return board->zobrist == position->zobrist &&
	memcmp(board->hashcoord,
	       position->hashcoord,
	       sizeof(position->hashcoord)) == 0;
}


int drawInsufficientMaterial(BoardT *board)
{
    int b1, b2;

    return
	/* K vs k */
	board->totalStrgh == 0 ||

	/* (KN or KB) vs k */
	(board->totalStrgh == EVAL_KNIGHT &&
	 board->playlist[PAWN].lgh + board->playlist[BPAWN].lgh == 0) ||

	/* KB vs kb, bishops on same color */
	(board->totalStrgh == (EVAL_BISHOP << 1) &&
	 board->playlist[BISHOP].lgh == 1 &&
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
	    if (PositionHit(board, &board->positions[ply & 127]) &&
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
	LogMoveShow(eLogEmerg, board, comstr, "diagnostic");
	assert(0);
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
	if (CHECK(coord[i], turn) == FRIEND)
	    result += WORTH(coord[i]);
	else if (CHECK(coord[i], turn) == ENEMY)
	    result -= WORTH(coord[i]);
    return result;
}


/* assumes enemy is down to 1 king and we have no pawns. */
static int endGameEval(BoardT *board, int turn)
{
    int ekcoord = board->playlist[BKING ^ turn].list[0];
    int kcoord = board->playlist[KING | turn].list[0];

    return board->playerStrgh[turn] +
	/* enemy king needs to be as close to the corner as possible. */
	gPreCalc.centerDistance[ekcoord] * 14 /* max 84 */ +
	/* Failing any improvement in the above, we should also try to close in
	   w/our king. */
	(14 - gPreCalc.distance[kcoord] [ekcoord]); /* max 14 */
}


#define QUIESCING (searchDepth < 0)

static int minimax(BoardT *board, uint8 *comstr, int alpha,
		   int strgh, CompStatsT *stats, PvT *goodPv)
/* evaluates a given board position from {board->ply}'s point of view. */
{
    int bestVal, newval, i, besti, capWorth;
    int oldval, turn, ncheck;
    int hashHit;
    int mightDraw; /* bool.  is it possible to hit a draw while evaluating from
		      this position.  Could be made more intelligent. */
    int searchDepth;

    HashPositionT *hp;
    UnMakeT unmake;
    MoveListT mvlist;
    PvT newPv;

    /* I'm trying to use lazy initialization for this function. */
    goodPv->depth = 0;
    turn = board->ply & 1;

    if (drawInsufficientMaterial(board) ||
	drawFiftyMove(board) ||
	drawThreefoldRepetition(board))
    {
	/* Draw detected.
	   Skew the eval a bit: If we have better/equal material, try not to
	   draw.  Otherwise, try to draw. */
	return board->playerStrgh[turn] >= board->playerStrgh[turn ^ 1] ?
	    -1 : 0;
    }

    ncheck = board->ncheck[turn];
    searchDepth = board->level - board->depth;

    /* Putting some endgame eval right here (no captures
       possible in anything-else vs k (unless pawns present), so movgen not
       needed).  Also (currently) don't bother with hashing since usually
       ncpPlies will be too high.
    */
    if (QUIESCING && ncheck == FLAG)
    {
	if (board->playerStrgh[turn] == 0 &&
	    board->playlist[BPAWN ^ turn].lgh == 0)
	{
	    return -endGameEval(board, turn ^ 1); /* (oh crap.) */
	}
	if (board->playerStrgh[turn ^ 1] == 0 &&
	    board->playlist[PAWN | turn].lgh == 0)
	{
	    return endGameEval(board, turn); /* (oh good.) */
	}
    }

    /* When quiescing (ncheck is a special case because we attempt to detect
       checkmate even during quiesce) we assume that we can at least preserve
       our current strgh, by picking some theoretical move that wasn't
       generated.  This actually functions as our node-level evaluation
       function, cleverly hidden. */
    bestVal = QUIESCING && ncheck == FLAG ? strgh : -EVAL_CHECKMATE;
    if (bestVal >= alpha) /* ie, will leave other side just as bad off
			     (if not worse) */
    {
	/* It is possible to hit this while quiescing.  If this is the case,
	   we would rather return before we even generate a movelist. */
	return bestVal;
    }

    /* Is it possible to draw by repetition from this position.
       I use 3 instead of 4 because the first quiesce depth may be a repeated
       position.
       Actually, in certain very rare cases, even the 2nd (or perhaps
       more?) quiesce depth might be a repeated position due to the way we
       handle check in quiesce, but I think that is not worth the computational
       cost of detecting. */
    mightDraw = 
	(board->ncpPlies >= 4 /* possible repeat */ && searchDepth >= 3) ||
	searchDepth >= 7 - board->ncpPlies;
    hashHit = 0;
    hp = NULL;

    /* Is there a suitable hit in the transposition table? */
    if (!mightDraw && gHash != NULL)
    {
	hp = &gHash[board->zobrist & gPreCalc.hashMask];
	if ((hashHit = PositionHit(board, &hp->p)) &&
	    (searchDepth <= hp->depth /* not too shallow */ ||
	     (QUIESCING && hp->depth != HASH_NOENTRY)) &&
	    (hp->alpha >= alpha || hp->eval >= alpha))
	{
	    stats->hashHitGood++;
	    if (comstr != NULL)
		/* suggest a move. */
		memcpy(comstr, hp->comstr, sizeof(hp->comstr));
	    /* record base ply for this move. */
	    hp->ply = board->ply - board->depth;
	    return hp->eval;
	}   
    }

    mlistGenerate(&mvlist, board, QUIESCING && ncheck == FLAG);
    LogMoveList(eLogDebug, &mvlist);

    if (comstr && comstr[0] != FLAG)
    {
	/* incremental search.  Make sure the move from the previous search is
	   evaluated first. */
	mlistFirstMove(&mvlist, board, comstr);
    }

    stats->funcCallCount++;
    besti = -1;

    /* Note:  ncheck for any side guaranteed to be correct *only* after
       the other side has made its move. */
    if (!(mvlist.lgh))
    {
	if (!QUIESCING && ncheck == FLAG)
	    /* bestVal is currently checkmate, or nothing to quiesce.  In this
	       case we must alter it to 'stalemate'. */
	{
	    bestVal = 0;
	}
	goto out;
    }

    if (QUIESCING)
    {
        /* once we know we're not mated, bestVal always == strgh. */
	if ((bestVal = strgh) >= alpha)
	{
	    goto out;
	}
	mlistSortByCap(&mvlist, board);
	LogMoveList(eLogDebug, &mvlist);
    }

    /* Save board position for later draw detection, if applicable. */
    if (mightDraw)
    {
	PositionSave(board);
    }

    for (i = 0, besti = 0, oldval = -EVAL_CHECKMATE;
	 i < mvlist.lgh;
	 i++)
    {
	capWorth = calcCapWorth(board, mvlist.list[i]);

	if (searchDepth <= 0 &&
	    capWorth + strgh <= bestVal &&
	    mvlist.list[i] [3] == FLAG)
	{
	    /* Last level, or quiescing;
	       The capture/promo/en passant is not good enough;
	       And there is no check.
	       So, this particular move will not improve things...
	       and we can skip it. */
	    if (QUIESCING || i >= mvlist.insrt - 1)
	    {
		/* ... in this case, the other moves will not help either,
		   so... */
		break;
	    }
	    continue;
	}

	if (searchDepth < -2 &&
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
	makemove(board, mvlist.list[i], &unmake); /* switches sides */
	stats->moveCount++;

	strgh += capWorth;

	board->depth++;
	newval = -minimax(board, NULL, -bestVal, -strgh, stats, &newPv);
	board->depth--;

	/* restore the current board position. */
	unmakemove(board, mvlist.list[i], &unmake);
	strgh -= capWorth;

	/* Note: if we need to move, we cannot trust 'newval'.  We must
	   go with the best value/move we already had. */
	if (ThinkerCompNeedsToMove(gThinker))
	{
	    if (comstr != NULL) /* root level */
	    {
		/* copy off the best move */
		memcpy(comstr, mvlist.list[besti], 4);
	    }
	    return bestVal;
	}

	if (newval >= bestVal)
	{
	    oldval = bestVal;  /* record 2ndbest val for history table. */
	    if (newval > bestVal)
	    {
		besti = i;
		bestVal = newval;

		if (board->depth < MAX_PV_DEPTH)
		{
		    /* this be a good move. */
		    memcpy(goodPv->pv, mvlist.list[i], 2);
		    goodPv->depth = MAX(newPv.depth, board->depth);

		    if (newPv.depth) /* copy the expected sequence. */
			memcpy(&goodPv->pv[2], newPv.pv, goodPv->depth << 1);

		    if (comstr) /* searching at root level, so let user know
				   the updated line. */
		    {
			goodPv->eval = bestVal;
			goodPv->level = board->level;
			ThinkerCompNotifyPv(gThinker, goodPv);
		    }
		}

		if (bestVal >= alpha) /* ie, will leave other side just as bad
					 off (if not worse) */
		{
		    break;	      /* why bother checking had bad the other
					 moves are? */
		}
	    }
	}
    }

    if (!QUIESCING && bestVal > oldval)
    {
	/* move is at least one point better than others. */
	board->hist[turn]
	    [mvlist.list[besti] [0]] [mvlist.list[besti] [1]] = board->ply;
    }

    if (comstr != NULL) /* root level */
	memcpy(comstr, mvlist.list[besti], 4); /* copy off the best move */

out:
    /* Do we need to replace the transposition table entry? */
    if (!mightDraw && gHash != NULL)
    {
	if (searchDepth > hp->depth ||
	    /* Replacing entries that came before this search is pretty
	       aggressive, but it works better than a 'numPieces' comparison.
	    */
	    hp->ply < board->ply - board->depth ||
	    /* I tried alpha >= hp->alpha on the theory that replacing more
	       often would give better locality, but the results are
	       inconclusive (to say the least). */
	    (searchDepth == hp->depth && alpha > hp->alpha))
	{
	    /* Yes.  Update transposition table.
	       (Every single element of this structure should be updated, since
	       it is not blanked for a newgame.)
	    */
	    if (!hashHit)
	    {
		memcpy(&hp->p, &board->zobrist, sizeof(PositionT));
	    }
	    hp->alpha = alpha;
	    hp->depth = searchDepth;
	    hp->eval = bestVal;
	    hp->ply = board->ply - board->depth;

	    /* copy off best move. */
	    if (besti >= 0)
	    {
		memcpy(hp->comstr, mvlist.list[besti], 4);
	    }
	    else
		hp->comstr[0] = FLAG; /* no possible move. */
	}
    }

    return bestVal;
}


static int canDraw(BoardT *board)
{
    return drawFiftyMove(board) || drawThreefoldRepetition(board);
}


static void computermove(BoardT *board, ThinkContextT *th)
{
    int value, strngth;
    CompStatsT stats = {0, 0, 0};
    PvT pv;
    int turn = board->ply & 1;
    /* save off ncheck, minimax() clobbers it. */
    int ncheck = board->ncheck[turn];
    int resigned = 0;
    MoveListT mvlist;
    UnMakeT unmake;
    uint8 comstr[4];
    int bWillDraw = 0;

    comstr[0] = FLAG;
    board->depth = 0;	/* start search from root depth. */

    /* If we can draw, do so w/out thinking. */
    if (canDraw(board))
    {
	ThinkerCompDraw(th, comstr);
	return;
    }

    mlistGenerate(&mvlist, board, 0);

    if (mvlist.lgh == 1)
    {
	/* only one move to make -- don't think about it. */
	memcpy(comstr, &mvlist.list[0], 4);
    }
    else
    {
	/* get starting strength, though it doesn't really matter */
	strngth = eval(board);

	for (board->level = 0; board->level <= board->maxLevel; board->level++)
	{
	    value = minimax(board, comstr, EVAL_CHECKMATE, strngth, &stats,
			    &pv);
	    board->ncheck[turn] = ncheck; /* restore ncheck */

	    if (ThinkerCompNeedsToMove(th))
	    {
		/* We're guaranteed to have at least one move available, so
		   use it. */
		break;
	    }
	    else if (value <= -EVAL_CHECKMATE)
	    {
		/* we're in a really bad situation */
		resigned = 1;
		break;
	    }
	}
	board->level = 0; /* reset board->level */
    }

    ThinkerCompNotifyStats(th, &stats);

    if (resigned)
    {
	ThinkerCompResign(th);
	return;
    }

    /* If we can draw after this move, do so. */
    makemove(board, comstr, &unmake);
    bWillDraw = canDraw(board);
    unmakemove(board, comstr, &unmake);    
    board->ncheck[turn] = ncheck; /* restore ncheck */ 

    if (bWillDraw)
    {
	ThinkerCompDraw(th, comstr);
    }
    else
    {
	ThinkerCompMove(th, comstr);
    }
}


typedef struct {
    sem_t *mySem;
    BoardT *board;
    ThinkContextT *th;
} CompArgsT;


static void *compThread(CompArgsT *args)
{
    CompArgsT myArgs = *args;

    LOG_DEBUG("compThread: created\n");
    sem_post(args->mySem);

    gThinker = myArgs.th;
    while(1)
    {
	/* wait for a think-command to come in. */
	ThinkerCompWaitThink(myArgs.th);
	/* Ponder on it, and recommend either: a move, draw, or resign. */
	computermove(myArgs.board, myArgs.th);
    }
}


void compThreadInit(BoardT *board, ThinkContextT *th)
{
    int err;
    sem_t mySem;
    CompArgsT args = {&mySem, board, th};
    pthread_t myThread;

    sem_init(&mySem, 0, 0);
    LOG_DEBUG("compThreadInit: creating\n");
    err = pthread_create(&myThread, NULL, (PTHREAD_FUNC) compThread, &args);
    assert(err == 0);
    sem_wait(&mySem);
    sem_destroy(&mySem);
}
