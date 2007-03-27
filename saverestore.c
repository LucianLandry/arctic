/***************************************************************************
          saverestore.c - (primitive) game save/restore functionality.
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
#include <string.h>
#include "ref.h"

/* returns: 0, if save successful, otherwise -1. */
int GameSave(BoardT *board)
{
    /* This is very lazy (most of the 'playlist' entries are meaningless),
       but effective. */
    FILE *myFile = fopen("arctic.sav", "w");
    int i = 0, len = (void *) &board->depth - (void *) board;

    if (myFile == NULL) return -1;

    for (i = 0; i < len; i++)
    {
	if (fputc(((char *) board)[i], myFile) == EOF)
	{
	    return -1;
	}
    } 

    if (fclose(myFile) == EOF) return -1;

    return 0;
}



void CopyBoard(BoardT *dest, BoardT *src)
{
    /* We attempt to copy every variable prior to 'board->depth'. */
    int i, j, piece, len = (void *) &src->depth - (void *) src;

    /* have something good to load, so copy it over. */
    memcpy(dest, src, len);
    /* we need to rebuild the playptr list. */
    for (i = 0; i < 64; i++)
    {
	dest->playptr[i] = NULL;
	if ((piece = dest->coord[i]))
	{
	    for (j = 0; j < dest->playlist[piece].lgh; j++)
	    {
		if (dest->playlist[piece].list[j] == i)
		{
		    dest->playptr[i] = &dest->playlist[piece].list[j];
		}
	    }
	}
    }
}


/* returns: 0, if save successful, otherwise -1. */
int GameRestore(BoardT *board)
{
    FILE *myFile = fopen("arctic.sav", "r");
    int i, len = (void *) &board->depth - (void *) board;
    int chr;
    BoardT myBoard;

    if (myFile == NULL)
    {
	return -1;
    }

    for (i = 0; i < len; i++)
    {
	if ((chr = fgetc(myFile)) == EOF)
	{
	    return -1;
	}
	((char *) &myBoard)[i] = chr;
    } 

    if (fclose(myFile) == EOF)
    {
	return -1;
    }

    /* have something good to load, so copy it over. */
    CopyBoard(board, &myBoard);

    return 0;
}
