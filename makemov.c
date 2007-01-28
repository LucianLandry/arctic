#include <stddef.h> /* NULL */
#include "ref.h"

static void newcbyte(BoardT *board)
/* updates castle status. */
{
    uint8 *coord = board->coord;
    int newcbyte = board->cbyte;
    if (coord[0] != ROOK)
	newcbyte &= ~WHITEQCASTLE;	/* no white queen castle */
    if (coord[7] != ROOK)
	newcbyte &= ~WHITEKCASTLE;	/* no white king castle */
    if (coord[4] != KING)
	newcbyte &= ~WHITECASTLE;	/* neither */
    if (coord[56] != BROOK)
	newcbyte &= ~BLACKQCASTLE;	/* no black queen castle */
    if (coord[63] != BROOK)
	newcbyte &= ~BLACKKCASTLE;	/* no black king castle */
    if (coord[60] != BKING)
	newcbyte &= ~BLACKCASTLE;	/* neither */

    if (newcbyte != board->cbyte)
    {
	board->zobrist ^=
	    (gPreCalc.zobrist.cbyte[board->cbyte] ^
	     gPreCalc.zobrist.cbyte[newcbyte]);
	board->cbyte = newcbyte;
    }
}


void makemove(BoardT *board, uint8 comstr[], UnMakeT *unmake)
{
    int enpass = comstr[2] && ISPAWN(comstr[2]);
    int promote = comstr[2] && !ISPAWN(comstr[2]);
    uint8 *coord = board->coord;
    uint8 mypiece = coord[comstr[0]];
    uint8 cappiece = coord[comstr[1]];
    uint8 newebyte;

    if (unmake != NULL)
    {
	/* Save off board information. */
	unmake->cappiece = cappiece;
	unmake->cbyte = board->cbyte;
	unmake->ebyte = board->ebyte;
	unmake->ncpPlies = board->ncpPlies;
	unmake->zobrist = board->zobrist;
    }

    /* adjust ncpPlies appropriately. */
    if (ISPAWN(mypiece) || cappiece)
	board->ncpPlies = 0;
    else
	board->ncpPlies++;

    /* king castling move? */
    if (ISKING(mypiece) && Abs(comstr[1] - comstr[0]) == 2)
    {
	if (comstr[1] == 6)
	    makemove(board, "\7\5\0", NULL);     /* move wkrook */
	else if (comstr[1] == 2)
	    makemove(board, "\0\3\0", NULL);     /* move wqrook */
	else if (comstr[1] == 62)
	    makemove(board, "\x3f\x3d\0", NULL); /* move bkrook */
	else
	    makemove(board, "\x38\x3b\0", NULL); /* move bqrook */
	board->ply--;	/* 'cause we're not switching sides... */
	board->zobrist ^= gPreCalc.zobrist.turn;
    }

    /* capture? better dump the captured piece from the playlist.. */
    if (cappiece)
    {
	delpiece(board, cappiece, comstr[1]);
    }
    else if (enpass)
    {
	delpiece(board, coord[board->ebyte], board->ebyte);
	COORDUPDATEZ(board, board->ebyte, 0);
    }

    /* now modify the pointer info in playptr. */
    board->playptr[comstr[1]] = board->playptr[comstr[0]];
    /* now modify coords in the playlist. */
    *board->playptr[comstr[1]] = comstr[1];

    /* el biggo question: did a promotion take place? Need to update
       stuff further then.  Can be ineff cause almost never occurs. */
    if (promote)
    {
	delpiece(board, mypiece, comstr[1]);
	addpiece(board, comstr[2], comstr[1]);
	COORDUPDATEZ(board, comstr[1], comstr[2]);
    }
    else
    {
	COORDUPDATEZ(board, comstr[1], mypiece);
    }
    COORDUPDATEZ(board, comstr[0], 0);

    newcbyte(board); /* update castle status. */

    /* update en passant status */
    newebyte = Abs(comstr[1] - comstr[0]) == 16 &&
	ISPAWN(mypiece) ? /* pawn moved 2 */
	comstr[1] : FLAG;
    if (newebyte != board->ebyte)
    {
	if (board->ebyte != FLAG)
	    board->zobrist ^= gPreCalc.zobrist.ebyte[board->ebyte];
	if (newebyte != FLAG)
	    board->zobrist ^= gPreCalc.zobrist.ebyte[newebyte];
	board->ebyte = newebyte;
    }

    board->ply++;
    board->zobrist ^= gPreCalc.zobrist.turn;
    board->ncheck[board->ply & 1] = comstr[3];

    /* concheck(board, "makemove"); */
}


void unmakemove(BoardT *board, uint8 comstr[], UnMakeT *unmake)
/* undoes the command 'comstr'. */
{
    int turn;
    int enpass = comstr[2] && ISPAWN(comstr[2]);
    int promote = comstr[2] && !ISPAWN(comstr[2]);
    uint8 cappiece;

    board->ply--;
    turn = board->ply & 1;

    if (unmake != NULL)
    {
	/* pop the old bytes.  It's counterintuitive to do this so soon.
	   Sorry.  Possible optimization: arrange the board variables
	   appropriately, and do a simple memcpy() */
	cappiece = unmake->cappiece;
	board->cbyte = unmake->cbyte;
	board->ebyte = unmake->ebyte; /* we need to do this before rest of
					 function. */
	board->ncpPlies = unmake->ncpPlies;
	board->zobrist = unmake->zobrist;
    }
    else
    {
	/* hopefully, this is an un-castling rook-move. */
	cappiece = 0;
    }

    /* king castling move? */
    if (ISKING(board->coord[comstr[1]]) &&
	Abs(comstr[1] - comstr[0]) == 2)
    {
	if (comstr[1] == 6)
	    unmakemove(board, "\7\5\0", NULL);     /* move wkrook */
	else if (comstr[1] == 2)
	    unmakemove(board, "\0\3\0", NULL);     /* move wqrook */
	else if (comstr[1] == 62)
	    unmakemove(board, "\x3f\x3d\0", NULL); /* move bkrook */
	else
	    unmakemove(board, "\x38\x3b\0", NULL); /* move bqrook */
	board->ply++;		/* since it wasn't really a move. */
    }

    /* el biggo question: did a promotion take place? Need to
       'depromote' then.  Can be ineff cause almost never occurs. */
    if (promote)
    {
	delpiece(board, comstr[2], comstr[1]);
	addpiece(board, PAWN | turn, comstr[1]);
	COORDUPDATE(board, comstr[0], PAWN | turn);
    }
    else
    {
	COORDUPDATE(board, comstr[0], board->coord[comstr[1]]); 
    }
    COORDUPDATE(board, comstr[1], cappiece);

    /* modify the pointer array. */
    board->playptr[comstr[0]] =
	board->playptr[comstr[1]];
    /* modify coords in playlist. */
    *(board->playptr[comstr[0]]) = *comstr;

    /* if capture, we need to
       add deleted record back to list. */
    if (cappiece)
    {
	addpiece(board, cappiece, comstr[1]);
    }
    else if (enpass)
    {
	COORDUPDATE(board, board->ebyte, comstr[2]);
	addpiece(board, comstr[2], board->ebyte);
    }

#if 0
    if (concheck(board, "unmakemove"))
    {
	LogMove(eLogDebug, board, comstr);
    }
#endif
}
