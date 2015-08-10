//--------------------------------------------------------------------------
//                 MoveList.cpp - MoveList-oriented functions.
//                           -------------------
//  copyright            : (C) 2011 by Lucian Landry
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "gDynamic.h"
#include "gPreCalc.h"
#include "log.h"
#include "MoveList.h"
#include "uiUtil.h"
#include "variant.h"



//--------------------------------------------------------------------------
//                       PRIVATE CLASS DECLARATIONS:
//--------------------------------------------------------------------------

namespace // start unnamed namespace
{

// Why do this?  2 wins here:
// 1) We can remove private methods from the header file
// 2) Since the methods are in an unnamed namespace, they can be inlined if
//    they are called only once.  Since this class contains hot code, it is
//    important that that happen when possible.
class PrivMoveList : public MoveList
{
public:
    void GenerateLegalMoves(const BoardT &board, bool generateCapturesOnly);
    void addMove(const BoardT &board, cell_t from, cell_t to,
                 PieceType promote, cell_t dc, cell_t chk);
private:
    void addMoveFast(const BoardT &board, cell_t from, cell_t to,
                     cell_t dc, cell_t chk);
    void addMoveCalcChk(const BoardT &board, cell_t from, cell_t to,
                        PieceType promote, cell_t dc);
    void promo(const BoardT &board, cell_t from, cell_t to, cell_t dc);
    void cappose(const BoardT &board, cell_t attcoord,
                 const PinsT &pinlist, uint8 turn, cell_t kcoord,
                 const PinsT &dclist);
    void probe(const BoardT &board, const cell_t *moves,
               cell_t from, uint8 turn, cell_t dc, Piece myPiece,
               const Piece *coord, cell_t ekcoord, int capOnly);
    void generateBishopRookMoves(const BoardT &board, cell_t from, uint8 turn,
                                 uint8 pintype, const int *dirs, cell_t dc);
    void generatePawnMoves(const BoardT &board, cell_t from, uint8 turn,
                           uint8 pintype, cell_t dc);
    void checkCastle(const BoardT &board, cell_t kSrc, cell_t kDst,
                     cell_t rSrc, cell_t rDst, bool isCastleOO);
    void generateKingCastleMoves(const BoardT &board, cell_t src, uint8 turn);
    void generateKingMoves(const BoardT &board, cell_t from, uint8 turn,
                           cell_t dc);
    void generateKnightMoves(const BoardT &board, cell_t from, uint8 turn,
                             cell_t dc);
};

// According to http://en.cppreference.com/w/cpp/utility/program/exit,
//  thread-local destructors will always run before any static destructors.
// That means the main thread will crash if it tries to push to the free list
//  in a static destructor.
// Of course the question remains ... is the storage space still there?
// According to:
//  http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2008/n2659.htm
// I am guessing it is, since "The address of a thread variable may be freely
//  used during the variable's lifetime by any thread in the program".
class FreeMoves
{
public:
    bool exiting;
    std::vector<std::vector<MoveT> *> freeMoves;
    FreeMoves() : exiting(false) {}
    ~FreeMoves() { exiting = true; }
};

} // end unnamed namespace
    
//--------------------------------------------------------------------------
//                       PRIVATE FUNCTIONS AND METHODS:
//--------------------------------------------------------------------------

// Optimization: ordering by occurence in profiling information (requiring
// forward declarations) was tried, but does not help.

static inline void addSrcCoord(CoordListT &attlist, cell_t from)
{
    attlist.coords[attlist.lgh++] = from;
}

static bool nopose(const Piece *coord, cell_t src, cell_t dest, cell_t hole)
// checks to see if there are any occupied squares between 'src' and 'dest'.
// returns: false if blocked, true if nopose.  Note:  doesn't check if
//  dir == DIRFLAG (none) or 8 (knight attack), so shouldn't be called in that
//  case.
// Also does not check if 'src' == 'dst'.
{
    int dir = gPreCalc.dir[src] [dest];
    cell_t *to = gPreCalc.moves[dir] [src];
    while (*to != dest)
    {
        if (!coord[*to].IsEmpty() && *to != hole)
            // 'hole' is used to skip over a certain square, pretending no
            // piece exists there.  This is useful in several cases.  (But
            // otherwise, 'hole' should be FLAG.)
            return false;   // some sq on the way to dest is occupied.
        to++;
    }
    return true; // notice we always hit dest before we hit end of list.
}

// These need -O2 to win, probably.
// Return the 'to' coordinate if a given move results in a check, or FLAG
// otherwise.
#define NIGHTCHK(to, ekcoord) \
    (gPreCalc.dir[to] [ekcoord] == 8 ? (to) : FLAG)

#define QUEENCHK(coord, to, from, ekcoord) \
    (gPreCalc.dir[to] [ekcoord] < 8 && \
     nopose(coord, to, ekcoord, from) ? (to) : FLAG)

#define BISHOPCHK(coord, to, from, ekcoord) \
    (!((gPreCalc.dir[to] [ekcoord]) & 0x9) /* !DIRFLAG or nightmove */ && \
     nopose(coord, to, ekcoord, from) ? (to) : FLAG)

#define ROOKCHK(coord, to, from, ekcoord) \
    (((gPreCalc.dir[to] [ekcoord]) & 1) /* !DIRFLAG */ && \
     nopose(coord, to, ekcoord, from) ? (to) : FLAG)

// Tried pre-calculating this, but it did not seem to be a win.
#define PAWNCHK(to, ekcoord, turn) \
    (abs(File(ekcoord) - File(to)) == 1 && \
     Rank(to) - Rank(ekcoord) == ((turn) << 1) - 1 ? (to) : FLAG)

// Given 'dc' (src coordinate of a piece that could possibly check the enemy),
// return 'dc' if the piece blocking it will give discovered check, or 'FLAG'
// otherwise.
#define CALCDC(dc, from, to) \
    ((dc) == FLAG ? FLAG : \
     gPreCalc.dir[from] [dc] == gPreCalc.dir[to] [dc] ? FLAG : \
     (dc))

static inline bool HistoryWindowHit(const BoardT &board, cell_t from, cell_t to)
{
    return
        board.level - board.depth > 1 && // disable when quiescing.
        abs(gVars.hist[board.turn] [from] [to] - board.ply) < gVars.hiswin;
}

// Does not take promotion, en passant, or castling into account.
static inline bool isPreferredMoveFast(const BoardT &board, cell_t from,
                                       cell_t to, cell_t chk)
{
    // capture, check, or history move w/ depth?  Want good spot.
    return !board.coord[to].IsEmpty() || chk != FLAG ||
        HistoryWindowHit(board, from, to);
}

static inline bool isPreferredMove(const BoardT &board, cell_t from,
                                   cell_t to, cell_t chk,
                                   PieceType promote)
{
    return (from != to && !board.coord[to].IsEmpty()) ||
        promote != PieceType::Empty || chk != FLAG ||
        HistoryWindowHit(board, from, to);
}

// Generate all possible enemy (!turn) sliding attack locations on 'from',
// whether blocked or not.  Note right now, we can generate a dir multiple x.
// (meaning, if (say) a Q and B are attacking a king, we will add both, where
// we might just want to add the closer piece.)
// We might want to change this, but given how it is used, that might be slower.
static void genSlide(CoordListT &dirlist, const BoardT &board, cell_t from,
                     uint8 turn)
{
    // Optimized.  Index into the enemy pieceList.
    // I'm also moving backward to preserve the move ordering in cappose().
    const CoordListT *pl = &board.pieceList[Piece(turn ^ 1, PieceType::Queen).ToIndex()];
    int i;
    cell_t to;
    
    dirlist.lgh = 0;   // init list.

    // find queen sliding attacks.
    for (i = 0; i < pl->lgh; i++)
    {
        to = pl->coords[i];
        if (gPreCalc.dir[from] [to] < 8) // !(DIRFLAG or nightmove)
            addSrcCoord(dirlist, to);
    }
    pl += (Piece(0, PieceType::Rook).ToIndex() - Piece(0, PieceType::Queen).ToIndex());

    // find rook sliding attacks.
    for (i = 0; i < pl->lgh; i++)
    {
        to = pl->coords[i];
        if (gPreCalc.dir[from] [to] & 1) // !DIRFLAG
            addSrcCoord(dirlist, to);
    }
    pl += (Piece(0, PieceType::Bishop).ToIndex() - Piece(0, PieceType::Rook).ToIndex());

    // find bishop sliding attacks.
    for (i = 0; i < pl->lgh; i++)
    {
        to = pl->coords[i];
        if (!(gPreCalc.dir[from] [to] & 0x9)) // !(DIRFLAG || nightmove)
            addSrcCoord(dirlist, to);
    }
}

// Attempt to calculate any discovered check on an enemy king by doing an
//  enpassant capture.
static cell_t enpassdc(const BoardT &board, cell_t capturingPawnCoord)
{
    uint8 turn = board.turn;
    CoordListT attlist;
    int i;
    cell_t ekcoord = board.pieceList[Piece(turn ^ 1, PieceType::King).ToIndex()].coords[0];
    cell_t ebyte = board.ebyte; // shorthand.
    cell_t dc;
    uint8 dir = gPreCalc.dir[ebyte][ekcoord];

    if (dir < 8 &&
        nopose(board.coord, ebyte, ekcoord, capturingPawnCoord))
    {
        // This is semi-lazy (we could do something more akin to findpins())
        // but it will get the job done and it does not need to be quick.
        // Generate our sliding attacks on this square.
        genSlide(attlist, board, ebyte, turn ^ 1);
        for (i = 0; i < attlist.lgh; i++)
        {
            dc = attlist.coords[i];
            if (gPreCalc.dir[dc][ebyte] == dir &&
                nopose(board.coord, dc, ebyte, capturingPawnCoord))
            {
                return dc;
            }
        }
    }

    return FLAG;
}

// Make sure an en passant capture will not put us in check.
// Normally this is indicated by 'findpins()', but if the king is on the
//  same rank as the capturing (and captured) pawn, that routine is not
//  sufficient.
static bool enpassLegal(const BoardT &board, cell_t capturingPawnCoord)
{
    uint8 turn = board.turn;
    CoordListT attlist;
    int i;
    cell_t kcoord = board.pieceList[Piece(turn, PieceType::King).ToIndex()].coords[0];
    cell_t ebyte = board.ebyte; // shorthand.
    cell_t a;
    int dir = gPreCalc.dir[kcoord] [capturingPawnCoord];
    
    if ((dir == 3 || dir == 7) &&
        /// (now we know gPreCalc.dir[kcoord] [ebyte] also == (3 || 7))
        nopose(board.coord, ebyte, kcoord, capturingPawnCoord))
    {
        // This is semi-lazy (we could do something more akin to findpins())
        // but it will get the job done and it does not need to be quick.
        // Generate our sliding attacks on this square.
        genSlide(attlist, board, ebyte, turn);
        for (i = 0; i < attlist.lgh; i++)
        {
            a = attlist.coords[i];
            LOG_DEBUG("enpassLegal: check %c%c\n",
                      AsciiFile(a), AsciiRank(a));
            if (dir == gPreCalc.dir[ebyte] [a] &&
                nopose(board.coord, a, ebyte, capturingPawnCoord))
            {
                LOG_DEBUG("enpassLegal: return %c%c\n",
                          AsciiFile(a), AsciiRank(a));
                return false;
            }
        }
    }

    return true;
}

// When 'attList' == NULL, returns: whether coord 'from' is "attacked" by a
// piece (whether this is a friend or enemy piece depends on whether
// turn == onwho).
// When attList != NULL, we just fill up attList and always return 0.
// Another optimization: we assume attList is valid when turn != onwho.
static bool attacked(CoordListT *attList, const BoardT &board, int from, uint8 turn,
                     int onwho)
{
    int i;
    CoordListT dirlist;
    uint8 *moves;
    cell_t kcoord, ekcoord, to;
    const CoordListT *pl = &board.pieceList[Piece(onwho ^ 1, PieceType::Knight).ToIndex()];

    // check knight attack
    for (i = 0; i < pl->lgh; i++)
    {
        if (gPreCalc.dir[from] [pl->coords[i]] == 8)
        {
            if (attList == NULL)
                return true;
            addSrcCoord(*attList, pl->coords[i]);
        }
    }
        
    const Piece *coord = board.coord; // shorthand
    kcoord = board.pieceList[Piece(onwho, PieceType::King).ToIndex()].coords[0];

    // check sliding attack.
    genSlide(dirlist, board, from, onwho);
    for (i = 0; i < dirlist.lgh; i++)
    {
        if (nopose(coord, from, dirlist.coords[i],
                   turn == onwho ? kcoord : FLAG))
        {
            if (attList == NULL)
                return true;
            addSrcCoord(*attList, dirlist.coords[i]);
        }
    }
        
    ekcoord = board.pieceList[Piece(onwho ^ 1, PieceType::King).ToIndex()].coords[0];
    // check king attack, but *only* when computing *enemy* attacks
    // (we already find possible king moves in generateKingMoves()).
    if (turn == onwho &&
        abs(Rank(ekcoord) - Rank(from)) < 2 &&
        abs(File(ekcoord) - File(from)) < 2)
    {
        return true; // king can never doublecheck.
    }

    // check pawn attack...
    moves = gPreCalc.moves[10 + onwho] [from];

    // if attacked square unocc'd, and *friend* attack, want pawn advances.
    if (turn != onwho && coord[from].IsEmpty())
    {
        to = *(moves + 2);

        if (to != FLAG && coord[to].IsEnemy(onwho) &&
            coord[to].IsPawn()) // pawn ahead
        {
            // when turn != onwho, we may assume there is a valid attList.
            // if (attList == NULL) return true;
            addSrcCoord(*attList, to);
        }
        // now try e2e4 moves.
        else if (Rank(from) == 4 - onwho && coord[to].IsEmpty())
        {
            to = *(moves + 3);
            if (coord[to].IsEnemy(onwho) && coord[to].IsPawn())
            {
                // if (attList == NULL) return true;
                addSrcCoord(*attList, to);
            }
        }
    }
    else // otherwise, want p captures.
    {
        for (i = 0;
             i < 2;
             i++, moves++)
        {
            if ((to = *moves) != FLAG && coord[to].IsEnemy(onwho) &&
                coord[to].IsPawn()) // enemy on diag
            {
                if (attList == NULL)
                    return true;
                addSrcCoord(*attList, to);
            }
        }

        // may have to include en passant
        if (from == board.ebyte && turn != onwho)
        {
            for (i = -1; i < 2; i += 2)
            {
                if (coord[from + i].IsEnemy(onwho) &&
                    coord[from + i].IsPawn() &&
                    Rank(from) == Rank(from + i))
                {
                    // if (attList == NULL) return 1;
                    addSrcCoord(*attList, from + i);
                }
            }
        }
    }
    return false; // gee.  Guess we're not attacked... or we filled the list.
}

static bool castleAttacked(const BoardT &board, cell_t src, cell_t dest)
// returns: 'true' iff any square between 'src' and 'dest' is attacked,
// *not* including 'src' (we already presume that is not attacked) but including
// 'dst'.
// Note:  doesn't check if dir == DIRFLAG (none) or 8 (knight attack), so shouldn't be called in that case.
// Also does not check if src == dst.
{
    int dir = gPreCalc.dir[src] [dest];
    cell_t *to = gPreCalc.moves[dir] [src];
    uint8 turn = board.turn;

    while (true)
    {
        if (attacked(NULL, board, *to, turn, turn))
            return true; // some sq on the way to dest is occupied.
        if (*to == dest)
            break;
        to++;
    }
    // (we should always hit dest before we hit end of list.)
    return false;
}

static void findpins(PinsT &pinList, const BoardT &board, int kcoord, uint8 turn)
{
    int i;
    cell_t *x;
    cell_t pinLoc;
    CoordListT dirList;
    const Piece *coord = board.coord;

    // initialize pin array.
    for (i = 0; i < 8; i++)
        pinList.ll[i] = FLAG64;
    genSlide(dirList, board, kcoord, turn);
    // only check the possible pin dirs.
    for (i = 0; i < dirList.lgh; i++)
    {
        x = gPreCalc.moves[gPreCalc.dir[kcoord] [dirList.coords[i]]] [kcoord];

        while (coord[*x].IsEmpty())
            x++;

        if (coord[*x].IsEnemy(turn))
            continue; // pinned piece must be friend.
        
        pinLoc = *x; // location of possible pinned piece

        // a nopose() check might be easier here, but would probably take longer
        //  to find an actual pin.
        do
        {
            x++; // find next occupied space
        } while (coord[*x].IsEmpty());

        if (*x != dirList.coords[i])
            continue; // must have found our sliding-attack piece
            
        // by process of elimination, we have pinned piece.
        pinList.c[pinLoc] = gPreCalc.dir[kcoord] [*x] & 3;
        // LOG_DEBUG("pn:%c%c", AsciiFile(i), AsciiRank(i));
    }
}

static void gendclist(PinsT &dcList, const BoardT &board, cell_t ekcoord,
                      uint8 turn)
// Fills in dclist.  Each coordinate, if !FLAG, is a piece capable of giving
//  discovered check, and its value is the source coordinate of the
//  corresponding checking piece.
// Note: scenarios where a king is on the same rank as a friendly pawn that
//  just did an a2a4-style move, and an enemy pawn that can capture it
//  en-passant and give discovered check by an enemy rook/queen are not
//  detected.
{
    int i;
    CoordListT attList;
    cell_t *x;
    const Piece *coord = board.coord;

    for (i = 0; i < 8; i++)
        dcList.ll[i] = FLAG64;

    // generate our sliding attacks on enemy king.
    genSlide(attList, board, ekcoord, turn ^ 1);
    // check the possible dirs for a discovered check piece.
    for (i = 0; i < attList.lgh; i++)
    {
        x = gPreCalc.moves
            [gPreCalc.dir[attList.coords[i]] [ekcoord]]
            [attList.coords[i]];

        while (coord[*x].IsEmpty())
            x++;

        if (coord[*x].IsEnemy(turn))
            continue; // dc piece must be friend.

        if (nopose(coord, *x, ekcoord, FLAG))
            dcList.c[*x] = attList.coords[i]; // yes, it is a dc piece
    }
}

// optimization thoughts:
// -- could use separate vector for preferred moves.
// -- there is room for further optimization here during quiescing, because
//    all our moves are "preferred".
void PrivMoveList::addMoveFast(const BoardT &board, cell_t from, cell_t to,
                               cell_t dc, cell_t chk)
{
    // prefetching &move.back() + 1 for a write doesn't seem to help here.
    MoveT move;

    // actually follows FLAG:coord:DOUBLE_CHECK convention.
    // Read: no check, check, doublecheck.
    move.chk = dc == FLAG ? chk :
        chk == FLAG ? dc : DOUBLE_CHECK;
    move.src = from; // translate to move
    move.dst = to;
    move.promote = PieceType::Empty;

    if (isPreferredMoveFast(board, from, to, move.chk))
    {
        // capture, promo, check, or history move w/ depth?  Want good spot.
        if (moves.size() == insrt)
        {
            moves.push_back(move);
            insrt++;
        }
        else
        {
            moves.push_back(moves[insrt]);
            moves[insrt++] = move;
        }
    }
    else
    {
        moves.push_back(move);
    }
}

// A slightly slower version of the above that takes the possibility of
// promotion, castling, and en passant into consideration.
void PrivMoveList::addMove(const BoardT &board, cell_t from, cell_t to,
                           PieceType promote, cell_t dc, cell_t chk)
{
    MoveT move;
    
    move.src = from; // translate to move
    move.dst = to;
    move.promote = promote;
    // actually follows FLAG:coord:DOUBLE_CHECK convention.
    // Read: no check, check, doublecheck.
    move.chk = dc == FLAG ? chk :
        chk == FLAG ? dc : DOUBLE_CHECK;

    if (isPreferredMove(board, from, to, move.chk, promote))
    {
        // capture, promo, check, or history move w/ depth?  Want good spot.
        if (moves.size() == insrt)
        {
            moves.push_back(move);
            insrt++;
        }
        else
        {
            moves.push_back(moves[insrt]);
            moves[insrt++] = move;
        }
    }
    else
    {
        moves.push_back(move);
    }
}

// An even slower version that calculates whether a piece gives check on the
//  fly.  As an optimization (or really, laziness), this version also does not
//  support castling!
void PrivMoveList::addMoveCalcChk(const BoardT &board, cell_t from, cell_t to,
                                  PieceType promote, cell_t dc)
{
    cell_t chk;
    PieceType chkPieceType;
    const Piece *coord = board.coord;

    chkPieceType = promote != PieceType::Empty ?
        promote : board.coord[from].Type();
    switch (chkPieceType)
    {
    case PieceType::Knight:
        chk = NIGHTCHK(to, ekcoord);
        break;
    case PieceType::Queen:
        chk = QUEENCHK(coord, to, from, ekcoord);
        break;
    case PieceType::Bishop:
        chk = BISHOPCHK(coord, to, from, ekcoord);
        break;
    case PieceType::Rook:
        chk = ROOKCHK(coord, to, from, ekcoord);
        break;
    case PieceType::Pawn:
        chk = PAWNCHK(to, ekcoord, board.turn);
        break;
    default:
        chk = FLAG; // Kings cannot give check.
        break;
    }
    addMove(board, from, to, promote, dc, chk);
}

// generate all the moves for a promoting pawn.
void PrivMoveList::promo(const BoardT &board, cell_t from, cell_t to, cell_t dc)
{
    const Piece *coord = board.coord;
    addMove(board, from, to, PieceType::Queen, dc,
            QUEENCHK(coord, to, from, ekcoord));
    addMove(board, from, to, PieceType::Knight, dc,
            NIGHTCHK(to, ekcoord));
    addMove(board, from, to, PieceType::Rook, dc,
            ROOKCHK(coord, to, from, ekcoord));
    addMove(board, from, to, PieceType::Bishop, dc,
            BISHOPCHK(coord, to, from, ekcoord));
}

void PrivMoveList::cappose(const BoardT &board, cell_t attcoord,
                           const PinsT &pinlist, uint8 turn, cell_t kcoord,
                           const PinsT &dclist)
// king in check by one piece.  Find moves that capture or interpose,
// starting on the attacking square (as captures are preferred) and
// proceeding in the direction of the king.
{
    cell_t *j;
    int i, pintype;
    CoordListT attList;
    cell_t src, dest, dc;
    PieceType enpassPieceType;

    j = gPreCalc.moves[gPreCalc.dir[attcoord] [kcoord]] [attcoord];

    while (attcoord != kcoord)
    {
        // LOG_DEBUG("%c%c ", AsciiFile(attcoord), AsciiRank(attcoord));
        attList.lgh = 0;
        attacked(&attList, board, attcoord, turn, turn ^ 1);
        // have to add possible moves right now.
        // so search the pinlist for possible pins on the attackers.

        for (i = 0; i < attList.lgh; i++)
        {
            src = attList.coords[i];
            dest = attcoord;
            enpassPieceType = PieceType::Empty;
            if (board.coord[src].IsPawn() &&
                Rank(src) == Rank(attcoord))  // special case: en passant
            {
                // It is worth noting that with a pawn-push discovered
                // check, we can never use en passant to get out of it.
                // So there is never occasion to need enpassLegal().
                assert(dest == board.ebyte);
                enpassPieceType = PieceType::Pawn;
                dest += (-2 * turn + 1) << 3;
            }

            /// LOG_DEBUG("Trying %c%c%c%c ",
            //            AsciiFile(src), AsciiRank(src),
            //            AsciiFile(dest), AsciiRank(dest));
            pintype = pinlist.c[src];
            if (pintype == FLAG ||
                // pinned knights simply cannot move.
                (!board.coord[src].IsKnight() &&
                 pintype == (gPreCalc.dir[src] [dest] & 3)))
                // check pin.
            {
                dc = dclist.c[src];
                dc = CALCDC(dc, src, dest);
                // The friendly king prevents the three-check-vector problem
                //  described in generatePawnMoves() (because it interposes at
                //  least one of the discovered checks), so the below code is
                //  sufficient.
                if (enpassPieceType != PieceType::Empty && dc == FLAG)
                    dc = enpassdc(board, src);

                if (board.coord[src].IsPawn() && (dest < 8 || dest > 55))
                    promo(board, src, dest, dc);
                else
                    addMoveCalcChk(board, src, dest, enpassPieceType, dc);
            }
        }
        if (board.coord[attcoord].IsKnight())
            break; // cannot attack interposing squares in n's case.
        attcoord = *j;
        j++;  // get next interposing place.
    }
}

// probes sliding moves.  Piece should either be pinned in this direction,
// or not pinned.
void PrivMoveList::probe(const BoardT &board, const cell_t *moves,
                         cell_t from, uint8 turn, cell_t dc, Piece myPiece,
                         /* These last are for optimization purposes.
                            The function is inlined (once) so there is little
                            bloat.
                         */
                         const Piece *coord, cell_t ekcoord, int capOnly)
{
    cell_t to;
    PieceRelationship relationship;
        
    for (; (to = *moves) != FLAG; moves++)
    {
        relationship = coord[to].Relationship(turn);
        if (int(relationship) > capOnly)
        {
            addMoveFast(board, from, to, dc,
                        (myPiece.IsQueen() ?
                         QUEENCHK(coord, to, from, ekcoord) :
                         myPiece.IsBishop() ?
                         BISHOPCHK(coord, to, from, ekcoord) :
                         ROOKCHK(coord, to, from, ekcoord))
                );
        }
        if (relationship != PieceRelationship::Empty)
        {
            // Occupied.  Can't probe further.
            break;
        }
    }
}

void PrivMoveList::generateBishopRookMoves(const BoardT &board, cell_t from,
                                           uint8 turn, uint8 pintype,
                                           const int *dirs, cell_t dc)
{
    const Piece *coord = board.coord;
    Piece myPiece(coord[from]);

    do
    {
        if (pintype == FLAG || pintype == ((*dirs) & 3))
        {
            // piece either pinned in this direction, or not pinned
            probe(board, gPreCalc.moves[*dirs] [from], from, turn, dc,
                  myPiece,
                  coord, ekcoord, capOnly);
        }
    } while (*(++dirs) != FLAG);
}

void PrivMoveList::generatePawnMoves(const BoardT &board, cell_t from,
                                     uint8 turn, uint8 pintype, cell_t dc)
{
    int pindir;
    cell_t to, to2;
    const Piece *coord = board.coord;
    const uint8 *moves = gPreCalc.moves[10 + turn] [from];
    cell_t dc1, dc2, pawnchk;
    int promote;

    // generate captures (if any).
    for (pindir = 2; pindir >= 0; pindir -= 2)
    {
        to = *(moves++);
        if (to != FLAG &&
            (pintype == FLAG || pintype == (pindir ^ (turn << 1))))
        {
            // enemy on diag?
            if (coord[to].IsEnemy(turn))
            {
                // can we promote?
                if (to > 55 || to < 8)
                    promo(board, from, to, CALCDC(dc, from, to));
                else
                {
                    // normal capture.
                    addMoveFast(board, from, to,
                                CALCDC(dc, from, to),
                                PAWNCHK(to, ekcoord, turn));
                }
            }

            // en passant?
            else if (to - 8 + (turn << 4) == board.ebyte &&
                     enpassLegal(board, from))
            {
                // yes.
                dc1 = CALCDC(dc, from, to);
                dc2 = enpassdc(board, from);
                pawnchk = PAWNCHK(to, ekcoord, turn);

                /* with en passant, must take into account check created
                   when captured pawn was pinned.  So, there are actually
                   2 potential discovered check vectors + the normal check
                   vector.  Triple check is impossible with normal pieces,
                   but if any two vectors have check, we need to make sure
                   we handle it 'correctly' (if hackily). */
                if (dc1 == FLAG && dc2 != FLAG)
                    dc1 = dc2;
                else if (pawnchk == FLAG && dc2 != FLAG)
                    pawnchk = dc2;

                addMove(board, from, to,
                        PieceType::Pawn,
                        dc1,
                        pawnchk);
            }
        }
    }

    // Generate pawn pushes.
    to = *moves;
    promote = (to > 55 || to < 8);
    if (promote >= capOnly &&
        coord[to].IsEmpty() &&
        (pintype == FLAG || pintype == 1))
        // space ahead
    {   // can we promote?
        if (promote)
            promo(board, from, to, CALCDC(dc, from, to));
        else
        {
            // check e2e4-like moves
            if ((from > 47 || from < 16) &&
                coord[(to2 = *(++moves))].IsEmpty())
            {
                addMoveFast(board, from, to2,
                            CALCDC(dc, from, to2),
                            PAWNCHK(to2, ekcoord, turn));
            }
            // add e2e3-like moves.
            addMoveFast(board, from, to,
                        CALCDC(dc, from, to),
                        PAWNCHK(to, ekcoord, turn));
        }
    }
}

void PrivMoveList::checkCastle(const BoardT &board, cell_t kSrc, cell_t kDst,
                               cell_t rSrc, cell_t rDst, bool isCastleOO)
{
    // 'src' assumed to == castling->start.king.
    const Piece *coord = board.coord;

    // Chess 960 castling rules (from wikipedia):
    //  "All squares between the king's initial and final squares
    //   (including the final square), and all squares between the
    //   rook's initial and final squares (including the final square),
    //   must be vacant except for the king and castling rook."
    if (
        // Check if rook can move.
        (rSrc == rDst ||
         ((coord[rDst].IsEmpty() || rDst == kSrc) &&
          nopose(coord, rSrc, rDst, kSrc))) &&
        // Check if king can move.
        (kSrc == kDst ||
         ((coord[kDst].IsEmpty() || kDst == rSrc) &&
          nopose(coord, kSrc, kDst, rSrc) &&
          !castleAttacked(board, kSrc, kDst))))
    {
        cell_t sq =
            isCastleOO ? board.turn : (1 << NUM_PLAYERS_BITS) | board.turn;
        addMove(board, sq, sq, PieceType::Empty, FLAG,
                ROOKCHK(coord, rDst, kSrc, ekcoord));
    }
}

void PrivMoveList::generateKingCastleMoves(const BoardT &board, cell_t src,
                                           uint8 turn)
{
    // 'src' assumed to == castling->start.king.
    if (!capOnly) // assumed true: && board.ncheck[turn] == FLAG
    {
        CastleCoordsT *castling = &gVariant->castling[turn];

        // check for kingside castle
        if (BoardCanCastleOO(&board, turn))
        {
            checkCastle(board,
                        src, castling->endOO.king,
                        castling->start.rookOO, castling->endOO.rook,
                        true);
        }

        // check for queenside castle.
        if (BoardCanCastleOOO(&board, turn))
        {
            checkCastle(board,
                        src, castling->endOOO.king,
                        castling->start.rookOOO, castling->endOOO.rook,
                        false);
        }
    }
}

void PrivMoveList::generateKingMoves(const BoardT &board, cell_t from,
                                     uint8 turn, cell_t dc)
{
    const int *idx;
    const Piece *coord = board.coord;
    cell_t to;

    static const int preferredKDirs[NUM_PLAYERS] [9] =
        /* prefer increase rank for White... after that, favor center,
           queenside, and kingside moves, in that order.  Similar for Black,
           but decrease rank.
        */
        {{1, 0, 2, 7, 3, 5, 6, 4, FLAG},
         {5, 6, 4, 7, 3, 1, 0, 2, FLAG}};

    for (idx = preferredKDirs[turn]; *idx != FLAG; idx++)
    {
        to = *(gPreCalc.moves[*idx] [from]);

        if (to != FLAG &&
            int(coord[to].Relationship(turn)) > capOnly &&
            /* I could optimize a few of these calls out if I already
               did this while figuring out the castling moves. ... but I doubt
               it's a win. */
            !attacked(NULL, board, to, turn, turn))
        {
            addMoveFast(board, from, to,
                        CALCDC(dc, from, to),
                        FLAG);
        }
    }
}

void PrivMoveList::generateKnightMoves(const BoardT &board, cell_t from,
                                       uint8 turn, cell_t dc)
{
    cell_t *moves = gPreCalc.moves[8 + turn] [from];

    for (; *moves != FLAG; moves++)
    {
        if (int(board.coord[*moves].Relationship(turn)) > capOnly)
        {
            addMoveFast(board, from, *moves, dc,
                        NIGHTCHK(*moves, ekcoord));
        }
    }
}

void PrivMoveList::GenerateLegalMoves(const BoardT &board,
                                      bool generateCapturesOnly)
{
    int x, i, len;
    uint8 turn = board.turn;
    PinsT dclist, pinlist;
    const CoordListT *pl;
    cell_t kcoord = board.pieceList[Piece(turn, PieceType::King).ToIndex()].coords[0];

    // Initialize internal scratchpad variables.
    DeleteAllMoves();
    ekcoord = board.pieceList[Piece(turn ^ 1, PieceType::King).ToIndex()].coords[0];
    capOnly = generateCapturesOnly;

    static const int preferredQDirs[NUM_PLAYERS] [9] =
        /* prefer increase rank for White... after that, favor center,
           kingside, and queenside moves, in that order.  Similar for Black,
           but decrease rank.
        */
        {{1, 2, 0, 3, 7, 5, 4, 6, FLAG},
         {5, 4, 6, 3, 7, 1, 2, 0, FLAG}};
    static const int preferredBDirs[NUM_PLAYERS] [5] =
        {{2, 0, 4, 6, FLAG},
         {4, 6, 2, 0, FLAG}};
    static const int preferredRDirs[NUM_PLAYERS] [5] =
        {{1, 3, 7, 5, FLAG},
         {5, 3, 7, 1, FLAG}};

    /* generate list of pieces that can potentially give
       discovered check. A very short list. Non-sorted.*/
    gendclist(dclist, board, ekcoord, turn);

    // find all king pins (yay puns :)
    findpins(pinlist, board, kcoord, turn);

    if (board.ncheck[turn] == FLAG)
    {
        // Not in check.

        // Generate king castling moves.
        generateKingCastleMoves(board, kcoord, turn);

        // Generate pawn moves.
        pl = &board.pieceList[Piece(turn, PieceType::Pawn).ToIndex()];
        len = pl->lgh;
        for (i = 0; i < len; i++)
        {
            x = pl->coords[i];
            generatePawnMoves(board, x, turn, pinlist.c[x], dclist.c[x]);
        }
        // Generate queen moves.
        // Note: it is never possible for qmove to result in discovered
        //  check.  We optimize for this.
        pl +=
            Piece(0, PieceType::Queen).ToIndex() -
            Piece(0, PieceType::Pawn).ToIndex();
        for (i = 0; i < pl->lgh; i++)
        {
            x = pl->coords[i];
            generateBishopRookMoves(board, x, turn, pinlist.c[x],
                                    preferredQDirs[turn], FLAG);
        }
        // Generate bishop moves.
        pl +=
            Piece(0, PieceType::Bishop).ToIndex() -
            Piece(0, PieceType::Queen).ToIndex();
        for (i = 0; i < pl->lgh; i++)
        {
            x = pl->coords[i];
            generateBishopRookMoves(board, x, turn, pinlist.c[x],
                                    preferredBDirs[turn], dclist.c[x]);
        }
        pl +=
            Piece(0, PieceType::Knight).ToIndex() -
            Piece(0, PieceType::Bishop).ToIndex();
        // Generate night moves.
        for (i = 0; i < pl->lgh; i++)
        {
            x = pl->coords[i];
            // A pinned knight cannot move w/out checking its king.
            if (pinlist.c[x] == FLAG)
            {
                generateKnightMoves(board, x, turn, dclist.c[x]);
            }
        }
        // Generate rook moves.
        pl +=
            Piece(0, PieceType::Rook).ToIndex() -
            Piece(0, PieceType::Knight).ToIndex();

        for (i = 0; i < pl->lgh; i++)
        {
            x = pl->coords[i];
            generateBishopRookMoves(board, x, turn, pinlist.c[x],
                                    preferredRDirs[turn], dclist.c[x]);
        }
    }
    else if (board.ncheck[turn] != DOUBLE_CHECK)
    {
        // In check by 1 piece (only), so capture or interpose.
        cappose(board, board.ncheck[turn], pinlist, turn, kcoord,
                dclist);
    }

    // generate king (non-castling) moves.
    generateKingMoves(board, kcoord, turn, dclist.c[kcoord]);

    // Selection Sorting the captures does no good, empirically.
    // But, probably will do good when we extend captures.
}
    
static thread_local FreeMoves gFreeMoves;

static inline std::vector<MoveT> *allocMoves()
{
    std::vector<MoveT> *result;

    // It is not currently anticipated that the constructor will run
    // after thread-local variables have been destroyed, so we skip that check.
    if (!gFreeMoves.freeMoves.empty())
    {
        result = gFreeMoves.freeMoves.back();
        gFreeMoves.freeMoves.pop_back();
    }
    else
    {
        result = new std::vector<MoveT>;
        // let us assume we never have an empty(-capacity) vector.
        result->reserve(1);
    }

    return result;
}

//--------------------------------------------------------------------------
//                       PUBLIC FUNCTIONS AND METHODS:
//--------------------------------------------------------------------------

cell_t calcNCheck(BoardT &board, uint8 myTurn, const char *context)
{
    CoordListT attList;
    cell_t kcoord;
    Piece myPiece;

    if (board.pieceList[Piece(myTurn, PieceType::King).ToIndex()].lgh != 1)
    {
        // We do not know how to calculate check for a non-standard board.
        // This can happen in the middle of editing a position.
        // Leave it to BoardSanityCheck() or another routine to catch this.
        return (board.ncheck[myTurn] = FLAG);
    }

    kcoord =
        board.pieceList[Piece(myTurn, PieceType::King).ToIndex()].coords[0];

    // Minor sanity-check of board.
    myPiece = board.coord[kcoord];
    if (!myPiece.IsKing())
    {
        LOG_EMERG("calcNCheck (%s): bad king kcoord %d, piece %d\n",
                  context, kcoord, myPiece.ToIndex());
        assert(0);
    }

    attList.lgh = 0;
    attacked(&attList, board, kcoord, myTurn, myTurn);
    board.ncheck[myTurn] =
        attList.lgh >= 2 ? DOUBLE_CHECK :
        attList.lgh == 1 ? attList.coords[0] :
        FLAG;
    return board.ncheck[myTurn];
}

MoveList::MoveList() : moves(*allocMoves())
{
    DeleteAllMoves();
}

MoveList::~MoveList()
{
    // Recycle our 'moves' vectors for later use, to prevent excess allocations.
    if (!gFreeMoves.exiting)
        gFreeMoves.freeMoves.push_back(&moves);
    else
        delete &moves;
}

const MoveList &MoveList::operator=(const MoveList &other)
{
    insrt = other.insrt;
    ekcoord = other.ekcoord;
    capOnly = other.capOnly;
    moves = other.moves;
    return *this;
}

void MoveList::GenerateLegalMoves(const BoardT &board,
                                  bool generateCapturesOnly)
{
    static_cast<PrivMoveList *>(this)->
        GenerateLegalMoves(board, generateCapturesOnly);
}

void MoveList::SortByCapWorth(const BoardT &board)
{
    int i, j, besti;
    int maxWorth, myWorth;
    int cwArray[insrt]; // relies on variable-length arrays.  (sorry)

    for (i = 0; i < insrt; i++)
        cwArray[i] = BoardCapWorthCalc(&board, moves[i]);

    // Perform selection sort (by capture worth).  I don't bother caching
    //  capture worth, but if it shows up badly on gprof I can try it.
    for (i = 0; i < insrt - 1; i++)
    {
        // Find the best-worth move...
        for (j = i, besti = i, maxWorth = 0;
             j < insrt;
             j++)
        {
            myWorth = cwArray[j];
            if (myWorth > maxWorth)
            {
                maxWorth = myWorth;
                besti = j;
            }
        }

        // ... and if it's not the first move, swap w/it.
        if (besti != i)
        {
            std::swap(moves[i], moves[besti]);
            std::swap(cwArray[i], cwArray[besti]);
        }
    }
}

// In current profiles, this needs to be fast, so the code is pointerrific.
void MoveList::UseAsFirstMove(MoveT firstMove)
{
    MoveT *foundMove = SearchSrcDstPromote(firstMove);

    if (foundMove == nullptr)
        return; // We have a missing or non-sensical move; just bail.

    MoveT myMove = *foundMove; // save off the found move

    MoveT *startMove = &moves[0];
    MoveT *firstNonPreferredMove = &moves[insrt];

    if (foundMove >= firstNonPreferredMove)
    {
        // This was a non-preferred move.  Move the 1st non-preferred
        // move into its spot.
        *foundMove = *firstNonPreferredMove;

        // Move the 1st move to (what will be) the last preferred move.
        *firstNonPreferredMove = *startMove;
        insrt++;
    }
    else
    {
        // This move was preferred.  Move the first move into its spot.
        *foundMove = *startMove;
    }

    *startMove = myMove; // Now replace the first move.
}

MoveT *MoveList::SearchSrcDst(MoveT move)
{
    auto end = moves.end();

    for (auto iter = moves.begin(); iter < end; ++iter)
    {
        if (move.src == iter->src && move.dst == iter->dst)
        {
            return &(*iter);
        }
    }

    return nullptr; // move not found.
}

MoveT *MoveList::SearchSrcDstPromote(MoveT move)
{
    auto end = moves.end();

    // The point of clobbering this is to be able to do a faster comparison
    //  check.
    move.chk = 0;

    for (auto iter = moves.begin(); iter < end; ++iter)
    {
        MoveT tmpMove = *iter;
        tmpMove.chk = 0;
        
        if (move == tmpMove)
        {
            return &(*iter);
        }
    }

    return nullptr; // move not found.
}

MoveT *MoveList::Search(MoveT move)
{
    auto end = moves.end();
    
    for (auto iter = moves.begin(); iter < end; ++iter)
    {
        if (move == *iter)
        {
            return &(*iter);
        }
    }

    return nullptr; // move not found.
}

void MoveList::AddMove(const BoardT &board, MoveT move)
{
    static_cast<PrivMoveList *>(this)->
        addMove(board, move.src, move.dst, move.promote, FLAG, move.chk);
}

// Delete the move at index 'idx'.
void MoveList::DeleteMove(int idx)
{
    MoveT *move = &moves[idx];

    // I used 'insrt - 1' here but really we should always decrement
    // insrt for any preferred moves.
    if (IsPreferredMove(idx))
    {
        // Copy the last preferred move over this move (may be same move).
        *move = moves[--insrt];
        
        moves[insrt] = moves.back();
        moves.pop_back();
    }
    else
    {
        *move = moves.back(); // (may copy the move over itself)
        moves.pop_back();
    }
}

void MoveList::Log(LogLevelT level)
{
    char tmpStr[MOVE_STRING_MAX];
    const MoveStyleT style = {mnCAN, csK2, true};

    if (level > LogLevel())
	return; // no-op.

    LogPrint(level, "{mvlist NumMoves %d insrt %d ekcoord %d capOnly %d ",
             // Log the private member variables.
	     NumMoves(), insrt, ekcoord, capOnly);

    for (int i = 0; i < NumMoves(); i++)
	LogPrint(level, "%s ", MoveToString(tmpStr, moves[i], &style, NULL));

    LogPrint(level, "}\n");
}
