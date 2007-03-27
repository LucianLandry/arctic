/***************************************************************************
                    debug.c - board consistency checking +
                              other support routines
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
#include "ref.h"

int concheck(BoardT *board, char *failString, int checkz)
/* returns 0 on success, 1 on failure. */
{
    int x, i, j;
    for (i = 0; i < 64; i++)
	if (board->coord[i] > 0)
	    if (*board->playptr[i] != i)
	    {
		LOG_EMERG("concheck(%s): failure at %c%c.\n",
			  failString,
			  File(i) + 'a', Rank(i) + '1');
		printplaylist(board);
		return 1;
	    }
    for (i = 0; i < BQUEEN + 1; i++)
	for (j = 0; j < board->playlist[i].lgh; j++)
	{
	    x = board->playlist[i].list[j];
	    if (board->coord[x] != i ||
		board->playptr[x] != &board->playlist[i].list[j])
	    {
		LOG_EMERG("concheck(%s): failure in list at %c%d.\n",
			  failString, i, j);
		printplaylist(board);
		return 1;
	    }
	}
    if (checkz && board->zobrist != calcZobrist(board))
    {
	LOG_EMERG("concheck(%s): failure in zobrist calc (%x, %x).\n",
		  failString, board->zobrist, calcZobrist(board));
	printplaylist(board);
	return 1;
    }
    return 0;
}


void printplaylist(BoardT *board)
{
    int i, j;
    for (i = 0; i < BQUEEN + 1; i++)
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
    LOG_EMERG("playlist results.\n");
}
