/***************************************************************************
                   makemov.c - make/unmake a move on BoardT
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


#include <stddef.h> // NULL
#include "ref.h"


void addpiece(BoardT *board, uint8 piece, int coord)
{
    board->playptr[coord] =
	&board->playlist[piece].list[board->playlist[piece].lgh++];
    *board->playptr[coord] = coord;
    board->totalStrgh += WORTH(piece);
    board->playerStrgh[piece & 1] += WORTH(piece);
}

/* Smarter (but slower): will not mangle board strgh when we add a king. */
void addpieceSmart(BoardT *board, uint8 piece, int coord)
{
    board->playptr[coord] =
	&board->playlist[piece].list[board->playlist[piece].lgh++];
    *board->playptr[coord] = coord;
    if (!ISKING(piece))
    {
	board->totalStrgh += WORTH(piece);
	board->playerStrgh[piece & 1] += WORTH(piece);
    }
}


void delpiece(BoardT *board, uint8 piece, int coord)
{
    board->playerStrgh[piece & 1] -= WORTH(piece);
    board->totalStrgh -= WORTH(piece);

    /* change coord in playlist and dec playlist lgh. */
    *board->playptr[coord] = board->playlist[piece].list
	[--board->playlist[piece].lgh];
    /* set the end playptr. */
    board->playptr[*board->playptr[coord]] = board->playptr[coord];
}


void delpieceSmart(BoardT *board, uint8 piece, int coord)
{
    if (!ISKING(piece))
    {
	board->playerStrgh[piece & 1] -= WORTH(piece);
	board->totalStrgh -= WORTH(piece);
    }

    /* change coord in playlist and dec playlist lgh. */
    *board->playptr[coord] = board->playlist[piece].list
	[--board->playlist[piece].lgh];
    /* set the end playptr. */
    board->playptr[*board->playptr[coord]] = board->playptr[coord];
}


void newcbyte(BoardT *board)
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


/* Bitboard.  Defines the castling bits we care about. */
#define CASTLEBB 0x9100000000000091LL

void makemove(BoardT *board, uint8 comstr[], UnMakeT *unmake)
{
    int enpass = ISPAWN(comstr[2]);
    int promote = comstr[2] && !enpass;
    uint8 src = comstr[0];
    uint8 dst = comstr[1];
    uint8 *coord = board->coord;
    uint8 mypiece = coord[src];
    uint8 cappiece = coord[dst];
    uint8 newebyte;
    int savedPly;

    ListT *myList;
    ListPositionT *myElem;

    if (unmake != NULL)
    {
	/* Save off board information. */
	unmake->cappiece = cappiece;
	unmake->cbyte = board->cbyte;
	unmake->ebyte = board->ebyte;
	unmake->ncpPlies = board->ncpPlies;
	unmake->zobrist = board->zobrist;
	unmake->repeatPly = board->repeatPly;
    }

    /* king castling move? */
    if (ISKING(mypiece) && Abs(dst - src) == 2)
    {
	savedPly = board->repeatPly;
	if (dst == 6)
	    makemove(board, (uint8 *) "\7\5\0", NULL);     /* move wkrook */
	else if (dst == 2)
	    makemove(board, (uint8 *) "\0\3\0", NULL);     /* move wqrook */
	else if (dst == 62)
	    makemove(board, (uint8 *) "\x3f\x3d\0", NULL); /* move bkrook */
	else
	    makemove(board, (uint8 *) "\x38\x3b\0", NULL); /* move bqrook */

	/* unclobber appropriate variables. */
	board->ply--;	/* 'cause we're not switching sides... */
	board->ncpPlies--;
	board->repeatPly = savedPly;
	board->zobrist ^= gPreCalc.zobrist.turn;
    }

    /* capture? better dump the captured piece from the playlist.. */
    if (cappiece)
    {
	delpiece(board, cappiece, dst);
    }
    else if (enpass)
    {
	delpiece(board, coord[board->ebyte], board->ebyte);
	COORDUPDATEZ(board, board->ebyte, 0);
    }

    /* now modify the pointer info in playptr. */
    board->playptr[dst] = board->playptr[src];
    /* now modify coords in the playlist. */
    *board->playptr[dst] = dst;

    /* el biggo question: did a promotion take place? Need to update
       stuff further then.  Can be ineff cause almost never occurs. */
    if (promote)
    {
	delpiece(board, mypiece, dst);
	addpiece(board, comstr[2], dst);
	COORDUPDATEZ(board, dst, comstr[2]);
    }
    else
    {
	COORDUPDATEZ(board, dst, mypiece);
    }
    COORDUPDATEZ(board, src, 0);

#if 0 /* not a win on x86-32, at least. */
    if ((((1LL << src) | (1LL << dst)) & CASTLEBB))
#endif
	newcbyte(board); /* update castle status. */

    /* update en passant status */
    newebyte = Abs(dst - src) == 16 &&
	ISPAWN(mypiece) ? /* pawn moved 2 */
	dst : FLAG;
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

    /* adjust ncpPlies appropriately. */
    if (ISPAWN(mypiece) || cappiece)
    {
	board->ncpPlies = 0;
	board->repeatPly = -1;
    }
    else if (++board->ncpPlies >= 4 && board->repeatPly == -1)
    {
	/* We might need to set repeatPly. */
	myList = &board->posList[board->zobrist & 127];
	LIST_DOFOREACH(myList, myElem) /* hopefully a short loop. */
	{
	    if (PositionHit(board, &myElem->p))
	    {
		board->repeatPly = board->ply;
		break;
	    }
	}
    }

    /* concheck(board, "makemove", 1); */
}


void unmakemove(BoardT *board, uint8 comstr[], UnMakeT *unmake)
/* undoes the command 'comstr'. */
{
    int turn;
    int enpass = ISPAWN(comstr[2]);
    int promote = comstr[2] && !enpass;
    uint8 src = comstr[0];
    uint8 dst = comstr[1];
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
	board->repeatPly = unmake->repeatPly;
    }
    else
    {
	/* hopefully, this is an un-castling rook-move. */
	cappiece = 0;
    }

    /* king castling move? */
    if (ISKING(board->coord[dst]) &&
	Abs(dst - src) == 2)
    {
	if (dst == 6)
	    unmakemove(board, (uint8 *) "\7\5\0", NULL);     /* move wkrook */
	else if (dst == 2)
	    unmakemove(board, (uint8 *) "\0\3\0", NULL);     /* move wqrook */
	else if (dst == 62)
	    unmakemove(board, (uint8 *) "\x3f\x3d\0", NULL); /* move bkrook */
	else
	    unmakemove(board, (uint8 *) "\x38\x3b\0", NULL); /* move bqrook */
	board->ply++;		/* since it wasn't really a move. */
    }

    /* el biggo question: did a promotion take place? Need to
       'depromote' then.  Can be ineff cause almost never occurs. */
    if (promote)
    {
	delpiece(board, comstr[2], dst);
	addpiece(board, PAWN | turn, dst);
	COORDUPDATE(board, src, PAWN | turn);
    }
    else
    {
	COORDUPDATE(board, src, board->coord[dst]); 
    }
    COORDUPDATE(board, dst, cappiece);

    /* modify the pointer array. */
    board->playptr[src] =
	board->playptr[dst];
    /* modify coords in playlist. */
    *(board->playptr[src]) = *comstr;

    /* if capture, we need to
       add deleted record back to list. */
    if (cappiece)
    {
	addpiece(board, cappiece, dst);
    }
    else if (enpass)
    {
	COORDUPDATE(board, board->ebyte, comstr[2]);
	addpiece(board, comstr[2], board->ebyte);
    }

#if 0
    if (concheck(board, "unmakemove", unmake != NULL))
    {
	LogMove(eLogDebug, board, comstr);
    }
#endif
}


void commitmove(BoardT *board, uint8 *comstr, ThinkContextT *th,
		GameStateT *gameState, int declaredDraw)
{
    MoveListT mvlist;
    int turn = board->ply & 1;
    ClockT *myClock;
    int origturn = turn;

    /* Give computer a chance to re-evaluate the position, if we insist
       on changing the board. */
    gameState->bDone[0] = gameState->bDone[1] = 0;

    myClock = gameState->clocks[turn];
    ClockStop(myClock);

    if (comstr != NULL)
    {
	ClockApplyIncrement(myClock, board);
	PositionSave(board);
	LOG_DEBUG("commiting move (%d %d): ",
		  board->ply >> 1, board->ply & 1);
	LogMove(eLogDebug, board, comstr);
	makemove(board, comstr, NULL);
    }
    concheck(board, "commitmove", 1);

#if 0 /* bldbg */
    if (board->ply == 29)
	LogSetLevel(eLogDebug);
    if (board->ply == 30)
	LogSetLevel(eLogEmerg);
#endif

    turn = board->ply & 1; /* needs reset. */

    gUI->boardRefresh(board);
    gUI->statusDraw(board, gameState);

    ClockStart(gameState->clocks[turn]);
    if (origturn != turn)
    {
	// switched sides to another player.  Calc the time
	// we want to make the next move at.
	GoaltimeCalc(gameState, board);
    }


    mlistGenerate(&mvlist, board, 0);

    if (drawInsufficientMaterial(board))
    {
	ClocksStop(gameState);
	gUI->notifyDraw("insufficient material");
	gameState->bDone[turn] = 1;
    }
    else if (!mvlist.lgh)
    {
	ClocksStop(gameState);
	if (board->ncheck[turn] == FLAG)
	{
	    gUI->notifyDraw("stalemate");
	}
	else
	{
	    gUI->notifyCheckmated(turn);
	}
	gameState->bDone[turn] = 1;
    }
    else if (declaredDraw)
    {
	/* Some draws are not automatic, and need to be notified separately. */
	gameState->bDone[turn] = 1;
    }

    BoardCopy(&gameState->savedBoard, board);

    if (gameState->control[turn] && !gameState->bDone[turn])
    {
	/* Computer needs to make next move; let it do so. */
	gUI->notifyThinking();
	ThinkerCmdThink(th);
    }
    else
    {
	gUI->notifyReady();
    }
}
