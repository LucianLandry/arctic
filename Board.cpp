//--------------------------------------------------------------------------
//                  board.cpp - BoardT-related functionality.
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU Lesser General Public License as
//   published by the Free Software Foundation; either version 2.1 of the
//   License, or (at your option) any later version.
//
//--------------------------------------------------------------------------

#include <stddef.h> // NULL
#include <stdlib.h> // exit(3)
#include <assert.h>

#include "gPreCalc.h"
#include "log.h"
#include "ref.h"
#include "ui.h"
#include "uiUtil.h"
#include "TransTable.h"
#include "Variant.h"

using arctic::File;
using arctic::Rank;

// #define DEBUG_CONSISTENCY_CHECK

static constexpr bool isPow2(int c)
{
    return c > 0 && (c & (c - 1)) == 0;
}

namespace // start unnamed namespace
{

class PrivBoard : public Board
{
public:
    void addPiece(cell_t coord, Piece piece);
    void setEmptyBoard();
    uint64 calcZobristFromMove(MoveT move) const;
    void doCastleMove(uint8 kSrc, uint8 kDst,
                      uint8 rSrc, uint8 rDst);
    void populateCastleCoords(bool castleOO, // alternative is OOO
                              cell_t &kSrc, cell_t &kDst,
                              cell_t &rSrc, cell_t &rDst) const;
    void capturePiece(cell_t coord, Piece piece);
    inline void removePiece(cell_t coord, Piece piece);
    void movePiece(cell_t src, cell_t dst, Piece piece);
    inline int positionInfoElementIndex(const PositionInfoElementT &elem) const;
    inline bool positionHit(uint64 posZobrist) const;
    inline void positionSave();
    inline void positionRestore();
    void updatePPieces();
    void syncPieceVectors(const Board &other);
    uint64 calcZobrist() const;
private:
    inline void updateCoord(cell_t coord, Piece piece);
    inline void addPieceZ(cell_t coord, Piece piece);
    inline void capturePieceZ(cell_t coord, Piece piece);
    inline void removePieceZ(cell_t coord, Piece piece);
    inline void updateCByte(uint8 newcbyte);
    inline void updateCByte();
    inline void updateEByte(cell_t newebyte);
};

} // end unnamed namespace

// Incremental update.  To be used everytime when board->coord[i] is updated.
// (This used to update a compressed equivalent of coord called 'hashCoord',
//  but now is just syntactic sugar.)
inline void PrivBoard::updateCoord(cell_t coord, Piece piece)
{
    SetPiece(coord, piece);
}

void PrivBoard::addPiece(cell_t coord, Piece piece)
{
    pieceCoords[piece.ToIndex()].push_back(coord);
    pPiece[coord] = &pieceCoords[piece.ToIndex()].back();
    totalStrength += piece.Worth();
    materialStrength[piece.Player()] += piece.Worth();
    updateCoord(coord, piece);
}

// Do everything necessary to add a piece to the board (except
// manipulating ebyte/cbyte).
inline void PrivBoard::addPieceZ(cell_t coord, Piece piece)
{
    addPiece(coord, piece);
    zobrist ^= gPreCalc.zobrist.coord[piece.ToIndex()] [coord];
}

void PrivBoard::capturePiece(cell_t coord, Piece piece)
{
    cell_t *capCoord = pPiece[coord];

    materialStrength[piece.Player()] -= piece.Worth();
    totalStrength -= piece.Worth();

    // change coord in pieceList and dec pieceList lgh.
    *capCoord = pieceCoords[piece.ToIndex()].back();
    pieceCoords[piece.ToIndex()].pop_back();

    // reset the end pPiece ptr to its new location.
    pPiece[*capCoord] = capCoord;
}

// like removePieceZ(), except assumes another piece will shortly fill this
// spot.  This might take on another meaning if we ever add variants like
// Crazyhouse.
inline void PrivBoard::capturePieceZ(cell_t coord, Piece piece)
{
    capturePiece(coord, piece);
    zobrist ^= gPreCalc.zobrist.coord[piece.ToIndex()] [coord];
}

// Do everything necessary to remove a piece from the board (except
// manipulating ebyte/cbyte and the zobrist).
inline void PrivBoard::removePiece(cell_t coord, Piece piece)
{
    capturePiece(coord, piece);
    pPiece[coord] = nullptr;
    updateCoord(coord, Piece());
}

// Do everything necessary to remove a piece from the board (except
// manipulating ebyte/cbyte).
inline void PrivBoard::removePieceZ(cell_t coord, Piece piece)
{
    capturePieceZ(coord, piece);
    pPiece[coord] = NULL;
    updateCoord(coord, Piece());
}

void PrivBoard::movePiece(cell_t src, cell_t dst, Piece piece)
{
    // Modify the pointer info in pPiece,
    // and the coords in the pieceList.
    *(pPiece[dst] = pPiece[src]) = dst;
    updateCoord(dst, piece);

    // These last two bits are technically unnecessary when we are unmaking a
    // move *and* it was a capture.
    pPiece[src] = NULL;
    updateCoord(src, Piece());
}

inline void PrivBoard::positionSave()
{
    PositionInfoElementT *myElem = &positions[ply & (NumSavedPositions - 1)];
    myElem->zobrist = zobrist;
    ListPush(&posList[zobrist & (NumSavedPositions - 1)], myElem);
}

inline void PrivBoard::positionRestore()
{
    if (unmakes.size() < NumSavedPositions)
        return;

    PositionInfoElementT *myElem = &positions[ply & (NumSavedPositions - 1)];
    myElem->zobrist = unmakes[unmakes.size() - NumSavedPositions].zobrist;
    ListPush(&posList[zobrist & (NumSavedPositions - 1)], myElem);
}

// This is useful for generating a hash for the initial board position, or
// (slow) validating the incrementally-updated hash.
uint64 PrivBoard::calcZobrist() const
{
    uint64 retVal = 0;
    int i;

    for (i = 0; i < NUM_SQUARES; i++)
        retVal ^= gPreCalc.zobrist.coord[PieceAt(i).ToIndex()] [i];

    retVal ^= gPreCalc.zobrist.cbyte[cbyte];
    if (Turn())
        retVal ^= gPreCalc.zobrist.turn;
    if (ebyte != FLAG)
        retVal ^= gPreCalc.zobrist.ebyte[ebyte];
    return retVal;
}

static inline uint8 calcCByteFromSrcDst(uint8 cbyte, uint8 src, uint8 dst)
{
    return cbyte == 0 ? 0 :
        cbyte & gPreCalc.castleMask[src] & gPreCalc.castleMask[dst];
}

static inline uint8 calcCByteFromCastle(uint8 cbyte, uint8 turn)
{
    return cbyte & ~(CASTLEBOTH << turn);
}

inline void PrivBoard::updateCByte(uint8 newcbyte)
{
    if (newcbyte != cbyte)
    {
        zobrist ^=
            (gPreCalc.zobrist.cbyte[cbyte] ^
             gPreCalc.zobrist.cbyte[newcbyte]);
        cbyte = newcbyte;
    }
}


inline void PrivBoard::updateEByte(cell_t newebyte)
{
    if (newebyte != ebyte)
    {
        if (ebyte != FLAG)
            zobrist ^= gPreCalc.zobrist.ebyte[ebyte];
        if (newebyte != FLAG)
            zobrist ^= gPreCalc.zobrist.ebyte[newebyte];
        ebyte = newebyte;
    }
}


// Updates castle status.
inline void PrivBoard::updateCByte()
{
    if (cbyte != 0) // be lazy when possible
        updateCByte(calcNewCByte());
}

// Returns whether an index is between 'start' and 'finish' (inclusive).
static bool serialBetween(int i, int start, int finish)
{
    return start <= finish ?
        i >= start && i <= finish : // 'normal' case.
        i >= start || i <= finish;  // wraparound case.
}

inline int PrivBoard::positionInfoElementIndex(const PositionInfoElementT &elem)
    const
{
    return &elem - positions;
}

inline bool PrivBoard::positionHit(uint64 posZobrist) const
{
    return zobrist == posZobrist;
}

void PrivBoard::populateCastleCoords(bool castleOO, // alternative is OOO
                                     cell_t &kSrc, cell_t &kDst,
                                     cell_t &rSrc, cell_t &rDst) const
{
    CastleCoordsT castling = Variant::Current()->Castling(turn);

    kSrc = castling.start.king;

    if (castleOO) // O-O castling
    {
        rSrc = castling.start.rookOO;
        kDst = castling.endOO.king;
        rDst = castling.endOO.rook;
    }
    else // assume O-O-O castling
    {
        rSrc = castling.start.rookOOO;
        kDst = castling.endOOO.king;
        rDst = castling.endOOO.rook;
    }
}

// returns true on success, false on failure.
bool Board::ConsistencyCheck(const char *failString) const
{
    const PrivBoard *priv = static_cast<const PrivBoard *>(this);
    int i;

    if (!IsLegal()) // check illegal position.
    {
        std::string errString;
        IsLegal(errString);
        LOG_EMERG("Board::ConsistencyCheck(%s): illegal position: %s\n",
                  failString, errString.c_str());
        Log(eLogEmerg);
        assert(0);
        return false;
    }

    for (i = 0; i < NUM_SQUARES; i++)
    {
        if (!PieceAt(i).IsEmpty() &&
            (pPiece[i] == nullptr || *pPiece[i] != i))
        {
            LOG_EMERG("Board::ConsistencyCheck(%s): failure at %c%c.\n",
                      failString,
                      AsciiFile(i), AsciiRank(i));
            Log(eLogEmerg);
            assert(0);
            return false;
        }
        // This requires a slight bit of extra work in BoardMove(Un)Make().
        // But it is the principle of least surprise.
        else if (PieceAt(i).IsEmpty() && pPiece[i] != nullptr)
        {
            LOG_EMERG("Board::ConsistencyCheck(%s): dangling pPiece at %c%c.\n",
                      failString,
                      AsciiFile(i), AsciiRank(i));
            Log(eLogEmerg);
            assert(0);
            return false;
        }
    }
    for (i = 0; i < kMaxPieces; i++)
    {
        for (const cell_t &coord : pieceCoords[i])
        {
            if (PieceAt(coord).ToIndex() != i || pPiece[coord] != &coord)
            {
                LOG_EMERG("Board::ConsistencyCheck(%s): failure in vector at "
                          "%d-%d (%d).\n",
                          failString, i, int(&coord - &pieceCoords[i][0]),
                          coord);
                Log(eLogEmerg);
                assert(0);
                return false;
            }
        }
    }
    if (zobrist != priv->calcZobrist())
    {
        LOG_EMERG("Board::ConsistencyCheck(%s): failure in zobrist calc "
                  "(%" PRIx64 ", %" PRIx64 ").\n",
                  failString, zobrist, priv->calcZobrist());
        Log(eLogEmerg);
        assert(0);
        return false;
    }
    return true;
}

// This is only meant to be used as a private helper func, not as a final state.
// It is assumed the Position has already been initialized to the empty
//  position.
void PrivBoard::setEmptyBoard()
{
    int i;
    
    ncheck = FLAG;
    zobrist = 0;
    for (i = NUM_PLAYERS; i < kMaxPieces; i++)
    {
        // Start at NUM_PLAYERS since "Empty" pieces are not tracked.
        // We need to reserve space ahead of time so the pPiece pointers do not
        //  go stale.
        pieceCoords[i].reserve(NUM_SQUARES);
        pieceCoords[i].resize(0);
    }
    for (i = 0; i < NUM_SQUARES; i++)
    {
        pPiece[i] = nullptr;
    }
    totalStrength = 0;
    for (i = 0; i < NUM_PLAYERS; i++)
    {
        materialStrength[i] = 0;
    }
    repeatPly = -1;

    for (i = 0; i < NumSavedPositions; i++)
    {
        ListInit(&posList[i]);
        ListElementInit(&positions[i].el);
        positions[i].zobrist = 0;
    }

    unmakes.resize(0);
}

// This is currently optimized for sanity and reuse, not speed.
bool Board::SetPosition(const class Position &position)
{
    PrivBoard *priv = static_cast<PrivBoard *>(this);

    if (!position.IsLegal())
        return false;

    // Wipe all undo/redo information etc.
    priv->setEmptyBoard();

    // Copy over the position proper
    Position::operator=(position);

    // Populate pieceCoords vector array, pPiece, totalStrength, and
    //  materialStrength.
    for (int i = 0; i < NUM_SQUARES; i++)
    {
        if (!PieceAt(i).IsEmpty())
            priv->addPiece(i, PieceAt(i));
    }

    // Now that those are setup, it is safe to:
    ncheck = calcNCheck("Board::Board");
    zobrist = priv->calcZobrist();

    return true;
}

Board::Board()
{
    // Sanity check.
    static_assert(NumSavedPositions >= 128 && isPow2(NumSavedPositions),
                  "NumSavedPositions must be >= 128 and a power of 2");
    if (!SetPosition(Variant::Current()->StartingPosition()))
        assert(0);
}

Board::Board(const Board &other)
{
    if (!other.ConsistencyCheck("Board::Board(const Board &)"))
        assert(0);
    *this = other;
}

Board &Board::operator=(const Board &other)
{
    PrivBoard *priv = static_cast<PrivBoard *>(this);

    if (this == &other)
        return *this;

    if (!SetPosition(other))  // wipes position undo/redo data.
        assert(0);

    priv->syncPieceVectors(other); // preserve randomization.

    repeatPly = other.repeatPly;

    // (note: the current position is not put into the hash until a later
    //  positionSave() call.)
    for (int i = 0; i < NumSavedPositions; i++)
    {
        PositionInfoElementT *myElem = &positions[i];

        // We try to avoid copying empty positions, just so we do not have to
        // check against them.
        if ((myElem->zobrist = other.positions[i].zobrist) != 0)
        {
            ListPush(&posList[myElem->zobrist & (NumSavedPositions - 1)],
                     myElem);
        }
    }
    
    unmakes = other.unmakes;

    return *this;
}

// Like MakeMove(), but do not actually make the move, just calculate
// a new zobrist.
uint64 PrivBoard::calcZobristFromMove(MoveT move) const
{
    bool enpass = move.IsEnPassant(); // en passant capture?
    bool promote = move.IsPromote();
    cell_t src = move.src;
    cell_t dst = move.dst;
    Piece myPiece(PieceAt(src));
    Piece capPiece(PieceAt(dst));
    uint64 result = zobrist;
    uint8 newcbyte;
    
    result ^= gPreCalc.zobrist.turn;

    if (ebyte != FLAG)
    {
        result ^= gPreCalc.zobrist.ebyte[ebyte];
    }

    if (move.IsCastle())
    {
        // Castling case, handle this specially (it can be relatively
        //  inefficient).
        uint8 turn = Turn(); // shorthand
        cell_t kSrc, kDst, rSrc, rDst;
        Piece kPiece(turn, PieceType::King);
        Piece rPiece(turn, PieceType::Rook);

        populateCastleCoords(move.IsCastleOO(),
                             kSrc, kDst, rSrc, rDst);

        newcbyte = calcCByteFromCastle(cbyte, turn);

        result ^=
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
        result ^=
            gPreCalc.zobrist.coord[capPiece.ToIndex()] [dst] ^
            // ... with the new piece that is supposed to be there ...
            gPreCalc.zobrist.coord[
                promote ? Piece(myPiece.Player(), move.promote).ToIndex() : myPiece.ToIndex()] [dst] ^
            // ... and remove the src piece from the source.
            gPreCalc.zobrist.coord[myPiece.ToIndex()] [src];

        if (abs(dst - src) == 16 && myPiece.IsPawn()) // pawn moved 2
        {
            result ^= gPreCalc.zobrist.ebyte[dst];
        }
        else if (enpass)
        {
            // Remove the pawn at the en passant square.
            result ^=
                gPreCalc.zobrist.coord[PieceAt(ebyte).ToIndex()] [ebyte];
        }
        else if ((newcbyte = calcCByteFromSrcDst(cbyte, src, dst)) !=
                 cbyte)
        {
            result ^=
                gPreCalc.zobrist.cbyte[cbyte] ^
                gPreCalc.zobrist.cbyte[newcbyte];
        }
    }

    return result;
}

void PrivBoard::doCastleMove(uint8 kSrc, uint8 kDst,
                             uint8 rSrc, uint8 rDst)
{
    // To accomodate variants like chess960, we must remove and re-add at least
    // one piece (to prevent piece clobbering).  Here, we choose the king.

    uint8 turn = Turn(); // shorthand
    Piece kPiece(turn, PieceType::King);
    Piece rPiece(turn, PieceType::Rook);

    removePiece(kSrc, kPiece);
    if (rSrc != rDst)
    {
        movePiece(rSrc, rDst, rPiece);
    }
    addPiece(kDst, kPiece);
}

void Board::MakeMove(MoveT move)
{
    bool enpass = move.IsEnPassant();
    bool promote = move.IsPromote();
    uint8 src = move.src;
    uint8 dst = move.dst;
    bool isCastle = move.IsCastle();
    Piece capPiece;
    uint8 newebyte, newcbyte;
    uint64 origZobrist = zobrist;
    bool repeatableMove = true;
    PrivBoard *priv = static_cast<PrivBoard *>(this);
    
    if (!isCastle)
        capPiece = PieceAt(dst);

#ifdef DEBUG_CONSISTENCY_CHECK
    ConsistencyCheck("Board::MakeMove");
#endif

    // We do not really need to do this when quiescing (if it is a non-
    //  repeatable move), but for normal moves, even 1 repeat (not a draw, yet)
    //  can effect the evaluation (via biasing against draw) and thus, can also
    //  affect the move we select.  To be revisited.
    priv->positionSave();
    
    // It is in fact faster (*barely*, 33.01 sec vs 33.05 sec for a
    // depth-10 search) to do this calculation ahead of time just so we can
    // prefetch it sooner, even when BoardZobristCalcFromMove() is not static.
    zobrist = priv->calcZobristFromMove(move);
    gTransTable.Prefetch(zobrist);

    assert(move != MoveNone); // This seems to happen too often.

    unmakes.resize(unmakes.size() + 1);
    UnMakeT &unmake = unmakes.back();
    
    // Save off board information.
    unmake.move = move;
    unmake.capPiece = capPiece;
    unmake.cbyte = cbyte;
    unmake.ebyte = ebyte;
    unmake.ncheck = ncheck;
    unmake.ncpPlies = ncpPlies;
    unmake.zobrist = origZobrist;
    unmake.repeatPly = repeatPly;

    // King castling move?
    if (isCastle)
    {
        cell_t kSrc, kDst, rSrc, rDst;

        repeatableMove = false;
        
        priv->populateCastleCoords(move.IsCastleOO(),
                                   kSrc, kDst, rSrc, rDst);

        priv->doCastleMove(kSrc, kDst, rSrc, rDst);
        newcbyte = calcCByteFromCastle(cbyte, turn);
        newebyte = FLAG;
    }
    else
    {
        Piece myPiece(PieceAt(src));
        newcbyte = calcCByteFromSrcDst(cbyte, src, dst);

        // Capture? better dump the captured piece from the pieceList..
        if (!capPiece.IsEmpty())
        {
            repeatableMove = false;
            priv->capturePiece(dst, capPiece);
        }
        else if (enpass)
        {
            priv->removePiece(ebyte, Piece(myPiece.Player() ^ 1, move.promote));
        }
        priv->movePiece(src, dst, myPiece);

        // El biggo question: did a promotion take place? Need to update
        // stuff further then.  Can be inefficient cause almost never occurs.
        if (promote)
        {
            priv->capturePiece(dst, myPiece);
            priv->addPiece(dst, Piece(myPiece.Player(), move.promote));
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

    cbyte = newcbyte;
    ebyte = newebyte;
    ply++;
    turn ^= 1;
    ncheck = move.chk;

    // Adjust ncpPlies appropriately.
    if (!repeatableMove)
    {
        ncpPlies = 0;
        repeatPly = -1;
    }
    else if (++ncpPlies >= 4 && repeatPly == -1)
    {
        PositionInfoElementT *myElem;
        
        // We might need to set repeatPly.
        ListT *myList = &posList[zobrist & (NumSavedPositions - 1)];
        LIST_DOFOREACH(myList, myElem) // Hopefully a short loop.
        {
            // idx(myElem) must be between board->ply - board->ncpPlies and
            // board->ply - 4 (inclusive) to be counted.
            if (serialBetween(priv->positionInfoElementIndex(*myElem),
                              ((ply - ncpPlies) &
                               (NumSavedPositions - 1)),
                              (ply - 4) & (NumSavedPositions - 1)) &&
                priv->positionHit(myElem->zobrist))
            {
                repeatPly = ply;
                break;
            }
        }
    }
#ifdef DEBUG_CONSISTENCY_CHECK
    ConsistencyCheck("Board::MakeMove2");
#endif
}


// Undoes a move on the board using the information stored in 'unmake'.
void Board::UnmakeMove()
{
    UnMakeT &unmake = unmakes.back();
    MoveT move = unmake.move;
    bool enpass = move.IsEnPassant();
    bool promote = move.IsPromote();
    uint8 src = move.src;
    uint8 dst = move.dst;
    Piece capPiece;
    PrivBoard *priv = static_cast<PrivBoard *>(this);
    
#ifdef DEBUG_CONSISTENCY_CHECK
    if (!ConsistencyCheck("Board::UnmakeMove1"))
    {
        LogMove(eLogEmerg, this, move, 0);
        assert(0);
    }
#endif
    ply--;
    turn ^= 1;

    // Pop the old bytes.  It's counterintuitive to do this so soon.
    // Sorry.
    capPiece = unmake.capPiece;
    cbyte = unmake.cbyte;
    ebyte = unmake.ebyte; // We need to do this before rest of the function.
    ncheck = unmake.ncheck;
    ncpPlies = unmake.ncpPlies;
    zobrist = unmake.zobrist;
    repeatPly = unmake.repeatPly;
    unmakes.pop_back();
    
    // King castling move?
    if (move.IsCastle())
    {
        cell_t kSrc, kDst, rSrc, rDst;

        priv->populateCastleCoords(move.IsCastleOO(),
                                   kSrc, kDst, rSrc, rDst);

        // (swapping the src and dst coordinates)
        priv->doCastleMove(kDst, kSrc, rDst, rSrc);
    }
    else
    {
        // El biggo question: did a promotion take place? Need to
        // 'depromote' then.  Can be inefficient cause almost never occurs.
        if (promote)
        {
            priv->capturePiece(dst, Piece(turn, move.promote));
            priv->addPiece(dst, Piece(turn, PieceType::Pawn));
        }
        priv->movePiece(dst, src, PieceAt(dst));

        // Add any captured piece back to the board.
        if (!capPiece.IsEmpty())
        {
            priv->addPiece(dst, capPiece);
        }
        else if (enpass)
        {
            // For multi-player support, it would be better to save this
            //  off as a captured piece.
            priv->addPiece(ebyte, Piece(turn ^ 1, move.promote));
        }
    }

    priv->positionRestore();

#ifdef DEBUG_CONSISTENCY_CHECK
    if (!ConsistencyCheck("Board::UnmakeMove2"))
    {
        LogMove(eLogEmerg, this, move, 0);
        assert(0);
    }
#endif
}


void PrivBoard::updatePPieces()
{
    int i;
    Piece piece;
    
    for (i = 0; i < NUM_SQUARES; i++)
    {
        pPiece[i] = NULL;
        piece = PieceAt(i);
        if (!piece.IsEmpty())
        {
            for (cell_t &coord : pieceCoords[piece.ToIndex()])
            {
                if (coord == i)
                    pPiece[i] = &coord;
            }
        }
    }
}

// preserves randomization (or lack thereof).
void PrivBoard::syncPieceVectors(const Board &other)
{
    const PrivBoard &privOther = static_cast<const PrivBoard &>(other);
    
    for (int i = 0; i < kMaxPieces; i++)
        pieceCoords[i] = privOther.pieceCoords[i];
    updatePPieces();
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


void Board::Randomize()
{
    int i, j, len;
    RandPosT randPos[NUM_SQUARES];
    PrivBoard *priv = static_cast<PrivBoard *>(this);
    
    memset(randPos, 0, sizeof(randPos));
    for (i = 0; i < kMaxPieces; i++)
    {
        auto &coordVec = pieceCoords[i];

        len = coordVec.size();
        for (j = 0; j < len; j++)
        {
            randPos[j].coord = coordVec[j];
            randPos[j].randPos = random();
        }

        qsort(randPos, len, sizeof(RandPosT),
              (RAND_COMPAREFUNC) randCompareHelper);

        for (j = 0; j < len; j++)
            coordVec[j] = randPos[j].coord;
    }

    priv->updatePPieces();
}

bool Board::IsNormalStartingPosition() const
{
    return Position() == Variant::Current()->StartingPosition();
}

bool Board::IsDrawInsufficientMaterial() const
{
    int b1, b2;

    if (
        // K vs k
        totalStrength == 0 ||

        // (KN or KB) vs k
        (totalStrength == Eval::Knight &&
         !PieceExists(Piece(0, PieceType::Pawn)) &&
         !PieceExists(Piece(1, PieceType::Pawn))))
    {
        return true;
    }

    if (
        // KB vs kb, bishops on same color
        totalStrength == (Eval::Bishop << 1) &&
        PieceCoords(Piece(0, PieceType::Bishop)).size() == 1 &&
        PieceCoords(Piece(1, PieceType::Bishop)).size() == 1)
    {
        b1 = PieceCoords(Piece(0, PieceType::Bishop))[0];
        b2 = PieceCoords(Piece(1, PieceType::Bishop))[0];
        return
            !((Rank(b1) + File(b1) +
               Rank(b2) + File(b2)) & 1);
    }

    return false;
}


// With minor modification, we could also detect 1 repeat, but it would be
// more expensive.
bool Board::IsDrawThreefoldRepetitionFast() const
{
    int repeats, myNcpPlies, myPly;
    const PrivBoard *priv = static_cast<const PrivBoard *>(this);
    
    // 4th ply would be first possible repeat, 8th ply is 2nd and final repeat.
    // Tried checking board->repeatPly != -1, but it just made things slower.
    // Reframing 'ncpPlies' into a 'stopPly' also does not seem to win.
    if (ncpPlies >= 8)
    {
        // FIXME: might rephrase this in terms of searching the posList
        // instead.  But it would only be for readability, since this
        // function is below the profiling threshold.
        repeats = 0;
        // Limit the counter to something useful.  This cripples the normal
        // case to prevent the pathological worst case (huge ncpPlies).
        myNcpPlies = MIN(ncpPlies, NumSavedPositions) - 4;
        for (myPly = ply - 4;
             myNcpPlies >= 4 || (repeats == 1 && myNcpPlies >= 0);
             myNcpPlies -= 2, myPly -= 2)
        {
            if (priv->positionHit
                (positions[myPly & (NumSavedPositions - 1)].zobrist) &&
                // At this point we have a full match.
                ++repeats == 2)
            {
                return true;
            }
        }
    }
    return false;
}

// BoardDrawThreefoldRepetition() is fast and (due to representing positions
//  by zobrist, which might, with very low probability, collide) not 100%
//  accurate.
// This function is slow and 100% accurate (modulo bugs).
// The way this function is typically used, we only need to search the last
//  ~100 plies, (because otherwise matching positions would also trigger the
//  50-move rule claimed draw) but in something like crazyhouse we would want
//  to search the entire move history.
bool Board::IsDrawThreefoldRepetition() const
{
    if (ncpPlies < 8)
        return false;

    int numRepeats = 0;
    Board tmpBoard(*this);

    for (int i = 0; i < ncpPlies; i++)
    {
        tmpBoard.UnmakeMove();        
        if (IsRepeatOf(tmpBoard.Position()) &&
            ++numRepeats == 2)
        {
            return true;
        }
    }
    return false;
}

// Calculates (roughly) how 'valuable' a move is.
int Board::CalcCapWorth(MoveT move) const
{
    if (move.IsCastle())
        return 0;

    Piece capPiece(PieceAt(move.dst));
    int capWorth = capPiece.Worth();

    if (!capPiece.IsEmpty() && capWorth == Eval::Royal)
        assert(0); // Captured king, cannot happen.

    if (move.promote != PieceType::Empty)
    {
        // Add in extra value for promotion or en passant
        // (for en passant, there is no 'capPiece')
        capWorth += Piece(0, move.promote).Worth();
        if (move.promote != PieceType::Pawn)
            capWorth -= Eval::Pawn;
    }

    return capWorth;
}

bool Board::IsLegalMove(MoveT move) const
{
    MoveList moveList;

    GenerateLegalMoves(moveList, false);
    return moveList.Search(move) != nullptr;
}

void Board::Log(LogLevelT level) const
{
    if (level > LogLevel())
    {
        return; // no-op
    }
    LogPrint(level, "{(Board %p) position ", this);
    Position::Log(level);

    LogPrint(level, " pieceCoords {");
    
    for (int i = 0; i < kMaxPieces; i++)
    {
        int size = pieceCoords[i].size();
        if (size)
        {
            LogPrint(level, "%s%d:{(vector) size: %d ",
                     (i == 0 ? "" : " "), i, size);
            for (int j = 0; j < size; j++)
            {
                LogPrint(level, "%s%c%c",
                         i == 0 ? "" : " ",
                         AsciiFile(pieceCoords[i][j]),
                         AsciiRank(pieceCoords[i][j]));
            }
            LogPrint(level, "}");
        }
    }
    LogPrint(level, "}}");
}

MoveT Board::MoveAt(int ply) const
{
    if (ply < BasePly() || ply >= Ply())
    {
        LOG_EMERG("%s: unexpected: requested ply %d, range %d-%d\n",
                  __func__, ply, BasePly(), Ply());
        assert(0);
        return MoveNone;
    }
    return unmakes[ply - BasePly()].move;
}

int Board::LastCommonPly(const Board &other) const
{
    int plyLow = MAX(BasePly(), other.BasePly());
    int plyHigh = MIN(Ply(), other.Ply());
    int i;
    
    if (plyLow > plyHigh)
        return -1; // No plies in common.

    // We do not wish for this to be destructive, so instead we use copies
    //  (which is slow).
    Board myTmp(*this), otherTmp(other);

    // Rewind each board back to the starting (maybe) common ply.
    for (i = 0; i < Ply() - plyLow; i++)
        myTmp.UnmakeMove();
    for (i = 0; i < other.Ply() - plyLow; i++)
        otherTmp.UnmakeMove();

    // When is a ply 'common'?
    // 1) at the start ply, the positions (including ncpPlies) must be the same.
    //  Keep in mind that we may be working with incomplete information
    //  (ncpPlies != 0), but this is best-effort and nobody sane should call us
    //  with that kind of position.
    if (myTmp.Position() != otherTmp.Position())
        return -1;

    // 2) the MoveAt()s for each board must be the same.
    for (i = plyLow; i < plyHigh; i++)
    {
        if (MoveAt(i) != other.MoveAt(i))
            break;
    }
    return i;
}
