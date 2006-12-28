#include <stdio.h>
#include <ctype.h>
#include <string.h> /* memcpy() */
#include "ref.h"


static void addsrccoord(struct srclist *attlist, int from)
{
    attlist->list[(attlist->lgh)++] = from;
}


/* generate all possible enemy (!turn) sliding attack locations on 'from',
   whether blocked or not.  Note right now, we can generate a dir multiple x.
   (meaning, if (say) a Q and B are attacking a king, we will add both, where
   we might just want to add the closer piece.)
   We should probly change this. */
static void genslide(BoardT *board, struct srclist *dirlist, int from,
		     int turn)
{
    int to, i;
    int mask = (turn ^ 1) << 5;
    dirlist->lgh = 0;	/* init list. */
    /* find queen sliding attacks. */
    for (i = 0; i < board->playlist['Q' | mask].lgh; i++)
    {
	to = board->playlist['Q' | mask].list[i];
	if (!(board->dir[from] [to] & 8)) /* !(nightmove or DIRFLAG) */
	    addsrccoord(dirlist, to);
    }
    /* find rook sliding attacks. */
    for (i = 0; i < board->playlist['R' | mask].lgh; i++)
    {
	to = board->playlist['R' | mask].list[i];
	if (board->dir[from] [to] & 1 && board->dir[from] [to] != DIRFLAG)
	    addsrccoord(dirlist, to);
    }
    /* find bishop sliding attacks. */
    for (i = 0; i < board->playlist['B' | mask].lgh; i++)
    {
	to = board->playlist['B' | mask].list[i];
	if (!(board->dir[from] [to] & 1) && board->dir[from] [to] < 8)
	    addsrccoord(dirlist, to);
    }
}


static void findpins(BoardT *board, int kcoord, union plist *pinlist, int turn)
{
    int a, odd, test, i;
    uint8 *x;
    struct srclist dirlist;
    char *coord = board->coord;

    /* initialize pin array. */
    for (i = 0; i < 16; i++)
	pinlist->l[i] = -1;
    genslide(board, &dirlist, kcoord, turn);
    /* only check the possible pin dirs. */
    for (a = 0; a < dirlist.lgh; a++)
    {
	x = board->moves[kcoord] [board->dir[kcoord] [dirlist.list[a]]];
	while (*x != FLAG && (test = check(coord[*x], turn)) == 2)
	    x++;
	if (*x == FLAG || test) continue; /* pinned piece must be friend. */
	i = *x; /* location of poss. pinned piece */

	/* a nopose() check might be easier here, but probably take longer to
	   find an actual pin. */
	do {
	    x++; /* find next occ'd space */
	} while (*x != FLAG && (test = check(coord[*x], turn)) == 2);
	if (*x == FLAG || test != 1) continue;	/* has to be enemy piece */
	odd = board->dir[kcoord] [dirlist.list[a]] & 1;
	if ((test = tolower(coord[*x])) != 'q' &&
	    !(odd && test == 'r') &&
	    (odd || test != 'b') /* only possibility left */)
	    continue;	/* not right kind */
	/* by process of elimination, we have pinned piece. */
	pinlist->c[i] = board->dir[kcoord] [dirlist.list[a]] & 3;
	/* set pintype */
        /* cprintf("pn:%c%c", File(i) + 'a',	Rank(i) + '1'); */
    }
}


static int findcoord(struct srclist *attlist, int targ)
/* Given a gendclist()-style (2-tuple) 'attlist',
   returns FLAG if targ not found in attlist, otherwise the checking piece
   (stored after targ by gendclist(). */
{
    int i;
    for (i = 0; i < attlist->lgh; i += 2)
	if (attlist->list[i] == targ)
	    return attlist->list[i + 1];
    return FLAG;
}


/* returns: whether coord 'from' is "attacked" by a piece (whether this is a
   friend or enemy piece depends on whether turn == onwho). */
static int attacked(struct srclist *attlist, BoardT *board, int from, int turn,
		    int onwho, int stp)
{
    int j, toind;
    int mask = (onwho ^ 1) << 5;
    int ekcoord = board->playlist['K' | mask].list[0];
    int kcoord = board->playlist['K' | (onwho << 5)].list[0];
    uint8 to;
    struct srclist dirlist;
    char *coord = board->coord;
    attlist->lgh = 0;
    /* check knight attack */
    for (j = 0; j < board->playlist['N' | mask].lgh; j++)
	if (board->dir[from] [board->playlist['N' | mask].list[j]] == 8)
	{
	    if (stp)
		return 1;
	    else addsrccoord(attlist, board->playlist['N' | mask].list[j]);
	}

    /* check sliding attack */
    genslide(board, &dirlist, from, onwho);
    for (j = 0; j < dirlist.lgh; j++)
	if ((turn == onwho && nopose(board, from, dirlist.list[j], kcoord)) ||
	    (turn != onwho && nopose(board, from, dirlist.list[j], FLAG)))
	{
	    if (stp)
		return 1;
	    else
		addsrccoord(attlist, dirlist.list[j]);
	}
    /* check king attack, but *only* in the case of *enemy* attack */
    /* (we already find possible king moves in kingmove()). */
    if (turn == onwho)
    {
	if (Abs(Rank(ekcoord) - Rank(from)) < 2 &&
	    Abs(File(ekcoord) - File(from)) < 2)
	    return 1; /* king can never doublecheck. */
    }

    /* check pawn attack... */
    toind = (onwho << 2) + 1;
    to = *(board->moves[from] [toind]);
    /* if attacked square unnoc'd, and *friend* attack, want pawn advances. */
    if (turn != onwho && check(coord[from], onwho) == 2)
    {
	if (to != FLAG && check(coord[to], onwho) == 1 &&
	    tolower(coord[to]) == 'p') /* pawn ahead */
	{
	    if (stp)
		return 1;
	    else addsrccoord(attlist, to);
	}
	/* now try e2e4 moves. */
	if (Rank(from) == 4 - onwho && check(coord[to], onwho) == 2)
	{
	    if (check(coord[(to = *(board->moves[from] [toind] + 1))], onwho)
		== 1 &&
		tolower(coord[to]) == 'p')
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
	    to = *(board->moves[from] [toind]);
	    if (to != FLAG && check(coord[to], onwho) == 1 &&
		tolower(coord[to]) == 'p') /* enemy on diag */
	    {
		if (stp)
		    return 1;
		else addsrccoord(attlist, to);
	    }

	    toind += 2;
	} while (toind & 2);
	if (from == board->ebyte) /* have to include en passant */
	    for (j = -1; j < 2; j += 2)
		if (check(coord[from + j], onwho) == 1 &&
		    tolower(coord[from + j]) == 'p' &&
		    Rank(from) == Rank(from+j))
		    /* operates on principle that if we check for *our*
		       attacks, then we want every one. */
		    addsrccoord(attlist, from + j);
    }
    return 0;	/* gee.  Guess we're not attacked... or we filled the list */
}


/* unconditional add.  Better watch... */
static void addmove(struct mlist *mvlist, BoardT *board, int from, int to,
		    int promote, int dc)
{
    uint8 *comstr;
    int chk, chkpiece, dir;
    int ekcoord = board->playlist['K' | (!(board->ply & 1) << 5)].list[0];

    /* now see if moving piece actually does any checking. */
    if (tolower(board->coord[from]) == 'k' && Abs(to - from) == 2)
	/* castling manuever. !@#$% special case */
    {
	dir = board->dir[(to + from) >> 1] [ekcoord];
	chk = ((dir & 1) && dir != DIRFLAG &&
	       nopose(board, (to + from) >> 1, ekcoord, FLAG)) ?
	    (to + from) >> 1 : FLAG;
    }
    else
    {
	chkpiece = promote ? promote : board->coord[from];
	dir = board->dir[to] [ekcoord];
	switch(tolower(chkpiece))
	{
	case 'n':
	    chk = dir == 8 ? to : FLAG;
	    break;
	case 'q':
	    chk = dir != DIRFLAG && dir < 8 &&
		nopose(board, to, ekcoord, from) ? to : FLAG;
	    break;
	case 'b':
	    chk = !(dir & 1) && dir < 8 &&
		nopose(board, to, ekcoord, from) ? to : FLAG;
	    break;
	case 'r': chk = (dir & 1) && dir != DIRFLAG &&
		      nopose(board, to,	ekcoord, from) ? to : FLAG;
	    break;
	case 'p': chk = Abs(File(ekcoord) - File(to)) == 1 &&
		      Rank(to) - Rank(ekcoord) == ((board->ply & 1)
						   << 1) - 1 ? to : FLAG;
	    break;
	default : chk = FLAG; /* ie, king. */
	}
    }
    if (board->coord[to] || promote || dc != FLAG || chk != FLAG ||
	(board->level - board->depth > 1 &&
	 Abs(board->hist[board->ply & 1] [from] [to]
	     - board->ply) < board->hiswin))
	/* capture or promo or history move w/ depth or check?  Want good
	   spot. */
	/* Should change when/if we add mobility func. */
    {
	memcpy(mvlist->list[(mvlist->lgh)++],
	       mvlist->list[mvlist->insrt], 4);
	comstr = mvlist->list[(mvlist->insrt)++];
    }
    else comstr = mvlist->list[(mvlist->lgh)++];
    comstr[0] = from; /* translate to command */
    comstr[1] = to;
    comstr[2] = promote;
    comstr[3] = chk == FLAG ? dc :
	dc == FLAG ? chk : DISCHKFLAG;
    /* actually follows FLAG:coord:DISCHKFLAG convention.
       Read: no check, check, doublecheck. */
}


static void promo(struct mlist *mvlist, BoardT *board, int from, int to,
		  int turn, int dc)
/* an alias for multiple add w/promo.  ASCII-dependent. */
{
    int mask = turn << 5;
    addmove(mvlist, board, from, to, (int) 'Q' | mask, dc);
    addmove(mvlist, board, from, to, (int) 'N' | mask, dc);
    addmove(mvlist, board, from, to, (int) 'R' | mask, dc);
    addmove(mvlist, board, from, to, (int) 'B' | mask, dc);
}


static void cappose(struct mlist *mvlist, BoardT *board, uint8 attcoord,
		    union plist *pinlist, int turn, uint8 kcoord,
		    struct srclist *dclist)
/* king in check by one piece.  Find moves that capture or interpose. */
{
    char *j;
    int i, pintype;
    struct srclist attlist;
    uint8 src, dest;
    int dc;
    j = board->moves[attcoord] [board->dir[attcoord] [kcoord]];

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
	    if (tolower(board->coord[src]) == 'p' &&
		Rank(src) == Rank(attcoord)) 	/* spec case: en passant */
		dest += (-2*turn+1) << 3;

	    /* cprintf("Trying %c%c%c%c ", File(src) + 'a', Rank(src) + '1',
	       File(dest) + 'a', Rank(dest) + '1'); */
	    pintype = pinlist->c[src];
	    if (pintype < 0 ||
		/* pinned knights simply cannot move. */
		(tolower(board->coord[src]) != 'n' &&
		 pintype == (board->dir[src] [dest] & 3)))
		/* check pin. */
	    {
		dc = findcoord(dclist, src);
		if (dc != FLAG && board->dir[src] [dc] ==
		    board->dir[dest] [dc])
		    dc = FLAG;
		if (tolower(board->coord[src]) == 'p' &&
		    (dest < 8 || dest > 55))
		    promo(mvlist, board, src, dest, turn, dc);
		else addmove(mvlist, board, src, dest, 0, dc);
	    }
	}
	if (tolower(board->coord[attcoord]) == 'n')
	    break; /* can't attack interposing squares in n's case. */
	attcoord = *j;
	j++;  /* get next interposing place. */
    }
}


static void gendclist(BoardT *board, struct srclist *dclist, int ekcoord,
		      int turn)
/* generates a list 'dclist' containing (all possible) 2-tuples consisting of:
   -- the source coordinate of a piece capable of giving discovered check
   -- the source coordinate of the corresponding checking piece.
*/
{
    int a, test;
    struct srclist attlist;
    uint8 *x;
    dclist->lgh = 0;	/* reset list. */
    /* generate our sliding attacks on enemy king. */
    genslide(board, &attlist, ekcoord, turn ^ 1);
    /* check the possible dirs for a discovered check piece. */
    for (a = 0; a < attlist.lgh; a++)
    {
	x = board->moves[attlist.list[a]]
	    [board->dir[attlist.list[a]] [ekcoord]];
	while ((test = check(board->coord[*x], turn)) == 2)
	    x++;
	if (test) continue; /* dc piece must be friend. */
	if (nopose(board, *x, ekcoord, FLAG)) /* yes, it is a dc piece */
	{
	    addsrccoord(dclist, *x);
	    addsrccoord(dclist, attlist.list[a]);
	}
    }
}


static void nightmove(struct mlist *mvlist, BoardT *board, int from, int turn,
		      int pintype, int dc)
{
    uint8 *moves = board->moves[from] [8];

    if (pintype > -1)
	return;	/* no way we can move w/out checking king. */

    for (; *moves != FLAG; moves++)
    {
	if (check(board->coord[*moves], turn) > 0)
	    addmove(mvlist, board, from, *moves, 0, dc);
    }
}


/* probes sliding moves.  Piece should not be pinned in this direction. */
static void probe(struct mlist *mvlist, BoardT *board, uint8 *moves,
		  int from, int turn, int dc)
{
    int i;
    for (; *moves != FLAG; moves++)
    {
	if ((i = check(board->coord[*moves], turn)) > 0)
	    /* enemy or unoccupied */
	    addmove(mvlist, board, from, *moves, 0, dc);
	if (i < 2) /* Occupied.  Can't probe further. */
	    break;
    }
}


static void brmove(struct mlist *mvlist, BoardT *board, int from, int turn,
		   int pintype, int start, int dc)
{
    while (start < 8)
    {
	if (pintype < 0 || pintype == (start & 3))
	    /* piece pinned in this dir, or not pinned */
	    probe(mvlist, board, board->moves[from] [start], from, turn, dc);
	start += 2;
    }
}


static void kingmove(struct mlist *mvlist, BoardT *board, int from, int turn,
		     int dc)
{
    int i;
    uint8 to;
    struct srclist attlist; /* filler */
    char *coord = board->coord;

    for (i = 0; i < 8; i++)
    {
	to = *(board->moves[from] [i]);
	if (to != FLAG &&
	    check(coord[to], turn) > 0 &&
	    !attacked(&attlist, board, to, turn, turn, 1))
	    addmove(mvlist, board, from, to, 0,
		    dc == FLAG ? FLAG :
		    board->dir[from] [dc] == board->dir[to] [dc] ? FLAG :
		    dc);
    }
    /* check for kingside castle */
    if ((board->cbyte >> (6 - (turn << 1))) & 1 && !coord[from + 1] &&
	!coord[from + 2] &&
	!attacked(&attlist, board, from + 2, turn, turn, 1) &&
	!attacked(&attlist, board, from + 1, turn, turn, 1) &&
	!attacked(&attlist, board, from, turn, turn, 1)
	/* not in check */ )
	addmove(mvlist, board, from, from + 2, 0, FLAG);
    /* check queenside castle */
    if ((board->cbyte >> (7 - (turn << 1))) & 1 && !coord[from - 1] &&
	!coord[from - 2] && !coord[from - 3] &&
	!attacked(&attlist, board, from - 1, turn, turn, 1) &&
	!attacked(&attlist, board, from - 2, turn, turn, 1) &&
	!attacked(&attlist, board, from, turn, turn, 1))
	addmove(mvlist, board, from, from - 2, 0, FLAG);
}



static int enpassdc(BoardT *board, int capPawnCoord)
{
    int turn = board->ply & 1;
    int ekcoord = board->playlist['K' | ((turn ^ 1) << 5)].list[0];
    struct srclist attlist;
    int ebyte = board->ebyte; // shorthand.
    int i;
    int a;

    if (board->dir[ebyte] [ekcoord] < 8 &&
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
	    if (board->dir[a] [ebyte] == board->dir[ebyte] [ekcoord] &&
		nopose(board, a, ebyte, capPawnCoord))
		return a;
	}
    }

    return FLAG;
}


static void pawnmove(struct mlist *mvlist, BoardT *board, uint8 *moves[],
		     int from, int turn, int pintype, int dc)
{
    int toind = (turn << 2);
    int to, to2;
    char *coord = board->coord;
    int enpass = 0;

    /* generate captures (if any). */
    do
    {
	to = *moves[toind];
	if (to != FLAG && (pintype < 0 || pintype == (toind & 3)))
	{
	    
	    if (check(coord[to], turn) == 1 /* enemy on diag */ ||
                /* en passant */
		(enpass = (to - 8 + (turn << 4) == board->ebyte)))
	    {
		/* can we promote? */
		if (to > 55 || to < 8)
		    promo(mvlist, board, from, to, turn,
			  dc == FLAG ? FLAG :
			  board->dir[from] [dc] == board->dir[to] [dc] ?
			  FLAG : dc);
		else
		{
		    if (enpass && dc == FLAG)
		    {
			/* with en passant, must take into account check
			   created when captured pawn was pinned.  Setting 'dc'
			   is slightly hacky but it works. */
			dc = enpassdc(board, from);
		    }
		    addmove(mvlist, board, from, to, 0,
			    dc == FLAG ? FLAG :
			    board->dir[from] [dc] == board->dir[to] [dc] ?
			    FLAG : dc);
		}
	    }
	}
	toind += 2;
    } while (toind & 3);

    /* generate pawn pushes. */
    toind -= 3;
    to = *moves[toind];
    if (check(coord[to], turn) == 2 && (pintype < 0 ||
					pintype == (toind & 3)))
	/* space ahead */
    {	/* can we promote? */
	if (to > 55 || to < 8)
	    promo(mvlist, board, from, to, turn,
		  dc == FLAG ? FLAG :
		  board->dir[from] [dc] == board->dir[to] [dc] ? FLAG :
		  dc);
	else
	{
	    /* check e2e4-like moves */
	    if ((from < 16 || from > 47) &&
		check(coord[(to2 = *(moves[toind]+1))], turn) == 2)
		addmove(mvlist, board, from, to2, 0,
			dc == FLAG ? FLAG :
			board->dir[from] [dc] == board->dir[to2] [dc] ?
			FLAG : dc);
	    /* add e2e3-like moves. */
	    addmove(mvlist, board, from, to, 0,
		    dc == FLAG ? FLAG :
		    board->dir[from] [dc] == board->dir[to] [dc] ? FLAG :
		    dc);
	}
    }
}


void genmlist(struct mlist *mvlist, BoardT *board, int turn)
{
    int x, i;
    int mask = turn << 5;
    struct srclist dclist;
    union plist pinlist;
    int kcoord = board->playlist['K' | mask].list[0];
    int ekcoord = board->playlist['K' | (!turn << 5)].list[0];
    mvlist->lgh = 0;
    mvlist->insrt = 0;

    /* generate list of pieces that can potentially give
       discovered check. A very short list. Non-sorted.*/
    gendclist(board, &dclist, ekcoord, turn);

    /* find all king pins (no pun intended :) */
    findpins(board, kcoord, &pinlist, turn);

    if (board->ncheck[turn] != FLAG && board->ncheck[turn] != DISCHKFLAG)
    {
	/* we're in check by 1 piece, so capture or interpose. */
	cappose(mvlist, board, board->ncheck[turn], &pinlist, turn, kcoord,
		&dclist);
    }
    else if (board->ncheck[turn] == FLAG) /* otherwise double check,
					     only kingmoves poss. */
    {
	/* we're not in check at this point. */
	/* generate pawn moves. */
	for (i = 0; i < board->playlist['P' | mask].lgh; i++)
	{
	    x = board->playlist['P' | mask].list[i];
	    pawnmove(mvlist, board, board->moves[x], x,
		     turn, pinlist.c[x], findcoord(&dclist, x));
	}
	/* generate queen moves. */
	/* Note it is never possible for qmove to result in discovered
	   check.  We can optimize for this. */
	for (i = 0; i < board->playlist['Q' | mask].lgh; i++)
	{
	    x = board->playlist['Q' | mask].list[i];
	    brmove(mvlist, board, x, turn, pinlist.c[x], 1, FLAG);
	    brmove(mvlist, board, x, turn, pinlist.c[x], 0, FLAG);
	}
	/* generate bishop moves. */
	for (i = 0; i < board->playlist['B' | mask].lgh; i++)
	{
	    x = board->playlist['B' | mask].list[i];
	    brmove(mvlist, board, x, turn, pinlist.c[x], 0,
		   findcoord(&dclist, x));
	}
	/* generate night moves. */
	for (i = 0; i < board->playlist['N' | mask].lgh; i++)
	{
	    x = board->playlist['N' | mask].list[i];
	    nightmove(mvlist, board, x, turn, pinlist.c[x],
		      findcoord(&dclist, x));
	}
	/* generate rook moves. */
	for (i = 0; i < board->playlist['R' | mask].lgh; i++)
	{
	    x = board->playlist['R' | mask].list[i];
	    brmove(mvlist, board, x, turn, pinlist.c[x], 1,
		   findcoord(&dclist, x));
	}
    } /* end 'no check' else */

    /* generate valid king moves. */
    kingmove(mvlist, board, kcoord, turn, findcoord(&dclist, kcoord));

    /* Selection Sorting the captures does no good, empirically. */
    /* But probably will do good when we extend captures. */
}


int check(char piece, int turn)
/* returns 0 if friend, 1 if enemy, 2 if unoccupied */
/* White's turn = 0. Black's is 1. */
/* ASCII-dependent. */
{
    return !piece ? 2 : (((piece >> 5) & 1) != turn);
}
