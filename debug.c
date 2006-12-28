#include <stdio.h>
#include "ref.h"

int concheck(BoardT *board)
/* returns 0 on success, 1 on failure. */
{
    int x, i, j;
    for (x = 0; x < 64; x++)
	if (board->coord[x] > 0)
	    if (*board->playptr[x] != x)
	    {
		LOG_EMERG("concheck: failure at %c%c.\n", File(x) + 'a',
			  Rank(x) + '1');
		printplaylist(board);
		return 1;
	    }
    for (i = 0; i < 'r' + 1; i++)
	for (j = 0; j < board->playlist[i].lgh; j++)
	{
	    x = board->playlist[i].list[j];
	    if (board->coord[x] != i ||
		board->playptr[x] != &board->playlist[i].list[j])
	    {
		LOG_EMERG("concheck: failure in list at %c%d.\n", i, j);
		printplaylist(board);
		return 1;
	    }
	}
    return 0;
}


void printplaylist(BoardT *board)
{
    int i, j;
    for (i = 0; i < 'r' + 1; i++)
    {
	if (board->playlist[i].lgh)
	{
	    LOG_EMERG("%c:", i);
	    for (j = 0; j < board->playlist[i].lgh; j++)
	    {
		LOG_EMERG("%c%c", File(board->playlist[i].list[j]) + 'a',
			Rank(board->playlist[i].list[j]) + '1');
	    }
	    LOG_EMERG(".\n");
	}
    }
    barf("playlist results.");
}
