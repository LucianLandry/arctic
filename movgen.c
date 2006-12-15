#include <stdio.h>
#include <conio.h>
#include <ctype.h>
#include "ref.h"

void findpins(struct brd *board, char coord[], char *moves[],
	union plist *pinlist, int turn)
{    int a, odd, test, i;
	char *x;
	struct srclist dirlist;
	int kcoord = board->playlist['K' | (turn << 5)].list[0];
	/* initialize pin array. */
	for (i = 0; i < 16; i++)
		pinlist->l[i] = -1;
	genslide(board, &dirlist, kcoord, turn);
	/* only check the possible pin dirs. */
	for (a = 0; a < dirlist.lgh; a++)
	{    x = moves[board->dir[kcoord] [dirlist.list[a]]];
		while (*x != FLAG && (test = check(coord[*x], turn)) == 2)
			x++;
		if (*x == FLAG || test) continue; /* pinned piece must be friend. */
		i = *x; /* location of poss. pinned piece */
		do {
			x++; /* find next occ'd space */
		} while (*x != FLAG && (test = check(coord[*x], turn)) == 2);
		if (*x == FLAG || test != 1) continue;	/* has to be enemy piece */
		odd = board->dir[kcoord] [dirlist.list[a]] & 1;
		if ((test = tolower(coord[*x])) != 'q' && !(odd &&
			test == 'r') && (odd || test != 'b') /* only
			possibility left */)
			continue;	/* not right kind */
		/* by process of elimination, we have pinned piece. */
		pinlist->c[i] = board->dir[kcoord] [dirlist.list[a]] & 3;
			/* set pintype */
/*		cprintf("pn:%c%c", File(i) + 'a',	Rank(i) + '1'); */
	}
}

void genslide(struct brd *board, struct srclist *dirlist, int from, int turn)
/* generate all possible enemy (!turn) sliding attack locations on 'from',
	whether blocked or not.  Note right now, we can generate a dir multiple x.
	We should probly change this. */
{	int to, i;
	int mask = (turn ^ 1) << 5;
	dirlist->lgh = 0;	/* init list. */
	/* find queen sliding attacks. */
	for (i = 0; i < board->playlist['Q' | mask].lgh; i++)
	{	to = board->playlist['Q' | mask].list[i];
		if (!(board->dir[from] [to] & 8))
			addsrccoord(dirlist, to);
	}
	/* find rook sliding attacks. */
	for (i = 0; i < board->playlist['R' | mask].lgh; i++)
	{	to = board->playlist['R' | mask].list[i];
		if (board->dir[from] [to] & 1 && board->dir[from] [to] > -1)
			addsrccoord(dirlist, to);
	}
	/* find bishop sliding attacks. */
	for (i = 0; i < board->playlist['B' | mask].lgh; i++)
	{	to = board->playlist['B' | mask].list[i];
		if (!(board->dir[from] [to] & 1) && board->dir[from] [to] < 8)
			addsrccoord(dirlist, to);
	}
}

void cappose(struct mlist *mvlist, struct brd *board, char *moves[] [9],
	char attcoord,	union plist *pinlist, int turn, char kcoord,
	struct srclist *dclist)
/* king in check by one piece.  Find moves that capture or interpose. */
{    char *j;
	int i, pintype;
	struct srclist attlist;
	char src, dest;
	int dc;
     j = moves[attcoord] [board->dir[attcoord] [kcoord]];

	while(attcoord != kcoord)
	{    /* cprintf("%c%c ", File(attcoord) + 'a', Rank(attcoord) + '1'); */
		attacked(&attlist, board, moves[attcoord], attcoord, turn,
			turn ^ 1, 0);
		/* have to add possible moves right now. */
		/* so search the pinlist for possible pins on the attackers. */

		for (i = 0; i < attlist.lgh; i++)
		{    src = attlist.list[i];
			dest = attcoord;
               if (tolower(board->coord[src]) == 'p' &&
				Rank(src) == Rank(attcoord)) 	/* spec case: en passant */
				dest += (-2*turn+1) << 3;

			/* cprintf("Trying %c%c%c%c ", File(src) + 'a', Rank(src) + '1',
				File(dest) + 'a', Rank(dest) + '1'); */
			pintype = pinlist->c[src];
			if (pintype < 0 || pintype == (board->dir[src] [dest] & 3))
				/* check pin. */
			{	dc = findcoord(dclist, src);
				if (dc != FLAG && board->dir[src] [dc] ==
					 board->dir[dest] [dc])
					 dc = FLAG;
				if (board->coord[src] == 'p' && (dest < 8 || dest > 55))
					promo(mvlist, board, src, dest, turn, dc);
				else add(mvlist, board, src, dest, 0, dc);
               }
		}
		if (tolower(board->coord[attcoord]) == 'n')
			break; /* can't attack interposing squares in n's case. */
		attcoord = *j;
		j++;  /* get next interposing place. */
	}
}



void gendclist(struct brd *board, struct srclist *dclist,
	int ekcoord, int turn)
/* generates a list containing all pieces capable of possibly giving
	discovered check and their corresponding checking pieces. */
{    int a, test;
	struct srclist attlist;
	char *x;
	dclist->lgh = 0;	/* reset list. */
	/* generate our sliding attacks on enemy king. */
	genslide(board, &attlist, ekcoord, !turn);
	/* check the possible dirs for a discovered check piece. */
	for (a = 0; a < attlist.lgh; a++)
	{    x = board->moves[attlist.list[a]]
					 [board->dir[attlist.list[a]] [ekcoord]];
		while ((test = check(board->coord[*x], turn)) == 2)
			x++;
		if (test) continue; /* dc piece must be friend. */
		if (nopose(board, *x, ekcoord, FLAG)) /* yes, it is a dc piece */
          {    addsrccoord(dclist, *x);
               addsrccoord(dclist, attlist.list[a]);
		}
	}
}


void genmlist(struct mlist *mvlist, struct brd *board, int turn)
{	int x, i, pintype;
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

	/* generate valid king moves. */
	kingmove(mvlist, board, board->moves, kcoord, turn,
		findcoord(&dclist, kcoord));

	/* find all king pins (npi :) */
	findpins(board, board->coord, board->moves[kcoord], &pinlist, turn);

	if (board->ncheck[turn] != FLAG && board->ncheck[turn] > -1)
	/* we're in check by 1 piece, so capture or interpose. */
		cappose(mvlist, board, board->moves,
			board->ncheck[turn], &pinlist, turn, kcoord, &dclist);
	else if (board->ncheck[turn] == FLAG) /* otherwise double check,
									 only kingmoves poss. */
	{	/* we're not in check at this point. */
		/* generate pawn moves. */
		for (i = 0; i < board->playlist['P' | mask].lgh; i++)
		{	x = board->playlist['P' | mask].list[i];
			pawnmove(mvlist, board, board->moves[x], x,
					turn, pinlist.c[x], findcoord(&dclist, x));
		}
          /* generate queen moves. */
		/* Note it is never possible for qmove to result in discovered
			check.  We can optimize for this. */
		for (i = 0; i < board->playlist['Q' | mask].lgh; i++)
		{	x = board->playlist['Q' | mask].list[i];
			brmove(mvlist, board, board->moves[x], x,
					turn, pinlist.c[x], 1, FLAG);
			brmove(mvlist, board, board->moves[x], x,
					turn, pinlist.c[x], 0, FLAG);
		}
		/* generate bishop moves. */
		for (i = 0; i < board->playlist['B' | mask].lgh; i++)
		{	x = board->playlist['B' | mask].list[i];
			brmove(mvlist, board, board->moves[x], x,
					turn, pinlist.c[x], 0, findcoord(&dclist, x));
		}
		/* generate night moves. */
		for (i = 0; i < board->playlist['N' | mask].lgh; i++)
		{	x = board->playlist['N' | mask].list[i];
			nightmove(mvlist, board, board->moves[x] [8], x,
					turn, pinlist.c[x], findcoord(&dclist, x));
		}
		/* generate rook moves. */
		for (i = 0; i < board->playlist['R' | mask].lgh; i++)
		{	x = board->playlist['R' | mask].list[i];
			brmove(mvlist, board, board->moves[x], x,
					turn, pinlist.c[x], 1, findcoord(&dclist, x));
		}
	} /* end 'no check' else */
	/* Selection Sorting the captures does no good, empirically. */
	/* But probably will do good when we extend captures. */
}


void addsrccoord(struct srclist *attlist, int from)
{	attlist->list[(attlist->lgh)++] = from; }

int findcoord(struct srclist *attlist, int targ)
/* returns FLAG if targ not found in attlist, otherwise checking piece
	(stored after targ by gendclist(). */
{	int i;
	for (i = 0; i < attlist->lgh; i += 2)
		if (attlist->list[i] == targ)
			return attlist->list[i + 1];
	return FLAG;
}

int attacked(struct srclist *attlist, struct brd *board, char *moves[],
	int from, int turn, int onwho, int stp)
{	int j, k, odd, toind;
	int mask = (onwho ^ 1) << 5;
     int ekcoord = board->playlist['K' | mask].list[0];
	int kcoord = board->playlist['K' | (onwho << 5)].list[0];
     char *i, to;
	struct srclist dirlist;
	char *coord = board->coord;
	attlist->lgh = 0;
	/* check knight attack */
	for (j = 0; j < board->playlist['N' | mask].lgh; j++)
		if (board->dir[from] [board->playlist['N' | mask].list[j]] == 8)
			if (stp)
				return 1;
			else addsrccoord(attlist, board->playlist['N' | mask].list[j]);

	/* check sliding attack */
	genslide(board, &dirlist, from, onwho);
	for (j = 0; j < dirlist.lgh; j++)
		if (turn == onwho && nopose(board, from, dirlist.list[j], kcoord)
			|| turn != onwho && nopose(board, from, dirlist.list[j], FLAG))
			if (stp)
				return 1;
			else
				addsrccoord(attlist, dirlist.list[j]);
	/* check king attack, but *only* in the case of *enemy* attack */
	/* (we already find possible king moves in kingmove()). */
	if (turn == onwho)
	{	if (Abs(Rank(ekcoord) - Rank(from)) < 2 &&
			Abs(File(ekcoord) - File(from)) < 2)
			return 1; /* king can never doublecheck. */
	}

	/* check pawn attack... */
	toind = (onwho << 2) + 1;
	to = *moves[toind];
	/* if attacked square unnoc'd, and *friend* attack, want pawn advances. */
	if (turn != onwho && check(coord[from], onwho) == 2)
	{	if (to != FLAG && check(coord[to], onwho) == 1 &&
			tolower(coord[to]) == 'p') /* pawn ahead */
			if (stp)
				return 1;
			else addsrccoord(attlist, to);
          /* now try e2e4 moves. */
		if (Rank(from) == 4 - onwho && check(coord[to], onwho) == 2)
		{	if (check(coord[(to = *(moves[toind] + 1))], onwho) == 1 &&
				tolower(coord[to]) == 'p')
				if (stp)
					return 1;
				else addsrccoord(attlist, to);
		}
	}
	else	/* otherwise, want p captures. */
	{	toind = onwho << 2;
		do
		{    to = *moves[toind];
			if (to != FLAG && check(coord[to], onwho) == 1 &&
					tolower(coord[to]) == 'p') /* enemy on diag */
					if (stp)
						return 1;
					else addsrccoord(attlist, to);

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

void nightmove(struct mlist *mvlist, struct brd *board, char *moves, int from,
	int turn, int pintype, int dc)
{	if (pintype > -1)
		return;	/* no way we can move w/out checking king. */

	for (; *moves != FLAG; moves++)
	{	if (check(board->coord[*moves], turn) > 0)
			add(mvlist, board, from, *moves, 0, dc);
	}
}

void probe(struct mlist *mvlist, struct brd *board, char *moves, int from,
	int turn, int dc)
/* probes sliding moves.  Piece should not be pinned in this direction. */
{	int i;
	for (; *moves != FLAG; moves++)
	{	if ((i = check(board->coord[*moves], turn)) > 0)
			/* enemy or unoccupied */
			add(mvlist, board, from, *moves, 0, dc);
		if (i < 2) /* Occupied.  Can't probe further. */
			break;
	}
}

void brmove(struct mlist *mvlist, struct brd *board, char *moves[], int from,
	int turn,	int pintype, int start, int dc)
{    while (start < 8)
	{	if (pintype < 0 || pintype == (start & 3))
		/* piece pinned in this dir or not pinned */
			probe(mvlist, board, moves[start], from, turn, dc);
		start += 2;
	}
}

void kingmove(struct mlist *mvlist, struct brd *board, char *moves[] [9],
	int from, int turn, int dc)
{	int i;
	char to;
	struct srclist attlist; /* filler */
	char *coord = board->coord;
	for (i = 0; i < 8; i++)
	{	to = *moves[from] [i];
		if (to != FLAG && check(coord[to], turn) > 0 && !attacked(&attlist,
				board, moves[to], to, turn, turn, 1))
          		add(mvlist, board, from, to, 0,
					dc == FLAG ? FLAG :
					board->dir[from] [dc] == board->dir[to] [dc] ? FLAG :
						dc);
	}
	/* check for kingside castle */
     if ((board->cbyte >> 6 - (turn << 1)) & 1 && !coord[from + 1] &&
		!coord[from + 2] &&
		!attacked(&attlist, board, moves[from+2], from + 2, turn, turn, 1) &&
		!attacked(&attlist, board, moves[from+1], from + 1, turn, turn, 1) &&
		!attacked(&attlist, board, moves[from], from, turn, turn, 1)
		/* not in check */ )
		add(mvlist, board, from, from + 2, 0, FLAG);
	/* check queenside castle */
	if ((board->cbyte >> 7 - (turn << 1)) & 1 && !coord[from - 1] &&
		!coord[from - 2] &&	!coord[from - 3] &&
		!attacked(&attlist, board, moves[from-1], from - 1, turn, turn, 1) &&
		!attacked(&attlist, board, moves[from-2], from - 2, turn, turn, 1) &&
		!attacked(&attlist, board, moves[from], from, turn, turn, 1))
		add(mvlist, board, from, from - 2, 0, FLAG);
}

void pawnmove(struct mlist *mvlist, struct brd *board, char *moves[], int from,
	int turn, int pintype, int dc)
{	int toind = (turn << 2) + 1;
	int to = *moves[toind];
	char *coord = board->coord;
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
		{	add(mvlist, board, from, to, 0,
				dc == FLAG ? FLAG :
				board->dir[from] [dc] == board->dir[to] [dc] ? FLAG :
					dc);
			/* now check e2e4-like moves */
			if ((from < 16 || from > 47) &&
				check(coord[(to = *(moves[toind]+1))], turn) == 2)
				add(mvlist, board, from, to, 0,
					dc == FLAG ? FLAG :
					board->dir[from] [dc] == board->dir[to] [dc] ?
						FLAG : dc);
		}
	}
	toind--;
	do
	{    to = *moves[toind];
		if (to != FLAG && (pintype < 0 || pintype == (toind & 3)))
		{	if (check(coord[to], turn) == 1 ||  /* enemy on diag */
				to - 8 + (turn << 4) == board->ebyte) /* en passant */
				/* can we promote? */
				if (to > 55 || to < 8)
					promo(mvlist, board, from, to, turn,
						dc == FLAG ? FLAG :
						board->dir[from] [dc] == board->dir[to] [dc] ?
							FLAG : dc);
				else add(mvlist, board, from, to, 0,
						dc == FLAG ? FLAG :
						board->dir[from] [dc] == board->dir[to] [dc] ?
							FLAG : dc);
		}
		toind += 2;
	} while (toind & 3);
}


void promo(struct mlist *mvlist, struct brd *board, int from, int to, int turn,
	int dc)
/* an alias for multiple add w/promo.  ASCII-dependent. */
{	int mask = turn << 5;
	add(mvlist, board, from, to, (int) 'N' | mask, dc);
	add(mvlist, board, from, to, (int) 'B' | mask, dc);
	add(mvlist, board, from, to, (int) 'R' | mask, dc);
	add(mvlist, board, from, to, (int) 'Q' | mask, dc);
}


void add(struct mlist *mvlist, struct brd *board, int from, int to,
	int promote, int dc)
/* unconditional add.  Better watch... */
{	char *comstr;
	int chk, chkpiece, dir;
     int ekcoord = board->playlist['K' | (!(board->move & 1) << 5)].list[0];

	/* now see if moving piece actually does any checking. */
	if (tolower(board->coord[from]) == 'k' && Abs(to - from) == 2)
	/* castling manuever. !@#$% special case */
	{	dir = board->dir[(to + from) >> 1] [ekcoord];
          chk = (dir & 1) && dir > -1 && nopose(board, (to + from) >> 1,
			ekcoord, FLAG) ? (to + from) >> 1 : FLAG;
	}
	else
	{	chkpiece = promote ? promote : board->coord[from];
		dir = board->dir[to] [ekcoord];
		switch(tolower(chkpiece))
		{	case 'n':	chk = dir == 8 ? to : FLAG;
					break;
			case 'q': chk = dir > -1 && dir < 8 && nopose(board, to,
						ekcoord, from) ? to : FLAG;
					break;
			case 'b': chk = !(dir & 1) && dir < 8 && nopose(board, to,
						ekcoord, from) ? to : FLAG;
					break;
			case 'r': chk = (dir & 1) && dir > -1 && nopose(board, to,
						ekcoord, from) ? to : FLAG;
					break;
			case 'p': chk = Abs(File(ekcoord) - File(to)) == 1 &&
						Rank(to) - Rank(ekcoord) == ((board->move & 1)
						<< 1) - 1 ? to : FLAG;
					break;
			default : chk = FLAG; /* ie, king. */
		}
	}
	if (board->coord[to] || promote || dc != FLAG || chk != FLAG ||
		board->level - board->depth > 1 &&
		Abs(board->hist[board->move & 1] [from] [to]
			- board->move) < board->hiswin)
		/* capture or promo or history move w/ depth or check?  Want good
			 spot. */
	/* Should change when/if we add mobility func. */
	{	ncopy(mvlist->list[(mvlist->lgh)++],
			 mvlist->list[mvlist->insrt], 4);
		comstr = mvlist->list[(mvlist->insrt)++];
	}
	else comstr = mvlist->list[(mvlist->lgh)++];
	comstr[0] = from; /* translate to command */
	comstr[1] = to;
	comstr[2] = promote;
	comstr[3] = chk == FLAG ? dc :
				dc == FLAG ? chk : -1;
				/* actually follows FLAG:coord:-1 convention.
				   Read: no check, check, doublecheck. */
}


int check(char piece, int turn)
/* returns 0 if friend, 1 if enemy, 2 if unoccupied */
/* White's turn = 0. Black's is 1. */
/* ASCII-dependent. */
{    return !piece ? 2 : (((piece >> 5) & 1) != turn);
}
