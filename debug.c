#include <stdio.h>
#include <conio.h>
#include "ref.h"

int concheck(struct brd *board)
/* returns 0 on success, 1 on failure. */
{	int x, y, i, j;
	gotoxy(1,1);
	for (x = 0; x < 64; x++)
		if (board->coord[x] > 0)
			if (*board->playptr[x] != x)
               {   	cprintf("concheck: failure at %c%c.", File(x) + 'a',
					Rank(x) + '1');
                    printplaylist(board);
				return 1;
			}
	for (i = 0; i < 'r' + 1; i++)
		for (j = 0; j < board->playlist[i].lgh; j++)
		{	x = board->playlist[i].list[j];
			if (board->coord[x] != i ||
				board->playptr[x] != &board->playlist[i].list[j])
			{	cprintf("concheck: failure in list at %c%d.", i, j);
				printplaylist(board);
				return 1;
			}
		}
	return 0;
}



void printlist(struct mlist *mvlist)
/* debugging function. */
{	int x;
	gotoxy(1,1);
	for (x = 0; x < mvlist->lgh; x++)
		printf("%c%c%c%c%c%c  ", File(mvlist->list[x] [0]) + 'a',
			Rank(mvlist->list[x] [0]) + '1',
			File(mvlist->list[x] [1]) + 'a',
			Rank(mvlist->list[x] [1]) + '1',
			File(mvlist->list[x] [3]) + 'a',
			Rank(mvlist->list[x] [3]) + '1');
	barf("possible moves.");
}


void printplaylist(struct brd *board)
{	int i, j;
	int lin = 0;
	for (i = 0; i < 'r' + 1; i++)
	{	if (board->playlist[i].lgh)
		{	gotoxy(1, 12+(lin++));
			cprintf("%c:", i);
			for (j = 0; j < board->playlist[i].lgh; j++)
			{	cprintf("%c%c", File(board->playlist[i].list[j]) + 'a',
					Rank(board->playlist[i].list[j]) + '1');
			}
			cprintf(".");
		}
	}
	barf("playlist results.");
}
