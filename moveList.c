//--------------------------------------------------------------------------
//                 moveList.c - MoveListT-oriented functions.
//                           -------------------
//  copyright            : (C) 2011 by Lucian Landry
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
#include <stdlib.h>
#include <string.h>

#include "gDynamic.h"
#include "gPreCalc.h"
#include "log.h"
#include "moveList.h"
#include "uiUtil.h"
#include "variant.h"

typedef union {    // stores pin info.
    uint64 ll[8];
    uint8 c[64];
} PinsT;

// Optimization: ordering by occurence in profiling information (requiring
// forward declarations) was tried, but does not help.

static inline void addSrcCoord(CoordListT *attlist, int from)
{
    attlist->coords[(attlist->lgh)++] = from;
}


// does not seem to help.
// #define PLENEMY(pl, turn) ((pl) + ((turn) ^ 1) - (turn))


static int nopose(uint8 *coord, int src, int dest, int hole)
// checks to see if there are any occupied squares between src and dest.
// returns: 0 if blocked, 1 if nopose.  Note:  doesn't check if dir == DIRFLAG
// (none) or 8 (knight attack), so shouldn't be called in that case.
// Also does not check if src == dst.
{
    int dir = gPreCalc.dir[src] [dest];
    uint8 *to = gPreCalc.moves[dir] [src];
    while (*to != dest)
    {
	if (coord[*to] && *to != hole)
	    // 'hole' is used to skip over a certain square, pretending no
	    // piece exists there.  This is useful in several cases.  (But
	    // otherwise, 'hole' should be FLAG.)
	    return 0;	// some sq on the way to dest is occupied.
	to++;
    }
    return 1; // notice we always hit dest before we hit end of list.
}

// These need -O2 to win, probably.
// Return the 'to' coordinate if a given move results in a check, or FLAG
// otherwise.
#define NIGHTCHK(to, ekcoord) \
    (gPreCalc.dir[to] [ekcoord] == 8 ? (to) : FLAG)

#define QUEENCHK(coord, to, from, ekcoord) \
    (gPreCalc.dir[to] [ekcoord] < 8 && \
     nopose(coord, to, ekcoord, from) ? (to) : FLAG)

#define BISHOPCHK(coord, to, from, ekcoord) \
    (!((gPreCalc.dir[to] [ekcoord]) & 0x9) /* !DIRFLAG or nightmove */ && \
     nopose(coord, to, ekcoord, from) ? (to) : FLAG)

#define ROOKCHK(coord, to, from, ekcoord) \
    (((gPreCalc.dir[to] [ekcoord]) & 1) /* !DIRFLAG */ && \
     nopose(coord, to, ekcoord, from) ? (to) : FLAG)

// Tried pre-calculating this, but it did not seem to be a win.
#define PAWNCHK(to, ekcoord, turn) \
    (abs(File(ekcoord) - File(to)) == 1 && \
     Rank(to) - Rank(ekcoord) == ((turn) << 1) - 1 ? (to) : FLAG)

// Given 'dc' (src coordinate of a piece that could possibly check the enemy),
// return 'dc' if the piece blocking it will give discovered check, or 'FLAG'
// otherwise.
#define CALCDC(dc, from, to) \
    ((dc) == FLAG ? FLAG : \
     gPreCalc.dir[from] [dc] == gPreCalc.dir[to] [dc] ? FLAG : \
     (dc))

static inline bool HistoryWindowHit(BoardT *board, int from, int to)
{
    return
	board->level - board->depth > 1 && // disable when quiescing.
	abs(gVars.hist[board->turn] [from] [to]	- board->ply) < gVars.hiswin;
}

// Does not take promotion or castling into account.
static inline bool isPreferredMoveFast(BoardT *board, int from, int to, int dc,
				       int chk)
{
    // capture, promo, check, or history move w/ depth?  Want good spot.
    return board->coord[to] || dc != FLAG || chk != FLAG ||
	HistoryWindowHit(board, from, to);
}

static inline bool isPreferredMove(BoardT *board, int from, int to, int dc,
				   int chk, int promote)
{
    return (from != to && board->coord[to]) || promote || dc != FLAG ||
	chk != FLAG || HistoryWindowHit(board, from, to);
}

// optimization thoughts:
// -- change mvlist structure to get rid of memcpy() (separate insert list)
//    ... although the compiler at -O2 inlines the memcpy().
// -- there is room for further optimization here during quiescing, because
//    all our moves are "preferred".

// unconditional add.  Better watch...
static void addMoveFast(MoveListT *mvlist, BoardT *board, int from, int to,
			int dc, int chk)
{
    MoveT *move;

    assert(mvlist->lgh != MLIST_MAX_MOVES);
    if (isPreferredMoveFast(board, from, to, dc, chk))
    {
	// capture, promo, check, or history move w/ depth?  Want good spot.
	mvlist->moves[mvlist->lgh++] =
	    *(move = &mvlist->moves[mvlist->insrt++]); // struct assign
	move->chk = dc == FLAG ? chk :
	    chk == FLAG ? dc : DOUBLE_CHECK;
	// actually follows FLAG:coord:DOUBLE_CHECK convention.
	// Read: no check, check, doublecheck.
    }
    else
    {
	move = &mvlist->moves[mvlist->lgh++];
	move->chk = FLAG;
    }
    move->src = from; // translate to move
    move->dst = to;
    move->promote = 0;
}


// A slightly slower version of the above that takes the possibility of
// promotion and castling into consideration.
static void addMove(MoveListT *mvlist, BoardT *board, int from, int to,
		    int promote, int dc, int chk)
{
    MoveT *move;

    assert(mvlist->lgh != MLIST_MAX_MOVES);
    if (isPreferredMove(board, from, to, dc, chk, promote))
    {
	// capture, promo, check, or history move w/ depth?  Want good spot.
	mvlist->moves[mvlist->lgh++] =
	    *(move = &mvlist->moves[mvlist->insrt++]); // struct assign
    }
    else move = &mvlist->moves[mvlist->lgh++];
    move->src = from; /* translate to move */
    move->dst = to;
    move->promote = promote;
    move->chk = dc == FLAG ? chk :
	chk == FLAG ? dc : DOUBLE_CHECK;
    // actually follows FLAG:coord:DOUBLE_CHECK convention.
    // Read: no check, check, doublecheck.
}


void mlistMoveAdd(MoveListT *mvlist, BoardT *board, MoveT *move)
{
    addMove(mvlist, board, move->src, move->dst, move->promote,
	    FLAG, move->chk);
}


// An even slower version that calculates whether a piece gives check on the
// fly.  As an optimization, this version also does not support castling!!
static void addMoveCalcChk(MoveListT *mvlist, BoardT *board, int from,
			   int to, int promote, int dc)
{
    int chk, chkpiece;
    int ekcoord = mvlist->ekcoord;
    uint8 *coord = board->coord;

    chkpiece = promote ? promote : board->coord[from];
    switch(chkpiece | 1)
    {
    case BNIGHT:
	chk = NIGHTCHK(to, ekcoord);
	break;
    case BQUEEN:
	chk = QUEENCHK(coord, to, from, ekcoord);
	break;
    case BBISHOP:
	chk = BISHOPCHK(coord, to, from, ekcoord);
	break;
    case BROOK:
	chk = ROOKCHK(coord, to, from, ekcoord);
	break;
    case BPAWN:
	chk = PAWNCHK(to, ekcoord, board->turn);
	break;
    default : chk = FLAG; // ie, king.
    }
    addMove(mvlist, board, from, to, promote, dc, chk);
}


static void promo(MoveListT *mvlist, BoardT *board, int from, int to,
		  int turn, int dc)
// generate all the moves for a promoting pawn.
{
    uint8 *coord = board->coord;
    addMove(mvlist, board, from, to, QUEEN | turn, dc,
	    QUEENCHK(coord, to, from, mvlist->ekcoord));
    addMove(mvlist, board, from, to, NIGHT | turn, dc,
	    NIGHTCHK(to, mvlist->ekcoord));
    addMove(mvlist, board, from, to, ROOK | turn, dc,
	    ROOKCHK(coord, to, from, mvlist->ekcoord));
    addMove(mvlist, board, from, to, BISHOP | turn, dc,
	    BISHOPCHK(coord, to, from, mvlist->ekcoord));
}


// Generate all possible enemy (!turn) sliding attack locations on 'from',
// whether blocked or not.  Note right now, we can generate a dir multiple x.
// (meaning, if (say) a Q and B are attacking a king, we will add both, where
// we might just want to add the closer piece.)
// We might want to change this, but given how it is used, that might be slower.
static void genSlide(BoardT *board, CoordListT *dirlist, int from,
		     int turn)
{
    int to, i;
    // Optimized.  Index into the enemy pieceList.
    // I'm also moving backward to preserve the move ordering in cappose().
    CoordListT *pl = &board->pieceList[BQUEEN ^ turn];

    dirlist->lgh = 0;	/* init list. */

    // find queen sliding attacks.
    for (i = 0; i < pl->lgh; i++)
    {
	to = pl->coords[i];
	if (gPreCalc.dir[from] [to] < 8) // !(DIRFLAG or nightmove)
	    addSrcCoord(dirlist, to);
    }
    pl += (ROOK - QUEEN);

    // find rook sliding attacks.
    for (i = 0; i < pl->lgh; i++)
    {
	to = pl->coords[i];
	if (gPreCalc.dir[from] [to] & 1) // !DIRFLAG
	    addSrcCoord(dirlist, to);
    }
    pl += (BISHOP - ROOK);

    // find bishop sliding attacks.
    for (i = 0; i < pl->lgh; i++)
    {
	to = pl->coords[i];
	if (!(gPreCalc.dir[from] [to] & 0x9)) // !(DIRFLAG || nightmove)
	    addSrcCoord(dirlist, to);
    }
}


// Attempt to calculate any discovered check on an enemy king by doing an
// enpassant capture.
static int enpassdc(BoardT *board, int capturingPawnCoord)
{
    int turn = board->turn;
    int ekcoord = board->pieceList[BKING ^ turn].coords[0];
    CoordListT attlist;
    int ebyte = board->ebyte; // shorthand.
    int i;
    int a;
    uint8 dir = gPreCalc.dir[ebyte][ekcoord];

    if (dir < 8 &&
	nopose(board->coord, ebyte, ekcoord, capturingPawnCoord))
    {
	// This is semi-lazy (we could do something more akin to findpins())
	// but it will get the job done and it does not need to be quick.
	// Generate our sliding attacks on this square.
	genSlide(board, &attlist, ebyte, turn ^ 1);
	for (i = 0; i < attlist.lgh; i++)
	{
	    a = attlist.coords[i];
	    if (gPreCalc.dir[a][ebyte] == dir &&
		nopose(board->coord, a, ebyte, capturingPawnCoord))
	    {
		return a;
	    }
	}
    }

    return FLAG;
}


// Make sure an en passant capture will not put us in check.
// Normally this is indicated by 'findpins()', but if the king is on the
// same rank as the capturing (and captured) pawn, that routine is not
// sufficient.
static int enpassLegal(BoardT *board, int capturingPawnCoord)
{
    int turn = board->turn;
    int kcoord = board->pieceList[KING | turn].coords[0];
    CoordListT attlist;
    int ebyte = board->ebyte; // shorthand.
    int dir = gPreCalc.dir[kcoord] [capturingPawnCoord];
    int i;
    int a;

    if ((dir == 3 || dir == 7) &&
	/// (now we know gPreCalc.dir[kcoord] [ebyte] also == (3 || 7))
	nopose(board->coord, ebyte, kcoord, capturingPawnCoord))
    {
	// This is semi-lazy (we could do something more akin to findpins())
	// but it will get the job done and it does not need to be quick.
	// Generate our sliding attacks on this square.
	genSlide(board, &attlist, ebyte, turn);
	for (i = 0; i < attlist.lgh; i++)
	{
	    a = attlist.coords[i];
	    LOG_DEBUG("enpassLegal: check %c%c\n",
		      AsciiFile(a), AsciiRank(a));
	    if (dir == gPreCalc.dir[ebyte] [a] &&
		nopose(board->coord, a, ebyte, capturingPawnCoord))
	    {
		LOG_DEBUG("enpassLegal: return %c%c\n",
			  AsciiFile(a), AsciiRank(a));
		return 0;
	    }
	}
    }

    return 1;
}


// When 'attList' == NULL, returns: whether coord 'from' is "attacked" by a
// piece (whether this is a friend or enemy piece depends on whether
// turn == onwho).
// When attList != NULL, we just fill up attList and always return 0.
// Another optimization: we assume attList is valid when turn != onwho.
static int attacked(CoordListT *attList, BoardT *board, int from, int turn,
		    int onwho)
{
    int i;
    uint8 to;
    CoordListT dirlist;
    uint8 *coord;
    uint8 *moves;
    int kcoord;
    int ekcoord;
    CoordListT *pl = &board->pieceList[BNIGHT ^ onwho];

    // check knight attack
    for (i = 0; i < pl->lgh; i++)
	if (gPreCalc.dir[from] [pl->coords[i]] == 8)
	{
	    if (attList == NULL)
		return 1;
	    addSrcCoord(attList, pl->coords[i]);
	}

    coord = board->coord; // shorthand
    kcoord = board->pieceList[KING | onwho].coords[0];

    // check sliding attack.
    genSlide(board, &dirlist, from, onwho);
    for (i = 0; i < dirlist.lgh; i++)
	if (nopose(coord, from, dirlist.coords[i],
		   turn == onwho ? kcoord : FLAG))
	{
	    if (attList == NULL)
		return 1;
	    addSrcCoord(attList, dirlist.coords[i]);
	}

    ekcoord = board->pieceList[BKING ^ onwho].coords[0];
    // check king attack, but *only* when computing *enemy* attacks
    // (we already find possible king moves in kingmove()).
    if (turn == onwho &&
	abs(Rank(ekcoord) - Rank(from)) < 2 &&
	abs(File(ekcoord) - File(from)) < 2)
    {
	return 1; // king can never doublecheck.
    }

    // check pawn attack...
    moves = gPreCalc.moves[10 + onwho] [from];

    // if attacked square unocc'd, and *friend* attack, want pawn advances.
    if (turn != onwho && CHECK(coord[from], onwho) == UNOCCD)
    {
	to = *(moves + 2);

	if (to != FLAG && CHECK(coord[to], onwho) == ENEMY &&
	    ISPAWN(coord[to])) // pawn ahead
	{
	    // when turn != onwho, we may assume there is a valid attList.
	    // if (attList == NULL) return 1;
	    addSrcCoord(attList, to);
	}
	// now try e2e4 moves.
	else if (Rank(from) == 4 - onwho && CHECK(coord[to], onwho) == UNOCCD)
	{
	    to = *(moves + 3);
	    if (CHECK(coord[to], onwho)	== ENEMY && ISPAWN(coord[to]))
	    {
		// if (attList == NULL) return 1;
		addSrcCoord(attList, to);
	    }
	}
    }
    else // otherwise, want p captures.
    {
	for (i = 0;
	     i < 2;
	     i++, moves++)
	{
	    if ((to = *moves) != FLAG && CHECK(coord[to], onwho) == ENEMY &&
		ISPAWN(coord[to])) // enemy on diag
	    {
		if (attList == NULL)
		    return 1;
		addSrcCoord(attList, to);
	    }
	}

	// may have to include en passant
	if (from == board->ebyte && turn != onwho)
	    for (i = -1; i < 2; i += 2)
		if (CHECK(coord[from + i], onwho) == ENEMY &&
		    ISPAWN(coord[from + i]) &&
		    Rank(from) == Rank(from + i))
		{
		    // if (attList == NULL) return 1;
		    addSrcCoord(attList, from + i);
		}
    }
    return 0;	/* gee.  Guess we're not attacked... or we filled the list */
}


static bool castleAttacked(BoardT *board, int src, int dest)
// returns: 'true' iff any square between 'src' and 'dest' is attacked,
// *not* including 'src' (we already presume that is not attacked) but including
// 'dst'.
// Note:  doesn't check if dir == DIRFLAG (none) or 8 (knight attack), so shouldn't be called in that case.
// Also does not check if src == dst.
{
    int dir = gPreCalc.dir[src] [dest];
    uint8 *to = gPreCalc.moves[dir] [src];
    uint8 turn = board->turn;

    while (1)
    {
	if (attacked(NULL, board, *to, turn, turn))
	    return true; // some sq on the way to dest is occupied.
	if (*to == dest)
	    break;
	to++;
    }
    // (we should always hit dest before we hit end of list.)
    return false;
}


static void cappose(MoveListT *mvlist, BoardT *board, uint8 attcoord,
		    PinsT *pinlist, int turn, uint8 kcoord,
		    PinsT *dclist)
// king in check by one piece.  Find moves that capture or interpose,
// starting on the attacking square (as captures are preferred) and
// proceeding in the direction of the king.
{
    uint8 *j;
    int i, pintype;
    CoordListT attList;
    uint8 src, dest;
    int dc;
    int enpassPiece;
    j = gPreCalc.moves[gPreCalc.dir[attcoord] [kcoord]] [attcoord];

    while(attcoord != kcoord)
    {
	// LOG_DEBUG("%c%c ", AsciiFile(attcoord), AsciiRank(attcoord));
	attList.lgh = 0;
	attacked(&attList, board, attcoord, turn, turn ^ 1);
	// have to add possible moves right now.
	// so search the pinlist for possible pins on the attackers.

	for (i = 0; i < attList.lgh; i++)
	{
	    src = attList.coords[i];
	    dest = attcoord;
	    enpassPiece = 0;
	    if (ISPAWN(board->coord[src]) &&
		Rank(src) == Rank(attcoord))  // special case: en passant
	    {
		// It is worth noting that with a pawn-push discovered
		// check, we can never use en passant to get out of it.
		// So there is never occasion to need enpassLegal().
		assert(dest == board->ebyte);
		enpassPiece = board->coord[dest];
		dest += (-2 * turn + 1) << 3;
	    }

	    /// LOG_DEBUG("Trying %c%c%c%c ",
	    //            AsciiFile(src), AsciiRank(src),
	    //            AsciiFile(dest), AsciiRank(dest));
	    pintype = pinlist->c[src];
	    if (pintype == FLAG ||
		// pinned knights simply cannot move.
		(!ISNIGHT(board->coord[src]) &&
		 pintype == (gPreCalc.dir[src] [dest] & 3)))
		// check pin.
	    {
		dc = dclist->c[src];
		dc = CALCDC(dc, src, dest);
                // The friendly king prevents the three-check-vector problem
		// described in pawnmove() (because it interposes at least one
		// of the discovered checks), so the below code is sufficient.
		if (enpassPiece && dc == FLAG)
		{
		    dc = enpassdc(board, src);
		}
		if (ISPAWN(board->coord[src]) && (dest < 8 || dest > 55))
		    promo(mvlist, board, src, dest, turn, dc);
		else addMoveCalcChk(mvlist, board, src, dest, enpassPiece, dc);
	    }
	}
	if (ISNIGHT(board->coord[attcoord]))
	    break; /* can't attack interposing squares in n's case. */
	attcoord = *j;
	j++;  /* get next interposing place. */
    }
}


/* probes sliding moves.  Piece should not be pinned in this direction. */
static inline void probe(MoveListT *mvlist, BoardT *board, uint8 *moves,
			 int from, int turn, int dc, int mypiece,
			 /* These last are for optimization purposes.
			    The function is inlined (once) so there is little
			    bloat.
			 */
			 uint8 *coord, int ekcoord, int capOnly)
{
    int i, to;
    for (; (to = *moves) != FLAG; moves++)
    {
	if ((i = CHECK(coord[to], turn)) > capOnly)
	    addMoveFast(mvlist, board, from, to, dc,
#if 0
			gPreCalc.attacks[gPreCalc.dir[to] [ekcoord]]
			[mypiece] &&
			nopose(coord, to, ekcoord, from) ?
			to : FLAG
#else
			(mypiece == BQUEEN ?
			 QUEENCHK(coord, to, from, ekcoord) :
			 mypiece == BBISHOP ?
			 BISHOPCHK(coord, to, from, ekcoord) :
			 ROOKCHK(coord, to, from, ekcoord))
#endif
		);
	if (i != UNOCCD) /* Occupied.  Can't probe further. */
	    break;
    }
}


static void brmove(MoveListT *mvlist, BoardT *board, int from, int turn,
		   int pintype, const int *dirs, int dc)
{
    uint8 *coord = board->coord;
    int ekcoord = mvlist->ekcoord;
    int capOnly = mvlist->capOnly;
#if 1
    int mypiece = coord[from] | 1;
#else
    int mypiece = coord[from] - ATTACKS_OFFSET;
#endif
    do
    {
	if (pintype == FLAG || pintype == ((*dirs) & 3))
	    /* piece pinned in this dir, or not pinned */
	    probe(mvlist, board, gPreCalc.moves[*dirs] [from], from, turn, dc,
		  mypiece,
		  coord, ekcoord, capOnly);
    } while (*(++dirs) != FLAG);
}


static void pawnmove(MoveListT *mvlist, BoardT *board, int from, int turn,
		     int pintype, int dc)
{
    int pindir;
    int to, to2;
    uint8 *coord = board->coord;
    uint8 *moves = gPreCalc.moves[10 + turn] [from];
    int dc1, dc2, pawnchk;
    int promote;

    /* generate captures (if any). */
    for (pindir = 2; pindir >= 0; pindir -= 2)
    {
	to = *(moves++);
	if (to != FLAG &&
	    (pintype == FLAG || pintype == (pindir ^ (turn << 1))))
	{
	    /* enemy on diag? */
	    if (CHECK(coord[to], turn) == ENEMY)
	    {
		/* can we promote? */
		if (to > 55 || to < 8)
		    promo(mvlist, board, from, to, turn,
			  CALCDC(dc, from, to));
		else
		{
		    /* normal capture. */
		    addMoveFast(mvlist, board, from, to,
				CALCDC(dc, from, to),
				PAWNCHK(to, mvlist->ekcoord, turn));
		}
	    }

	    /* en passant? */
	    else if (to - 8 + (turn << 4) == board->ebyte &&
		     enpassLegal(board, from))
	    {
		/* yes. */
		dc1 = CALCDC(dc, from, to);
		dc2 = enpassdc(board, from);
		pawnchk = PAWNCHK(to, mvlist->ekcoord, turn);

		/* with en passant, must take into account check created
		   when captured pawn was pinned.  So, there are actually
		   2 potential discovered check vectors + the normal check
		   vector.  Triple check is impossible with normal pieces,
		   but if any two vectors have check, we need to make sure
		   we handle it 'correctly' (if hackily). */
		if (dc1 == FLAG && dc2 != FLAG)
		    dc1 = dc2;
		else if (pawnchk == FLAG && dc2 != FLAG)
		    pawnchk = dc2;

		addMove(mvlist, board, from, to,
			board->coord[board->ebyte],
			dc1,
			pawnchk);
	    }
	}
    }

    /* generate pawn pushes. */
    to = *moves;
    promote = (to > 55 || to < 8);
    if (promote >= mvlist->capOnly &&
	CHECK(coord[to], turn) == UNOCCD &&
	(pintype == FLAG || pintype == 1))
	/* space ahead */
    {	/* can we promote? */
	if (promote)
	    promo(mvlist, board, from, to, turn,
		  CALCDC(dc, from, to));
	else
	{
	    /* check e2e4-like moves */
	    if ((from > 47 || from < 16) &&
		CHECK(coord[(to2 = *(++moves))], turn) == UNOCCD)
		addMoveFast(mvlist, board, from, to2,
			    CALCDC(dc, from, to2),
			    PAWNCHK(to2, mvlist->ekcoord, turn));
	    /* add e2e3-like moves. */
	    addMoveFast(mvlist, board, from, to,
			CALCDC(dc, from, to),
			PAWNCHK(to, mvlist->ekcoord, turn));
	}
    }
}

static void checkCastle(MoveListT *mvlist, BoardT *board, int kSrc, int kDst,
			int rSrc, int rDst, bool isCastleOO)
{
    // 'src' assumed to == castling->start.king.
    uint8 *coord = board->coord;

    // Chess 960 castling rules (from wikipedia):
    //  "All squares between the king's initial and final squares
    //   (including the final square), and all squares between the
    //   rook's initial and final squares (including the final square),
    //   must be vacant except for the king and castling rook."
    if (
	// Check if rook can move.
	(rSrc == rDst ||
	 ((coord[rDst] == 0 || rDst == kSrc) &&
	  nopose(coord, rSrc, rDst, kSrc))) &&
	// Check if king can move.
	(kSrc == kDst ||
	 ((coord[kDst] == 0 || kDst == rSrc) &&
	  nopose(coord, kSrc, kDst, rSrc) &&
	  !castleAttacked(board, kSrc, kDst))))
    {
	int sq =
	    isCastleOO ? board->turn : (1 << NUM_PLAYERS_BITS) | board->turn;
	addMove(mvlist, board, sq, sq, 0, FLAG,
		ROOKCHK(coord, rDst, kSrc, mvlist->ekcoord));
    }
}

static void kingcastlemove(MoveListT *mvlist, BoardT *board, int src,
			   int turn)
{
    // 'src' assumed to == castling->start.king.
    if (!mvlist->capOnly) // assumed true: && board->ncheck[turn] == FLAG
    {
	CastleCoordsT *castling = &gVariant->castling[turn];

	// check for kingside castle
	if (BoardCanCastleOO(board, turn))
	{
	    checkCastle(mvlist, board,
			src, castling->endOO.king,
			castling->start.rookOO, castling->endOO.rook,
			true);
	}

	// check for queenside castle.
	if (BoardCanCastleOOO(board, turn))
	{
	    checkCastle(mvlist, board,
			src, castling->endOOO.king,
			castling->start.rookOOO, castling->endOOO.rook,
			false);
	}
    }
}

static void kingmove(MoveListT *mvlist, BoardT *board, int from, int turn,
		     int dc)
{
    const int *idx;
    uint8 to;
    uint8 *coord = board->coord;

    static const int preferredKDirs[NUM_PLAYERS] [9] =
	/* prefer increase rank for White... after that, favor center,
	   queenside, and kingside moves, in that order.  Similar for Black,
	   but decrease rank.
	*/
	{{1, 0, 2, 7, 3, 5, 6, 4, FLAG},
	 {5, 6, 4, 7, 3, 1, 0, 2, FLAG}};

    for (idx = preferredKDirs[turn]; *idx != FLAG; idx++)
    {
	to = *(gPreCalc.moves[*idx] [from]);

	if (to != FLAG &&
	    CHECK(coord[to], turn) > mvlist->capOnly &&
	    /* I could optimize a few of these calls out if I already
	       did this while figuring out the castling moves. ... but I doubt
	       it's a win. */
	    !attacked(NULL, board, to, turn, turn))
	    addMoveFast(mvlist, board, from, to,
			CALCDC(dc, from, to),
			FLAG);
    }
}


static void findpins(BoardT *board, int kcoord, PinsT *pinlist, int turn)
{
    int a, test, i;
    uint8 *x;
    CoordListT dirlist;
    uint8 *coord = board->coord;

    /* initialize pin array. */
    for (i = 0; i < 8; i++)
	pinlist->ll[i] = FLAG64;
    genSlide(board, &dirlist, kcoord, turn);
    /* only check the possible pin dirs. */
    for (a = 0; a < dirlist.lgh; a++)
    {
	x = gPreCalc.moves[gPreCalc.dir[kcoord] [dirlist.coords[a]]] [kcoord];
	while ((test = CHECK(coord[*x], turn)) == UNOCCD)
	    x++;
	if (test) continue; /* pinned piece must be friend. */
	i = *x; /* location of poss. pinned piece */

	/* a nopose() check might be easier here, but probably take longer to
	   find an actual pin. */
	do {
	    x++; /* find next occ'd space */
	} while ((test = CHECK(coord[*x], turn)) == UNOCCD);

	if (*x != dirlist.coords[a])
	    continue; /* must have found our sliding-attack piece */

	/* by process of elimination, we have pinned piece. */
	pinlist->c[i] = gPreCalc.dir[kcoord] [dirlist.coords[a]] & 3;
        /* LOG_DEBUG("pn:%c%c", AsciiFile(i), AsciiRank(i)); */
    }
}


static void nightmove(MoveListT *mvlist, BoardT *board, int from, int turn,
		      int dc)
{
    uint8 *moves = gPreCalc.moves[8 + turn] [from];

    for (; *moves != FLAG; moves++)
    {
	if (CHECK(board->coord[*moves], turn) > mvlist->capOnly)
	    addMoveFast(mvlist, board, from, *moves, dc,
			NIGHTCHK(*moves, mvlist->ekcoord));
    }
}


static void gendclist(BoardT *board, PinsT *dclist, int ekcoord,
		      int turn)
// fills in dclist.  Each coordinate, if !FLAG, is a piece capable of giving
// discovered check, and its value is the source coordinate of the
// corresponding checking piece.
// Note: scenarios where a king is on the same rank as a friendly pawn that
// just did an a2a4-style move, and an enemy pawn that can capture it en-passant
// and give discovered check by an enemy rook/queen are not detected.
{
    int i, test;
    CoordListT attList;
    uint8 *x;
    uint8 *coord = board->coord;

    for (i = 0; i < 8; i++)
	dclist->ll[i] = FLAG64;

    // generate our sliding attacks on enemy king.
    genSlide(board, &attList, ekcoord, turn ^ 1);
    // check the possible dirs for a discovered check piece.
    for (i = 0; i < attList.lgh; i++)
    {
	x = gPreCalc.moves
	    [gPreCalc.dir[attList.coords[i]] [ekcoord]]
	    [attList.coords[i]];
	while ((test = CHECK(coord[*x], turn)) == UNOCCD)
	    x++;
	if (test) continue; // dc piece must be friend.
	if (nopose(coord, *x, ekcoord, FLAG)) // yes, it is a dc piece
	{
	    dclist->c[*x] = attList.coords[i];
	}
    }
}


void mlistGenerate(MoveListT *mvlist, BoardT *board, int capOnly)
{
    int x, i, len;
    int turn = board->turn;
    PinsT dclist, pinlist;
    CoordListT *pl;
    int kcoord = board->pieceList[KING | turn].coords[0];
    int ekcoord = board->pieceList[BKING ^ turn].coords[0];

    mvlist->lgh = 0;
    mvlist->insrt = 0;
    mvlist->ekcoord = ekcoord;
    mvlist->capOnly = capOnly;

    static const int preferredQDirs[NUM_PLAYERS] [9] =
	/* prefer increase rank for White... after that, favor center,
	   kingside, and queenside moves, in that order.  Similar for Black,
	   but decrease rank.
	*/
	{{1, 2, 0, 3, 7, 5, 4, 6, FLAG},
	 {5, 4, 6, 3, 7, 1, 2, 0, FLAG}};
    static const int preferredBDirs[NUM_PLAYERS] [5] =
	{{2, 0, 4, 6, FLAG},
	 {4, 6, 2, 0, FLAG}};
    static const int preferredRDirs[NUM_PLAYERS] [5] =
	{{1, 3, 7, 5, FLAG},
	 {5, 3, 7, 1, FLAG}};

    /* generate list of pieces that can potentially give
       discovered check. A very short list. Non-sorted.*/
    gendclist(board, &dclist, ekcoord, turn);

    /* find all king pins (no pun intended :) */
    findpins(board, kcoord, &pinlist, turn);

    if (board->ncheck[turn] == FLAG)
    {
	/* Not in check. */

	/* generate king castling moves. */
	kingcastlemove(mvlist, board, kcoord, turn);

	/* generate pawn moves. */
	pl = &board->pieceList[PAWN | turn];
	len = pl->lgh;
	for (i = 0; i < len; i++)
	{
	    x = pl->coords[i];
	    pawnmove(mvlist, board, x, turn, pinlist.c[x], dclist.c[x]);
	}
	/* generate queen moves. */
	/* Note it is never possible for qmove to result in discovered
	   check.  We optimize for this. */
	pl += (QUEEN - PAWN);
	for (i = 0; i < pl->lgh; i++)
	{
	    x = pl->coords[i];
	    brmove(mvlist, board, x, turn, pinlist.c[x], preferredQDirs[turn],
		   FLAG);
	}
	/* generate bishop moves. */
	pl += (BISHOP - QUEEN);
	for (i = 0; i < pl->lgh; i++)
	{
	    x = pl->coords[i];
	    brmove(mvlist, board, x, turn, pinlist.c[x], preferredBDirs[turn],
		   dclist.c[x]);
	}
	pl += (NIGHT - BISHOP);
	/* generate night moves. */
	for (i = 0; i < pl->lgh; i++)
	{
	    x = pl->coords[i];
	    /* A pinned knight cannot move w/out checking its king. */
	    if (pinlist.c[x] == FLAG)
		nightmove(mvlist, board, x, turn, dclist.c[x]);
	}
	/* generate rook moves. */
	pl += (ROOK - NIGHT);
	for (i = 0; i < pl->lgh; i++)
	{
	    x = pl->coords[i];
	    brmove(mvlist, board, x, turn, pinlist.c[x], preferredRDirs[turn],
		   dclist.c[x]);
	}
    }
    else if (board->ncheck[turn] != DOUBLE_CHECK)
    {
	// In check by 1 piece (only), so capture or interpose.
	cappose(mvlist, board, board->ncheck[turn], &pinlist, turn, kcoord,
		&dclist);
    }

    /* generate king (non-castling) moves. */
    kingmove(mvlist, board, kcoord, turn, dclist.c[kcoord]);

    /* Selection Sorting the captures does no good, empirically. */
    /* But probably will do good when we extend captures. */
}


static inline void swapFast(int *a, int *b)
{
    *a ^= *b;
    *b ^= *a;
    *a ^= *b;
}


void mlistSortByCap(MoveListT *mvlist, BoardT *board)
{
    int i, j, besti;
    int maxWorth, myWorth;
    int cwArray[MLIST_MAX_MOVES];

    for (i = 0; i < mvlist->insrt; i++)
    {
	cwArray[i] = BoardCapWorthCalc(board, &mvlist->moves[i]);
    }

    /* perform selection sort (by capture worth).  I don't bother caching
       capture worth, but if it shows up badly on gprof I can try it. */
    for (i = 0; i < mvlist->insrt - 1; i++)
    {
	/* find the best-worth move... */
	for (j = i, besti = i, maxWorth = 0;
	     j < mvlist->insrt;
	     j++)
	{
	    myWorth = cwArray[j];
	    if (myWorth > maxWorth)
	    {
		maxWorth = myWorth;
		besti = j;
	    }
	}

	/* ... and if it's not the first move, swap w/it. */
	if (besti != i)
	{
	    swapFast((int *) &mvlist->moves[i], (int *) &mvlist->moves[besti]);
	    swapFast(&cwArray[i], &cwArray[besti]);
	}
    }
}

#define MOVES_EQUAL(move1, move2) \
    (*((int *) &(move1)) == *((int *) &(move2)))

// In current profiles, this needs to be fast, so the code is pointerrific.
void mlistFirstMove(MoveListT *mvlist, MoveT *move)
{
    MoveT myMove = *move;
    MoveT *start = &mvlist->moves[0];
    MoveT *end = start + mvlist->lgh;
    MoveT *mv;
    MoveT tmp;

    myMove.chk = 0;

    // Find the move in question.
    for (mv = start; mv < end; mv++)
    {
	tmp = *mv;
	tmp.chk = 0;
	// Most of the speedup comes from here; it is much faster than
	// checking !memcmp(mv, move, 3).
	if (MOVES_EQUAL(myMove, tmp))
	{
	    // 'insrt' points to the first non-preferred move.
	    MoveT *insrt = start + mvlist->insrt;
	    myMove = *mv; // save off the found move

	    if (mv >= insrt)
	    {
		// This was a non-preferred move.  Move the 1st non-preferred
		// move into its spot.
		*mv = *insrt;
		// Move the 1st move to the last preferred move.
		*insrt = *start;
		mvlist->insrt++;
	    }
	    else
	    {
		// This move was preferred.  Move the first move into its spot.
		*mv = *start;
	    }
	    *start = myMove; // Now replace the first move.
	    return;
	}
    }

    // At this point we have a missing or non-sensical move.  Just return.
}


MoveT *mlistSearch(MoveListT *mvlist, MoveT *move, int howmany)
{
    int i;
    for (i = 0; i < mvlist->lgh; i++)
	if (!memcmp(&mvlist->moves[i], move, howmany))
	    return &mvlist->moves[i];
    return NULL;
}


// force-update ncheck() for an assumed king-coord 'i'.
// Slow; should only be used for setup.
int calcNCheck(BoardT *board, int myturn, const char *context)
{
    CoordListT attList;
    int kcoord, mypiece;

    if (board->pieceList[KING | myturn].lgh != 1)
    {
	// We do not know how to calculate check for a non-standard board.
	// This can happen in the middle of editing a position.
	// Leave it to BoardSanityCheck() or another routine to catch this.
	return (board->ncheck[myturn] = FLAG);
    }

    kcoord = board->pieceList[KING | myturn].coords[0];

    // Minor sanity-check of board.
    mypiece = board->coord[kcoord];
    if (!ISKING(mypiece))
    {
	LOG_EMERG("calcNCheck (%s): bad king kcoord %d, piece %d\n",
		  context, kcoord, mypiece);
	assert(0);
    }

    attList.lgh = 0;
    attacked(&attList, board, kcoord, myturn, myturn);
    board->ncheck[myturn] =
	attList.lgh >= 2 ? DOUBLE_CHECK :
	attList.lgh == 1 ? attList.coords[0] :
	FLAG;
    return board->ncheck[myturn];
}

// Delete the move at index 'idx'.
void mlistMoveDelete(MoveListT *mvlist, int idx)
{
    MoveT *move = &mvlist->moves[idx];

    // I used 'mvlist->insrt - 1' here but really we should always decrement
    // insrt for any preferred moves.
    if (mvlist->insrt > idx)
    {
	// Preferred move.  Copy the last preferred move over this move
	// (may be same move).
	*move =
	    mvlist->moves[--mvlist->insrt]; // struct assign
	mvlist->moves[mvlist->insrt] =
	    mvlist->moves[--mvlist->lgh]; // struct assign
    }
    else
    {
	*move =
	    mvlist->moves[--mvlist->lgh]; // struct assign
    }
}
