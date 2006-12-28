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


/* returns: 0, if save successful, otherwise -1. */
int GameRestore(BoardT *board)
{
    FILE *myFile = fopen("arctic.sav", "r");
    int i, j, piece, len = (void *) &board->depth - (void *) board;
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
    memcpy(board, &myBoard, len);
    /* we need to rebuild the playptr list.  The addresses do not necessarily
       stick if the program is executed later. */
    for (i = 0; i < 64; i++)
    {
	board->playptr[i] = NULL;
	if ((piece = board->coord[i]))
	{
	    for (j = 0; j < board->playlist[piece].lgh; j++)
	    {
		if (board->playlist[piece].list[j] == i)
		{
		    board->playptr[i] = &board->playlist[piece].list[j];
		}
	    }
	}
    }

    return 0;
}
