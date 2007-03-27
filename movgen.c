/***************************************************************************
             movgen.c - move generator for a given BoardT position.
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


#include <stdio.h>
#include <ctype.h>
#include <string.h> /* memcpy() */
#include <assert.h>
#include "ref.h"


static inline void addsrccoord(CoordListT *attlist, int from)
{
    attlist->list[(attlist->lgh)++] = from;
}


/* generate all possible enemy (!turn) sliding attack locations on 'from',
   whether blocked or not.  Note right now, we can generate a dir multiple x.
   (meaning, if (say) a Q and B are attacking a king, we will add both, where
   we might just want to add the closer piece.)
   We should probly change this. */
static void genslide(BoardT *board, CoordListT *dirlist, int from,
		     int turn)
{
    int to, i;
    /* Optimized.  Index into the enemy playlist.
       I'm also moving backward to preserve the move ordering in cappose().
    */
    CoordListT *pl = &board->playlist[BQUEEN ^ turn];

    dirlist->lgh = 0;	/* init list. */

    /* find queen sliding attacks. */
    for (i = 0; i < pl->lgh; i++)
    {
	to = pl->list[i];
	if (gPreCalc.dir[from] [to] < 8) /* !(DIRFLAG or nightmove) */
	    addsrccoord(dirlist, to);
    }
    pl += (ROOK - QUEEN);

    /* find rook sliding attacks. */
    for (i = 0; i < pl->lgh; i++)
    {
	to = pl->list[i];
	if (gPreCalc.dir[from] [to] & 1) /* !DIRFLAG */
	    addsrccoord(dirlist, to);
    }
    pl += (BISHOP - ROOK);

    /* find bishop sliding attacks. */
    for (i = 0; i < pl->lgh; i++)
    {
	to = pl->list[i];
	if (!(gPreCalc.dir[from] [to] & 0x9)) /* !(DIRFLAG || nightmove) */
	    addsrccoord(dirlist, to);
    }
}


static void findpins(BoardT *board, int kcoord, PListT *pinlist, int turn)
{
    int a, test, i;
    uint8 *x;
    CoordListT dirlist;
    uint8 *coord = board->coord;

    /* initialize pin array. */
    for (i = 0; i < 8; i++)
	pinlist->ll[i] = FLAG64;
    genslide(board, &dirlist, kcoord, turn);
    /* only check the possible pin dirs. */
    for (a = 0; a < dirlist.lgh; a++)
    {
	x = gPreCalc.moves[kcoord] [gPreCalc.dir[kcoord] [dirlist.list[a]]];
	while ((test = CHECK(coord[*x], turn)) == UNOCCD)
	    x++;
	if (test) continue; /* pinned piece must be friend. */
	i = *x; /* location of poss. pinned piece */

	/* a nopose() check might be easier here, but probably take longer to
	   find an actual pin. */
	do {
	    x++; /* find next occ'd space */
	} while ((test = CHECK(coord[*x], turn)) == UNOCCD);

	if (*x != dirlist.list[a])
	    continue; /* must have found our sliding-attack piece */

	/* by process of elimination, we have pinned piece. */
	pinlist->c[i] = gPreCalc.dir[kcoord] [dirlist.list[a]] & 3;
	/* set pintype */
        /* cprintf("pn:%c%c", File(i) + 'a',	Rank(i) + '1'); */
    }
}


static int nopose(BoardT *board, int src, int dest, int hole)
/* checks to see if there are any occupied squares between src and dest.
   returns: 0 if blocked, 1 if nopose.  Note:  doesn't check if dir == DIRFLAG
   (none) or 8 (knight attack), so shouldn't be called in that case. */
{
    int dir = gPreCalc.dir[src] [dest];
    uint8 *to = gPreCalc.moves[src] [dir];
    while (*to != dest)
    {
	if (board->coord[*to] && *to != hole)
	    /* hole is used to extend attacks along checking ray in
	       attacked().  In this case it's our friendly kcoord.
	       Usually, it should be FLAG. */
	    return 0;	/* some sq on the way to dest is occ'd. */
	to++;
    }
    return 1;	/* notice we always hit dest before we hit end o' list. */
}


/* does not seem to help. */
/* #define PLENEMY(pl, turn) ((pl) + ((turn) ^ 1) - (turn)) */


/* returns: whether coord 'from' is "attacked" by a piece (whether this is a
   friend or enemy piece depends on whether turn == onwho). */
static int attacked(CoordListT *attlist, BoardT *board, int from, int turn,
		    int onwho, int stp)
{
    int j, toind;
    uint8 to;
    CoordListT dirlist;
    uint8 *coord;
    int kcoord = board->playlist[KING | onwho].list[0];
    int ekcoord = board->playlist[BKING ^ onwho].list[0];
    CoordListT *pl = &board->playlist[BNIGHT ^ onwho];

    attlist->lgh = 0;

    /* check knight attack */
    for (j = 0; j < pl->lgh; j++)
	if (gPreCalc.dir[from] [pl->list[j]] == 8)
	{
	    if (stp)
		return 1;
	    addsrccoord(attlist, pl->list[j]);
	}

    /* check sliding attack */
    genslide(board, &dirlist, from, onwho);
    for (j = 0; j < dirlist.lgh; j++)
	if (nopose(board, from, dirlist.list[j],
		   turn == onwho ? kcoord : FLAG))
	{
	    if (stp)
		return 1;
	    addsrccoord(attlist, dirlist.list[j]);
	}

    /* check king attack, but *only* in the case of *enemy* attack */
    /* (we already find possible king moves in kingmove()). */
    if (turn == onwho &&
	Abs(Rank(ekcoord) - Rank(from)) < 2 &&
	Abs(File(ekcoord) - File(from)) < 2)
    {
	return 1; /* king can never doublecheck. */
    }

    coord = board->coord; // shorthand.

    /* check pawn attack... */
    toind = (onwho << 2) + 1;
    to = *(gPreCalc.moves[from] [toind]);
    /* if attacked square unnoc'd, and *friend* attack, want pawn advances. */
    if (turn != onwho && CHECK(coord[from], onwho) == UNOCCD)
    {
	if (to != FLAG && CHECK(coord[to], onwho) == ENEMY &&
	    ISPAWN(coord[to])) /* pawn ahead */
	{
	    if (stp)
		return 1;
	    else addsrccoord(attlist, to);
	}
	/* now try e2e4 moves. */
	if (Rank(from) == 4 - onwho && CHECK(coord[to], onwho) == UNOCCD)
	{
	    to = *(gPreCalc.moves[from] [toind] + 1);
	    if (CHECK(coord[to], onwho)	== ENEMY && ISPAWN(coord[to]))
	    {
		if (stp)
		    return 1;
		else addsrccoord(attlist, to);
	    }
	}
    }
    else	/* otherwise, want p captures. */
    {
	toind = onwho << 2;
	do
	{
	    to = *(gPreCalc.moves[from] [toind]);
	    if (to != FLAG && CHECK(coord[to], onwho) == ENEMY &&
		ISPAWN(coord[to])) /* enemy on diag */
	    {
		if (stp)
		    return 1;
		else addsrccoord(attlist, to);
	    }

	    toind += 2;
	} while (toind & 2);
	if (from == board->ebyte) /* have to include en passant */
	    for (j = -1; j < 2; j += 2)
		if (CHECK(coord[from + j], onwho) == ENEMY &&
		    ISPAWN(coord[from + j]) &&
		    Rank(from) == Rank(from+j))
		    /* operates on principle that if we check for *our*
		       attacks, then we want every one. */
		    addsrccoord(attlist, from + j);
    }
    return 0;	/* gee.  Guess we're not attacked... or we filled the list */
}


/* These need -O2 to win, probably. */
#define NIGHTCHK(board, to, ekcoord) \
    (gPreCalc.dir[to] [ekcoord] == 8 ? (to) : FLAG)

#define QUEENCHK(board, to, from, ekcoord) \
    (gPreCalc.dir[to] [ekcoord] < 8 && \
     nopose(board, to, ekcoord, from) ? (to) : FLAG)

#define BISHOPCHK(board, to, from, ekcoord) \
    (!((gPreCalc.dir[to] [ekcoord]) & 0x9) /* !DIRFLAG or nightmove */ && \
     nopose(board, to, ekcoord, from) ? (to) : FLAG)

#define ROOKCHK(board, to, from, ekcoord) \
    (((gPreCalc.dir[to] [ekcoord]) & 1) /* !DIRFLAG */ && \
     nopose(board, to, ekcoord, from) ? (to) : FLAG)

#define PAWNCHK(board, to, ekcoord, turn) \
    (Abs(File(ekcoord) - File(to)) == 1 && \
     Rank(to) - Rank(ekcoord) == ((turn) << 1) - 1 ? (to) : FLAG)

#define CALCDC(board, dc, from, to) \
    ((dc) == FLAG ? FLAG : \
     gPreCalc.dir[from] [dc] == gPreCalc.dir[to] [dc] ? FLAG : \
     (dc))

/* optimization thoughts:
   -- change mvlist structure to get rid of memcpy() (separate insert list)
      ... although the compiler at -O2 probably inlines the memcpy().
      (... treating as comstr[] as int loses slightly, with all the shifting
      ops???)
   -- there may be room for further optimization here during quiescing, because
      all our moves are "preferred".
*/

/* unconditional add.  Better watch... */
static void addmove(MoveListT *mvlist, BoardT *board, int from, int to,
		    int dc, int chk)
{
    uint8 *comstr;

    if (board->coord[to] || dc != FLAG || chk != FLAG ||
	(board->level - board->depth > 1 &&
	 Abs(board->hist[board->ply & 1] [from] [to]
	     - board->ply) < board->hiswin))
	/* capture, promo, check, or history move w/ depth?  Want good
	   spot. */
    {
	memcpy(mvlist->list[mvlist->lgh++],
	       (comstr = mvlist->list[mvlist->insrt++]), 4);
    }
    else comstr = mvlist->list[(mvlist->lgh)++];
    comstr[0] = from; /* translate to command */
    comstr[1] = to;
    comstr[2] = 0;
    comstr[3] = dc == FLAG ? chk :
	chk == FLAG ? dc : DISCHKFLAG;
    /* actually follows FLAG:coord:DISCHKFLAG convention.
       Read: no check, check, doublecheck. */
}


static void addmovePromote(MoveListT *mvlist, BoardT *board, int from,
			   int to, int promote, int dc, int chk)
{
    uint8 *comstr;

    if (board->coord[to] || promote || dc != FLAG || chk != FLAG ||
	(board->level - board->depth > 1 &&
	 Abs(board->hist[board->ply & 1] [from] [to]
	     - board->ply) < board->hiswin))
	/* capture, promo, check, or history move w/ depth?  Want good
	   spot. */
    {
	memcpy(mvlist->list[mvlist->lgh++],
	       (comstr = mvlist->list[mvlist->insrt++]), 4);
    }
    else comstr = mvlist->list[(mvlist->lgh)++];
    comstr[0] = from; /* translate to command */
    comstr[1] = to;
    comstr[2] = promote;
    comstr[3] = dc == FLAG ? chk :
	chk == FLAG ? dc : DISCHKFLAG;
    /* actually follows FLAG:coord:DISCHKFLAG convention.
       Read: no check, check, doublecheck. */
}


static void addmoveCalcChk(MoveListT *mvlist, BoardT *board, int from,
			   int to, int promote, int dc)
{
    int chk, chkpiece;
    int ekcoord = mvlist->ekcoord;

    /* See if moving piece actually does any checking. */
    if (ISKING(board->coord[from]) &&
	((to - from) & 0x3) == 2 /* slower:  Abs(to - from) == 2 */)
	/* castling manuever. !@#$% special case */
    {
	chk = ROOKCHK(board, (to + from) >> 1, from, ekcoord);
    }
    else
    {
	chkpiece = promote ? promote : board->coord[from];
	switch(chkpiece | 1)
	{
	case BNIGHT:
	    chk = NIGHTCHK(board, to, ekcoord);
	    break;
	case BQUEEN:
	    chk = QUEENCHK(board, to, from, ekcoord);
	    break;
	case BBISHOP:
	    chk = BISHOPCHK(board, to, from, ekcoord);
	    break;
	case BROOK:
	    chk = ROOKCHK(board, to, from, ekcoord);
	    break;
	case BPAWN:
	    chk = PAWNCHK(board, to, ekcoord, board->ply & 1);
	    break;
	default : chk = FLAG; /* ie, king. */
	}
    }
    addmovePromote(mvlist, board, from, to, promote, dc, chk);
}


static void promo(MoveListT *mvlist, BoardT *board, int from, int to,
		  int turn, int dc)
/* an alias for multiple add w/promo. */
{
    addmovePromote(mvlist, board, from, to, QUEEN | turn, dc,
		   QUEENCHK(board, to, from, mvlist->ekcoord));
    addmovePromote(mvlist, board, from, to, NIGHT | turn, dc,
		   NIGHTCHK(board, to, mvlist->ekcoord));
    addmovePromote(mvlist, board, from, to, ROOK | turn, dc,
		   ROOKCHK(board, to, from, mvlist->ekcoord));
    addmovePromote(mvlist, board, from, to, BISHOP | turn, dc,
		   BISHOPCHK(board, to, from, mvlist->ekcoord));
}


static int enpassdc(BoardT *board, int capPawnCoord)
{
    int turn = board->ply & 1;
    int ekcoord = board->playlist[BKING ^ turn].list[0];
    CoordListT attlist;
    int ebyte = board->ebyte; // shorthand.
    int i;
    int a;

    if (gPreCalc.dir[ebyte] [ekcoord] < 8 &&
	nopose(board, ebyte, ekcoord, FLAG))
    {
	/* This is semi-lazy (we could do something more akin to findpins())
	   but it will get the job done and it does not need to be quick.
	   Generate our sliding attacks on this square.
	*/
	genslide(board, &attlist, ebyte, turn ^ 1);
	for (i = 0; i < attlist.lgh; i++)
	{
	    a = attlist.list[i];
	    if (gPreCalc.dir[a] [ebyte] == gPreCalc.dir[ebyte] [ekcoord] &&
		nopose(board, a, ebyte, capPawnCoord))
	    {
		return a;
	    }
	}
    }

    return FLAG;
}


/* Make sure an en passant capture will not put us in check.
   Normally this is indicated by 'findpins()', but if the king is on the
   same rank as the capturing (and captured) pawn, it will not work. */
static int enpassLegal(BoardT *board, int capPawnCoord)
{
    int turn = board->ply & 1;
    int kcoord = board->playlist[KING | turn].list[0];
    CoordListT attlist;
    int ebyte = board->ebyte; // shorthand.
    int dir = gPreCalc.dir[kcoord] [capPawnCoord];
    int i;
    int a;

    if ((dir == 3 || dir == 7) &&
	/* (now we know gPreCalc.dir[kcoord] [ebyte] also == (3 || 7)) */
	nopose(board, ebyte, kcoord, capPawnCoord))
    {
	/* This is semi-lazy (we could do something more akin to findpins())
	   but it will get the job done and it does not need to be quick.
	   Generate our sliding attacks on this square.
	*/
	genslide(board, &attlist, ebyte, turn);
	for (i = 0; i < attlist.lgh; i++)
	{
	    a = attlist.list[i];
	    LOG_DEBUG("enpassLegal: check %c%c\n",
		      File(a) + 'a', Rank(a) + '1');
	    if (dir == gPreCalc.dir[ebyte] [a] &&
		nopose(board, a, ebyte, capPawnCoord))
	    {
		LOG_DEBUG("enpassLegal: return %c%c\n",
			  File(a) + 'a', Rank(a) + '1');
		return 0;
	    }
	}
    }

    return 1;
}


static void cappose(MoveListT *mvlist, BoardT *board, uint8 attcoord,
		    PListT *pinlist, int turn, uint8 kcoord,
		    PListT *dclist)
/* king in check by one piece.  Find moves that capture or interpose. */
{
    char *j;
    int i, pintype;
    CoordListT attlist;
    uint8 src, dest;
    int dc;
    int enpass;
    j = gPreCalc.moves[attcoord] [gPreCalc.dir[attcoord] [kcoord]];

    while(attcoord != kcoord)
    {
	/* cprintf("%c%c ", File(attcoord) + 'a', Rank(attcoord) + '1'); */
	attacked(&attlist, board, attcoord, turn, turn ^ 1, 0);
	/* have to add possible moves right now. */
	/* so search the pinlist for possible pins on the attackers. */

	for (i = 0; i < attlist.lgh; i++)
	{
	    src = attlist.list[i];
	    dest = attcoord;
	    enpass = 0;
	    if (ISPAWN(board->coord[src]) &&
		Rank(src) == Rank(attcoord)) 	/* spec case: en passant */
	    {
		assert(dest == board->ebyte);
		enpass = board->coord[dest];
		dest += (-2 * turn + 1) << 3;
	    }

	    /* cprintf("Trying %c%c%c%c ", File(src) + 'a', Rank(src) + '1',
	       File(dest) + 'a', Rank(dest) + '1'); */
	    pintype = pinlist->c[src];
	    if (pintype == FLAG ||
		/* pinned knights simply cannot move. */
		(!ISNIGHT(board->coord[src]) &&
		 pintype == (gPreCalc.dir[src] [dest] & 3)))
		/* check pin. */
	    {
		dc = dclist->c[src];
		dc = CALCDC(board, dc, src, dest);
		if (enpass && dc == FLAG)
		{
		    dc = enpassdc(board, attcoord);
		}
		if (ISPAWN(board->coord[src]) &&
		    (dest < 8 || dest > 55))
		    promo(mvlist, board, src, dest, turn, dc);
		else addmoveCalcChk(mvlist, board, src, dest, enpass, dc);
	    }
	}
	if (ISNIGHT(board->coord[attcoord]))
	    break; /* can't attack interposing squares in n's case. */
	attcoord = *j;
	j++;  /* get next interposing place. */
    }
}


static void gendclist(BoardT *board, PListT *dclist, int ekcoord,
		      int turn)
/* fills in dclist.  Each coordinate, if !FLAG, is a piece capable of giving
   discovered check, and its value is the source coordinate of the
   corresponding checking piece.
*/
{
    int i, test;
    CoordListT attlist;
    uint8 *x;

    for (i = 0; i < 8; i++)
	dclist->ll[i] = FLAG64;

    /* generate our sliding attacks on enemy king. */
    genslide(board, &attlist, ekcoord, turn ^ 1);
    /* check the possible dirs for a discovered check piece. */
    for (i = 0; i < attlist.lgh; i++)
    {
	x = gPreCalc.moves[attlist.list[i]]
	    [gPreCalc.dir[attlist.list[i]] [ekcoord]];
	while ((test = CHECK(board->coord[*x], turn)) == UNOCCD)
	    x++;
	if (test) continue; /* dc piece must be friend. */
	if (nopose(board, *x, ekcoord, FLAG)) /* yes, it is a dc piece */
	{
	    dclist->c[*x] = attlist.list[i];
	}
    }
}


static void nightmove(MoveListT *mvlist, BoardT *board, int from, int turn,
		      int dc)
{
    uint8 *moves = gPreCalc.moves[from] [8 + turn];

    for (; *moves != FLAG; moves++)
    {
	if (CHECK(board->coord[*moves], turn) > mvlist->capOnly)
	    addmove(mvlist, board, from, *moves, dc,
		    NIGHTCHK(board, *moves, mvlist->ekcoord));
    }
}


/* probes sliding moves.  Piece should not be pinned in this direction. */
static inline void probe(MoveListT *mvlist, BoardT *board, uint8 *moves,
			 int from, int turn, int dc, int mypiece)
{
    int i;
    for (; *moves != FLAG; moves++)
    {
	if ((i = CHECK(board->coord[*moves], turn)) > mvlist->capOnly)
	    addmove(mvlist, board, from, *moves, dc,
#if 0
		    gPreCalc.attacks[gPreCalc.dir[*moves] [mvlist->ekcoord]]
		    [mypiece] &&
		    nopose(board, *moves, mvlist->ekcoord, from) ?
		    *moves : FLAG
#else
		    (mypiece == BQUEEN ?
		     QUEENCHK(board, *moves, from, mvlist->ekcoord) :
		     mypiece == BBISHOP ?
		     BISHOPCHK(board, *moves, from, mvlist->ekcoord) :
		     ROOKCHK(board, *moves, from, mvlist->ekcoord))
#endif
		);
	if (i != UNOCCD) /* Occupied.  Can't probe further. */
	    break;
    }
}


static void brmove(MoveListT *mvlist, BoardT *board, int from, int turn,
		   int pintype, const int *dirs, int dc)
{
#if 1
    int mypiece = board->coord[from] | 1;
#else
    int mypiece = board->coord[from];
#endif
    do
    {
	if (pintype == FLAG || pintype == ((*dirs) & 3))
	    /* piece pinned in this dir, or not pinned */
	    probe(mvlist, board, gPreCalc.moves[from] [*dirs], from, turn, dc,
		  mypiece);
    } while (*(++dirs) != FLAG);
}


static void kingmove(MoveListT *mvlist, BoardT *board, int from, int turn,
		     int dc)
{
    const int *idx;
    uint8 to;
    CoordListT attlist; /* filler */
    uint8 *coord = board->coord;

    static const int preferredKDirs[2] [9] =
	/* prefer increase rank for White... after that, favor center,
	   queenside, and kingside moves, in that order.  Similar for Black,
	   but decrease rank.
	*/
	{{1, 0, 2, 7, 3, 5, 6, 4, FLAG},
	 {5, 6, 4, 7, 3, 1, 0, 2, FLAG}};

    if (board->ncheck[turn] == FLAG)
    {
	/* check for kingside castle */
	if (((board->cbyte >> turn) & 1) &&
	    !coord[from + 1] && !coord[from + 2] &&
	    !attacked(&attlist, board, from + 1, turn, turn, 1) &&
	    !attacked(&attlist, board, from + 2, turn, turn, 1))
	    addmove(mvlist, board, from, from + 2, FLAG,
		    ROOKCHK(board, from + 1, from, mvlist->ekcoord));
	/* check queenside castle */
	if (((board->cbyte >> (turn + 2)) & 1) &&
	    !coord[from - 1] && !coord[from - 2] && !coord[from - 3] &&
	    !attacked(&attlist, board, from - 1, turn, turn, 1) &&
	    !attacked(&attlist, board, from - 2, turn, turn, 1))
	    addmove(mvlist, board, from, from - 2, FLAG,
		    ROOKCHK(board, from - 1, from, mvlist->ekcoord));
    }

    for (idx = preferredKDirs[turn]; *idx != FLAG; idx++)
    {
	to = *(gPreCalc.moves[from] [*idx]);
	if (to != FLAG &&
	    CHECK(coord[to], turn) > mvlist->capOnly &&
	    /* I could optimize a few of these calls out if I already
	       did this while figuring out the castling moves. ... but I doubt
	       it's a win. */
	    !attacked(&attlist, board, to, turn, turn, 1))
	    addmove(mvlist, board, from, to,
		    CALCDC(board, dc, from, to),
		    FLAG);
    }
}


static void pawnmove(MoveListT *mvlist, BoardT *board, int from, int turn,
		     int pintype, int dc)
{
    int toind = (turn << 2);
    int to, to2;
    uint8 *coord = board->coord;
    uint8 **moves = gPreCalc.moves[from];
    int mydc;
    int enemy;
    int promote;

    /* generate captures (if any). */
    do
    {
	to = *moves[toind];
	if (to != FLAG &&
	    (pintype == FLAG || pintype == (toind & 3)) &&
	    /* enemy on diag? */
	    ((enemy = (CHECK(coord[to], turn) == ENEMY)) ||
	     /* en passant? */
	     ((to - 8 + (turn << 4) == board->ebyte) &&
	      enpassLegal(board, from))))
	{
	    /* can we promote? */
	    if (to > 55 || to < 8)
		promo(mvlist, board, from, to, turn,
		      CALCDC(board, dc, from, to));
	    else if (enemy)
	    {
		/* normal capture. */
		addmove(mvlist, board, from, to,
			CALCDC(board, dc, from, to),
			PAWNCHK(board, to, mvlist->ekcoord, turn));
	    }
	    else
	    {
		/* must be en passant. */
		mydc = dc == FLAG ?
		    /* with en passant, must take into account check created
		       when captured pawn was pinned.  Setting 'dc' is slightly
		       hacky but it works. */
		    enpassdc(board, from) :
		    CALCDC(board, dc, from, to);
		addmovePromote(mvlist, board, from, to,
			       board->coord[board->ebyte],
			       mydc,
			       PAWNCHK(board, to, mvlist->ekcoord,
				       turn));
	    }
	}
	toind += 2;
    } while (toind & 3);

    /* generate pawn pushes. */
    to = *moves[(toind -= 3)];
    promote = (to > 55 || to < 8);
    if (promote >= mvlist->capOnly &&
	CHECK(coord[to], turn) == UNOCCD &&
	(pintype == FLAG || pintype == (toind & 3)))
	/* space ahead */
    {	/* can we promote? */
	if (promote)
	    promo(mvlist, board, from, to, turn,
		  CALCDC(board, dc, from, to));
	else
	{
	    /* check e2e4-like moves */
	    if ((from < 16 || from > 47) &&
		CHECK(coord[(to2 = *(moves[toind]+1))], turn) == UNOCCD)
		addmove(mvlist, board, from, to2,
			CALCDC(board, dc, from, to2),
			PAWNCHK(board, to2, mvlist->ekcoord, turn));
	    /* add e2e3-like moves. */
	    addmove(mvlist, board, from, to,
		    CALCDC(board, dc, from, to),
		    PAWNCHK(board, to, mvlist->ekcoord, turn));
	}
    }
}


void mlistGenerate(MoveListT *mvlist, BoardT *board, int capOnly)
{
    int x, i, len;
    int turn = board->ply & 1;
    PListT dclist, pinlist;
    CoordListT *pl;
    int kcoord = board->playlist[KING | turn].list[0];
    int ekcoord = board->playlist[BKING ^ turn].list[0];

    mvlist->lgh = 0;
    mvlist->insrt = 0;
    mvlist->ekcoord = ekcoord;
    mvlist->capOnly = capOnly;

    static const int preferredQDirs[2] [9] =
	/* prefer increase rank for White... after that, favor center,
	   kingside, and queenside moves, in that order.  Similar for Black,
	   but decrease rank.
	*/
	{{1, 2, 0, 3, 7, 5, 4, 6, FLAG},
	 {5, 4, 6, 3, 7, 1, 2, 0, FLAG}};
    static const int preferredBDirs[2] [5] =
	{{2, 0, 4, 6, FLAG},
	 {4, 6, 2, 0, FLAG}};
    static const int preferredRDirs[2] [5] =
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
	/* generate pawn moves. */
	pl = &board->playlist[PAWN | turn];
	len = pl->lgh;
	for (i = 0; i < len; i++)
	{
	    x = pl->list[i];
	    pawnmove(mvlist, board, x, turn, pinlist.c[x], dclist.c[x]);
	}
	/* generate queen moves. */
	/* Note it is never possible for qmove to result in discovered
	   check.  We optimize for this. */
	pl += (QUEEN - PAWN);
	for (i = 0; i < pl->lgh; i++)
	{
	    x = pl->list[i];
	    brmove(mvlist, board, x, turn, pinlist.c[x], preferredQDirs[turn],
		   FLAG);
	}
	/* generate bishop moves. */
	pl += (BISHOP - QUEEN);
	for (i = 0; i < pl->lgh; i++)
	{
	    x = pl->list[i];
	    brmove(mvlist, board, x, turn, pinlist.c[x], preferredBDirs[turn],
		   dclist.c[x]);
	}
	pl += (NIGHT - BISHOP);
	/* generate night moves. */
	for (i = 0; i < pl->lgh; i++)
	{
	    x = pl->list[i];
	    /* A pinned knight cannot move w/out checking its king. */
	    if (pinlist.c[x] == FLAG)
		nightmove(mvlist, board, x, turn, dclist.c[x]);
	}
	/* generate rook moves. */
	pl += (ROOK - NIGHT);
	for (i = 0; i < pl->lgh; i++)
	{
	    x = pl->list[i];
	    brmove(mvlist, board, x, turn, pinlist.c[x], preferredRDirs[turn],
		   dclist.c[x]);
	}
    }
    else if (board->ncheck[turn] != DISCHKFLAG)
    {
	/* In check by 1 piece (only), so capture or interpose. */
	cappose(mvlist, board, board->ncheck[turn], &pinlist, turn, kcoord,
		&dclist);
    }

    /* generate valid king moves. */
    kingmove(mvlist, board, kcoord, turn, dclist.c[kcoord]);

    /* Selection Sorting the captures does no good, empirically. */
    /* But probably will do good when we extend captures. */
}


/* Force 'comstr' as first move in 'mvlist'.  Does not need to be fast. */
void mlistFirstMove(MoveListT *mvlist, BoardT *board, uint8 *comstr)
{
    int i;

    /* Find the move in question. */
    for (i = 0; i < mvlist->lgh; i++)
    {
	if (!memcmp(mvlist->list[i], comstr, 4))
	{
	    break;
	}
	if (!memcmp(mvlist->list[i], comstr, 2))
	{
	    LOG_DEBUG("similar move: %d, %d %d vs %d(%c) %d\n",
		      i,
		      mvlist->list[i] [2], mvlist->list[i] [3],
		      comstr[2], comstr[2], comstr[3]);
	}
    }

    if (i >= mvlist->lgh)
    {
	LOG_EMERG("Missing move!: ebyte %d, move ", board->ebyte);
	LogMove(eLogEmerg, board, comstr);
	assert(0);
    }

    if (i >= mvlist->insrt)
    {
	mvlist->insrt++; /* count this as a 'cool' move */
    }

    /* move the rest of the moves out of the way... */
    for (; i > 0; i--)
    {
	memcpy(mvlist->list[i], mvlist->list[i - 1], 4);
    }

    /* ... and replace the first move. */
    memcpy(mvlist->list[i], comstr, 4);
}


void mlistSortByCap(MoveListT *mvlist, BoardT *board)
{
    int i, j, besti;
    int maxWorth, myWorth;
    uint8 comstr[4];

    /* perform selection sort (by capture worth).  I don't bother caching
       capture worth, but if it shows up badly on gprof I can try it. */
    for (i = 0; i < mvlist->insrt - 1; i++)
    {
	/* find the best-worth move... */
	for (j = i, besti = i, maxWorth = 0;
	     j < mvlist->insrt;
	     j++)
	{
	    myWorth = calcCapWorth(board, mvlist->list[j]);
	    if (myWorth > maxWorth)
	    {
		maxWorth = myWorth;
		besti = j;
	    }
	}

	/* ... and if it's not the first move, swap w/it. */
	if (besti != i)
	{
	    memcpy(comstr, mvlist->list[i], 4);
	    memcpy(mvlist->list[i], mvlist->list[besti], 4);
	    memcpy(mvlist->list[besti], comstr, 4);
	}
    }
}


/* force-update ncheck() for an assumed king-coord 'i'.
   Slow; should only be used for setup. */
int calcNCheck(BoardT *board, int kcoord, char *context)
{
    CoordListT attlist;
    int mypiece = board->coord[kcoord];
    int myturn = mypiece & 1;

    if (!ISKING(mypiece))
    {
	LOG_EMERG("calcNCheck (%s): bad king kcoord %d, piece %d\n",
		  context, kcoord, mypiece);
	assert(0);
    }

    attacked(&attlist, board, kcoord, myturn, myturn, 0);
    board->ncheck[myturn] =
	attlist.lgh >= 2 ? DISCHKFLAG :
	attlist.lgh == 1 ? attlist.list[0] :
	FLAG;
    return board->ncheck[myturn];
}
