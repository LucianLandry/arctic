#include <stdio.h>	/* has NULL value */
#include "ref.h"

void newcbyte(char coord[], char *cbyte)
/* updates castle status. */
{	if (coord[0] != 'R')
		*cbyte &= 0x7f;			/* no white queen castle */
	if (coord[7] != 'R')
		*cbyte &= 0xbf;			/* no white king castle */
	if (coord[4] != 'K')
		*cbyte &= 0x3f;			/* neither */
	if (coord[56] != 'r')
		*cbyte &= 0xdf;			/* no black queen castle */
	if (coord[63] != 'r')
		*cbyte &= 0xef;			/* no black king castle */
	if (coord[60] != 'k')
		*cbyte &= 0xcf;			/* neither */
}

void makemove(struct brd *board, char comstr[], char cappiece)
{	int enpass = tolower(board->coord[comstr[0]]) == 'p' &&
				comstr[1] - comstr[0] & 7 && !cappiece;
	int promote = comstr[2] && tolower(comstr[2]) != 'p';
	/* king castling move? */
	if (tolower(board->coord[comstr[0]]) == 'k' &&
		Abs(comstr[1] - comstr[0]) == 2)
	{	if (comstr[1] == 6)
			makemove(board, "\7\5\0", cappiece); /* move wkrook */
		else if (comstr[1] == 2)
			makemove(board, "\0\3\0", cappiece); /* move wqrook */
		else	if (comstr[1] == 62)
			makemove(board, "?=\0", cappiece); /* move bkrook */
		else	makemove(board, "8;\0", cappiece); /* move bqrook */
		board->move--;	/* 'cause we're not switching sides... */
	}
	/* capture? better dump the captured piece from the playlist.. */
	if (cappiece || enpass)
	{	delpiece(board, board->coord[enpass ? board->ebyte : comstr[1]],
			enpass ? board->ebyte : comstr[1]);
		if (enpass)
		{    comstr[2] = board->coord[board->ebyte]; /* set for unmakemove */
			board->coord[board->ebyte] = 0;
		}
	}
     /* now modify the pointer info in playptr. */
	board->playptr[comstr[1]] = board->playptr[comstr[0]];
	/* now modify coords in the playlist. */
	*board->playptr[comstr[1]] = comstr[1];

	/* el biggo question: did a promotion take place? Need to update
		stuff further then.  Can be ineff cause almost never occurs. */
	if (promote)
	{	delpiece(board, board->coord[comstr[0]], comstr[1]);
		addpiece(board, comstr[2], comstr[1]);
     }
	else
		newcbyte(board->coord, &board->cbyte); /* update castle status. */

	board->coord[comstr[1]] = comstr[2] && tolower(comstr[2])
		!= 'p' ? comstr[2] : board->coord[comstr[0]];
	board->coord[comstr[0]] = 0;

     /* update en passant status */
	board->ebyte = Abs(comstr[1]-comstr[0]) == 16 &&
		tolower(board->coord[comstr[1]]) == 'p' /* pawn moved 2 */
		? comstr[1] : FLAG;
	board->move++;
	board->ncheck[board->move & 1] = comstr[3];
}

void unmakemove(struct brd *board, char comstr[], char cappiece)
/* undoes the command 'comstr'. */
{	int enpass = tolower(comstr[2]) == 'p';
     int turn = (board->move & 1) ^ 1;
	int promote = comstr[2] && tolower(comstr[2]) != 'p';
	/* king castling move? */
	if (tolower(board->coord[comstr[1]]) == 'k' &&
		Abs(comstr[1]-comstr[0]) == 2)
	{	if (comstr[1] == 6)
			unmakemove(board, "\7\5\0", cappiece); /* move wkrook */
		else if (comstr[1] == 2) /* wqcastle */
			unmakemove(board, "\0\3\0", cappiece); /* move qrook */
		else if (comstr[1] == 62)
			unmakemove(board, "?=\0", cappiece);
		else
			unmakemove(board, "8;\0", cappiece);
		board->move++;		/* since it wasn't really a move. */
	}
	/* el biggo question: did a promotion take place? Need to
	'depromote' then.  Can be ineff cause almost never occurs. */
	if (promote)
	{	delpiece(board, comstr[2], comstr[1]);
		addpiece(board, 'P' | (turn << 5), comstr[1]);
     }

	/* modify the pointer array. */
	board->playptr[comstr[0]] =
		board->playptr[comstr[1]];

	board->coord[comstr[0]] = promote ? 'P' | (turn << 5)
		: board->coord[comstr[1]];

	board->coord[comstr[1]] = cappiece;
	/* modify coords in playlist. */
	*(board->playptr[comstr[0]]) = *comstr;

	if (enpass) /* en passant capture */
		board->coord[board->ebyte] = comstr[2];

	/* if capture, we need to
	add deleted record back to list. */
	if (cappiece || enpass)
	{    addpiece(board, enpass ? comstr[2] : cappiece,
			enpass ? board->ebyte : comstr[1]);
	}
	board->move--;
}
