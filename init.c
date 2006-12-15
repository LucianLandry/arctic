#include "ref.h"

void init(char *moves[] [9], char mvarray[], char dir[] [64])
{	int i, d, j;
	char *ptr = mvarray;
	for (d = 0; d < 8; d++) /* d signifies direction */
	{	switch(d) {
			case 0: diaginit(d,  0,  1,  8, moves, ptr); break;
			case 2: diaginit(d,  7, -1,  8, moves, ptr); break;
			case 4: diaginit(d, 63, -1, -8, moves, ptr); break;
			case 6: diaginit(d, 56,  1, -8, moves, ptr); break;
			case 1: rowinit(d,  0,  8, 1, moves, ptr); break;
			case 3: rowinit(d,  0,  1, 8, moves, ptr); break;
			case 5: rowinit(d, 56, -8, 1, moves, ptr); break;
			case 7: rowinit(d,  7, -1, 8, moves, ptr); break;
			default: break;
		};
		ptr += 64;
	}
	for (i = 0; i < 64; i++)	/* store knight moves. */
	{	moves[i] [8] = ptr;
		while(*ptr != FLAG)
			ptr++;
		ptr++;
	}
	/* initialize direction array. */
	for(i = 0; i < 64; i++)
		for (j= 0; j < 64; j++)
			dir[i] [j] = dirf(i, j);
}

int dirf(int from, int to)
{	int res;
	int rdiff = Rank(to) - Rank(from);
	int fdiff = File(to) - File(from);
	if (!rdiff)
     	res = 3;				/* - move */
	else if (!fdiff)
		res = 1;				/* | move */
	else if (rdiff == fdiff)
		res = 2;				/* / move */
	else if (rdiff == -fdiff)
		res = 0;				/* \ move */
	else if (Abs(rdiff) + Abs(fdiff) == 3)
		return 8;				/* night move */
	else return -1;			/* no direction whatsoever. */
	return from < to ? res : res + 4;
}

void rowinit(int d, int start, int finc, int sinc, char *moves[] [9],
	char *ptr)
{	int temp, i;
	int row = 0;
	char *oldptr = ptr;
	gotoxy(1, 1);
	for (i = temp = start; row < 8;)
	{    moves[i] [d] = ptr++;
		if (*moves[i] [d] == FLAG)
		{	i = (temp += sinc);
			row++;
		}
		else i += finc;
	}
	i = ((int) ptr) - ((int) oldptr);
	gotoxy(1, 13);
}

void diaginit(int d, int start, int finc, int sinc, char *moves[] [9],
	char *ptr)
{	int temp, i, j;
	char *oldptr = ptr;
	gotoxy(1, 1);
	for (i = start; Abs(i - start) < 8; i += finc)
		for (j = i; ; j += sinc - finc)
		{	moves[j] [d] = ptr++;
			if (*moves[j] [d] == FLAG)
				break;
		}
	for (temp = (i = start + sinc + finc * 7); Abs(i - temp) < 49; i += sinc)
	     for (j = i; ; j += sinc - finc)
		{	moves[j] [d] = ptr++;
			if (*moves[j] [d] == FLAG)
				break;
		}
	j = ((int) ptr) - ((int) oldptr);
	gotoxy(1, 12);
}

void new(struct brd *board, int col[])
{    int x, y, i, j;
	char *tempptr;

	ncopy(board->coord, "RNBQKBNRPPPPPPPP", 16);		/* white */
     for (x = 16; x < 48; x++)					/* unocc'd middle */
		board->coord[x] = 0;
	ncopy(&(board->coord[48]), "pppppppprnbqkbnr", 16); /* black */

	/* init playlist and playptr. */
	for (i = 0; i < 'r' + 1; i++)
		board->playlist[i].lgh = 0;
	for (i = 0; i < 64; i++)
		if (board->coord[i])
			addpiece(board, board->coord[i], i);

	/* reset history table. */
	for (i = 0; i < 2; i++)
		for (x = 0; x < 63; x++)
			for (y = 0; y < 63; y++)
				board->hist[i] [x] [y] = -100;

	update(board->coord, col);
	board->move = 0;					/* white goes first */
	board->cbyte = 0xF0;				/* All can castle */
	board->ebyte = FLAG;				/* no enpassant */
	board->ncheck[0] = FLAG;				/* no one's in check. */
	board->depth = 0;					/* we're not hypothesizing :) */
}

void addpiece(struct brd *board, char piece, int coord)
{	board->playptr[coord] = &board->playlist[piece].list[board->
		playlist[piece].lgh++];
	*board->playptr[coord] = coord;
}

void delpiece(struct brd *board, char piece, int coord)
{	/* change coord in playlist and dec playlist lgh. */
	*board->playptr[coord] = board->playlist[piece].list
		[--board->playlist[piece].lgh];
	/* set the end playptr. */
	board->playptr[*board->playptr[coord]] = board->playptr[coord];
}
