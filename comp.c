#include <stdio.h> /* has NULL value */
#include "ref.h"
#include <conio.h>	/* for 'Thinking'... */

int worth(char a)
/* in absolute terms, from white's point o' view. */
{	switch(tolower(a)) {
		case 'p': return 1;
		case 'b':
		case 'n': return 3;
		case 'r': return 5;
		case 'q': return 9;
		case 'k': return -1; /*error condition. */
		default:	return 0;
	}
}

int eval(char coord[], int turn)
{	int i, j, result;
	result = 0;
	for (i = 0; i < 64; i++)
		if (!check(coord[i], turn))	/* friend */
			result += worth(coord[i]);
		else if (check(coord[i], turn) == 1) /* enemy */
			result -= worth(coord[i]);
	return result;
}

void computermove(struct brd *board, int *show, int col[])
{	int value, strngth;
	long nodes, ack;
	char pv[20];
	char comstr[4];
	board->depth = 0;	/* start search from root depth. */
	gotoxy(26, 24);
	textcolor(RED);
	cprintf("Thinking");
	strngth = eval(board->coord, board->move & 1);
	/* get starting strength, though it doesn't really matter */
	ack = 0;
	nodes = 0;
	value = minimax(board, comstr, board->move & 1, 1000, strngth,
					&nodes, &ack, show, col, pv);
	gotoxy(1, 1);
	cprintf("%ld %ld", nodes, ack);
	if (value < -500) /* we're in a really bad situation */
		barf("Computer resigns.");
	else if (comstr[0] != FLAG) /* check for valid move; may be stalemate */
	{	makemove(board, comstr, board->coord[comstr[1]]);
		update(board->coord, col);
	}
}


int minimax(struct brd *board, char comstr[], int turn, int alpha, int strgh,
	long *nodes, long *ack, int *show, int col[], char *goodpv)
/* evaluates a given board position from {turn}'s point of view. */
{    int val, newval, i, besti, cap;
	char newpv[20];
	int oldval = -1000;
	char ocbyte, oebyte;
	char newstr[4], cappiece;
	struct mlist mvlist;
	comstr[0] = FLAG;
	genmlist(&mvlist, board, turn);
	(*ack)++;
	/* Note:  ncheck for any side guaranteed to be correct *only* after
			the other side has made its move. */
	if (!(mvlist.lgh))
     	if (board->ncheck[turn] != FLAG)
			return -1000;	/* checkmate */
		else return 0;		/* stalemate */
	val = -1000;
	turn ^= 1;		/* switch sides */
	for (i = 0; i < mvlist.lgh; i++)
	{	ocbyte = board->cbyte;
		oebyte = board->ebyte;
		cappiece = board->coord[mvlist.list[i] [1]];
		makemove(board, mvlist.list[i], cappiece);
		(*nodes)++;
		strgh += (cap = worth(cappiece));
		if (cap == -1) /* captured king */
		{	update(board->coord, col);
			gotoxy(1,1);
			cprintf("move was %c%c%c%c", File(mvlist.list[i] [0]) + 'a',
				Rank(mvlist.list[i] [0]) + '1',
				File(mvlist.list[i] [1]) + 'a',
				Rank(mvlist.list[i] [1]) + '1');
			barf("diagnostic.");
		}

		if (*show)
		{	if (concheck(board))
			{	if (barf("show results.") == 's')
					*show = 0;
				update(board->coord, col);
				gotoxy(1,1);
				cprintf("move was %c%c%c%c",
				File(mvlist.list[i] [0]) + 'a',
				Rank(mvlist.list[i] [0]) + '1',
				File(mvlist.list[i] [1]) + 'a',
				Rank(mvlist.list[i] [1]) + '1');
				barf("board pos.");
			}
		}

		if (board->depth >= board->level)
			newval = strgh;
		else
		{    board->depth++;
			newval = -minimax(board, newstr, turn, -val, -strgh,
						nodes, ack, show, col, newpv);
			board->depth--;
		}
		if (newval >= val)
		{	oldval = val;  /* record 2ndbest val for history table. */
			if (newval > val)
			{	besti = i;
				val = newval;

			ncopy(goodpv, mvlist.list[i], 2);	/* this be a good move. */
			if (board->level - board->depth)	/* copy the expected
										sequence. */
				ncopy(&goodpv[2], newpv,
					(board->level - board->depth) << 1);
			if (!board->depth) /* searching at root level, so let user
							  know the updated line. */
				printpv(goodpv, board->level);
			}
		}
		board->cbyte = ocbyte;
		board->ebyte = oebyte;
		unmakemove(board, mvlist.list[i], cappiece);
          strgh -= cap;
		if (val >= alpha) /* ie, will leave other side just as bad off
							(if not worse) */
			break;	/* why bother checking had bad the alternatives
							are? */

		if (board->depth == board->level &&
			 i >= mvlist.insrt - 1 ||
			board->depth == board->level - 1 &&
			 i >= mvlist.insrt - 1 &&
				val >= strgh)
								/* ran out of cool moves.
								Don't search further. */
			break;
	}
	ncopy(comstr, mvlist.list[besti], 4);
	if (val > oldval) /* move is at least one point better than others. */
		board->hist[turn ^ 1] [comstr[0]] [comstr[1]] = board->move;
	return val;
}
