//--------------------------------------------------------------------------
//                      move.c - chess moves for Arctic
//                           -------------------
//  copyright            : (C) 2013 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU Library General Public License as
//   published by the Free Software Foundation; either version 2 of the
//   License, or (at your option) any later version.
//
//--------------------------------------------------------------------------

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "board.h"
#include "move.h"
#include "moveList.h"
#include "uiUtil.h"
#include "variant.h"

const MoveT gMoveNone = {FLAG, 0, 0, 0};

// Pull information about whose turn it is from this move.
// It only works for castling moves!
static uint8 moveCastleToTurn(MoveT move)
{
    return move.src & NUM_PLAYERS_MASK;
}

// Return whether a move *looks* sane, without knowing anything about whether
//  it is actually legal or not.
static bool moveIsSane(MoveT move)
{
    return
	move.src < NUM_SQUARES &&
	move.dst < NUM_SQUARES &&

	move.promote != 0x1  && // (black 'empty', reserved)
	move.promote < NUM_PIECE_TYPES &&

	(move.chk == FLAG || move.chk == DOUBLE_CHECK ||
	 move.chk < NUM_SQUARES) &&

	// Do not allow a "non-"move (unless we are castling)
	(move.src != move.dst ||
	 (moveCastleToTurn(move) < NUM_PLAYERS &&
	  (move.src >> NUM_PLAYERS_BITS) <= 1 &&
	  move.promote == 0));
}

// Safely print a move that seems to make no sense.
static char *moveToStringInsane(char *result, MoveT move)
{
    sprintf(result, "(INS! %x.%x.%x.%x)",
	    move.src, move.dst, move.promote, move.chk);
    return result;
}

static char *moveToStringMnDebug(char *result, MoveT move)
{
    sprintf(result, "%c%c%c%c.%d.%c%c",
	    AsciiFile(move.src),
	    AsciiRank(move.src),
	    AsciiFile(move.dst),
	    AsciiRank(move.dst),
	    move.promote,
	    (move.chk == FLAG ? 'F' :
	     move.chk == DOUBLE_CHECK ? 'D' :
	     AsciiFile(move.chk)),
	    (move.chk == FLAG ? 'F' :
	     move.chk == DOUBLE_CHECK ? 'D' :
	     AsciiRank(move.chk)));
    return result;
}


// Assumes castling has been mangled, and that the move is sane.
static int moveToStringMnCAN(char *result, MoveT move)
{
    char promoString[2] =
	{(move.promote && !ISPAWN(move.promote) ?
	  tolower(nativeToAscii(move.promote)) :
	  0), 0};
    return sprintf(result, "%c%c%c%c%s",
		   AsciiFile(move.src),
		   AsciiRank(move.src),
		   AsciiFile(move.dst),
		   AsciiRank(move.dst),
		   promoString);
}

static bool canUseK2Notation(CastleStartCoordsT *start, cell_t dst)
{
    cell_t rookOO, rookOOO, king;

    rookOO = start->rookOO;
    rookOOO = start->rookOOO;
    king = start->king;

    if (rookOO != rookOOO &&
	((rookOO <= king && rookOOO <= king) ||
	 (rookOO >= king && rookOOO >= king)))
    {
	// Cannot use king-moves-2 notation when both rooks are different,
	// but on the same side of the king (although this does not occur in
	// any variations I am aware of); because the move would be ambiguous.
	return false;
    }

    // Can only use king-moves-2 notation if the destination is on the same
    // rank.
    return
	Rank(king) == Rank(dst) &&
	abs(dst - king) == 2;
}

// Attempt to transmute our normal castle style to a king-moves-2 style
// for printing.  Returns 'false' (and modifies nothing) if this is impossible.
static bool moveMangleCsK2(MoveT *move)
{
    CastleStartCoordsT *start;
    bool castleOO = MoveIsCastleOO(*move);
    cell_t rook, king, dst;

    start = &gVariant->castling[moveCastleToTurn(*move)].start;
    king = start->king;
    rook = castleOO ? start->rookOO : start->rookOOO;
    dst = king + (rook > king ? 2 : -2);

    if (!canUseK2Notation(start, dst))
    {
	return false;
    }

    move->src = king;
    move->dst = dst;
    return true;
}

// Attempt to transmute our normal castle style to a king-capture-rook style
// for printing.  Returns 'false' (and modifies nothing) if this is impossible.
static void moveMangleCsKxR(MoveT *move)
{
    CastleStartCoordsT *start;
    bool castleOO = MoveIsCastleOO(*move);
    cell_t king, dst;

    start = &gVariant->castling[moveCastleToTurn(*move)].start;
    dst = castleOO ? start->rookOO : start->rookOOO;
    king = start->king;

    move->src = king;
    move->dst = dst;
}

// Assumes castling is handled separately, when castleStyle is
//  csOO || csFIDE.  At this point we treat a king castle like
//  any other move even though it will not be technically legal.
static int moveToStringMnSAN(char *result, MoveT move, BoardT *board)
{
    // See the 'algebraic notation (chess)' article on Wikipedia for details
    //  about SAN.
    uint8 *coord = board->coord;
    uint8 src = move.src;
    uint8 dst = move.dst;
    uint8 mypiece = coord[src];
    char *sanStr = result;
    int i;
    bool isCastle = src == dst;
    bool isCapture = !isCastle && (coord[dst] || ISPAWN(move.promote));
    bool isPromote = move.promote && !ISPAWN(move.promote);
    bool sameFile = true, sameRank = true;
    MoveListT mvlist;

    if (!ISPAWN(mypiece))
	// Print piece (type) to move.
	sanStr += sprintf(sanStr, "%c", nativeToBoardAscii(mypiece));
    else if (isCapture)
	// Need to spew the file we are capturing from.
	sanStr += sprintf(sanStr, "%c", AsciiFile(src));

    mlistGenerate(&mvlist, board, 0);

    // Is there ambiguity about which piece will be moved?
    for (i = 0; i < mvlist.lgh; i++)
    {
	if (!ISPAWN(mypiece) && // already taken care of, above
	    mvlist.moves[i].src != src &&
	    mvlist.moves[i].dst == dst &&
	    coord[mvlist.moves[i].src] == mypiece)
	{
	    // Yes.  Note: both conditions could easily be true.
	    if (sameFile)
		sameFile = File(mvlist.moves[i].src) == File(src);
	    if (sameRank)
		sameRank = Rank(mvlist.moves[i].src) == Rank(src);
	}
    }

    // ... disambiguate the src piece, if necessary.
    if (!sameFile)
	sanStr += sprintf(sanStr, "%c", AsciiFile(src));
    if (!sameRank)
	sanStr += sprintf(sanStr, "%c", AsciiRank(src));

    if (isCapture)
	sanStr += sprintf(sanStr, "x");

    // spew the destination coord.
    sanStr += sprintf(sanStr, "%c%c", AsciiFile(dst), AsciiRank(dst));

    if (isPromote)
	// spew piece type to promote to.
	sanStr += sprintf(sanStr, "%c", nativeToBoardAscii(move.promote));

    return sanStr - result; // return number of non-NULL bytes written
}

static bool moveIsLegal(MoveT move, BoardT *board)
{
    MoveListT moveList;

    mlistGenerate(&moveList, board, 0);
    return mlistSearch(&moveList, &move, 4) != NULL;
}

char *MoveToString(char *result,
		   MoveT move,
		   const MoveStyleT *style,
		   // Used for disambiguation and legality checks, when !NULL.
		   // Not mangled, but may be altered (to test checkmate).
		   struct BoardS *board)
{
    // These shorthand copies may be modified.
    MoveNotationT notation = style->notation;
    MoveCastleStyleT castleStyle = style->castleStyle;
    bool showCheck = style->showCheck;
    char *moveStr = result;

    if (!moveIsSane(move))
    {
	// With our hashing scheme, we may end up with moves that are not
	//  legal, but we should never end up with moves that are not sane
	//  (except possibly gMoveNone).
	// We still may want to print such a move before we assert (or
	//  whatever).
	return moveToStringInsane(result, move);
    }
    if (board != NULL && !moveIsLegal(move, board))
    {
	result[0] = '\0';
	return result;
    }
    if (notation == mnDebug)
    {
	return moveToStringMnDebug(result, move);
    }
    if (MoveIsCastle(move))
    {
	// Transmute the move if we need to (and can); otherwise fall back to
	//  our default.
	if (castleStyle == csKxR)
	{
	    moveMangleCsKxR(&move);
	}
	else if (castleStyle == csK2 && !moveMangleCsK2(&move))
	{
	    castleStyle = csOO;
	}
    }
    if (MoveIsCastle(move))
    {
	if (castleStyle == csOO)
	{
	    sprintf(result, MoveIsCastleOO(move) ? "O-O" : "O-O-O");
	    return result;
	}
	if (castleStyle == csFIDE)
	{
	    sprintf(result, MoveIsCastleOO(move) ? "0-0" : "0-0-0");
	    return result;
	}
    }
    if (notation == mnSAN && board == NULL)
    {
	// Cannot use SAN with no board context.
	notation = mnCAN;
    }

    moveStr +=
	notation == mnSAN ? moveToStringMnSAN(result, move, board) :
	// Assume mnCAN at this point.
	moveToStringMnCAN(result, move);

    if (showCheck && move.chk != FLAG)
    {
	bool isMate = false;
	MoveListT mvlist;
	UnMakeT unmake;

	if (board != NULL)
	{
	    // Piece in check.  Is this checkmate?
	    BoardMoveMake(board, &move, &unmake);
	    mlistGenerate(&mvlist, board, 0);
	    BoardMoveUnmake(board, &unmake);
	    isMate = (mvlist.lgh == 0);
	}

	moveStr += sprintf(moveStr, "%c", isMate ? '#' : '+');
    }

    return result;
}

// Syntactic sugar.
void MoveStyleSet(MoveStyleT *style,
		  MoveNotationT notation,
		  MoveCastleStyleT castleStyle,
		  bool showCheck)
{
    style->notation = notation;
    style->castleStyle = castleStyle;
    style->showCheck = showCheck;
}

bool MoveIsPromote(MoveT move, struct BoardS *board)
{
    return
	!MoveIsCastle(move) &&
	ISPAWN(board->coord[move.src]) &&
	(move.dst > 55 || move.dst < 8);
}

// This is only a partial move creation routine as it does not fill in
//  'move->chk', and in fact, clobbers it.
void MoveCreateFromCastle(MoveT *move, bool castleOO, int turn)
{
    move->src = castleOO ? turn : (1 << NUM_PLAYERS_BITS) | turn;
    move->dst = move->src;
    move->promote = 0;
    move->chk = 0;
}

// Attempt to take a king-moves-2 or KxR-style move and convert it
//  to the correct format.  Does nothing if the move is not actually detected
//  as a castle.
// We need a 'board' arg (or at least cbyte) because otherwise (for example) a
//  king capturing its own rook one space to the right could be confused with
//  just moving the king one space to the right.
// Assumes we are 'unmangling' a move from the players whose turn it is.
void MoveUnmangleCastle(MoveT *move, struct BoardS *board)
{
    CastleStartCoordsT *start; // shorthand
    cell_t dst = move->dst, src = move->src, rookOO;
    bool isCastleOO;
    int turn = board->turn;

    if (MoveIsCastle(*move))
    {
	return; // do not unmangle if this move is already a castle request
    }

    start = &gVariant->castling[turn].start;
    rookOO = start->rookOO;

    if (src != start->king ||
	!BoardCanCastle(board, turn))
    {
	return;
    }

    // We know now we're at least trying to move a 'king' that can castle.
    if (dst == rookOO || dst == start->rookOOO)
    {
	// Attempting KxR.
	isCastleOO = (dst == rookOO);
    }
    else if (abs(dst - src) == 2)
    {
	// Attempting K moves 2.
	if (!canUseK2Notation(start, dst))
	{
	    return;
	}
	isCastleOO =
	    (rookOO > src && dst > move->src) ||
	    (rookOO < src && dst < move->src);
    }
    else
    {
	return; // King not moving 2, and not capturing own rook
    }

    if ((isCastleOO && BoardCanCastleOO(board, turn)) ||
	(!isCastleOO && BoardCanCastleOOO(board, turn)))
    {
	MoveCreateFromCastle(move, isCastleOO, turn);
    }
}
