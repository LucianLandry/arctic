//--------------------------------------------------------------------------
//                  board.cpp - BoardT-related functionality.
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
#include "variant.h"

// #define DEBUG_CONSISTENCY_CHECK

// Incremental update.  To be used everytime when board->coord[i] is updated.
// (This used to update a compressed equivalent of coord called 'hashCoord',
//  but now is just syntactic sugar.)
static inline void coordUpdate(BoardT *board, uint8 i, Piece piece)
{
    board->coord[i] = piece;
}

static void pieceAdd(BoardT *board, int coord, Piece piece)
{
    board->pPiece[coord] =
        &(board->pieceList[piece.ToIndex()]
          .coords[board->pieceList[piece.ToIndex()].lgh++]);
    *board->pPiece[coord] = coord;
    board->totalStrength += piece.Worth();
    board->playerStrength[piece.Player()] += piece.Worth();
    coordUpdate(board, coord, piece);
}

// Do everything necessary to add a piece to the board (except
// manipulating ebyte/cbyte).
static inline void pieceAddZ(BoardT *board, int coord, Piece piece)
{
    pieceAdd(board, coord, piece);
    board->zobrist ^= gPreCalc.zobrist.coord[piece.ToIndex()] [coord];
}

static void pieceCapture(BoardT *board, int coord, Piece piece)
{
    uint8 *pieceListCoord = board->pPiece[coord];

    board->playerStrength[piece.Player()] -= piece.Worth();
    board->totalStrength -= piece.Worth();

    // change coord in pieceList and dec pieceList lgh.
    *pieceListCoord = board->pieceList[piece.ToIndex()].coords
        [--board->pieceList[piece.ToIndex()].lgh];
    // reset the end pPiece ptr to its new location.
    board->pPiece[*pieceListCoord] = pieceListCoord;
}

// like pieceRemoveZ(), except assumes another piece will shortly fill this
// spot.  This might take on another meaning if we ever add variants like
// Crazyhouse.
static inline void pieceCaptureZ(BoardT *board, int coord, Piece piece)
{
    pieceCapture(board, coord, piece);
    board->zobrist ^= gPreCalc.zobrist.coord[piece.ToIndex()] [coord];
}

// Do everything necessary to remove a piece from the board (except
// manipulating ebyte/cbyte and the zobrist).
static inline void pieceRemove(BoardT *board, int coord, Piece piece)
{
    pieceCapture(board, coord, piece);
    board->pPiece[coord] = NULL;
    coordUpdate(board, coord, Piece());
}

// Do everything necessary to remove a piece from the board (except
// manipulating ebyte/cbyte).
static inline void pieceRemoveZ(BoardT *board, int coord, Piece piece)
{
    pieceCaptureZ(board, coord, piece);
    board->pPiece[coord] = NULL;
    coordUpdate(board, coord, Piece());
}

static void pieceMove(BoardT *board, int src, int dst, Piece piece)
{
    // Modify the pointer info in pPiece,
    // and the coords in the pieceList.
    *(board->pPiece[dst] = board->pPiece[src]) = dst;
    coordUpdate(board, dst, piece);

    // These last two bits are technically unnecessary when we are unmaking a
    // move *and* it was a capture.
    board->pPiece[src] = NULL;
    coordUpdate(board, src, Piece());
}

// This is useful for generating a hash for the initial board position, or
// (slow) validating the incrementally-updated hash.
static uint64 BoardZobristCalc(BoardT *board)
{
    uint64 retVal = 0;
    int i;
    for (i = 0; i < NUM_SQUARES; i++)
    {
        retVal ^= gPreCalc.zobrist.coord[board->coord[i].ToIndex()] [i];
    }
    retVal ^= gPreCalc.zobrist.cbyte[board->cbyte];
    if (board->turn)
        retVal ^= gPreCalc.zobrist.turn;
    if (board->ebyte != FLAG)
        retVal ^= gPreCalc.zobrist.ebyte[board->ebyte];
    return retVal;
}


static inline uint8 cbyteCalcFromSrcDst(uint8 cbyte, uint8 src, uint8 dst)
{
    return cbyte == 0 ? 0 :
        cbyte & gPreCalc.castleMask[src] & gPreCalc.castleMask[dst];
}

static inline uint8 cbyteCalcFromCastle(uint8 cbyte, uint8 turn)
{
    return cbyte & ~(CASTLEBOTH << turn);
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
    int cbyte = board->cbyte; // shorthand
    int i;
    Piece *coord;
    CastleStartCoordsT *castleStart;

    if (cbyte == 0)
    {
        return; // be lazy when possible
    }

    coord = board->coord; // shorthand
    
    for (i = 0; i < NUM_PLAYERS; i++)
    {
        castleStart = &gVariant->castling[i].start;

        if (coord[castleStart->king] != Piece(i, PieceType::King))
        {
            // No O-O or O-O-O castling.
            cbyte &= ~(CASTLEBOTH << i);
        }
        else
        {
            if (coord[castleStart->rookOO] != Piece(i, PieceType::Rook))
            {
                // No O-O castling.
                cbyte &= ~(CASTLEOO << i);
            }
            if (coord[castleStart->rookOOO] != Piece(i, PieceType::Rook))
            {
                // No O-O-O castling.
                cbyte &= ~(CASTLEOOO << i);
            }
        }
    }

    cbyteUpdate(board, cbyte);
}

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

static void getCastleCoords(BoardT *board,
                            bool castleOO, // alternative is OOO
                            uint8 *kSrc, uint8 *kDst,
                            uint8 *rSrc, uint8 *rDst)
{
    CastleCoordsT *castling = &gVariant->castling[board->turn];

    *kSrc = castling->start.king;

    if (castleOO) // O-O castling
    {
        *rSrc = castling->start.rookOO;
        *kDst = castling->endOO.king;
        *rDst = castling->endOO.rook;
    }
    else // assume O-O-O castling
    {
        *rSrc = castling->start.rookOOO;
        *kDst = castling->endOOO.king;
        *rDst = castling->endOOO.rook;
    }
}

// returns 0 on success, 1 on failure.
int BoardConsistencyCheck(BoardT *board, const char *failString, int checkz)
{
    int i, j, coord;
    for (i = 0; i < NUM_SQUARES; i++)
    {
        if (!board->coord[i].IsEmpty() && *board->pPiece[i] != i)
        {
            LOG_EMERG("BoardConsistencyCheck(%s): failure at %c%c.\n",
                      failString,
                      AsciiFile(i), AsciiRank(i));
            LogPieceList(board);
            exit(0);
            return 1;
        }
        // This requires a slight bit of extra work in BoardMove(Un)Make().
        // But it is the principle of least surprise.
        else if (board->coord[i].IsEmpty() && board->pPiece[i] != NULL)
        {
            LOG_EMERG("BoardConsistencyCheck(%s): dangling pPiece at %c%c.\n",
                      failString,
                      AsciiFile(i), AsciiRank(i));
            LogPieceList(board);
            exit(0);
            return 1;
        }
    }
    for (i = 0; i < kMaxPieces; i++)
    {
        for (j = 0; j < board->pieceList[i].lgh; j++)
        {
            coord = board->pieceList[i].coords[j];
            if (board->coord[coord].ToIndex() != i ||
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
                  "(%" PRIx64 ", %" PRIx64 ").\n",
                  failString, board->zobrist, BoardZobristCalc(board));
        LogPieceList(board);
        exit(0);
        return 1;
    }
    return 0;
}


void BoardInit(BoardT *board)
{
    // blank everything.
    memset(board, 0, sizeof(BoardT));
    CvInit(&board->cv);
}

// Like BoardMoveMake(), but do not actually make the move, just calculate
// a new zobrist.
static uint64 BoardZobristCalcFromMove(BoardT *board, MoveT move)
{
    uint64 zobrist = board->zobrist;
    bool enpass = move.promote == PieceType::Pawn; // en passant capture?
    bool promote = !(move.promote == PieceType::Empty || enpass);
    uint8 src = move.src;
    uint8 dst = move.dst;
    Piece *coord = board->coord; // shorthand
    Piece myPiece(coord[src]);
    Piece capPiece(coord[dst]);
    uint8 ebyte = board->ebyte;
    uint8 cbyte = board->cbyte, newcbyte;

    zobrist ^= gPreCalc.zobrist.turn;

    if (ebyte != FLAG)
    {
        zobrist ^= gPreCalc.zobrist.ebyte[ebyte];
    }

    if (MoveIsCastle(move))
    {
        // Castling case, handle this specially (it can be relatively inefficient).
        uint8 turn = board->turn; // shorthand
        uint8 kSrc, kDst, rSrc, rDst;
        Piece kPiece(turn, PieceType::King);
        Piece rPiece(turn, PieceType::Rook);

        getCastleCoords(board,
                        MoveIsCastleOO(move),
                        &kSrc, &kDst, &rSrc, &rDst);

        newcbyte = cbyteCalcFromCastle(cbyte, turn);

        zobrist ^=
            // Move the king to its destination.  This is "simple"
            // since we can assume no capture, en passant, or promotion takes
            // place.
            gPreCalc.zobrist.coord[kPiece.ToIndex()] [kDst] ^
            gPreCalc.zobrist.coord[kPiece.ToIndex()] [kSrc] ^

            // Do the same for the rook.
            gPreCalc.zobrist.coord[rPiece.ToIndex()] [rDst] ^
            gPreCalc.zobrist.coord[rPiece.ToIndex()] [rSrc] ^

            // And update the castling status.
            gPreCalc.zobrist.cbyte[cbyte] ^
            gPreCalc.zobrist.cbyte[newcbyte];
    }
    else
    {
        // Normal case.
        zobrist ^=
            gPreCalc.zobrist.coord[capPiece.ToIndex()] [dst] ^
            // ... with the new piece that is supposed to be there ...
            gPreCalc.zobrist.coord[
                promote ? Piece(myPiece.Player(), move.promote).ToIndex() : myPiece.ToIndex()] [dst] ^
            // ... and remove the src piece from the source.
            gPreCalc.zobrist.coord[myPiece.ToIndex()] [src];

        if (abs(dst - src) == 16 && myPiece.IsPawn()) // pawn moved 2
        {
            zobrist ^= gPreCalc.zobrist.ebyte[dst];
        }
        else if (enpass)
        {
            // Remove the pawn at the en passant square.
            zobrist ^=
                gPreCalc.zobrist.coord[coord[ebyte].ToIndex()] [ebyte];
        }
        else if ((newcbyte = cbyteCalcFromSrcDst(cbyte, src, dst)) != board->cbyte)
        {
            zobrist ^=
                gPreCalc.zobrist.cbyte[cbyte] ^
                gPreCalc.zobrist.cbyte[newcbyte];
        }
    }

    TransTablePrefetch(zobrist);
    return zobrist;
}

static void doCastleMove(BoardT *board,
                         uint8 kSrc, uint8 kDst,
                         uint8 rSrc, uint8 rDst)
{
    // To accomodate variants like chess960, we must remove and re-add at least
    // one piece (to prevent piece clobbering).  Here, we choose the king.

    uint8 turn = board->turn; // shorthand
    Piece kPiece(turn, PieceType::King);
    Piece rPiece(turn, PieceType::Rook);

    pieceRemove(board, kSrc, kPiece);
    if (rSrc != rDst)
    {
        pieceMove(board, rSrc, rDst, rPiece);
    }
    pieceAdd(board, kDst, kPiece);

}

void BoardMoveMake(BoardT *board, MoveT move, UnMakeT *unmake)
{
    bool enpass = move.promote == PieceType::Pawn;
    bool promote = !(move.promote == PieceType::Empty || enpass);
    uint8 src = move.src;
    uint8 dst = move.dst;
    Piece *coord = board->coord;
    bool isCastle = MoveIsCastle(move);
    Piece capPiece;
    uint8 newebyte, newcbyte;

    ListT *myList;
    PositionElementT *myElem;

    uint64 origZobrist = board->zobrist;
    bool repeatableMove = true;
    
    if (!isCastle)
    {
        capPiece = coord[dst];
    }
    
    // It is in fact faster (*barely*, 33.01 sec vs 33.05 sec for a
    // depth-10 search) to do this calculation ahead of time just so we can
    // prefetch it sooner, even when BoardZobristCalcFromMove() is not static.
    board->zobrist = BoardZobristCalcFromMove(board, move);
    TransTablePrefetch(board->zobrist);

    assert(src != FLAG); // This seems to happen too often.

#ifdef DEBUG_CONSISTENCY_CHECK
    BoardConsistencyCheck(board, "BoardMoveMake1", 1);
#endif

    if (unmake != NULL)
    {
        // Save off board information.
        unmake->move = move; // struct copy
        unmake->capPiece = capPiece;
        unmake->cbyte = board->cbyte;
        unmake->ebyte = board->ebyte;
        unmake->ncheck = board->ncheck[board->turn];
        unmake->ncpPlies = board->ncpPlies;
        unmake->zobrist = origZobrist;
        unmake->repeatPly = board->repeatPly;
    }

    // King castling move?
    if (isCastle)
    {
        uint8 kSrc, kDst, rSrc, rDst;

        repeatableMove = false;
        
        getCastleCoords(board,
                        MoveIsCastleOO(move),
                        &kSrc, &kDst, &rSrc, &rDst);

        doCastleMove(board, kSrc, kDst, rSrc, rDst);
        newcbyte = cbyteCalcFromCastle(board->cbyte, board->turn);
        newebyte = FLAG;
    }
    else
    {
        Piece myPiece(coord[src]);
        newcbyte = cbyteCalcFromSrcDst(board->cbyte, src, dst);

        // Capture? better dump the captured piece from the pieceList..
        if (!capPiece.IsEmpty())
        {
            repeatableMove = false;
            pieceCapture(board, dst, capPiece);
        }
        else if (enpass)
        {
            pieceRemove(board, board->ebyte, Piece(myPiece.Player() ^ 1, move.promote));
        }
        pieceMove(board, src, dst, myPiece);

        // El biggo question: did a promotion take place? Need to update
        // stuff further then.  Can be inefficient cause almost never occurs.
        if (promote)
        {
            pieceCapture(board, dst, myPiece);
            pieceAdd(board, dst, Piece(myPiece.Player(), move.promote));
        }

        if (myPiece.IsPawn())
        {
            repeatableMove = false;
            newebyte = abs(dst - src) == 16 ? // pawn moved 2
                dst : FLAG;
        }
        else
        {
            newebyte = FLAG;
        }
    }

    board->cbyte = newcbyte;
    board->ebyte = newebyte;
    board->ply++;
    board->turn ^= 1;
    board->ncheck[board->turn] = move.chk;

    // Adjust ncpPlies appropriately.
    if (!repeatableMove)
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


// Undoes a move on the board using the information stored in 'unmake'.
void BoardMoveUnmake(BoardT *board, UnMakeT *unmake)
{
    int turn;
    MoveT move = unmake->move; // struct copy
    bool enpass = move.promote == PieceType::Pawn;
    bool promote = !(move.promote == PieceType::Empty || enpass);
    uint8 src = move.src;
    uint8 dst = move.dst;
    Piece capPiece;

#ifdef DEBUG_CONSISTENCY_CHECK
    if (BoardConsistencyCheck(board, "BoardMoveUnmake1", unmake != NULL))
    {
        LogMove(eLogEmerg, board, &move);
        assert(0);
    }
#endif

    board->ply--;
    board->turn ^= 1;
    turn = board->turn;

    // Pop the old bytes.  It's counterintuitive to do this so soon.
    // Sorry.  Possible optimization: arrange the board variables
    // appropriately, and do a simple memcpy().
    capPiece = unmake->capPiece;
    board->cbyte = unmake->cbyte;
    board->ebyte = unmake->ebyte; // We need to do this before rest of
                                  // the function.
    board->ncheck[turn] = unmake->ncheck;
    board->ncpPlies = unmake->ncpPlies;
    board->zobrist = unmake->zobrist;
    board->repeatPly = unmake->repeatPly;

    // King castling move?
    if (MoveIsCastle(move))
    {
        uint8 kSrc, kDst, rSrc, rDst;

        getCastleCoords(board,
                        MoveIsCastleOO(move),
                        &kSrc, &kDst, &rSrc, &rDst);

        // (swapping the src and dst coordinates)
        doCastleMove(board, kDst, kSrc, rDst, rSrc);
    }
    else
    {
        // El biggo question: did a promotion take place? Need to
        // 'depromote' then.  Can be inefficient cause almost never occurs.
        if (promote)
        {
            pieceCapture(board, dst, Piece(turn, move.promote));
            pieceAdd(board, dst, Piece(turn, PieceType::Pawn));
        }
        pieceMove(board, dst, src, board->coord[dst]);

        // Add any captured piece back to the board.
        if (!capPiece.IsEmpty())
        {
            pieceAdd(board, dst, capPiece);
        }
        else if (enpass)
        {
            // For multi-player support, it would be better to save this
            //  off as a captured piece.
            pieceAdd(board, board->ebyte, Piece(turn ^ 1, move.promote));
        }
    }

#ifdef DEBUG_CONSISTENCY_CHECK
    if (BoardConsistencyCheck(board, "BoardMoveUnmake2", unmake != NULL))
    {
        LogMove(eLogEmerg, board, &move);
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
         (!board->coord[ebyte].IsPawn() ||
          !board->coord[ebyte].IsEnemy(turn) ||
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
    int i, j;

    // Check: pawns must not be on 1st or 8th rank.
    for (i = 0; i < NUM_SQUARES; i++)
    {
        if (i == 8) i = 56; // skip to the 8th rank.
        if (board->coord[i].IsPawn())
        {
            return reportError(silent,
                               "Error: Pawn detected on 1st or 8th rank.");
        }
    }

    // Check: only one king (of each color) on board.
    for (i = 0; i < NUM_PLAYERS; i++)
    {
        int numKings = board->pieceList[Piece(i, PieceType::King).ToIndex()].lgh;
        if (numKings != 1)
        {
            return reportError(silent,
                               "Error: Need one king of each color (player %d, found %d).",
                               i, numKings);
        }
    }

    // Check: the side *not* on move must not be in check.
    if (calcNCheck(*board, board->turn ^ 1, "BoardSanityCheck") != FLAG)
    {
        return reportError(silent,
                           "Error: Side not on move (%d) is in check.",
                           board->turn ^ 1);
    }

    // Check: Kings must not be adjacent to each other (calcNCheck() does not
    // test for this).
    for (i = 0; i < NUM_PLAYERS; i++)
    {
        kcoord = board->pieceList[Piece(i, PieceType::King).ToIndex()].coords[0];

        for (j = i + 1; j < NUM_PLAYERS; j++)
        {
            kcoord2 =
                board->pieceList[Piece(j, PieceType::King).ToIndex()].coords[0];

            if (abs(Rank(kcoord) - Rank(kcoord2)) < 2 &&
                abs(File(kcoord) - File(kcoord2)) < 2)
            {
                return reportError(
                    silent,
                    "Error: Kings (%d, %d) are adjacent to each other.",
                    i, j);
            }
        }
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
    int i, j;
    Piece piece;
    
    for (i = 0; i < NUM_SQUARES; i++)
    {
        board->pPiece[i] = NULL;
        piece = board->coord[i];
        if (!piece.IsEmpty())
        {
            for (j = 0; j < board->pieceList[piece.ToIndex()].lgh; j++)
            {
                if (board->pieceList[piece.ToIndex()].coords[j] == i)
                {
                    board->pPiece[i] =
                        &board->pieceList[piece.ToIndex()].coords[j];
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
    for (i = 0; i < kMaxPieces; i++)
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
    copyHelper(dest, src, offsetof(BoardT, depth));
}

void BoardSet(BoardT *board, Piece pieces[], int cbyte, int ebyte, int turn,
              // These are usually 0.
              int firstPly, int ncpPlies)
{
    int i;
    Piece myPieces[NUM_SQUARES];

    // 'feature': prevent 'pieces' overwrite even if it == board->coord.
    for (i = 0; i < NUM_SQUARES; i++)
    {
        myPieces[i] = pieces[i];
    }
    
    BoardInit(board);

    // copy all of the pieces over.
    for (i = 0; i < NUM_SQUARES; i++)
    {
        board->coord[i] = myPieces[i];
    }

    // init pieceList/pPiece.
    for (i = 0; i < NUM_SQUARES; i++)
    {
        if (!board->coord[i].IsEmpty())
        {
            pieceAdd(board, i, board->coord[i]);
        }
    }
    
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
        calcNCheck(*board, i, "BoardSet");
    }

    board->zobrist = BoardZobristCalc(board);
}


// Could put in a check regarding the turn, but for the only user of this
// function, that is actually not desirable.
int BoardIsNormalStartingPosition(BoardT *board)
{
    return board->cbyte == CASTLEALL && board->ebyte == FLAG &&
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
         (board->pieceList[Piece(0, PieceType::Pawn).ToIndex()].lgh +
          board->pieceList[Piece(1, PieceType::Pawn).ToIndex()].lgh == 0)))
    {
        return true;
    }

    if (
        // KB vs kb, bishops on same color
        board->totalStrength == (EVAL_BISHOP << 1) &&
        board->pieceList[Piece(0, PieceType::Bishop).ToIndex()].lgh == 1 &&
        board->pieceList[Piece(1, PieceType::Bishop).ToIndex()].lgh == 1)
    {
        b1 = board->pieceList[Piece(0, PieceType::Bishop).ToIndex()].coords[0];
        b2 = board->pieceList[Piece(1, PieceType::Bishop).ToIndex()].coords[0];
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
bool BoardDrawThreefoldRepetitionFull(BoardT *board, struct SaveGameS *sgame)
{
    BoardT prevPositionsBoard;
    int numRepeats = 0;
    // + 1 because the 1st compare might be the same ply
    int searchPlies = board->ncpPlies + 1;

    BoardInit(&prevPositionsBoard);
    SaveGameGotoPly(sgame, SaveGameLastPly(sgame), &prevPositionsBoard,
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
             SaveGameGotoPly(sgame, SaveGameCurrentPly(sgame) - 1,
                             &prevPositionsBoard, NULL) == 0);
    return false;
}

// Calculates (roughly) how 'valuable' a move is.
int BoardCapWorthCalc(const BoardT *board, MoveT move)
{
    Piece capPiece;
    int capWorth;
    const CvT *cv;
    int i;
    char result[MOVE_STRING_MAX];
    static const MoveStyleT msCwc = {mnDebug, csOO, false};

    if (MoveIsCastle(move))
    {
        return 0;
    }

    capPiece = board->coord[move.dst];
    capWorth = capPiece.Worth();

    if (!capPiece.IsEmpty() && capWorth == EVAL_ROYAL)
    {
        // Captured king, cannot happen.
        cv = &board->cv;
        // prints out moves in reverse order.
        for (i = MIN(MAX_CV_DEPTH, board->depth) - 1;
             i >= 0 && cv->moves[i].src != FLAG;
             i--)
        {
            LOG_EMERG("%d:%s\n", i,
                      MoveToString(result, cv->moves[i], &msCwc, NULL));
        }

        // Possibly ran out of moves on a searcherThread.  Check if the
        // main thread has any additional moves.
        cv = CompMainCv();
        for (;
             i >= 0 && cv->moves[i].src != FLAG;
             i--)
        {
            LOG_EMERG("%d:%s\n", i,
                      MoveToString(result, cv->moves[i], &msCwc, NULL));
        }
        LogMoveShow(eLogEmerg, board, move, "diagnostic");
        assert(0);
    }

    if (move.promote != PieceType::Empty)
    {
        // Add in extra value for promotion or en passant
        // (for en passant, there is no 'capPiece')
        capWorth += Piece(0, move.promote).Worth();
        if (move.promote != PieceType::Pawn)
        {
            capWorth -= EVAL_PAWN;
        }
    }

    return capWorth;
}


// The following routines are meant to be used by edit-position style routines.
// The (new) philosophy is to keep the BoardT consistent where possible.
// Here, 'piece' can be 0 (empty square).
void BoardPieceSet(BoardT *board, int coord, Piece piece)
{
    int i = coord;

    if (board->coord[i] == piece)
    {
        return; // nothing to do
    }
    if (!board->coord[i].IsEmpty())
    {
        // remove the original piece on the board.
        pieceRemoveZ(board, i, board->coord[i]);

        if (board->ebyte == i)
        {
            BoardEbyteSet(board, FLAG);
        }
    }
    if (!piece.IsEmpty())
    {
        pieceAddZ(board, i, piece);
    }
    BoardCbyteUpdate(board);
    calcNCheck(*board, board->turn, "BoardPieceSet");
}

void BoardCbyteSet(BoardT *board, int cbyte)
{
    cbyteUpdate(board, cbyte);
    // In case somebody turned on something they were not supposed to,
    // mask it back out.  Assumes the board pieces are setup.
    BoardCbyteUpdate(board);
}

void BoardEbyteSet(BoardT *board, int ebyte)
{
    // Override the ebyte variable if necessary.
    if (ebyte != FLAG &&
        !((board->turn && board->coord[ebyte] == Piece(0, PieceType::Pawn) &&
           ebyte >= 24 && ebyte < 32) ||
          (!board->turn && board->coord[ebyte] == Piece(1, PieceType::Pawn) &&
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
        calcNCheck(*board, board->turn, "BoardTurnSet");
    }
}
