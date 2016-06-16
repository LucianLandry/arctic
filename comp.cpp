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

#include <assert.h>
#include <stddef.h>   // NULL
#include <string.h>
#include <thread>

#include "Board.h"
#include "Eval.h"
#include "gDynamic.h"
#include "gPreCalc.h"
#include "HistoryWindow.h"
#include "log.h"
#include "ref.h"
#include "Thinker.h"
#include "TransTable.h"
#include "uiUtil.h"

// Since I spent a lot of time trying to do it, here is a treatise on why
// one cannot shrink the search window to the bounds of the hash evaluation of
// the current board position.
//
// For starters, it is not valid to dec 'beta'.  For an orig window
// "             alpha                        beta"
// We don't want to return:
// "                            hashhighbound          Eval::Win"
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
// FIXME 'gStats' should maybe not be a global ...
// but I'm trying to avoid passing extra args to updatePv(), minimax(), and trymove().
static ThinkerStatsT gStats;

#define HASH_MISS 1
#define HASH_HIT 0

// Forward declarations.
static Eval minimax(Thinker *th, int alpha, int beta, SearchPv *goodPv,
                    int *hashHitOnly);

// Assumes neither side has any pawns.
static int endGameEval(const Board &board, int turn)
{
    cell_t ekcoord =
        board.PieceCoords(Piece(turn ^ 1, PieceType::King))[0];
    cell_t kcoord =
        board.PieceCoords(Piece(turn, PieceType::King))[0];

    return
        // enemy king needs to be as close to a corner as possible.
        gPreCalc.centerDistance[ekcoord] * 14 /* max 84 */ +
        // Failing any improvement in the above, we should also try to close in
        // w/our own king.
        (14 - gPreCalc.distance[kcoord] [ekcoord]); /* max 14 */
}

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

static void notifyNewPv(Thinker *th, const SearchPv &goodPv, Eval eval)
{
    // Searching at root level, so let user know the updated line.
    DisplayPv pv;
    pv.Set(th->Context().maxDepth, eval, goodPv);
    th->RspNotifyPv(gStats, pv);

    // Update the tracked principal variation.
    gVars.pv.Update(pv);
}

static Eval tryMove(Thinker *th, MoveT move, int alpha, int beta,
                    SearchPv *newPv, int *hashHitOnly)
{
    Eval myEval;
    int &curDepth = th->Context().depth;
    Board &board = th->Context().board;
    
    LOGMOVE_DEBUG(&board, move, curDepth);
    board.MakeMove(move); // switches sides

    curDepth++;

    // massage alpha/beta for mate detection so that we can "un-massage" the
    // returned bounds later w/out violating our alpha/beta.
    // Okay, example: let's think about the case where we try to find a mate in one.
    // alpha = Eval::Win - 1, beta = Eval::Win. maxLevel = 0.
    // minimax called, a=Eval::Loss, b = Eval::Loss.  quiescing and ncheck.  {Eval::Loss, Eval::Loss}
    // We get {Eval::Win, Eval::Win}, which we return as {Eval::Win - 1, Eval::Win - 1}.
    // If far side not in check, we get ... probably quiescing, so {strgh, strgh}, but
    // worst case a fail high of {Eval::Loss, Eval::Win}.  We return
    // {Eval::Loss - 1, Eval::Win - 1} which works.
    if (alpha >= Eval::WinThreshold && alpha < Eval::Win)
    {
        alpha++;
    }
    else if (alpha <= Eval::LossThreshold && alpha > Eval::Loss)
    {
        alpha--;
    }
    if (beta >= Eval::WinThreshold && beta < Eval::Win)
    {
        beta++;
    }
    else if (beta <= Eval::LossThreshold && beta > Eval::Loss)
    {
        beta--;
    }

    myEval = minimax(th, -beta, -alpha, newPv, hashHitOnly).Invert();

    curDepth--;

    // restore the current board position.
    board.UnmakeMove();

    // Enable calculation of plies to win/loss by tweaking the eval.
    // Slightly hacky.
    // If we got here, we could make a move and had to try it.
    // Therefore neither bound can be Eval::Win.
    myEval.DecayTo(Eval::WinThreshold - 1);

#ifdef ENABLE_DEBUG_LOGGING
    char tmpStr[kMaxEvalStringLen];
    LOG_DEBUG("eval: %d %s %d\n",
              alpha, myEval.ToLogString(tmpStr), beta);
#endif

    return myEval;
}

static int potentialImprovement(const Board &board)
{
    uint8 turn = board.Turn();
    int improvement = 0;
    int lowcoord, highcoord;

    do
    {
        // Traipse through the enemy piece vectors.
        if (board.PieceExists(Piece(turn ^ 1, PieceType::Queen)))
        {
            improvement = Eval::Queen;
            break;
        }
        if (board.PieceExists(Piece(turn ^ 1, PieceType::Rook)))
        {
            improvement = Eval::Rook;
            break;
        }
        if (board.PieceExists(Piece(turn ^ 1, PieceType::Bishop)))
        {
            improvement = Eval::Bishop;
            break;
        }
        if (board.PieceExists(Piece(turn ^ 1, PieceType::Knight)))
        {
            improvement = Eval::Knight;
            break;
        }
        if (board.PieceExists(Piece(turn ^ 1, PieceType::Pawn)))
        {
            improvement = Eval::Pawn;
            break;
        }
    } while (0);

    // If we have at least a pawn on the 6th or 7th rank, we could also improve
    // by promotion.  (We include 6th rank because this potentialImprovement()
    // routine is really lazy, and calculated before any depth-1 move, as
    // opposed to after each one).
    const std::vector<cell_t> &pvec =
        board.PieceCoords(Piece(turn, PieceType::Pawn));
    int len = pvec.size();

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
        for (auto coord : pvec)
        {
            if (coord >= lowcoord && coord <= highcoord)
            {
                improvement += Eval::Queen - Eval::Pawn;
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
static Eval tryNextHashMove(Thinker *th, int alpha, int beta, SearchPv *newPv,
                            MoveList *mvlist, int *cookie, MoveT *hashMove)
{
    Eval myEval(EvalLoss);
    int hashHitOnly = HASH_MISS;
    int i;
    MoveT move;

    for (i = *cookie; i < mvlist->NumMoves(); i++)
    {
        move = mvlist->Moves(i);

        hashHitOnly = HASH_HIT; // assume the best case
        myEval = tryMove(th, move, alpha, beta, newPv, &hashHitOnly);
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

// Evaluates a given board position from {board->turn}'s point of view.
static Eval minimax(Thinker *th, int alpha, int beta, SearchPv *goodPv,
                    int *hashHitOnly)
{
    // Trying to order the declared variables by their struct size, to
    // increase cache hits, does not work.  Trying instead by functionality.
    // and/or order of usage, is also not reliable.
    bool mightDraw; // Is it possible to hit a draw while evaluating from
                    //  this position.
    bool masterNode;  // multithread support.

    MoveT hashMove;
    Eval hashEval;
    Eval retVal;
    MoveT move;
    int preEval, improvement, i, secondBestVal;
    int cookie;
    Eval myEval;
    Board &board = th->Context().board;
    int &curDepth = th->Context().depth;
    int searchDepth = th->Context().maxDepth - curDepth;
    uint16 basePly = board.Ply() - curDepth;
    int strgh = board.RelativeMaterialStrength();
#define QUIESCING (searchDepth < 0)

    // I'm trying to use lazy initialization for this function.
    gStats.nodes++;
    if (!QUIESCING)
    {
        gStats.nonQNodes++;
    }
    goodPv->Clear();

    if (board.IsDrawInsufficientMaterial() ||
        board.IsDrawFiftyMove() ||
        board.IsDrawThreefoldRepetitionFast())
    {
        // Draw detected.
        // Skew the eval a bit: If we have equal or better material, try not to
        // draw.  Otherwise, try to draw.
        return Eval(biasDraw(strgh, curDepth));
    }

    bool inCheck = board.IsInCheck();
    uint8 turn   = board.Turn();
    
    if (board.RepeatPly() != -1)
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
        improvement = -biasDraw(strgh, curDepth);
        strgh -= improvement;
    }
    else
    {
        improvement = 0;
    }

    if (QUIESCING && !inCheck)
    {
        // Putting some endgame eval right here.  No strength change is
        // possible if opponent only has king (unless we have pawns), so movgen
        // is not needed.  Also, (currently) don't bother with hashing since
        // usually ncpPlies will be too high.
        if (board.MaterialStrength(turn ^ 1) == 0 &&
            !board.PieceExists(Piece(turn, PieceType::Pawn)))
        {
            return Eval(strgh + endGameEval(board, turn)); // (oh good.)
        }

        // When quiescing (inCheck is a special case because we attempt to
        // detect checkmate even during quiesce) we assume that we can at least
        // preserve our current strgh, by picking some theoretical move that
        // wasn't generated.  This actually functions as our node-level
        // evaluation function, cleverly hidden.
        if (strgh >= beta)
        {
            return Eval(strgh, Eval::Win);
        }
    }

    /* Is it possible to draw by repetition from this position.
       I use 3 instead of 4 because the first quiesce depth may be a repeated
       position.
       Actually, in certain very rare cases, even the 2nd (or perhaps
       more?) quiesce depth might be a repeated position due to the way we
       handle check in quiesce, but I think that is not worth the computational
       cost of detecting. */
    mightDraw =
        board.RepeatPly() == -1 ? // no repeats upto this position ?
        (searchDepth >= MAX(5, 7 - board.NcpPlies())) :
        // (7 - ncpPlies below would work, but this should be better:)
        (searchDepth >= 3 - (board.Ply() - board.RepeatPly()));

    // Is there a suitable hit in the transposition table?
    if ((!mightDraw || board.NcpPlies() == 0) &&
        gTransTable.IsHit(&hashEval, &hashMove, board.Zobrist(), searchDepth,
                          basePly, alpha, beta, &gStats))
    {
        // record the move (if there is one).
        if (goodPv->Update(hashMove))
            notifyNewPv(th, *goodPv, hashEval);

        return hashEval;
    }
    if (hashHitOnly != NULL)
    {
        *hashHitOnly = HASH_MISS;
        // actual bounds should not matter.
        return Eval(Eval::Loss, Eval::Win);
    }

    MoveList mvlist;

    if (curDepth || !th->Context().mvlist.NumMoves())
    {
        // At this point, (expensive) move generation is required.
        gStats.moveGenNodes++;
        board.GenerateLegalMoves(mvlist, QUIESCING && !inCheck);
    }
    else
    {
        mvlist = th->Context().mvlist;
    }
    MOVELIST_LOGDEBUG(mvlist);

    if (QUIESCING &&
        !board.PieceExists(Piece(0, PieceType::Pawn)) &&
        !board.PieceExists(Piece(1, PieceType::Pawn)))
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

    if (!mvlist.NumMoves())
    {
        retVal.Set(inCheck    ? Eval::Loss : // checkmate detected
                   !QUIESCING ? 0 :          // stalemate detected
                   strgh);

        // Update the transposition table entry if needed.
        gTransTable.ConditionalUpdate(retVal, MoveNone, board.Zobrist(),
                                      searchDepth, basePly, &gStats);
        return retVal;
    }

    if (QUIESCING)
    {
        // Once we know we're not mated, alpha always >= strgh.
        if (strgh >= beta)
        {
            retVal.Set(strgh, Eval::Win);
            // Update the transposition table entry if needed.
            gTransTable.ConditionalUpdate(retVal, MoveNone, board.Zobrist(),
                                          searchDepth, basePly, &gStats);
            return retVal;
        }

        alpha = MAX(strgh, alpha);
        if (mvlist.NumMoves() > 1)
        {
            mvlist.SortByCapWorth(board);
            MOVELIST_LOGDEBUG(mvlist);
        }

        // If we find no better moves ...
        retVal.Set(strgh);
    }
    else
    {
        // This doesn't work well, perhaps poor interaction w/history table:
        // mvlist->SortByCapWorth(board);
        
        // Try the principal variation move (if applicable) first.
        mvlist.UseAsFirstMove(gVars.pv.Hint(curDepth));

        // If we find no better moves ...
        retVal.Set(Eval::Loss, alpha);
    }

#if 1
    masterNode = (th->IsRootThinker() &&
                  searchDepth > 1); // not subject to futility pruning
#else // disables move delegation.
    masterNode = false;
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
        curDepth != 0 &&
        mvlist.NumMoves() > 1 ? 0 : -1;
#else // disables trying hashed moves first.
    cookie = -1;
#endif

    SearchPv childPv(curDepth + 1);
    MoveT bestMove = MoveNone;
    
    for (i = 0, secondBestVal = alpha;
         (i < mvlist.NumMoves() ||
          (masterNode && ThinkerSearchersAreSearching()));
         i++)
    {
        assert(i <= mvlist.NumMoves());

        if (cookie > -1)
        {
            i--; // this counters i++

            myEval = tryNextHashMove(th, alpha, beta, &childPv,
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
                ThinkerSearchersMakeMove(move);
                myEval = tryMove(th, move, alpha, beta, &childPv, nullptr);
                ThinkerSearchersUnmakeMove();
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
                myEval = ThinkerSearchersWaitOne(move, childPv);
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
                  board.CalcCapWorth(move) + strgh + improvement)
                 <= alpha))
            {
                /* Last level + no possibility to draw, or quiescing;
                   The capture/promo/en passant is not good enough;
                   And there is no check.
                   So, this particular move will not improve things...
                   and we can skip it.

                   (In the case of searchDepth == 1, the logic works unless
                   there is a capturing checkmate at depth '-1', which is rare
                   enough that I am willing to live with it.)

                   This is the familiar 'futility pruning'.
                */

                // (however, we do need to bump the highbound.  Otherwise, a
                //  depth-0 position can be mistakenly evaluated as +checkmate.)
                retVal.BumpHighBoundTo(preEval);

                if (!mvlist.IsPreferredMove(i + 1))
                {
                    // ... in this case, the other moves will not help either,
                    //  so...
                    break;
                }
                continue;
            }

            myEval = tryMove(th, move, alpha, beta, &childPv, nullptr);
        }

        // If we need to move, we cannot trust (and should not hash) 'myEval'.
        // We must go with the best value/move we already had ... if any.
        if (th->CompNeedsToMove() ||
            (gVars.maxNodes != NO_LIMIT && gStats.nodes >= gVars.maxNodes))
        {
            if (masterNode)
            {
                // Wait for any searchers to terminate.
                ThinkerSearchersBail();
            }
            return retVal.BumpHighBoundToWin();
        }

        // In case of a <= alpha exact eval, this can at least tighten
        //  the evaluation of this position.  Even though we don't record the
        //  move, I think that's good enough to avoid 'bestVal'.
        retVal.BumpTo(myEval);

        int newLowBound = myEval.LowBound();
        if (newLowBound >= alpha)
        {
            // This does *not* practically disable the history table,
            //  because most moves should fail w/{Eval::Loss, alpha}.
            secondBestVal = alpha;  // record 2ndbest val for history table.
        }

        if (newLowBound > alpha)
        {
            // This is an unquestionably better move.
            bestMove = move;
            alpha = newLowBound;

            if (goodPv->UpdateFromChildPv(bestMove, childPv))
                notifyNewPv(th, *goodPv, myEval);
            
            if (newLowBound >= beta) // ie, will leave other side just as bad
                                     // off (if not worse)
            {
                if (masterNode && ThinkerSearchersAreSearching())
                {
                    ThinkerSearchersBail();
                    retVal.BumpHighBoundToWin();
                }
                else if (cookie != -1 || i != mvlist.NumMoves() - 1)
                {
                    retVal.BumpHighBoundToWin();
                }
                // (else, we should have got through the last move
                //  and do not need to clobber the highBound.)

                break;        // why bother checking how bad the other
                              // moves are?
            }
            else if (!myEval.IsExactVal())
            {
                char tmpStr[kMaxEvalStringLen];

                // alpha < lowbound < beta needs an exact evaluation.
                LOG_EMERG("alhb: %d %s %d\n",
                          alpha, myEval.ToLogString(tmpStr), beta);
                assert(0);
            }
        }
        else
        {
            assert(myEval <= alpha);
        }
    }

    if (searchDepth > 5 && // The empirical testing results were fuzzy, but
                           //  this appears to work decently.
        alpha > secondBestVal &&
        // Do not add moves that will automatically be preferred -- picked this
        // up from a chess alg site.  It does seem to help our speed
        // (slightly).
        bestMove.promote == PieceType::Empty &&
        (bestMove.IsCastle() || // castling is not currently preferred
         board.PieceAt(bestMove.dst).IsEmpty()))
    {
        assert(bestMove != MoveNone);
        // move is at least one point better than others.
        gHistoryWindow.StoreMove(bestMove, turn, board.Ply());
    }

    // Update the transposition table entry if needed.
    gTransTable.ConditionalUpdate(retVal, bestMove, board.Zobrist(),
                                  searchDepth, basePly, &gStats);

    return retVal;
}

// These draws are claimed, not automatic.  Other draws are automatic.
static bool canClaimDraw(const Board &board)
{
    // Testing only.  The whole point of b->IDTR() is that it might properly
    // catch (or not catch) draws that b->IDTRF() won't.
    // assert(board->IsDrawThreefoldRepetition() ==
    //        board->IsDrawThreefoldRepetitionFast());
    return
        board.IsDrawFiftyMove() ||
        board.IsDrawThreefoldRepetition();
}

// This (currently hard-coded) routine tries to find a balance between trying
// trying not to resign too early (for a human opponent at least) while still
// giving up a clearly lost game.
// There is currently no integration between this function and our move choice
// (ie avoiding resignation vs avoiding mate), so we might sacrifice a queen
// or something to avoid mate as long as possible, only to turn around and
// resign on the next move.
// Assumes the 'board' passed in is set to our turn.
static bool shouldResign(const Board &board, Eval myEval, bool bPonder)
{
    return
        // do not resign while pondering; let opponent make move
        // (or possibly run out of time)
        !bPonder &&
        // opponent has a clear mating strategy
        myEval <= Eval::LossThreshold &&
        // We are down by at least a rook's worth of material
        board.RelativeMaterialStrength() <= -Eval::Rook &&
        // We do not have a queen (the theory being that things could quickly
        // turn around if the opponent makes a mistake)
        !board.PieceExists(Piece(board.Turn(), PieceType::Queen));
}

static void computermove(Thinker *th, bool bPonder)
{
    Eval myEval;
    SearchPv pv(0);
    Board &board = th->Context().board;
    int resigned = 0;
    MoveList mvlist;
    MoveT move = MoveNone;
    bool bWillDraw = false;

    // Do impose some kind of max search depth to prevent a tight loop (and a
    // lot of spew) when running into the fifty-move rule.  If I could think
    // of an elegant (not compute-hogging) way to detect that further-depth
    // searches would be futile, I would implement it.
    int maxSearchDepth = gVars.maxLevel == NO_LIMIT ? 100 : gVars.maxLevel;

    th->Context().depth = 0; // start search from root depth.

    // Clear stats.
    memset(&gStats, 0, sizeof(gStats));

    // If we can claim a draw without moving, do so w/out thinking.
    if (canClaimDraw(board))
    {
        th->RspDraw(move);
        return;
    }

    if (gVars.randomMoves)
    {
        board.Randomize();
    }

    board.GenerateLegalMoves(mvlist, false);

    // Use the principal variation move (if it exists) if we run out of
    // time before we figure out a move to recommend.
    mvlist.UseAsFirstMove(gVars.pv.Hint(0));

    // Use this move if we cannot (or choose not to) come up with a better one.
    move = mvlist.Moves(0);
    
    if (bPonder ||

        // do not think, if we only have one move to make.
        (mvlist.NumMoves() != 1 &&

         // Special case optimization (normal game, 1st move).
         // The move is not worth thinking about any further.
         !board.IsNormalStartingPosition()))
    {
        // setup known search parameters across the slaves.
        ThinkerSearchersSetBoard(board);

        int &maxDepth = th->Context().maxDepth;
        
        for (maxDepth = gVars.pv.SuggestSearchStartLevel();
             maxDepth <= maxSearchDepth;
             maxDepth++)
        {
            // Setup known search parameters across the slaves.
            ThinkerSearchersSetDepthAndLevel(th->Context().depth, maxDepth);

            LOG_DEBUG("ply %d searching level %d\n", board.Ply(), maxDepth);
            myEval = minimax(th,
                             // Could use Eval::LossThreshold here w/a
                             // different resign strategy, but right now we
                             // prefer the most accurate score possible.
                             Eval::Loss + maxDepth,
                             // Try to find the shortest mates possible.
                             Eval::Win - (maxDepth + 1),
                             &pv, nullptr);

            // minimax() might find MoveNone if it has to bail before it can fully
            //  think about the first move.
            if (pv.Moves(0) != MoveNone)
            {
                move = pv.Moves(0);
            }

            if (th->CompNeedsToMove())
                break;
                
#ifdef ENABLE_DEBUG_LOGGING
            {
                char tmpStr[kMaxEvalStringLen];
                LOG_DEBUG("top-level eval: %d %s %d\n",
                          Eval::Loss + maxDepth,
                          myEval.ToLogString(tmpStr),
                          Eval::Win - (maxDepth + 1));
            }
#endif
            
            gVars.pv.CompletedSearch();

            if (gVars.canResign && shouldResign(board, myEval, bPonder))
            {
                // we're in a really bad situation
                resigned = 1;
                break;
            }
            if (
                // We could stop at (for example) Eval::WinThreshold instead of
                //  'Eval::Win - maxDepth' here, but that triggers an
                //  interesting issue where we might jump between 2 mating
                //  positions (because other mating positions have been flushed
                //  from the transposition table) until the opponent can draw by
                //  repetition.
                // The logic here should work whether or not we are pondering.
                myEval <= Eval::Loss + maxDepth ||
                myEval >= Eval::Win - (maxDepth + 1))
            {
                break;
            }
        }

        th->Context().maxDepth = 0; // reset this
    }

    th->RspNotifyStats(gStats);

    if (resigned)
    {
        th->RspResign();
        return;
    }

    // If we can draw after this move, do so.
    board.MakeMove(move);
    bWillDraw = canClaimDraw(board);
    board.UnmakeMove();

    if (bWillDraw)
        th->RspDraw(move);
    else
        th->RspMove(move);
}

void Thinker::threadFunc()
{
    if (IsRootThinker())
    {
        while (1)
        {
            // wait for a think- or ponder-command to come in.
            eThinkMsgT cmd = CompWaitThinkOrPonder();
            // Think on it, and recommend either: a move, draw, or resign.
            computermove(this, cmd == eCmdPonder);
        }
    }
    else
    {
        // We cycle, basically:
        // -- waiting on a board position/move combo from the compThread
        // -- searching the move
        // -- returning the return parameters (early, if NeedsToMove).
        //
        // We end up doing a lot of stuff in the searcherThread instead of
        // compThread since we want things to be as multi-threaded as possible.
        while (1)
        {
            CompWaitSearch();
            // If we make the constructor use a memory pool, we should probably
            //  still micro-optimize this.
            SearchPv pv(Context().depth + 1);
        
            // Make the appropriate move, bump depth etc.
            searchArgs.eval =
                tryMove(this,
                        searchArgs.move,
                        searchArgs.alpha,
                        searchArgs.beta,
                        &pv,
                        nullptr);

            RspSearchDone(searchArgs.move, searchArgs.eval, pv);
        }
    }
}
