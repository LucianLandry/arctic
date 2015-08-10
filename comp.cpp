//--------------------------------------------------------------------------
//                   comp.c - computer 'AI' functionality.
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU Library General Public License as
//   published by the Free Software Foundation; either version 2 of the
//   License, or (at your option) any later version.
//
//--------------------------------------------------------------------------

#include <stddef.h>   // NULL
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>   // exit()
#include <poll.h>     // poll(2)

#include "aThread.h"
#include "board.h"
#include "comp.h"
#include "gDynamic.h"
#include "gPreCalc.h"
#include "log.h"
#include "position.h"
#include "ref.h"
#include "thinker.h"
#include "transTable.h"
#include "uiUtil.h"

// Since I spent a lot of time trying to do it, here is a treatise on why
// one cannot shrink the search window to the bounds of the hash evaluation of
// the current board position.
//
// For starters, it is not valid to dec 'beta'.  For an orig window
// "             alpha                        beta"
// We don't want to return:
// "                            hashhighbound          EVAL_WIN"
// ... that is all well and good, but also not enough information.
// We could attempt to compensate for that by using the hashed move
// eval, but it is not necessarily the best move,
// "hashlowbound                hashhighbound"
// and so cannot satisfy our requirements either!
// 
// For similar reasons, we cannot inc alpha when hashed-beta > beta.
//
// Also,
// If the lowbound is in the window, and I use the hasheval for the
// hashmove, (and I cannot.  I need an exact eval in that case!) there
// is still nothing guaranteeing that our evals will line up w/in the
// window (even at the same searchdepth, because re-searches can hit
// hashes of deeper search depth, which change the evaluation).  So
// our hash 'window' is at best an educated guess.  We could use that
// for a kind of PVS (principal variation search), but we do not implement
// that.


// Null window note: currently, a move which is "as good" as the null window
// could fail either high or low (which way is undefined).

// Hashing note: if you see something like (where we play white):
// 223426 <first : 4 ...
// 224562 <first : 5 0 0 0 Ra2 dxc4 Qxc4 Qc8 axb7 Qxb7 Rxa8.
// 230517 <first : move a1a2
// ...
// 236631 <second: 16. ... d5c4
// ...
// 236786 <first : 0 -100 0 0 Qxc4.
// ...
// 236787 <first : 4 -100 0 0 Qxc4.
// 237425 <first : 5 -100 0 0 Qxc4 Nxg3 hxg3 Rc8 axb7 Rxc4 Bxc4.
//
// Don't Panic.  What this means is, we started the depth6 search on Ra2
// and the Qxc4 branch was found wanting (because it was searched deeper).
// So, that was hashed.  But we did not complete the Ra2 evaluation before
// time expired.


// Globals.
// FIXME 'gThinker' should maybe not be a global ...
// but I'm trying to avoid passing around extra args to updatePv().
static ThinkContextT *gThinker;
// same argument for gStats, but it affects minimax() and trymove() as well!
CompStatsT gStats;

#define HASH_MISS 1
#define HASH_HIT 0

// Forward declarations.
static PositionEvalT minimax(BoardT *board, int alpha, int beta,
                             PvT *goodPv, ThinkContextT *th, int *hashHitOnly);


// Assumes neither side has any pawns.
static int endGameEval(BoardT *board, int turn)
{
    int ekcoord =
        board->pieceList[Piece(turn ^ 1, PieceType::King).ToIndex()].coords[0];
    int kcoord =
        board->pieceList[Piece(turn, PieceType::King).ToIndex()].coords[0];

    return
        // enemy king needs to be as close to a corner as possible.
        gPreCalc.centerDistance[ekcoord] * 14 /* max 84 */ +
        // Failing any improvement in the above, we should also try to close in
        // w/our own king.
        (14 - gPreCalc.distance[kcoord] [ekcoord]); /* max 14 */
}


#define QUIESCING (searchDepth < 0)

// 'alpha' is the lowbound for the search (any move must be at least this
// good).
// It is also roughly equivalent to 'bestVal' (for any move so far), except
// perhaps it is higher than any of them.
//
// 'beta' is the highbound for the search (if we find a move at least this
// good, we don't need to worry about searching the rest of the moves).
//
// 'lowBound' and 'highBound' are the possible limits of the best 'move' found
// so far.

static inline PositionEvalT invertEval(PositionEvalT eval)
{
    return (PositionEvalT) {-eval.highBound, -eval.lowBound};
}

#define SET_BOUND(low, high) \
do { retVal.lowBound = (low); retVal.highBound = (high); } while (0)

#define BUMP_BOUND_TO(eval) \
do { if (retVal.lowBound < (eval).lowBound) \
         retVal.lowBound = (eval).lowBound; \
     if (retVal.highBound < (eval).highBound) \
         retVal.highBound = (eval).highBound; } while (0)

#define RETURN_BOUND(low, high) \
do { return (PositionEvalT) {(low), (high)}; } while (0)


void updatePv(BoardT *board, PvT *goodPv, PvT *childPv, MoveT move,
              int eval)
{
    int depth = board->depth;

    if (depth < MAX_PV_DEPTH && move.src != FLAG)
    {
        // This be a good move.
        goodPv->moves[0] = move; // struct assign

        if (childPv && childPv->depth)
        {
            goodPv->depth = childPv->depth;
            memcpy(&goodPv->moves[1], &childPv->moves[0], (goodPv->depth - depth) * sizeof(MoveT));
        }
        else
        {
            goodPv->depth = depth;
        }

        if (depth == 0)
        {
            PvRspArgsT pvArgs;

            // Searching at root level, so let user know the updated line.
            goodPv->eval = eval;
            goodPv->level = board->level;

            memcpy(&pvArgs.stats, &gStats, sizeof(gStats));
            memcpy(&pvArgs.pv, goodPv, sizeof(PvT));
            ThinkerRspNotifyPv(gThinker, &pvArgs);

            // Update the tracked principal variation.
            gPvUpdate(goodPv);
        }
    }
    else
    {
        goodPv->depth = 0;
    }
}

static PositionEvalT tryMove(BoardT *board, MoveT move,
                             int alpha, int beta, PvT *newPv,
                             ThinkContextT *th, int *hashHitOnly)
{
    UnMakeT unmake;
    PositionEvalT myEval;
    CvT *cv = &board->cv;

    LOGMOVE_DEBUG(board, move);
    BoardMoveMake(board, move, &unmake); // switches sides

    // It seems silly to do 2 checks for this and I should probably just
    // assert(board->depth < MAX_CV_DEPTH) since under ordinary circumstances,
    // this should never happen.  But I guess I'd rather have the extra check
    // and not ever have to worry about a crash.
    if (board->depth < MAX_CV_DEPTH)
    {
        cv->moves[board->depth] = move; // struct assign
    }
    board->depth++;

    // massage alpha/beta for mate detection so that we can "un-massage" the
    // returned bounds later w/out violating our alpha/beta.
    // Okay, example: let's think about the case where we try to find a mate in one.
    // alpha = EVAL_WIN - 1, beta = EVAL_WIN. maxLevel = 0.
    // minimax called, a=EVAL_LOSS, b = EVAL_LOSS.  quiescing and ncheck.  {EVAL_LOSS, EVAL_LOSS}
    // We get {EVAL_WIN, EVAL_WIN}, which we return as {EVAL_WIN - 1, EVAL_WIN - 1}.
    // If far side not in check, we get ... probably quiescing, so {strgh, strgh}, but
    // worst case a fail high of {EVAL_LOSS, EVAL_WIN}.  We return
    // {EVAL_LOSS - 1, EVAL_WIN - 1} which works.
    if (alpha >= EVAL_WIN_THRESHOLD && alpha < EVAL_WIN)
    {
        alpha++;
    }
    else if (alpha <= EVAL_LOSS_THRESHOLD && alpha > EVAL_LOSS)
    {
        alpha--;
    }
    if (beta >= EVAL_WIN_THRESHOLD && beta < EVAL_WIN)
    {
        beta++;
    }
    else if (beta <= EVAL_LOSS_THRESHOLD && beta > EVAL_LOSS)
    {
        beta--;
    }

    myEval = invertEval(minimax(board, -beta, -alpha,
                                newPv, th, hashHitOnly));

    board->depth--;
    if (board->depth < MAX_CV_DEPTH)
    {
        cv->moves[board->depth].src = FLAG;
    }

    // restore the current board position.
    BoardMoveUnmake(board, &unmake);

    // Enable calculation of plies to win/loss by tweaking the eval.
    // Slightly hacky.
    // If we got here, we could make a move and had to try it.
    // Therefore neither bound can be EVAL_WIN.
    if (myEval.lowBound >= EVAL_WIN_THRESHOLD)
    {
        myEval.lowBound--;
    }
    else if (myEval.lowBound <= EVAL_LOSS_THRESHOLD)
    {
        myEval.lowBound++;
    }
    if (myEval.highBound >= EVAL_WIN_THRESHOLD)
    {
        myEval.highBound--;
    }
    else if (myEval.highBound <= EVAL_LOSS_THRESHOLD)
    {
        myEval.highBound++;
    }

    LOG_DEBUG("eval: %d %d %d %d\n",
              alpha, myEval.lowBound, myEval.highBound, beta);

    return myEval;
}

static int potentialImprovement(BoardT *board)
{
    int turn = board->turn;
    int improvement = 0;
    int lowcoord, highcoord;
    int i, x, len;
    // traipse through the enemy pieceList.
    CoordListT *pl =
        &board->pieceList[Piece(turn ^ 1, PieceType::Queen).ToIndex()];

    do
    {
        if (pl->lgh)
        {
            improvement = EVAL_QUEEN;
            break;
        }
        pl += Piece(0, PieceType::Rook).ToIndex() -
            Piece(0, PieceType::Queen).ToIndex();
        if (pl->lgh)
        {
            improvement = EVAL_ROOK;
            break;
        }
        pl += Piece(0, PieceType::Bishop).ToIndex() -
            Piece(0, PieceType::Rook).ToIndex();
        if (pl->lgh)
        {
            improvement = EVAL_BISHOP;
            break;
        }
        pl += Piece(0, PieceType::Knight).ToIndex() -
            Piece(0, PieceType::Bishop).ToIndex();
        if (pl->lgh)
        {
            improvement = EVAL_KNIGHT;
            break;
        }
        pl += Piece(0, PieceType::Pawn).ToIndex() -
            Piece(0, PieceType::Knight).ToIndex();
        if (pl->lgh)
        {
            improvement = EVAL_PAWN;
            break;
        }
    } while (0);

    // If we have at least a pawn on the 6th or 7th rank, we could also improve
    // by promotion.  (We include 6th rank because this potentialImprovement()
    // routine is really lazy, and calculated before any depth-1 move, as
    // opposed to after each one).
    pl = &board->pieceList[Piece(turn, PieceType::Pawn).ToIndex()];
    len = pl->lgh;
    if (len)
    {
        if (turn)
        {
            lowcoord = 8;   // 7th rank, black
            highcoord = 23; // 6th rank, black
        }
        else
        {
            lowcoord = 40;  // 6th rank, white
            highcoord = 55; // 7th rank, white
        }
        for (i = 0; i < len; i++)
        {
            x = pl->coords[i];
            if (x >= lowcoord && x <= highcoord)
            {
                improvement += EVAL_QUEEN - EVAL_PAWN;
                break;
            }
        }
    }
    return improvement;
}


// Evaluates the next hashed move in 'mvlist'.
// 'cookie' keeps track of our place in 'mvlist'.
// Returns the evaluation of the found move
// (if no move found, 'cookie' is set to -1).
// Side effect: removes the move from the list.
static PositionEvalT tryNextHashMove(BoardT *board, int alpha, int beta,
                                     PvT *newPv, ThinkContextT *th,
                                     MoveList *mvlist, int *cookie,
                                     MoveT *hashMove)
{
    PositionEvalT myEval = {EVAL_LOSS, EVAL_LOSS};
    int hashHitOnly = HASH_MISS;
    int i;
    MoveT move;

    for (i = *cookie; i < mvlist->NumMoves(); i++)
    {
        move = mvlist->Moves(i);

        hashHitOnly = HASH_HIT; // assume the best case
        myEval = tryMove(board, move,
                         alpha, beta, newPv, th, &hashHitOnly);
        if (hashHitOnly == HASH_HIT)
            break;
    }

    if (hashHitOnly == HASH_HIT)
    {
        // We found a move, and 'evaluated' it. ...
        // Copy off and remove it.
        *hashMove = move; // struct assign
        mvlist->DeleteMove(i);
        *cookie = i;
    }
    else
    {
        // ran off end of list.  Eval is not valid.
        *cookie = -1;
    }
    return myEval;
}


static int biasDraw(int strgh, int depth)
{
    return
        // even strgh.  Assume the opponent is pernicious and wants to seek a
        // draw.  But if computer's ply, avoid draw.  This is slightly
        // asymmetrical and will skew scores if both sides are computer.
        // Otherwise we should be okay, but this is tricky.  FIXME?
        strgh == 0 ? (depth & 1 ? 1 : -1) :
        // Bias against a draw if we are up material.
        strgh > 0 ? -1 :
        // If we are down material, bias for a draw.
        1;
}

// Note: the simple lock/unlock of the hashLock is good for about a 6%
// slowdown ... even using spinlocks.  So it could be to our advantage to check
// for SMP before actually doing it. ...
// although on second thought, that has a measurable slowdown of its own, so I
// will optimize for the multithread case.

// Evaluates a given board position from {board->turn}'s point of view.
static PositionEvalT minimax(BoardT *board, int alpha, int beta,
                             PvT *goodPv, ThinkContextT *th, int *hashHitOnly)
{
    // Trying to order the declared variables by their struct size, to
    // increase cache hits, does not work.  Trying instead by functionality.
    // and/or order of usage, is also not reliable.
    int turn, ncheck, searchDepth;

    int mightDraw; // bool.  Is it possible to hit a draw while evaluating from
                   // this position.
    MoveT hashMove;
    PositionEvalT hashEval;
    MoveT bestMove;
    PositionEvalT retVal;
    int masterNode;  // multithread support.
    MoveT move;
    int preEval, improvement, i, secondBestVal;
    int cookie;
    PositionEvalT myEval;
    int newVal;
    UnMakeT unmake;
    PvT newPv;
    MoveList mvlist;
    int strgh;
    uint16 basePly = board->ply - board->depth;

    // I'm trying to use lazy initialization for this function.
    goodPv->depth = 0;
    turn = board->turn;
    strgh = board->playerStrength[turn] - board->playerStrength[turn ^ 1];
    searchDepth = board->level - board->depth;

    gStats.nodes++;
    if (!QUIESCING)
    {
        gStats.nonQNodes++;
    }

    if (BoardDrawInsufficientMaterial(board) ||
        BoardDrawFiftyMove(board) ||
        BoardDrawThreefoldRepetition(board))
    {
        // Draw detected.
        // Skew the eval a bit: If we have equal or better material, try not to
        // draw.  Otherwise, try to draw.  (here, strgh is used as a tmp
        // variable)
        strgh = biasDraw(strgh, board->depth);
        RETURN_BOUND(strgh, strgh);
    }

    ncheck = board->ncheck[turn];

    if (board->repeatPly != -1)
    {
        // Detected repeated position.  Fudge things a bit and (again) try to
        // avoid this situation when we are winning or even.  This also may
        // help us avoid a "we either draw or lose material" situation beyond
        // the search window.
        //
        // I tried implementing this as just returning 'draw' which also
        // cuts down on the search tree, but this screws up the eval of losing
        // positions -- thanks to back-propagation, we could mistakenly
        // prefer the position over a move that won or kept material.
        improvement = -biasDraw(strgh, board->depth);
        strgh -= improvement;
    }
    else
    {
        improvement = 0;
    }

    if (QUIESCING && ncheck == FLAG)
    {
        // Putting some endgame eval right here.  No strength change is
        // possible if opponent only has king (unless we have pawns), so movgen
        // is not needed.  Also, (currently) don't bother with hashing since
        // usually ncpPlies will be too high.
        if (board->playerStrength[turn ^ 1] == 0 &&
            board->pieceList[Piece(turn, PieceType::Pawn).ToIndex()].lgh == 0)
        {
            strgh += endGameEval(board, turn); // (oh good.)
            RETURN_BOUND(strgh, strgh);
        }

        // When quiescing (ncheck is a special case because we attempt to
        // detect checkmate even during quiesce) we assume that we can at least
        // preserve our current strgh, by picking some theoretical move that
        // wasn't generated.  This actually functions as our node-level
        // evaluation function, cleverly hidden.
        if (strgh >= beta)
        {
            RETURN_BOUND(strgh, EVAL_WIN);
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

    // Is there a suitable hit in the transposition table?
    if ((!mightDraw || board->ncpPlies == 0) &&
        TransTableQuickHitTest(board->zobrist) &&
        TransTableHit(&hashEval, &hashMove, board->zobrist, searchDepth,
                      basePly, alpha, beta))
    {
        // record the move (if there is one).
        updatePv(board, goodPv, NULL, hashMove, hashEval.lowBound);
        return hashEval;
    }
    if (hashHitOnly != NULL)
    {
        *hashHitOnly = HASH_MISS;
        /* actual bounds should not matter. */
        RETURN_BOUND(EVAL_LOSS, EVAL_WIN);
    }

    gStats.moveGenNodes++;
    if (board->depth || !th->searchArgs.mvlist.NumMoves())
    {
        mvlist.GenerateLegalMoves(*board, QUIESCING && ncheck == FLAG);
    }
    else
    {
        mvlist = th->searchArgs.mvlist;
    }
    MOVELIST_LOGDEBUG(mvlist);

    // bestMove must be initialized before we goto out.
    bestMove = MoveNone; // struct assign

    if (QUIESCING &&
        board->pieceList[Piece(0, PieceType::Pawn).ToIndex()].lgh == 0 &&
        board->pieceList[Piece(1, PieceType::Pawn).ToIndex()].lgh == 0)
    {
        // Endgame.  Add some intelligence to the eval.  This allows us to
        // win scenarios like KQ vs KN.
        if (strgh >= 0)
        {
            strgh += endGameEval(board, turn); // (oh good.)
        }
        else
        {
            strgh -= endGameEval(board, turn ^ 1); // (oh bad.)
        }
    }

    /* Note:  ncheck for any side guaranteed to be correct *only* after
       the other side has made its move. */
    if (!mvlist.NumMoves())
    {
        strgh =
            ncheck != FLAG ? EVAL_LOSS : // checkmate detected
            !QUIESCING     ? 0 :         // stalemate detected
            strgh;
        SET_BOUND(strgh, strgh);
        goto out;
    }

    if (QUIESCING)
    {
        /* once we know we're not mated, alpha always >= strgh. */
        if (strgh >= beta)
        {
            SET_BOUND(strgh, EVAL_WIN);
            goto out;
        }

        alpha = MAX(strgh, alpha);
        if (mvlist.NumMoves() > 1)
        {
            mvlist.SortByCapWorth(*board);
            MOVELIST_LOGDEBUG(mvlist);
        }

        // If we find no better moves ...
        SET_BOUND(strgh, strgh);
    }
    else
    {
        /* This doesn't work well, perhaps poor interaction w/history table. */
        /* mlistSortByCap(&mvlist, board); */
        if (board->depth <= gVars.pv.level &&
            gVars.pv.moves[board->depth].src != FLAG)
        {
            // Try the principal variation move (if applicable) first.
            mvlist.UseAsFirstMove(gVars.pv.moves[board->depth]);
        }

        // Save board position for later draw detection, if applicable.
        // (It is never applicable when we are quiescing)
        if (mightDraw)
        {
            BoardPositionSave(board);
        }

        // If we find no better moves ...
        SET_BOUND(EVAL_LOSS, alpha);
    }


#if 1
    masterNode = (th == gThinker && // master node
                  searchDepth > 1); // not subject to futility pruning
#else // disables move delegation.
    masterNode = 0;
#endif

    move = MoveNone;

    if (searchDepth == 1)
    {
        improvement += potentialImprovement(board);
    }

#if 1
    cookie = searchDepth > 3 /* adjust to taste */ &&
        /* Needed to avoid scenarios where we pick a crappy hashed move,
           and then run out of time before evaluating the good move we meant
           to pick. */
        board->depth != 0 &&
        mvlist.NumMoves() > 1 ? 0 : -1;
#else // disables trying hashed moves first.
    cookie = -1;
#endif

    for (i = 0, secondBestVal = alpha;
         i < mvlist.NumMoves() || (masterNode && ThinkerSearchersSearching());
         i++)
    {
        assert(i <= mvlist.NumMoves());

        if (cookie > -1)
        {
            i--; // this counters i++

            myEval = tryNextHashMove(board, alpha, beta, &newPv, th,
                                     &mvlist, &cookie, &hashMove);
            if (cookie == -1) /* no move found? */
                continue;
            move = hashMove;
            /* Otherwise, use this myEval/move combination to adjust our
               variables. */
        }

        // We ran out of hashed moves (or trying them first is disabled).
        // Can we delegate a move?
        else if (masterNode)
        {
            if (i == 0)
            {
                // LOG_DEBUG("bldbg: comp1\n");
                // First move is special (for PV).  We process it (almost)
                // normally.
                move = mvlist.Moves(i);
                ThinkerSearchersMoveMake(move, &unmake, mightDraw);
                myEval = tryMove(board, move, alpha, beta, &newPv, th, NULL);
                ThinkerSearchersMoveUnmake(&unmake);
            }
            else if (i < mvlist.NumMoves() &&  // have a move to search?
                     // have someone to delegate it to?
                     ThinkerSearcherGetAndSearch(alpha, beta, mvlist.Moves(i)))
            {
                // We delegated it successfully.
                // LOG_DEBUG("bldbg: comp2\n");
                continue;
            }
            else 
            {
                // LOG_DEBUG("bldbg: comp3\n");
                // Either do not have a move to search, or
                // nobody to search on it.  Wait for an eval to become
                // available.
                // LOG_DEBUG("bldbg: comp3.5\n");
                myEval = ThinkerSearchersWaitOne(&move, &newPv);
                i--; // this counters i++
            }
        }
        else
        {
            // Normal search.
            move = mvlist.Moves(i);

            if ((QUIESCING || (searchDepth < 2 && !mightDraw)) &&
                move.chk == FLAG &&
                ((preEval =
                  BoardCapWorthCalc(board, move) + strgh + improvement)
                 <= alpha))
            {
                /* Last level + no possibility to draw, or quiescing;
                   The capture/promo/en passant is not good enough;
                   And there is no check.
                   So, this particular move will not improve things...
                   and we can skip it.

                   (In the case of searchDepth == 1, we assume we cannot
                   improve our position more than a queen capture, which
                   is true unless there is a capturing checkmate at depth '-1',
                   which is rare enough that I am willing to live with it.)

                   This is the familiar 'futility pruning'.
                */

                /* (however, we do need to bump the highbound.  Otherwise, a
                   depth-0 position can be mistakenly evaluated as +checkmate.)
                */
                retVal.highBound = MAX(retVal.highBound, preEval);

                /* *if* alpha > origalpha, we found a newVal > origalpha.  In
                   this case, we want to set an exact value for this.  However,
                   an alpha < newVal < beta should actually have an exactval
                   already.
                */

                if (!mvlist.IsPreferredMove(i + 1))
                {
                    // ... in this case, the other moves will not help either,
                    //  so...
                    break;
                }
                continue;
            }

            myEval = tryMove(board, move, alpha, beta, &newPv, th, NULL);
        }

        /* Note: If we need to move, we cannot trust (and should not hash)
           'myEval'.  We must go with the best value/move we already had ... if any. */
        if (ThinkerCompNeedsToMove(gThinker) ||
            (gVars.maxNodes != NO_LIMIT && gStats.nodes >= gVars.maxNodes))
        {
            if (masterNode)
            {
                // Wait for any searchers to terminate.
                ThinkerSearchersBail();
            }
            RETURN_BOUND(retVal.lowBound, EVAL_WIN);
        }

        // In case of a <= alpha exact eval, this can at least tighten
        // the evaluation of this position.  Even though we don't record the
        // move, I think that's good enough to avoid 'bestVal'.
        BUMP_BOUND_TO(myEval);

        newVal = myEval.lowBound;
        if (newVal >= alpha)
        {
            /* This does *not* practically disable the history table,
               because most moves should fail w/{EVAL_LOSS, alpha}. */
            secondBestVal = alpha;  /* record 2ndbest val for history table. */
        }

        if (newVal > alpha)
        {
            bestMove = move; // struct assign
            alpha = newVal;

            updatePv(board, goodPv, &newPv, bestMove, newVal);

            if (newVal >= beta) // ie, will leave other side just as bad
                                // off (if not worse)
            {
                if (masterNode && ThinkerSearchersSearching())
                {
                    ThinkerSearchersBail();
                    retVal.highBound = EVAL_WIN;
                }
                else if (cookie != -1 || i != mvlist.NumMoves() - 1)
                {
                    retVal.highBound = EVAL_WIN;
                }
                // (else, we should have got through the last move
                //  and do not need to clobber the highBound.)

                break;        // why bother checking how bad the other
                              // moves are?
            }
            else if (myEval.lowBound != myEval.highBound)
            {
                /* alpha < lowbound < beta needs an exact evaluation. */
                LOG_EMERG("alhb: %d %d %d %d\n",
                          alpha, myEval.lowBound, myEval.highBound, beta);
                assert(0);
            }
        }
        else
        {
            assert(myEval.highBound <= alpha);
        }
    }


    if (!QUIESCING && alpha > secondBestVal &&
        // Do not add moves that will automatically be preferred -- picked this
        // up from a chess alg site.  It does seem to help our speed
        // (slightly).
        bestMove.promote == PieceType::Empty &&
        (MoveIsCastle(bestMove) || // castling is not currently preferred
         board->coord[bestMove.dst].IsEmpty()))
    {
        assert(bestMove.src != FLAG); // aka MoveNone.src
        /* move is at least one point better than others. */
        gVars.hist[turn]
            [bestMove.src] [bestMove.dst] = board->ply;
    }

out:

    if (TransTableSize())
    {
        // Update the transposition table entry if needed.
        TransTableConditionalUpdate(retVal, bestMove, board->zobrist,
                                    searchDepth, basePly);
    }

    return retVal;
}

// These draws are claimed, not automatic.  Other draws are automatic.
static bool canClaimDraw(BoardT *board, SaveGameT *sgame)
{
    // Testing only.  The whole point of BDTRF() is that it might properly
    // catch (or not catch) draws that BDTR() won't.
    // assert(BoardDrawThreefoldRepetition(board) ==
    //        BoardDrawThreefoldRepetitionFull(board, sgame));
    return
        BoardDrawFiftyMove(board) ||
        BoardDrawThreefoldRepetitionFull(board, sgame);
}

// This (currently hard-coded) routine tries to find a balance between trying
// trying not to resign too early (for a human opponent at least) while still
// giving up a clearly lost game.
// There is currently no integration between this function and our move choice
// (ie avoiding resignation vs avoiding mate), so we might sacrifice a queen
// or something to avoid mate as long as possible, only to turn around and
// resign on the next move.
// Assumes the 'board' passed in is set to our turn.
static bool shouldResign(BoardT *board, PositionEvalT myEval, bool bPonder)
{
    return
        // do not resign while pondering; let opponent make move
        // (or possibly run out of time)
        !bPonder &&
        // opponent has a clear mating strategy
        myEval.highBound <= EVAL_LOSS_THRESHOLD &&
        // We are down by at least a rook's worth of material
        (board->playerStrength[board->turn ^ 1] -
         board->playerStrength[board->turn] >= EVAL_ROOK) &&
        // We do not have a queen (the theory being that things could quickly
        // turn around if the opponent makes a mistake)
        board->pieceList[Piece(board->turn, PieceType::Queen).ToIndex()].lgh == 0;
}


static void computermove(ThinkContextT *th, bool bPonder)
{
    PositionEvalT myEval;
    PvT pv;
    BoardT *board = &th->searchArgs.localBoard;
    int resigned = 0;
    MoveList mvlist;
    UnMakeT unmake;
    MoveT move = MoveNone;
    bool bWillDraw = false;

    // Do impose some kind of max search depth to prevent a tight loop (and a
    // lot of spew) when running into the fifty-move rule.  If I could think
    // of an elegant (not compute-hogging) way to detect that further-depth
    // searches would be futile, I would implement it.
    int maxSearchDepth = gVars.maxLevel == NO_LIMIT ? 100 : gVars.maxLevel;

    pv.moves[0] = MoveNone;
    board->depth = 0;   // start search from root depth.

    // Clear stats.
    memset(&gStats, 0, sizeof(gStats));

    // If we can claim a draw, do so w/out thinking.
    if (canClaimDraw(board, &th->searchArgs.sgame))
    {
        ThinkerRspDraw(th, move);
        return;
    }

    if (gVars.randomMoves)
    {
        BoardRandomize(board);
    }

    mvlist.GenerateLegalMoves(*board, false);
    
    if (!bPonder &&

        // only one move to make -- do not think about it.
        (mvlist.NumMoves() == 1 ||

         // Special case optimization (normal game, 1st move).
         // The move is not worth thinking about any further.
         BoardIsNormalStartingPosition(board)))
    {
        move = mvlist.Moves(0); // struct assign
    }
    else
    {
        // Use the principal variation move (if it exists) if we run out of
        // time before we figure out a move to recommend.
        mvlist.UseAsFirstMove(gVars.pv.moves[0]);
        
        // setup known search parameters across the slaves.
        ThinkerSearchersBoardSet(board);

        for (board->level =
             // Always try to find the shortest mate if we have stumbled
             // onto one.  But normally we start at a deeper level just to
             // save the cycles.
             abs(gVars.pv.eval) >= EVAL_WIN_THRESHOLD && !bPonder ? 0 :

             // We start the search at the same level as the PV if we did not
             // complete the search, or at the next level if we did.  If the PV
             // level is zero, we just start over because the predicted move may
             // not have been made.
             gVars.pv.level +
             (gVars.pv.level && gVars.pv.depth == PV_COMPLETED_SEARCH ?
              1 : 0);

             board->level <= maxSearchDepth;
             board->level++)
        {
            /* setup known search parameters across the slaves. */
            ThinkerSearchersSetDepthAndLevel(board->depth, board->level);

            LOG_DEBUG("ply %d searching level %d\n", board->ply, board->level);
            myEval = minimax(board,
                             // Could use EVAL_LOSS_THRESHOLD here w/a
                             // different resign strategy, but right now we
                             // prefer the most accurate score possible.
                             EVAL_LOSS + board->level,
                             // Try to find the shortest mates possible.
                             EVAL_WIN - (board->level + 1),
                             &pv, th, NULL);
            LOG_DEBUG("top-level eval: %d %d %d %d\n",
                      EVAL_LOSS + (board->level + 2),
                      myEval.lowBound,
                      myEval.highBound,
                      EVAL_WIN - (board->level + 1));

            if (ThinkerCompNeedsToMove(th))
            {
                break;
            }

            // Hacky.  Mark that we have completed search for this level.
            gVars.pv.depth = PV_COMPLETED_SEARCH;

            if (gVars.canResign && shouldResign(board, myEval, bPonder))
            {
                // we're in a really bad situation
                resigned = 1;
                break;
            }
            if (
                // If we know are mating, or getting mated, further evaluation
                // is unnecessary.
                // (The logic works whether we are pondering or not.)
                myEval.highBound <= EVAL_LOSS_THRESHOLD ||
                myEval.lowBound >= EVAL_WIN_THRESHOLD)
            {
                break;
            }
        }

        board->level = 0; /* reset board->level */
        move = pv.moves[0];
    }

    ThinkerRspNotifyStats(th, &gStats);

    if (resigned)
    {
        ThinkerRspResign(th);
        return;
    }

    // We may not actually have found any decent move (forced to move, or
    // pondering and side to move is about to be checkmated, for instance)
    // For the former, we cannot assume we are in a lost position.
    // In either case, just use the first move.
    if (move.src == FLAG)
    {
        move = mvlist.Moves(0); // struct assign
    }

    /* If we can draw after this move, do so. */
    BoardMoveMake(board, move, &unmake);
    bWillDraw = canClaimDraw(board, &th->searchArgs.sgame);
    BoardMoveUnmake(board, &unmake);

    if (bWillDraw)
    {
        ThinkerRspDraw(th, move);
    }
    else
    {
        ThinkerRspMove(th, move);
    }
}


static void *searcherThread(SearcherArgsT *args)
{
    BoardT *board;
    ThinkContextT *th = args->th;

    ThreadNotifyCreated("searcherThread", (ThreadArgsT *) args);

    // Shorthand.
    board = &th->searchArgs.localBoard;

    // We cycle, basically:
    // -- waiting on a board position/move combo from the compThread
    // -- searching the move
    // -- returning the return parameters (early, if NeedsToMove).
    //
    // We end up doing a lot of stuff in the searcherThread instead of
    // compThread since we want things to be as multi-threaded as possible.
    while (1)
    {
        ThinkerCompWaitSearch(th);

        // Make the appropriate move, bump depth etc.
        th->searchArgs.eval =
            tryMove(board,
                    th->searchArgs.move,
                    th->searchArgs.alpha,
                    th->searchArgs.beta,
                    &th->searchArgs.pv,
                    th,
                    NULL);

        ThinkerRspSearchDone(th);
    }
    return NULL; // never get here.
}



typedef struct {
    ThreadArgsT args;
    ThinkContextT *th;
} CompArgsT;


static void *compThread(CompArgsT *args)
{
    CompArgsT myArgs = *args; // struct copy
    ThreadNotifyCreated("compThread", (ThreadArgsT *) args);
    eThinkMsgT cmd;
    gThinker = myArgs.th;

    while(1)
    {
        /* wait for a think- or ponder-command to come in. */
        cmd = ThinkerCompWaitThinkOrPonder(myArgs.th);
        /* Think on it, and recommend either: a move, draw, or resign. */
        computermove(myArgs.th, cmd == eCmdPonder);
    }
    return NULL; // never get here.
}


CvT *CompMainCv(void)
{
    return &gThinker->searchArgs.localBoard.cv;
}


int CompCurrentLevel(void)
{
    return gThinker->searchArgs.localBoard.level;
}


void CompThreadInit(ThinkContextT *th)
{
    CompArgsT args = {gThreadDummyArgs, th};

    // initialize main compThread.
    ThreadCreate((THREAD_FUNC) compThread, (ThreadArgsT *) &args);

    // initialize searcher threads.
    ThinkerSearchersCreate(gPreCalc.numProcs, (THREAD_FUNC) searcherThread);
}
