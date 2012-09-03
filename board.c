//--------------------------------------------------------------------------
//                  board.c - BoardT-related functionality.
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
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

#include <stddef.h> // NULL
#include <stdlib.h> // exit(3)
#include <assert.h>

#include "comp.h"
#include "gDynamic.h"
#include "gPreCalc.h"
#include "log.h"
#include "ref.h"
#include "ui.h"
#include "uiUtil.h"
#include "transTable.h"

// #define DEBUG_CONSISTENCY_CHECK

// Incremental update.  To be used everytime when board->coord[i] is updated.
// (This used to update a compressed equivalent of coord called 'hashCoord',
//  but now is just syntactic sugar.)
static inline void CoordUpdate(BoardT *board, uint8 i, uint8 newVal)
{
    board->coord[i] = newVal;
}


static inline void CoordUpdateZ(BoardT *board, uint8 i, uint8 newVal)
{
    board->zobrist ^=
        (gPreCalc.zobrist.coord[board->coord[i]] [i] ^
         gPreCalc.zobrist.coord[newVal] [i]);
    CoordUpdate(board, i, newVal);
}



// This is useful for generating a hash for the initial board position, or
// (slow) validating the incrementally-updated hash.
static uint64 BoardZobristCalc(BoardT *board)
{
    uint64 retVal = 0;
    int i;
    for (i = 0; i < NUM_SQUARES; i++)
    {
	retVal ^= gPreCalc.zobrist.coord[board->coord[i]] [i];
    }
    retVal ^= gPreCalc.zobrist.cbyte[board->cbyte];
    if (board->turn)
	retVal ^= gPreCalc.zobrist.turn;
    if (board->ebyte != FLAG)
	retVal ^= gPreCalc.zobrist.ebyte[board->ebyte];
    return retVal;
}


// returns 0 on success, 1 on failure.
int BoardConsistencyCheck(BoardT *board, char *failString, int checkz)
{
    int i, j, coord;
    for (i = 0; i < NUM_SQUARES; i++)
    {
	if (board->coord[i] > 0 && *board->pPiece[i] != i)
	{
	    LOG_EMERG("BoardConsistencyCheck(%s): failure at %c%c.\n",
		      failString,
		      AsciiFile(i), AsciiRank(i));
	    LogPieceList(board);
	    exit(0);
	    return 1;
	}
#if 1 // bldbg: trying this.  This requires a slight bit of extra work in
      // BoardMove(Un)Make().  But it is the principle of least surprise.
	else if (board->coord[i] == 0 && board->pPiece[i] != NULL)
	{
	    LOG_EMERG("BoardConsistencyCheck(%s): dangling pPiece at %c%c.\n",
		      failString,
		      AsciiFile(i), AsciiRank(i));
	    LogPieceList(board);
	    exit(0);
	    return 1;
	}
#endif
    }
    for (i = 0; i < NUM_PIECE_TYPES; i++)
    {
	for (j = 0; j < board->pieceList[i].lgh; j++)
	{
	    coord = board->pieceList[i].coords[j];
	    if (board->coord[coord] != i ||
		board->pPiece[coord] != &board->pieceList[i].coords[j])
	    {
		LOG_EMERG("BoardConsistencyCheck(%s): failure in list at "
			  "%d-%d.\n",
			  failString, i, j);
		LogPieceList(board);
		exit(0);
		return 1;
	    }
	}
    }
    if (checkz && board->zobrist != BoardZobristCalc(board))
    {
	LOG_EMERG("BoardConsistencyCheck(%s): failure in zobrist calc "
		  "(%"PRIx64", %"PRIx64").\n",
		  failString, board->zobrist, BoardZobristCalc(board));
	LogPieceList(board);
	exit(0);
	return 1;
    }
    return 0;
}


static void pieceAdd(BoardT *board, int coord, uint8 piece)
{
    board->pPiece[coord] =
	&board->pieceList[piece].coords[board->pieceList[piece].lgh++];
    *board->pPiece[coord] = coord;
    board->totalStrength += WORTH(piece);
    board->playerStrength[piece & 1] += WORTH(piece);
}


static void pieceDelete(BoardT *board, int coord, uint8 piece)
{
    board->playerStrength[piece & 1] -= WORTH(piece);
    board->totalStrength -= WORTH(piece);

    // change coord in pieceList and dec pieceList lgh.
    *board->pPiece[coord] = board->pieceList[piece].coords
	[--board->pieceList[piece].lgh];
    // set the end pPiece.
    board->pPiece[*board->pPiece[coord]] = board->pPiece[coord];
}


static inline void cbyteUpdate(BoardT *board, int newcbyte)
{
    if (newcbyte != board->cbyte)
    {
	board->zobrist ^=
	    (gPreCalc.zobrist.cbyte[board->cbyte] ^
	     gPreCalc.zobrist.cbyte[newcbyte]);
	board->cbyte = newcbyte;
    }
}


static inline void ebyteUpdate(BoardT *board, int newebyte)
{
    if (newebyte != board->ebyte)
    {
	if (board->ebyte != FLAG)
	    board->zobrist ^= gPreCalc.zobrist.ebyte[board->ebyte];
	if (newebyte != FLAG)
	    board->zobrist ^= gPreCalc.zobrist.ebyte[newebyte];
	board->ebyte = newebyte;
    }
}


// Updates castle status.
static void BoardCbyteUpdate(BoardT *board)
{
    uint8 *coord = board->coord;
    int cbyte = board->cbyte;

    if (coord[0] != ROOK)
	cbyte &= ~WHITEQCASTLE;	// no white queen castle.
    if (coord[7] != ROOK)
	cbyte &= ~WHITEKCASTLE;	// no white king castle.
    if (coord[4] != KING)
	cbyte &= ~WHITECASTLE;	// no white queen or king castle.
    if (coord[56] != BROOK)
	cbyte &= ~BLACKQCASTLE;	// no black queen castle.
    if (coord[63] != BROOK)
	cbyte &= ~BLACKKCASTLE;	// no black king castle.
    if (coord[60] != BKING)
	cbyte &= ~BLACKCASTLE;	// no black queen or king castle.

    cbyteUpdate(board, cbyte);
}


// These are the rook-moves involved in normal castling.
static MoveT gWKRookCastleMove = {7, 5, 0, 0};
static MoveT gWQRookCastleMove = {0, 3, 0, 0};
static MoveT gBKRookCastleMove = {63, 61, 0, 0};
static MoveT gBQRookCastleMove = {56, 59, 0, 0};


// Returns if an index is between 'start' and 'finish' (inclusive).
static int serialBetween(int i, int start, int finish)
{
    return start <= finish ?
	i >= start && i <= finish : // 'normal' case.
	i >= start || i <= finish;
}


static inline int BoardPositionElemToIdx(BoardT *board, PositionElementT *elem)
{
    return elem - board->positions;
}


void BoardInit(BoardT *board)
{
    // blank everything.
    memset(board, 0, sizeof(BoardT));
    CvInit(&board->cv);
}


void BoardMoveMake(BoardT *board, MoveT *move, UnMakeT *unmake)
{
    int enpass = ISPAWN(move->promote);
    int promote = move->promote && !enpass;
    uint8 src = move->src;
    uint8 dst = move->dst;
    uint8 *coord = board->coord;
    uint8 mypiece = coord[src];
    uint8 cappiece = coord[dst];
    uint8 newebyte;
    int savedPly;

    ListT *myList;
    PositionElementT *myElem;

    assert(src != FLAG); // This seems to happen too often.

#ifdef DEBUG_CONSISTENCY_CHECK
    BoardConsistencyCheck(board, "BoardMoveMake1", 1);
#endif

    if (unmake != NULL)
    {
	// Save off board information.
	unmake->cappiece = cappiece;
	unmake->cbyte = board->cbyte;
	unmake->ebyte = board->ebyte;
	unmake->ncheck = board->ncheck[board->turn];
	unmake->ncpPlies = board->ncpPlies;
	unmake->zobrist = board->zobrist;
	unmake->repeatPly = board->repeatPly;
    }

    // King castling move?
    if (ISKING(mypiece) && abs(dst - src) == 2)
    {
	savedPly = board->repeatPly;
	BoardMoveMake(board,
		      (dst == 6  ? &gWKRookCastleMove : // move wkrook
		       dst == 2  ? &gWQRookCastleMove : // move wqrook
		       dst == 62 ? &gBKRookCastleMove : // move bkrook
		       &gBQRookCastleMove), // move bqrook
		      NULL);

	// unclobber appropriate variables.
	board->ply--;	// 'cause we're not switching sides...
	board->turn ^= 1;
	board->ncpPlies--;
	board->repeatPly = savedPly;
	board->zobrist ^= gPreCalc.zobrist.turn;
    }

    // Capture? better dump the captured piece from the pieceList..
    if (cappiece)
    {
	pieceDelete(board, dst, cappiece);
    }
    else if (enpass)
    {
	pieceDelete(board, board->ebyte, coord[board->ebyte]);
	CoordUpdateZ(board, board->ebyte, 0);
#if 1 // bldbg
	board->pPiece[board->ebyte] = NULL;
#endif
    }

    // Modify the pointer info in pPiece,
    // and the coords in the pieceList.
    *(board->pPiece[dst] = board->pPiece[src]) = dst;
#if 1 // bldbg
    board->pPiece[src] = NULL;
#endif

    // El biggo question: did a promotion take place? Need to update
    // stuff further then.  Can be inefficient cause almost never occurs.
    if (promote)
    {
	pieceDelete(board, dst, mypiece);
	pieceAdd(board, dst, move->promote);
	CoordUpdateZ(board, dst, move->promote);
    }
    else
    {
	CoordUpdateZ(board, dst, mypiece);
    }
    CoordUpdateZ(board, src, 0);

    BoardCbyteUpdate(board); // Update castle status.

    // Update en passant status.
    newebyte = abs(dst - src) == 16 &&
	ISPAWN(mypiece) ? /* pawn moved 2 */
	dst : FLAG;

    ebyteUpdate(board, newebyte);

    board->ply++;
    board->turn ^= 1;
    board->zobrist ^= gPreCalc.zobrist.turn;
    TransTablePrefetch(board->zobrist);
    board->ncheck[board->turn] = move->chk;

    // Adjust ncpPlies appropriately.
    if (ISPAWN(mypiece) || cappiece)
    {
	board->ncpPlies = 0;
	board->repeatPly = -1;
    }
    else if (++board->ncpPlies >= 4 && board->repeatPly == -1)
    {
	// We might need to set repeatPly.
	myList = &board->posList[board->zobrist & (NUM_SAVED_POSITIONS - 1)];
	LIST_DOFOREACH(myList, myElem) // Hopefully a short loop.
	{
	    // idx(myElem) must be between board->ply - board->ncpPlies and
	    // board->ply - 1 (inclusive) to be counted.
	    if (serialBetween(BoardPositionElemToIdx(board, myElem),
			      ((board->ply - board->ncpPlies) &
			       (NUM_SAVED_POSITIONS - 1)),
			      (board->ply - 1) & (NUM_SAVED_POSITIONS - 1)) &&
		BoardPositionHit(board, myElem->zobrist))
	    {
		board->repeatPly = board->ply;
		break;
	    }
	}
    }
#ifdef DEBUG_CONSISTENCY_CHECK
    BoardConsistencyCheck(board, "BoardMoveMake2", 1);
#endif
}


// Undoes the move 'move'.
void BoardMoveUnmake(BoardT *board, MoveT *move, UnMakeT *unmake)
{
    int turn;
    int enpass = ISPAWN(move->promote);
    int promote = move->promote && !enpass;
    uint8 src = move->src;
    uint8 dst = move->dst;
    uint8 cappiece;

#ifdef DEBUG_CONSISTENCY_CHECK
    if (BoardConsistencyCheck(board, "BoardMoveUnmake1", unmake != NULL))
    {
	LogMove(eLogEmerg, board, move);
	assert(0);
    }
#endif

    board->ply--;
    board->turn ^= 1;
    turn = board->turn;

    if (unmake != NULL)
    {
	// Pop the old bytes.  It's counterintuitive to do this so soon.
	// Sorry.  Possible optimization: arrange the board variables
	// appropriately, and do a simple memcpy().
	cappiece = unmake->cappiece;
	board->cbyte = unmake->cbyte;
	board->ebyte = unmake->ebyte; // We need to do this before rest of
	                              // the function.
	board->ncheck[turn] = unmake->ncheck;
	board->ncpPlies = unmake->ncpPlies;
	board->zobrist = unmake->zobrist;
	board->repeatPly = unmake->repeatPly;
    }
    else
    {
	// Hopefully, this is the rook-move part of an un-castling move.
	cappiece = 0;
    }

    // King castling move?
    if (ISKING(board->coord[dst]) &&
	abs(dst - src) == 2)
    {
	BoardMoveUnmake(board,
			(dst == 6  ? &gWKRookCastleMove : // move wkrook
			 dst == 2  ? &gWQRookCastleMove : // move wqrook
			 dst == 62 ? &gBKRookCastleMove : // move bkrook
			 &gBQRookCastleMove), // move bqrook
			NULL);
	board->ply++;		// since it wasn't really a move.
	board->turn ^= 1;
    }

    // El biggo question: did a promotion take place? Need to
    // 'depromote' then.  Can be inefficient cause almost never occurs.
    if (promote)
    {
	pieceDelete(board, dst, move->promote);
	pieceAdd(board, dst, PAWN | turn);
	CoordUpdate(board, src, PAWN | turn);
    }
    else
    {
	CoordUpdate(board, src, board->coord[dst]); 
    }
    CoordUpdate(board, dst, cappiece);

    // Modify the pointer array,
    // and the coords in the pieceList.
    *(board->pPiece[src] = board->pPiece[dst]) = src;
#if 1 // bldbg
    board->pPiece[dst] = NULL;
#endif

    // If capture, we need to add deleted record back to list.
    if (cappiece)
    {
	pieceAdd(board, dst, cappiece);
    }
    else if (enpass)
    {
	CoordUpdate(board, board->ebyte, move->promote);
	pieceAdd(board, board->ebyte, move->promote);
    }

#ifdef DEBUG_CONSISTENCY_CHECK
    if (BoardConsistencyCheck(board, "BoardMoveUnmake2", unmake != NULL))
    {
	LogMove(eLogEmerg, board, move);
	assert(0);
    }
#endif
}


// returns (boolean) is the board's 'ebyte' field invalid.
static int BoardBadEbyte(BoardT *board)
{
    int ebyte = board->ebyte;
    int turn = board->turn;

    return
	(ebyte != FLAG &&
	 (!ISPAWN(board->coord[ebyte]) ||
	  CHECK(board->coord[ebyte], turn) != ENEMY ||
	  // for black, ebyte must be a4-h4.
	  (turn && (ebyte < 24 || ebyte > 31)) ||
	  // for white, ebyte must be a5-h5.
	  (!turn && (ebyte < 32 || ebyte > 39))));
}


// returns (boolean) is the board's 'cbyte' field invalid.
static int BoardBadCbyte(BoardT *board)
{
    int cbyte = board->cbyte;
    int result;

    BoardCbyteUpdate(board);
    result = (cbyte != board->cbyte);
    board->cbyte = cbyte;
    return result;
}


// This is (now) intended to be as thorough a consistency check as possible.
// Returns -1 if there is a problem, 0 otherwise.
int BoardSanityCheck(BoardT *board, int silent)
{
    int kcoord;
    int kcoord2;
    int i;

    // Check: pawns must not be on 1st or 8th rank.
    for (i = 0; i < NUM_SQUARES; i++)
    {
	if (i == 8) i = 56; // skip to the 8th rank.
	if (ISPAWN(board->coord[i]))
	{
	    return reportError(silent,
			       "Error: Pawn detected on 1st or 8th rank.");
	}
    }

    // Check: only one king (of each color) on board.
    if (board->pieceList[KING].lgh != 1 ||
	board->pieceList[BKING].lgh != 1)
    {
	return reportError(silent,
			   "Error: Need one king of each color (%d, %d).",
			   board->pieceList[KING].lgh,
			   board->pieceList[BKING].lgh);
    }

    // Check: the side *not* on move must not be in check.
    if (calcNCheck(board, board->turn ^ 1, "BoardSanityCheck") != FLAG)
    {
	return reportError(silent,
			   "Error: Side not on move (%d) is in check.",
			   board->turn ^ 1);
    }

    // Check: Kings must not be adjacent to each other (calcNCheck() does not
    // test for this).
    kcoord  = board->pieceList[KING].coords[0];
    kcoord2 = board->pieceList[BKING].coords[0];
    if (abs(Rank(kcoord) - Rank(kcoord2)) < 2 &&
	abs(File(kcoord) - File(kcoord2)) < 2)
    {
	return reportError(silent,
			   "Error: Side not on move (%d) is in check by king.",
			   board->turn ^ 1);
    }

    // Check: for bad ebyte (en passant byte).
    if (BoardBadEbyte(board))
    {
	return reportError(silent, "Error: bad ebyte (%d).", board->ebyte);
    }

    // Check: for bad cbyte (castling byte).
    if (BoardBadCbyte(board))
    {
	return reportError(silent, "Error: bad cbyte (%d).", board->cbyte);
    }

    // Check: ply must be >= ncpPlies.
    // (plies < ncpPlies should not be possible, and could screw up
    //  3fold repetiion calculation.)
    if (board->ply < 0 || board->ncpPlies > board->ply)
    {
	return reportError(silent, "Error: bad ply/ncpPlies (%d, %d)",
			   board->ply, board->ncpPlies);
    }

    // Check: it must be white or black's turn.
    if (board->turn >= NUM_PLAYERS)
    {
	return reportError(silent, "Error: bad turn (%d)", board->turn);
    }

    return 0;
}


static void BoardUpdatePPieces(BoardT *board)
{
    int i, j, piece;

    for (i = 0; i < NUM_SQUARES; i++)
    {
	board->pPiece[i] = NULL;
	if ((piece = board->coord[i]))
	{
	    for (j = 0; j < board->pieceList[piece].lgh; j++)
	    {
		if (board->pieceList[piece].coords[j] == i)
		{
		    board->pPiece[i] = &board->pieceList[piece].coords[j];
		}
	    }
	}
    }
}


// Random-move support.
typedef struct {
    int coord;
    int randPos;
} RandPosT;
typedef int (*RAND_COMPAREFUNC)(const void *, const void *);
static int randCompareHelper(const RandPosT *p1, const RandPosT *p2)
{
    return p1->randPos - p2->randPos;
}


void BoardRandomize(BoardT *board)
{
    int i, j, len;
    RandPosT randPos[NUM_SQUARES];
    CoordListT *pieceList;

    memset(randPos, 0, sizeof(randPos));
    for (i = 0; i < NUM_PIECE_TYPES; i++)
    {
	pieceList = &board->pieceList[i];

	len = pieceList->lgh;
	for (j = 0; j < len; j++)
	{
	    randPos[j].coord = pieceList->coords[j];
	    randPos[j].randPos = random();
	}

	qsort(randPos, len, sizeof(RandPosT),
	      (RAND_COMPAREFUNC) randCompareHelper);

	for (j = 0; j < len; j++)
	{
	    pieceList->coords[j] = randPos[j].coord;
	}
    }

    BoardUpdatePPieces(board);
}


static void copyHelper(BoardT *dest, BoardT *src, int len)
{
    int i;
    PositionElementT *myElem;
    // Prevent this from taking too long in pathological cases.
    int numPositions = MIN(src->ncpPlies, NUM_SAVED_POSITIONS);

    // Have something good to load, so copy it over.
    memmove(dest, src, len);

    // We need to rebuild the pPiece array.
    BoardUpdatePPieces(dest);

    // Must also rebuild the posList hash.  We could cheat and manipulate
    // pointers, but if we really need that, we should just look at skipping
    // the whole thing.
    for (i = 0; i < NUM_SAVED_POSITIONS; i++)
    {
	ListInit(&dest->posList[i]);
	ListElementInit(&dest->positions[i].el);
    }

    // (note: the current position is not put into the hash until a later
    // BoardPositionSave() call.)
    for (i = 0; i < numPositions; i++)
    {
	myElem = &dest->positions[(dest->ply - (dest->ncpPlies - i)) &
				  (NUM_SAVED_POSITIONS - 1)];
	// We try to avoid copying empty positions, just so we do not have to
	// check against them.
	if (myElem->zobrist != 0)
	{
	    ListPush(&dest->posList[myElem->zobrist &
				    (NUM_SAVED_POSITIONS - 1)],
		     myElem);
	}
    }
}


void BoardCopy(BoardT *dest, BoardT *src)
{
    // We attempt to copy every variable prior to 'board->depth'.
    copyHelper(dest, src, (void *) &src->depth - (void *) src);
}

void BoardSet(BoardT *board, uint8 *pieces, int cbyte, int ebyte, int turn,
	      // These are usually 0.
	      int firstPly, int ncpPlies)
{
    int i;
    uint8 myPieces[NUM_SQUARES];

    // 'feature': prevent 'pieces' overwrite even if it == board->coord.
    memcpy(myPieces, pieces, NUM_SQUARES);

    BoardInit(board);

    // copy all of the pieces over.
    memcpy(board->coord, myPieces, NUM_SQUARES);

    // init pieceList/pPiece.
    for (i = 0; i < NUM_SQUARES; i++)
	if (board->coord[i])
	    pieceAdd(board, i, board->coord[i]);

    board->turn = turn;
    board->repeatPly = -1;

    // cbyte, ebyte handling.
    board->cbyte = cbyte;
    board->ebyte = ebyte;

    board->ply = firstPly;
    board->ncpPlies = ncpPlies;

    // ncheck handling (assumes 1 K of each color).
    for (i = 0; i < NUM_PLAYERS; i++)
    {
	calcNCheck(board, i, "BoardSet");
    }

    board->zobrist = BoardZobristCalc(board);
}


// Could put in a check regarding the turn, but for the only user of this
// function, that is actually not desirable.
int BoardIsNormalStartingPosition(BoardT *board)
{
    return board->cbyte == ALLCASTLE && board->ebyte == FLAG &&
	board->ncpPlies == 0 &&
	memcmp(board->coord, gPreCalc.normalStartingPieces, NUM_SQUARES) == 0;
}


bool BoardDrawInsufficientMaterial(BoardT *board)
{
    int b1, b2;

    if (
	// K vs k
	board->totalStrength == 0 ||

	// (KN or KB) vs k
	(board->totalStrength == EVAL_KNIGHT &&
	 board->pieceList[PAWN].lgh + board->pieceList[BPAWN].lgh == 0))
    {
	return true;
    }

    if (
	// KB vs kb, bishops on same color
	board->totalStrength == (EVAL_BISHOP << 1) &&
	board->pieceList[BISHOP].lgh == 1 &&
	board->pieceList[BBISHOP].lgh == 1)
    {
	b1 = board->pieceList[BISHOP].coords[0];
	b2 = board->pieceList[BBISHOP].coords[0];
	return
	    !((Rank(b1) + File(b1) +
	       Rank(b2) + File(b2)) & 1);
    }

    return false;
}


// With minor modification, we could also detect 1 repeat, but it would be
// more expensive.
bool BoardDrawThreefoldRepetition(BoardT *board)
{
    int repeats, ncpPlies, ply;

    // 4th ply would be first possible repeat, 8th ply is 2nd and final repeat.
    // Tried checking board->repeatPly != -1, but it just made things slower.
    // Reframing 'ncpPlies' into a 'stopPly' also does not seem to win.
    if (board->ncpPlies >= 8)
    {
	// FIXME: might rephrase this in terms of searching the posList
	// instead.  But it would only be for readability, since this
	// function is below the profiling threshold.
	repeats = 0;
	// Limit the counter to something useful.  This cripples the normal
	// case to prevent the pathological worst case (huge ncpPlies).
	ncpPlies = MIN(board->ncpPlies, NUM_SAVED_POSITIONS) - 4;
	for (ply = board->ply - 4;
	     ncpPlies >= 4 || (repeats == 1 && ncpPlies >= 0);
	     ncpPlies -= 2, ply -= 2)
	{
	    if (BoardPositionHit
		(board, board->positions[ply & (NUM_SAVED_POSITIONS - 1)].zobrist)
		&&
		// At this point we have a full match.
		++repeats == 2)
	    {
		return true;
	    }
	}
    }
    return false;
}

bool BoardPositionsSame(BoardT *b1, BoardT *b2)
{
    return
	!memcmp(b1->coord, b2->coord, sizeof(b1->coord)) &&
	b1->cbyte == b2->cbyte &&
	b1->ebyte == b2->ebyte &&
	b1->turn == b2->turn;
}

// BoardDrawThreefoldRepetition() is fast and (due to representing positions
// by zobrist) very close to but not 100% accurate.
// This function is slow and 100% accurate (modulo bugs).
// The way this function is typically used, we only need to search the last
// ~100 plies, (because otherwise matching positions would also trigger the
// 50-move rule claimed draw) but in something like crazyhouse we would want
// to search the entire move history.  For now we do that even though it's
// slower.
bool BoardDrawThreefoldRepetitionFull(BoardT *board, void *sgame)
{
    BoardT prevPositionsBoard;
    int numRepeats = 0;
    SaveGameT *mySgame = sgame;
    // + 1 because the 1st compare might be the same ply
    int searchPlies = board->ncpPlies + 1;

    BoardInit(&prevPositionsBoard);
    SaveGameGotoPly(mySgame, SaveGameLastPly(mySgame), &prevPositionsBoard,
		    NULL);
    do
    {
	if (board->ply != prevPositionsBoard.ply &&
	    BoardPositionsSame(board, &prevPositionsBoard) &&
	    ++numRepeats == 2)
	{
	    return true;
	}
    } while (--searchPlies > 0 &&
	     SaveGameGotoPly(mySgame, SaveGameCurrentPly(mySgame) - 1,
			     &prevPositionsBoard, NULL) == 0);
    return false;
}

// Calculates (roughly) how 'valuable' a move is.
int BoardCapWorthCalc(BoardT *board, MoveT *move)
{
    int cappiece = board->coord[move->dst];
    int capWorth = WORTH(cappiece);
    CvT *cv;
    int i;
    char result[15];

    if (cappiece && capWorth == EVAL_ROYAL) // Captured king, cannot happen
    {
	cv = &board->cv;
	// prints out moves in reverse order.
	for (i = MIN(MAX_CV_DEPTH, board->depth) - 1;
	     i >= 0 && cv->moves[i].src != FLAG;
	     i--)
	{
	    LOG_EMERG("%d:%s\n", i, moveToFullStr(result, &cv->moves[i]));
	}

	// Possibly ran out of moves on a searcherThread.  Check if the
	// main thread has any additional moves.
	cv = CompMainCv();
	for (;
	     i >= 0 && cv->moves[i].src != FLAG;
	     i--)
	{
	    LOG_EMERG("%d:%s\n", i, moveToFullStr(result, &cv->moves[i]));
	}
	LogMoveShow(eLogEmerg, board, move, "diagnostic");
	assert(0);
    }

    if (move->promote)
    {
	// Add in extra value for promotion or en passant
	// (for en passant, there is no 'cappiece')
	capWorth += WORTH(move->promote);
	if (!ISPAWN(move->promote))
	    capWorth -= EVAL_PAWN;
    }

    return capWorth;
}


// The following routines are meant to be used by edit-position style routines.
// The (new) philosophy is to keep the BoardT consistent where possible.
// Here, 'piece' can be 0 (empty square).
void BoardPieceSet(BoardT *board, int coord, int piece)
{
    int i = coord;

    if (board->coord[i] == piece)
    {
	return; // nothing to do
    }
    if (board->coord[i])
    {
	// remove the original piece on the board.
	pieceDelete(board, i, board->coord[i]);

	if (board->ebyte == i)
	{
	    BoardEbyteSet(board, FLAG);
	}
    }
    if (piece)
    {
	pieceAdd(board, i, piece);
    }
    CoordUpdateZ(board, i, piece);
    BoardCbyteUpdate(board);
    calcNCheck(board, board->turn, "BoardPieceSet");
}

void BoardCbyteSet(BoardT *board, int cbyte)
{
    cbyteUpdate(board, cbyte);
    // In case somebody turned on something they were not supposed to,
    // mask it back out.
    BoardCbyteUpdate(board);
}

void BoardEbyteSet(BoardT *board, int ebyte)
{
    // Override the ebyte variable if necessary.
    if (ebyte != FLAG &&
	!((board->turn && board->coord[ebyte] == PAWN &&
	   ebyte >= 24 && ebyte < 32) ||
	  (!board->turn && board->coord[ebyte] == BPAWN &&
	   ebyte >= 32 && ebyte < 40)))
    {
	ebyte = FLAG;
    }
    ebyteUpdate(board, ebyte);
}

void BoardTurnSet(BoardT *board, int turn)
{
    if (turn != board->turn)
    {
	board->turn = turn;
	board->zobrist ^= gPreCalc.zobrist.turn;
	// Because it cannot be valid, now
	BoardEbyteSet(board, FLAG);
	calcNCheck(board, board->turn, "BoardTurnSet");
    }
}
