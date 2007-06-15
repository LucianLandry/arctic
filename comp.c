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
#include <errno.h>
#include <stdlib.h> // exit()
#include "ref.h"


/* Since I spent a lot of time trying to do it, here is a treatise on why
   one cannot shrink the search window to the bounds of the hash evaluation.

   For starters, it is not valid to dec 'beta'.  For an orig window
   "             alpha                        beta"
   We don't want to return:
   "                            hashhighbound          EVAL_CHECKMATE"
   ... that is all well and good, but also not enough information.
   We could attempt to compensate for that by using the hashed move
   eval, but it is not necessarily the best move,
   "hashlowbound                hashhighbound"
   and so cannot satisfy our requirements either!
   
   For similar reasons, we cannot inc alpha when hashed-beta > beta.

   Also,
   If the lowbound is in the window, and I use the hasheval for the
   hashmove, (and I cannot.  I need an exact eval in that case!) there
   is still nothing guaranteeing that our evals will line up w/in the
   window (even at the same searchdepth, because re-searches can hit
   hashes of deeper search depth, which change the evaluation).  So
   our hash 'window' is at best an educated guess.  We could use that
   for a kind of PVS, but we do not implement that.
*/

/* Null window note: currently, a move which is "as good" as the null window
   could fail either high or low (which way is undefined). */

/* Hashing note: if you see something like (where we play white):
   223426 <first : 4 ...
   224562 <first : 5 0 0 0 Ra2 dxc4 Qxc4 Qc8 axb7 Qxb7 Rxa8.
   230517 <first : move a1a2
   ...
   236631 <second: 16. ... d5c4
   ...
   236786 <first : 0 -100 0 0 Qxc4.
   ...
   236787 <first : 4 -100 0 0 Qxc4.
   237425 <first : 5 -100 0 0 Qxc4 Nxg3 hxg3 Rc8 axb7 Rxc4 Bxc4.

   Don't panic.  What this means is, we started the depth6 search on Ra2
   and the Qxc4 branch was found wanting (because it was searched deeper).
   So, that was hashed.  But we did not complete the Ra2 evaluation before
   time expired.
*/


/* FIXME 'gThinker' should maybe not be a global ...
   but I'm trying to avoid passing around extra args to updatePv(). */
static ThinkContextT *gThinker;

// Forward declarations.
static EvalT minimax(BoardT *board, int alpha, int beta,
		     int strgh, CompStatsT *stats, PvT *goodPv,
		     ThinkContextT *th);

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
	    if (PositionHit(board, &board->positions[ply & 127].p) &&
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

    if (capWorth == EVAL_KING) /* captured king, cannot happen */
    {
	LogMoveShow(eLogEmerg, board, comstr, "diagnostic");
	assert(0);
    }

    if (comstr[2])
    {
	/* add in extra value for promotion or en passant. */
	capWorth += WORTH(comstr[2]);
	if (!ISPAWN(comstr[2]))
	    capWorth -= EVAL_PAWN;
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


/*
  'alpha' is the lowbound for the search (any move must be at least this good).
  It is also roughly equivalent to 'bestVal' (for any move so far), except
  perhaps it is higher than any of them.

  'beta' is the highbound for the search (if we find a move at least this
  good, we don't need to worry about searching the rest of the moves).

  'lowBound' and 'highBound' are the possible limits of the best 'move' found
  so far.

  I can take care of 'optimization' later.
*/


static inline EvalT invertEval(EvalT eval)
{
    return (EvalT) {-eval.highBound, -eval.lowBound};
}

#define SET_BOUND(low, high) \
do { retVal.lowBound = (low); retVal.highBound = (high); } while (0)

#define BUMP_BOUND_TO(eval) \
do { if (retVal.lowBound < (eval).lowBound) \
         retVal.lowBound = (eval).lowBound; \
     if (retVal.highBound < (eval).highBound) \
         retVal.highBound = (eval).highBound; } while (0)

#define RETURN_BOUND(low, high) \
do { return (EvalT) {(low), (high)}; } while (0)


void updatePv(BoardT *board, PvT *goodPv, PvT *childPv, uint8 *comstr,
	      int eval)
{
    int depth = board->depth;

    if (depth < MAX_PV_DEPTH && comstr[0] != FLAG)
    {
	/* this be a good move. */
	memcpy(goodPv->pv, comstr, 4);

	if (childPv && childPv->depth)
	{
	    goodPv->depth = childPv->depth;
	    memcpy(&goodPv->pv[4], childPv->pv, (goodPv->depth - depth) << 2);
	}
	else
	{
	    goodPv->depth = depth;
	}

	if (depth == 0)
	{
	    /* searching at root level, so let user know the updated
	       line. */
	    goodPv->eval = eval;
	    goodPv->level = board->level;
	    ThinkerRspNotifyPv(gThinker, goodPv);
	}
    }
    else
    {
	goodPv->depth = 0;
    }
}

static EvalT tryMove(BoardT *board, uint8 *comstr, CompStatsT *stats,
		     int newStrgh, int alpha, int beta, PvT *newPv,
		     ThinkContextT *th)
{
    UnMakeT unmake;
    EvalT myEval;

    LogMove(eLogDebug, board, comstr);
    LogSetLevel(eLogEmerg);
    makemove(board, comstr, &unmake); /* switches sides */
    stats->moveCount++;

    board->depth++;

    myEval = invertEval(minimax(board, -beta, -alpha,
				-newStrgh, stats, newPv, th));
    board->depth--;

    /* restore the current board position. */
    unmakemove(board, comstr, &unmake);

    LOG_DEBUG("eval: %d %d %d %d\n",
	      alpha, myEval.lowBound, myEval.highBound, beta);
    return myEval;
}


// returns index of completed searcher.
static int compWaitOne(void)
{
    int res;
    int i;
    eThinkMsgT rsp;

    while ((res = poll(gVars.pfds, gPreCalc.numProcs, -1)) == -1
	   && errno == EINTR)
    {
	continue;
    }
    assert(res > 0); // other errors should not happen
    for (i = 0; i < gPreCalc.numProcs; i++)
    {
	assert(!(gVars.pfds[i].revents & (POLLERR | POLLHUP | POLLNVAL)));
	if (gVars.pfds[i].revents & POLLIN)
	{
	    // Received response from slave.
	    rsp = ThinkerRecvRsp(&gVars.searchArgs[i].th, NULL, 0);
	    assert(rsp == eRspSearchDone);
	    gVars.numThinking--;
	    return i;
	}
    }
    assert(0);
    return -1;
}


static void compWaitAll(void)
{
    while (gVars.numThinking > 0)
    {
	compWaitOne();
    }
}


static SearchArgsT *searcherGet(void)
{
    int i;
    for (i = 0; i < gPreCalc.numProcs; i++)
    {
	if (!ThinkerCompIsThinking(&gVars.searchArgs[i].th))
	{
	    return &gVars.searchArgs[i];
	}
    }
    assert(0);
    return NULL;
}


static void searchersMakeMove(uint8 *comstr, UnMakeT *unmake, int mightDraw)
{
    int i;
    SearchArgsT *sa;
    for (i = 0; i < gPreCalc.numProcs; i++)
    {
	sa = &gVars.searchArgs[i];
	if (mightDraw)
	{
	    PositionSave(&sa->board);
	}
	makemove(&sa->board, comstr, unmake);
	sa->board.depth++;
    }
}


static void searchersUnmakeMove(uint8 *comstr, UnMakeT *unmake)
{
    int i;
    SearchArgsT *sa;
    for (i = 0; i < gPreCalc.numProcs; i++)
    {
	sa = &gVars.searchArgs[i];
	sa->board.depth--;
	unmakemove(&sa->board, comstr, unmake);
    }
}


static void searcherSearch(uint8 *comstr, int alpha, int beta, int strgh)
{
    // Delegate a move.
    SearchArgsT *sa = searcherGet();
    sa->strgh = strgh;
    sa->alpha = alpha;
    sa->beta = beta;
    memcpy(sa->comstr, comstr, 4);
    ThinkerCmdThink(&sa->th);
    gVars.numThinking++;
}


/* Note: the simple lock/unlock of the hashLock is good for about an 8%
   slowdown (FIXME: measure this better, also measure error)  So it is to our
   advantage to check for SMP before actually doing it. ... although on second
   thought, that has a measurable slowdown of its own, so I will optimize for
   the multithread case.
*/
static EvalT minimax(BoardT *board, int alpha, int beta,
		     int strgh, CompStatsT *stats, PvT *goodPv,
		     ThinkContextT *th)
/* evaluates a given board position from {board->ply}'s point of view. */
{
    int newval, i, capWorth;
    int secondBestVal, turn, ncheck;
    int mightDraw; /* bool.  is it possible to hit a draw while evaluating from
		      this position.  Could be made more intelligent. */
    /* alpha (the low search window) can be ... anything.  So if we want a
       more accurate evaluation, we need to track 'bestVal' separately.
    */
    int bestVal;
    int searchDepth;

    EvalT retVal, myEval;

    HashPositionT *hp;
    MoveListT mvlist;
    PvT newPv;

    uint8 *bestComstr, *comstr;

    int hashHit = 0;
    uint8 hashComstr[4];
    EvalT hashEval;
    uint8 hashDepth;
    pthread_mutex_t *hashLock;

    int masterNode;  // multithread support.
    int searchIndex;
    UnMakeT unmake;

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
	bestVal = board->playerStrgh[turn] >= board->playerStrgh[turn ^ 1] ?
	    -1 : 0;
	RETURN_BOUND(bestVal, bestVal);
    }

    ncheck = board->ncheck[turn];
    searchDepth = board->level - board->depth;

    if (QUIESCING && ncheck == FLAG)
    {
	/* Putting some endgame eval right here (no captures
	   possible in anything-else vs k (unless pawns present), so movgen not
	   needed).  Also (currently) don't bother with hashing since usually
	   ncpPlies will be too high.
	*/
	if (board->playerStrgh[turn] == 0 &&
	    board->playlist[BPAWN ^ turn].lgh == 0)
	{
	    bestVal = -endGameEval(board, turn ^ 1); /* (oh crap.) */
	    RETURN_BOUND(bestVal, bestVal);
	}
	if (board->playerStrgh[turn ^ 1] == 0 &&
	    board->playlist[PAWN | turn].lgh == 0)
	{
	    bestVal = endGameEval(board, turn); /* (oh good.) */
	    RETURN_BOUND(bestVal, bestVal);
	}

	/* When quiescing (ncheck is a special case because we attempt to
	   detect checkmate even during quiesce) we assume that we can at least
	   preserve our current strgh, by picking some theoretical move that
	   wasn't generated.  This actually functions as our node-level
	   evaluation function, cleverly hidden. */
	if (strgh >= beta)
	{
	    RETURN_BOUND(strgh, EVAL_CHECKMATE);
	}
    }

    /* Is it possible to draw by repetition from this position.
       I use 3 instead of 4 because the first quiesce depth may be a repeated
       position.
       Actually, in certain very rare cases, even the 2nd (or perhaps
       more?) quiesce depth might be a repeated position due to the way we
       handle check in quiesce, but I think that is not worth the computational
       cost of detecting. */
    // mightDraw = (searchDepth >= 7 - board->ncpPlies); original.
    mightDraw =
	board->repeatPly == -1 ? /* no repeats upto this position */
	(searchDepth >= MAX(5, 7 - board->ncpPlies)) :
	/* (7 - ncpPlies below would work, but this should be better:) */
	(searchDepth >= 3 - (board->ply - board->repeatPly));

    hp = NULL;
    hashEval.lowBound = hashEval.highBound = 0;
    hashDepth = HASH_NOENTRY;
    hashLock = NULL;

    /* Is there a suitable hit in the transposition table? */
    if ((!mightDraw || board->ncpPlies == 0) && gVars.hash != NULL)
    {
	// maybe ...
	hp = &gVars.hash[board->zobrist & gPreCalc.hashMask];
	hashLock = &gVars.hashLocks[board->zobrist & (NUM_HASH_LOCKS - 1)];

	/* (hp must be locked when it is read or written to.) */
	pthread_mutex_lock(hashLock);

	if (PositionHit(board, &hp->p) && hp->depth != HASH_NOENTRY)
	{
	    hashHit = (searchDepth <= hp->depth || QUIESCING);
	    hashEval = hp->eval;
	    memcpy(hashComstr, hp->comstr, 4);
	    hashDepth = hp->depth;
	
	    if ((hashHit &&
		 /* know eval exactly? */
		 (hashEval.highBound == hashEval.lowBound ||
		  /* know it's good enough? */
		  hashEval.lowBound >= beta ||
		  /* know it's bad enough? */
		  hashEval.highBound <= alpha)) ||

		/* -checkmate or checkmate is good, regardless of depth. */
		hashEval.highBound == -EVAL_CHECKMATE ||
		hashEval.lowBound == EVAL_CHECKMATE)
	    {
		stats->hashHitGood++;

		/* re-record base ply for this move. */
		hp->ply = board->ply - board->depth;
		/* in case of checkmate.  Not proven to be better. */
		hp->depth = MAX(hp->depth, searchDepth);

		LOG_DEBUG("hashHit alhb: %d %d %d %d %d 0x%x\n",
			  alpha, hp->eval.lowBound, hp->eval.highBound, beta,
			  hp->depth, hp->p.zobrist);

		pthread_mutex_unlock(hashLock);

		/* record the move (if there is one). */
		updatePv(board, goodPv, NULL, hashComstr, hashEval.lowBound);
		return hashEval;
	    }
	}
	pthread_mutex_unlock(hashLock);
    }

    mlistGenerate(&mvlist, board, QUIESCING && ncheck == FLAG);
    LogMoveList(eLogDebug, &mvlist);

    stats->funcCallCount++;

    /* bestComstr must be initialized before we goto out. */
    bestComstr = NULL;

    /* Note:  ncheck for any side guaranteed to be correct *only* after
       the other side has made its move. */
    if (!(mvlist.lgh))
    {
	bestVal =
	    ncheck != FLAG ? -EVAL_CHECKMATE : /* checkmate detected */
	    !QUIESCING ? 0 : /* stalemate detected */
	    strgh;
	SET_BOUND(bestVal, bestVal);
	goto out;
    }

    /* This doesn't work well, perhaps poor interaction w/history table. */
    /* mlistSortByCap(&mvlist, board); */

    if (board->depth == 0 && goodPv->pv[0] != FLAG)
    {
	/* root node, incremental search.  Make sure the move from the previous
	   search is evaluated first. */
	mlistFirstMove(&mvlist, board, goodPv->pv);
    }

    bestVal = -EVAL_CHECKMATE;
    if (QUIESCING)
    {
        /* once we know we're not mated, alpha always >= strgh. */
	if (strgh >= beta)
	{
	    SET_BOUND(strgh, EVAL_CHECKMATE);
	    goto out;
	}

	bestVal = strgh;
	alpha = MAX(bestVal, alpha);
	mlistSortByCap(&mvlist, board);
	LogMoveList(eLogDebug, &mvlist);
    }

    /* Save board position for later draw detection, if applicable. */
    if (mightDraw)
    {
	PositionSave(board);
    }

    /* If there are no better moves, 'bestVal' is really the best we can
       do. */
    SET_BOUND(bestVal, bestVal);

#if 1
    masterNode = (th == gThinker && // master node
		  searchDepth > 0); // not subject to futility pruning
#else
    masterNode = 0;
#endif

    comstr = NULL;
    capWorth = EVAL_KING;

    /* (It is harmless to set bestComstr = (first move) when quiescing, and
       necessary when not.) */
    for (i = 0, bestComstr = mvlist.list[0], secondBestVal = alpha;
	 i < mvlist.lgh || (masterNode && gVars.numThinking > 0);
	 i++)
    {
	assert(i <= mvlist.lgh);
	if (i < mvlist.lgh)
	{
	    comstr = mvlist.list[i];
	    capWorth = calcCapWorth(board, comstr);
	}

	/* can we delegate a move? */
	if (masterNode)
	{
	    if (i == 0)
	    {
		LOG_DEBUG("bldbg: comp1\n");
		// First move is special (for PV).  We process it (almost)
		// normally.
		searchersMakeMove(comstr, &unmake, mightDraw);
		myEval = tryMove(board, comstr, stats, capWorth + strgh,
				 alpha, beta, &newPv, th);
		searchersUnmakeMove(comstr, &unmake);
	    }
	    else if (i < mvlist.lgh &&  // have a move to search?
		     // have someone to delegate it to?
		     gVars.numThinking < gPreCalc.numProcs)
	    {
		LOG_DEBUG("bldbg: comp2\n");
		// Yes.  Delegate this move.
		searcherSearch(comstr, alpha, beta, strgh);
		continue;
	    }
	    else 
	    {
		LOG_DEBUG("bldbg: comp3\n");
		// Either do not have a move to search, or
		// nobody to search on it.  Wait for an eval to become
		// available.
		searchIndex = compWaitOne();
		LOG_DEBUG("bldbg: comp3.5\n");
		myEval = gVars.searchArgs[searchIndex].eval;
		comstr = gVars.searchArgs[searchIndex].comstr;
		// yuck?  I do not have to copy the whole thing; just
		// 'pv->depth << 2' worth of pv.pv should do.  See updatePv().
		memcpy(&newPv, &gVars.searchArgs[searchIndex].pv, sizeof(PvT));
		i--; // this counters i++
	    }
	}
	else
	{
	    // Normal search.
	    if ((QUIESCING || (searchDepth == 0 && !mightDraw)) &&
		capWorth + strgh <= alpha &&
		comstr[3] == FLAG)
	    {
		/* Last level + no possibility to draw, or quiescing;
		   The capture/promo/en passant is not good enough;
		   And there is no check.
		   So, this particular move will not improve things...
		   and we can skip it. */

		/* (however, we do need to bump the highbound.  Otherwise, a
		   depth-0 position can be mistakenly evaluated as +checkmate.)
		*/
		retVal.highBound = MAX(retVal.highBound, capWorth + strgh);

		/* *if* alpha > origalpha, we found a bestVal > origalpha.  In
		   this case, we want to set an exact value for this.  However,
		   an alpha < bestVal < beta should actually have an exactval
		   already.
		*/

		if (i >= mvlist.insrt - 1)
		{
		    /* ... in this case, the other moves will not help either,
		       so... */
		    break;
		}
		continue;
	    }

	    myEval = tryMove(board, comstr, stats, capWorth + strgh,
			     alpha, beta, &newPv, th);
	}

        /* Note: If we need to move, we cannot trust 'myEval'.  We must
	   go with the best value/move we already had. */
	if (ThinkerCompNeedsToMove(gThinker))
	{
	    if (masterNode)
	    {
		// Wait for any searchers to terminate.
		compWaitAll();
	    }
	    RETURN_BOUND(retVal.lowBound, EVAL_CHECKMATE);
	}

	BUMP_BOUND_TO(myEval);

	newval = myEval.lowBound;
	if (newval >= alpha)
	{
	    /* This doesn't practically turn the history table off,
	       because most moves should fail w/{-EVAL_CHECKMATE, alpha}. */
	    secondBestVal = alpha;  /* record 2ndbest val for history table. */
	}

	if (newval > alpha)
	{
	    bestComstr = comstr;
	    bestVal = alpha = newval;

	    updatePv(board, goodPv, &newPv, bestComstr, bestVal);

	    if (bestVal >= beta) /* ie, will leave other side just as bad
				     off (if not worse) */
	    {
		if (masterNode && gVars.numThinking > 0)
		{
		    compWaitAll();
		    retVal.highBound = EVAL_CHECKMATE;
		}
		else if (i != mvlist.lgh - 1)
		    retVal.highBound = EVAL_CHECKMATE;
		break;	      /* why bother checking how bad the other
				 moves are? */
	    }
	    else if (myEval.lowBound != myEval.highBound)
	    {
		/* alpha < lowbound < beta needs an exact evaluation. */
		LOG_EMERG("alhb: %d %d %d %d\n",
			  alpha, myEval.lowBound, myEval.highBound, beta);
		assert(0);
	    }
	}
	else if (newval > bestVal)
	{
	    /* The move sucks, but it's better than what we have now. */
	    bestComstr = comstr;
	    bestVal = newval;
	}
    }


    if (!QUIESCING && bestVal > secondBestVal
	// Do not add moves that will automatically be preferred -- picked this
	// up from a chess alg site.  It does seem to help our speed
	// (slightly).
	&& bestComstr[2] == 0 && board->coord[bestComstr[1]] == 0)
    {
	/* move is at least one point better than others. */
	gVars.hist[turn]
	    [bestComstr[0]] [bestComstr[1]] = board->ply;
    }

out:
#if 0
    /* (I take this out because with the revised algorithm, we are currently
       affecting maybe .002% (extreme max) of our evaluations while losing
       about 2% in calculating speed.  In short it is not worth it.)

       Perhaps when we get more fine-grained evaluation, we can try this again.
    */
    if (hashHit &&
	hashDepth > (QUIESCING ? -1 : searchDepth) &&
	/*
	  Does not work (even with < instead of <=).  In a nutshell, it is
	  more of a crime to return a possibly-complete-garbage move (due to
	  the nature of alpha-beta) as an exact value than it is to return
	  a move that we know is a little bit off (at least in the case of
	  "<").  These arguments apply to >=, > as well but at one level closer
	  to the root.

	  Note: Not a real problem, but this original code can also result in
	  us appearing to discard a perfectly good move in the PV for another
	  one of the same value (especially at level zero).  This is because we
	  know from the hash that the original move is (at best, with <=) only
	  as good as the more deeply-evaluated move.
	  (hashEval.highBound <= retVal.lowBound ||
	   hashEval.lowBound >= retVal.highBound))
	*/
	bestComstr && !memcmp(bestComstr, hashComstr, 4) &&
	(hashEval.highBound < retVal.lowBound ||
	 hashEval.lowBound > retVal.highBound))
    {
	/* If the hash eval is out of our range, at a greater
	   depth, our current evaluation is clearly "wrong".  Because we
	   must return an exact eval (not a range), the new eval will also be
	   "wrong", but chances are it will be less so.
	*/
	if (hashEval.highBound <= retVal.lowBound)
	{
	    SET_BOUND(hashEval.highBound, hashEval.highBound);
	}
	else
	{
	    SET_BOUND(hashEval.lowBound, hashEval.lowBound);
	}

	stats->hashHitPartial++;

	/* bestComstr = hashComstr; not needed since same move */

	/* record the move (if there is one). */
	updatePv(board, goodPv, NULL, hashComstr, hashEval.lowBound);
    }
#endif

    /* Do we need to replace the transposition table entry? */
    if (hp != NULL)
    {
	pthread_mutex_lock(hashLock);

        /* (HASH_NOENTRY should always trigger here) */
	if (searchDepth > hp->depth ||
	    /* Replacing entries that came before this search is
	       aggressive, but it works better than a 'numPieces' comparison.
	    */
	    hp->ply < board->ply - board->depth ||
	    /* Otherwise, use the position that gives us as much info as
	       possible, and after that the most recently used (ie this move).
	    */
	    (searchDepth == hp->depth &&
	     (retVal.highBound - retVal.lowBound) <=
	     (hp->eval.highBound - hp->eval.lowBound)))
	{
	    /* Yes.  Update transposition table.
	       Every single element of this structure should always be updated,
	       since:
	       -- it is not blanked for a newgame
	       -- even if hashHit, the hash entry might have been overwritten in
	       the meantime (because of the ply check, or because another
	       thread clobbered the table). */
	    memcpy(&hp->p, &board->zobrist, sizeof(PositionT));

	    hp->eval = retVal; /* struct copy */
	    hp->depth = searchDepth;
	    hp->ply = board->ply - board->depth;

	    /* copy off best move. */
	    if (bestComstr != NULL)
	    {
		memcpy(hp->comstr, bestComstr, 4);
	    }
	    else
		hp->comstr[0] = FLAG; /* no associated move. */
	}

	pthread_mutex_unlock(hashLock);
    }

    return retVal;
}


static int canDraw(BoardT *board)
{
    return drawFiftyMove(board) || drawThreefoldRepetition(board);
}


static void computermove(BoardT *board, ThinkContextT *th)
{
    int i, strngth;
    EvalT myEval;
    CompStatsT stats = {0, 0, 0, 0, 0};
    PvT pv;
    int turn = board->ply & 1;
    /* save off ncheck, minimax() clobbers it. */
    int ncheck = board->ncheck[turn];
    int resigned = 0;
    MoveListT mvlist;
    UnMakeT unmake;
    uint8 *comstr = pv.pv;
    int bWillDraw = 0;

    comstr[0] = FLAG;
    board->depth = 0;	/* start search from root depth. */

    /* If we can draw, do so w/out thinking. */
    if (canDraw(board))
    {
	ThinkerRspDraw(th, comstr);
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
	if (gVars.randomMoves)
	{
	    BoardRandomize(board);
	}

	/* get starting strength, though it doesn't really matter */
	strngth = eval(board);

	/* setup known search parameters across the slaves. */
	for (i = 0; i < gPreCalc.numProcs; i++)
	{
	    BoardCopy(&gVars.searchArgs[i].board, board);
	    gVars.searchArgs[i].stats = &stats;
	    gVars.searchArgs[i].strgh = strngth;
	}

	for (board->level = 0; board->level <= gVars.maxLevel; board->level++)
	{
	    /* setup known search parameters across the slaves. */
	    for (i = 0; i < gPreCalc.numProcs; i++)
	    {
		gVars.searchArgs[i].board.depth = board->depth;
		gVars.searchArgs[i].board.level = board->level;
	    }

	    LOG_DEBUG("ply %d searching level %d\n", board->ply, board->level);
	    myEval = minimax(board, -EVAL_CHECKMATE, EVAL_CHECKMATE,
			     strngth, &stats, &pv, th);
	    board->ncheck[turn] = ncheck; /* restore ncheck */

	    if (ThinkerCompNeedsToMove(th))
	    {
		/* We're guaranteed to have at least one move available, so
		   use it. */
		break;
	    }
	    else if (myEval.lowBound <= -EVAL_CHECKMATE)
	    {
		/* we're in a really bad situation */
		resigned = 1;
		break;
	    }
	    else if (myEval.lowBound >= EVAL_CHECKMATE)
	    {
		/* we don't need further evaluation in order to mate. */
		break;
	    }

	    else if (board->ply == 0 && isNormalStartingPosition(board))
	    {
		/* Special case optimization (normal game, 1st move).
		   The move is not worth thinking about any further. */
		break;
	    }
	}
	board->level = 0; /* reset board->level */
    }

    ThinkerRspNotifyStats(th, &stats);

    if (resigned)
    {
	ThinkerRspResign(th);
	return;
    }

    /* If we can draw after this move, do so. */
    makemove(board, comstr, &unmake);
    bWillDraw = canDraw(board);
    unmakemove(board, comstr, &unmake);    
    board->ncheck[turn] = ncheck; /* restore ncheck */ 

    if (bWillDraw)
    {
	ThinkerRspDraw(th, comstr);
    }
    else
    {
	ThinkerRspMove(th, comstr);
    }
}


typedef struct {
    ThreadArgsT args;
    SearchArgsT *sa;
} SearcherArgsT;


static void *searcherThread(SearcherArgsT *args)
{
    SearcherArgsT myArgs = *args; // struct copy
    ThreadNotifyCreated("searcherThread", args);

    BoardT *board;
    uint8 *comstr;
    ThinkContextT *th = &myArgs.sa->th;

    // Shorthand.
    board = &myArgs.sa->board;
    comstr = myArgs.sa->comstr;

    // We cycle, basically:
    // -- waiting on a board position/move combo from the compThread
    // -- searching the move
    // -- returning the return parameters (early, if NeedsToMove).
    //
    // We end up doing a lot of stuff in the searcherThread instead of
    // compThread since we want things to be as multi-threaded as possible.
    while (1)
    {
	ThinkerCompWaitThink(th);

	// Make the appropriate move, bump depth etc.
	myArgs.sa->eval = tryMove(board, comstr, myArgs.sa->stats,
				  (myArgs.sa->strgh +
				   calcCapWorth(board, comstr)),
				  myArgs.sa->alpha,
				  myArgs.sa->beta,
				  &myArgs.sa->pv,
				  th);

	ThinkerRspSearchDone(th);
    }
}



typedef struct {
    ThreadArgsT args;
    BoardT *board;
    ThinkContextT *th;
} CompArgsT;


static void *compThread(CompArgsT *args)
{
    CompArgsT myArgs = *args; // struct copy
    ThreadNotifyCreated("compThread", args);

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
    CompArgsT args = {gThreadDummyArgs, board, th};
    SearcherArgsT sargs;
    int i;

    // initialize main compThread.
    ThreadCreate(compThread, &args);

    // initialize searcher threads.
    for (i = 0; i < gPreCalc.numProcs; i++)
    {
	sargs.sa = &gVars.searchArgs[i];
	ThinkerInit(&sargs.sa->th);
	ThreadCreate(searcherThread, &sargs);

	// (also initialize the associated poll structure.)
	memset(&gVars.pfds[i], 0, sizeof(struct pollfd));
	gVars.pfds[i].fd = sargs.sa->th.masterSock;
	gVars.pfds[i].events = POLLIN;
    }

}
