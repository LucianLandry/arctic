//--------------------------------------------------------------------------
//               BoardMoveGen.cpp - move-generation for Boards.
//                           -------------------
//  copyright            : (C) 2015 by Lucian Landry
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

#include "Board.h"
#include "gPreCalc.h"
#include "MoveList.h"
#include "ref.h"
#include "uiUtil.h" // AsciiFile(), AsciiRank()
#include "Variant.h"

using arctic::File;
using arctic::Rank;

// stores pin info.  Only move-generation code should use this union.
typedef union
{
    uint8  c[NUM_SQUARES];
    uint64 ll[8]; // hardcoding '8' is rather brittle.
} PinsT;

typedef struct {
    int lgh;
    cell_t coords[NUM_SQUARES]; // src coord.  Not usually larger than 16, but
                                // with edit-position (or bughouse) we might get
                                // extra pieces ...
} CoordListT;

namespace // start unnamed namespace
{

// Why do this?  2 wins here:
// 1) We can remove private methods from the header file
// 2) Since the methods are in an unnamed namespace, they can be inlined if
//    they are called only once.  Since this class contains hot code, it is
//    important that that happen when possible.
class PrivBoard : public Board
{
public:
    void GenerateLegalMoves(MoveList &mvlist, bool generateCapturesOnly) const;
    bool attacked(CoordListT *attList, int from, uint8 turn, int onwho) const;
private:
    void addMoveCalcChk(MoveList &mvlist, cell_t from, cell_t to,
                        PieceType promote, cell_t dc) const;
    void promo(MoveList &mvlist, cell_t from, cell_t to, cell_t dc) const;
    void cappose(MoveList &mvlist, cell_t attcoord,
                 const PinsT &pinlist, cell_t kcoord,
                 const PinsT &dclist) const;
    void probe(MoveList &mvlist, const cell_t *moves,
               cell_t from, cell_t dc, Piece myPiece,
               cell_t ekcoord, int capOnly, MoveT move) const;
    void generateBishopRookMoves(MoveList &mvlist, cell_t from,
                                 uint8 pintype, const int *dirs,
                                 cell_t dc, cell_t ekcoord,
                                 int capOnly) const;
    void generatePawnMoves(MoveList &mvlist, cell_t from,
                           uint8 pintype, cell_t dc, cell_t ekcoord,
                           int capOnly) const;
    void checkCastle(MoveList &mvlist, cell_t kSrc, cell_t kDst,
                     cell_t rSrc, cell_t rDst, bool isCastleOO,
                     cell_t ekcoord) const;
    void generateKingCastleMoves(MoveList &mvlist, cell_t src,
                                 cell_t ekcoord, int capOnly) const;
    void generateKingMoves(MoveList &mvlist, cell_t from,
                           cell_t dc, int capOnly) const;
    void generateKnightMoves(MoveList &mvlist, cell_t from, cell_t dc,
                             cell_t ekcoord, int capOnly) const;
    bool nopose(cell_t src, cell_t dest, cell_t hole) const;
    cell_t enpassdc(cell_t capturingPawnCoord) const;
    void genSlide(CoordListT &dirlist, cell_t from, uint8 onwho) const;
    bool enpassLegal(cell_t capturingPawnCoord) const;
    bool castleAttacked(cell_t src, cell_t dest) const;
    void findpins(PinsT &pinList, int kcoord, uint8 turn) const;
    void gendclist(PinsT &dcList, cell_t ekcoord, uint8 turn) const;
};

} // end unnamed namespace

// Optimization: ordering by occurence in profiling information (requiring
// forward declarations) was tried, but does not help.

static inline void addSrcCoord(CoordListT &attlist, cell_t from)
{
    attlist.coords[attlist.lgh++] = from;
}

static inline cell_t mergeChk(cell_t chk1, cell_t chk2)
{
    return
        chk1 == FLAG ? chk2 :
        chk2 == FLAG ? chk1 :
        DOUBLE_CHECK;
}

// These need -O2 to win, probably.
// Return the 'to' coordinate if a given move results in a check, or FLAG
// otherwise.
#define NIGHTCHK(to, ekcoord) \
    (gPreCalc.dir[to] [ekcoord] == 8 ? (to) : FLAG)

#define QUEENCHK(to, from, ekcoord) \
    (gPreCalc.dir[to] [ekcoord] < 8 && \
     nopose(to, ekcoord, from) ? (to) : FLAG)

#define BISHOPCHK(to, from, ekcoord) \
    (!((gPreCalc.dir[to] [ekcoord]) & 0x9) /* !DIRFLAG or nightmove */ && \
     nopose(to, ekcoord, from) ? (to) : FLAG)

#define ROOKCHK(to, from, ekcoord) \
    (((gPreCalc.dir[to] [ekcoord]) & 1) /* !DIRFLAG */ && \
     nopose(to, ekcoord, from) ? (to) : FLAG)

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

// Generate all possible enemy (!onwho) sliding attack locations on 'from',
// whether blocked or not.  Note right now, we can generate a dir multiple x.
// (meaning, if (say) a Q and B are attacking a king, we will add both, where
// we might just want to add the closer piece.)
// We might want to change this, but given how it is used, that might be slower.
void PrivBoard::genSlide(CoordListT &dirlist, cell_t from, uint8 onwho) const
{
    dirlist.lgh = 0;   // init list.

    // find queen sliding attacks.
    for (cell_t coord : PieceCoords(Piece(onwho ^ 1, PieceType::Queen)))
    {
        if (gPreCalc.dir[from] [coord] < 8) // !(DIRFLAG or nightmove)
            addSrcCoord(dirlist, coord);
    }

    // find rook sliding attacks.
    for (cell_t coord : PieceCoords(Piece(onwho ^ 1, PieceType::Rook)))
    {
        if (gPreCalc.dir[from] [coord] & 1) // !DIRFLAG
            addSrcCoord(dirlist, coord);
    }

    // find bishop sliding attacks.
    for (cell_t coord : PieceCoords(Piece(onwho ^ 1, PieceType::Bishop)))
    {
        if (!(gPreCalc.dir[from] [coord] & 0x9)) // !(DIRFLAG || nightmove)
            addSrcCoord(dirlist, coord);
    }
}

// Attempt to calculate any discovered check on an enemy king by doing an
//  enpassant capture.
cell_t PrivBoard::enpassdc(cell_t capturingPawnCoord) const
{
    CoordListT attlist;
    int i;
    uint8 turn = Turn();
    cell_t ekcoord = PieceCoords(Piece(turn ^ 1, PieceType::King))[0];
    cell_t ebyte = EnPassantCoord(); // shorthand.
    cell_t dc;
    uint8 dir = gPreCalc.dir[ebyte][ekcoord];

    if (dir < 8 &&
        nopose(ebyte, ekcoord, capturingPawnCoord))
    {
        // This is semi-lazy (we could do something more akin to findpins())
        // but it will get the job done and it does not need to be quick.
        // Generate our sliding attacks on this square.
        genSlide(attlist, ebyte, turn ^ 1);
        for (i = 0; i < attlist.lgh; i++)
        {
            dc = attlist.coords[i];
            if (gPreCalc.dir[dc][ebyte] == dir &&
                nopose(dc, ebyte, capturingPawnCoord))
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
bool PrivBoard::enpassLegal(cell_t capturingPawnCoord) const
{
    uint8 turn = Turn();
    CoordListT attlist;
    int i;
    cell_t ebyte = EnPassantCoord();
    cell_t kcoord = PieceCoords(Piece(turn, PieceType::King))[0];
    cell_t a;
    int dir = gPreCalc.dir[kcoord] [capturingPawnCoord];
    
    if ((dir == 3 || dir == 7) &&
        /// (now we know gPreCalc.dir[kcoord] [ebyte] also == (3 || 7))
        nopose(ebyte, kcoord, capturingPawnCoord))
    {
        // This is semi-lazy (we could do something more akin to findpins())
        // but it will get the job done and it does not need to be quick.
        // Generate our sliding attacks on this square.
        genSlide(attlist, ebyte, turn);
        for (i = 0; i < attlist.lgh; i++)
        {
            a = attlist.coords[i];
            LOG_DEBUG("enpassLegal: check %c%c\n",
                      AsciiFile(a), AsciiRank(a));
            if (dir == gPreCalc.dir[ebyte] [a] &&
                nopose(a, ebyte, capturingPawnCoord))
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
bool PrivBoard::attacked(CoordListT *attList, int from, uint8 turn, int onwho) const
{
    int i;
    CoordListT dirlist;
    cell_t *moves;
    cell_t kcoord, ekcoord, to;

    // check knight attack
    for (cell_t coord : PieceCoords(Piece(onwho ^ 1, PieceType::Knight)))
    {
        if (gPreCalc.dir[from] [coord] == 8)
        {
            if (attList == NULL)
                return true;
            addSrcCoord(*attList, coord);
        }
    }
        
    kcoord = PieceCoords(Piece(onwho, PieceType::King))[0];

    // check sliding attack.
    genSlide(dirlist, from, onwho);
    for (i = 0; i < dirlist.lgh; i++)
    {
        if (nopose(from, dirlist.coords[i],
                   turn == onwho ? kcoord : FLAG))
        {
            if (attList == NULL)
                return true;
            addSrcCoord(*attList, dirlist.coords[i]);
        }
    }

    ekcoord = PieceCoords(Piece(onwho ^ 1, PieceType::King))[0];
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
    if (turn != onwho && PieceAt(from).IsEmpty())
    {
        to = moves[2];

        if (to != FLAG && PieceAt(to).IsEnemy(onwho) &&
            PieceAt(to).IsPawn()) // pawn ahead
        {
            // when turn != onwho, we may assume there is a valid attList.
            // if (attList == NULL) return true;
            addSrcCoord(*attList, to);
        }
        // now try e2e4 moves.
        else if (Rank(from) == 4 - onwho && PieceAt(to).IsEmpty())
        {
            to = moves[3];
            if (PieceAt(to).IsEnemy(onwho) && PieceAt(to).IsPawn())
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
            if ((to = *moves) != FLAG && PieceAt(to).IsEnemy(onwho) &&
                PieceAt(to).IsPawn()) // enemy on diag
            {
                if (attList == NULL)
                    return true;
                addSrcCoord(*attList, to);
            }
        }

        // may have to include en passant
        if (from == EnPassantCoord() && turn != onwho)
        {
            for (i = -1; i < 2; i += 2)
            {
                if (PieceAt(from + i).IsEnemy(onwho) &&
                    PieceAt(from + i).IsPawn() &&
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

bool PrivBoard::castleAttacked(cell_t src, cell_t dest) const
// returns: 'true' iff any square between 'src' and 'dest' is attacked,
// *not* including 'src' (we already presume that is not attacked) but including
// 'dst'.
// Note:  doesn't check if dir == DIRFLAG (none) or 8 (knight attack), so shouldn't be called in that case.
// Also does not check if src == dst.
{
    int dir = gPreCalc.dir[src] [dest];
    cell_t *to = gPreCalc.moves[dir] [src];
    uint8 turn = Turn();

    while (true)
    {
        if (attacked(NULL, *to, turn, turn))
            return true; // some sq on the way to dest is occupied.
        if (*to == dest)
            break;
        to++;
    }
    // (we should always hit dest before we hit end of list.)
    return false;
}

void PrivBoard::findpins(PinsT &pinList, int kcoord, uint8 turn) const
{
    int i;
    cell_t *x;
    cell_t pinLoc;
    CoordListT dirList;

    // initialize pin array.
    for (i = 0; i < 8; i++)
        pinList.ll[i] = FLAG64;
    genSlide(dirList, kcoord, turn);
    // only check the possible pin dirs.
    for (i = 0; i < dirList.lgh; i++)
    {
        x = gPreCalc.moves[gPreCalc.dir[kcoord] [dirList.coords[i]]] [kcoord];

        while (PieceAt(*x).IsEmpty())
            x++;

        if (PieceAt(*x).IsEnemy(turn))
            continue; // pinned piece must be friend.
        
        pinLoc = *x; // location of possible pinned piece

        // a nopose() check might be easier here, but would probably take longer
        //  to find an actual pin.
        do
        {
            x++; // find next occupied space
        } while (PieceAt(*x).IsEmpty());

        if (*x != dirList.coords[i])
            continue; // must have found our sliding-attack piece
            
        // by process of elimination, we have pinned piece.
        pinList.c[pinLoc] = gPreCalc.dir[kcoord] [*x] & 3;
        // LOG_DEBUG("pn:%c%c", AsciiFile(i), AsciiRank(i));
    }
}

void PrivBoard::gendclist(PinsT &dcList, cell_t ekcoord, uint8 turn) const
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

    for (i = 0; i < 8; i++)
        dcList.ll[i] = FLAG64;

    // generate our sliding attacks on enemy king.
    genSlide(attList, ekcoord, turn ^ 1);
    // check the possible dirs for a discovered check piece.
    for (i = 0; i < attList.lgh; i++)
    {
        x = gPreCalc.moves
            [gPreCalc.dir[attList.coords[i]] [ekcoord]]
            [attList.coords[i]];

        while (PieceAt(*x).IsEmpty())
            x++;

        if (PieceAt(*x).IsEnemy(turn))
            continue; // dc piece must be friend.

        if (nopose(*x, ekcoord, FLAG))
            dcList.c[*x] = attList.coords[i]; // yes, it is a dc piece
    }
}


bool PrivBoard::nopose(cell_t src, cell_t dest, cell_t hole) const
// checks to see if there are any occupied squares between 'src' and 'dest'.
// returns: false if blocked, true if nopose.  Note:  doesn't check if
//  dir == DIRFLAG (none) or 8 (knight attack), so shouldn't be called in that
//  case.
// Also does not check if 'src' == 'dest'.
{
    int dir = gPreCalc.dir[src] [dest];
    cell_t *to = gPreCalc.moves[dir] [src];
    while (*to != dest)
    {
        if (!PieceAt(*to).IsEmpty() && *to != hole)
            // 'hole' is used to skip over a certain square, pretending no
            // piece exists there.  This is useful in several cases.  (But
            // otherwise, 'hole' should be FLAG.)
            return false;   // some sq on the way to dest is occupied.
        to++;
    }
    return true; // notice we always hit dest before we hit end of list.
}

    
// An even slower version that calculates whether a piece gives check on the
//  fly.  As an optimization (or really, laziness), this version also does not
//  support castling!
void PrivBoard::addMoveCalcChk(MoveList &mvlist, cell_t from, cell_t to,
                               PieceType promote, cell_t dc) const
{
    cell_t chk;
    PieceType chkPieceType;
    cell_t ekcoord = PieceCoords(Piece(Turn() ^ 1, PieceType::King))[0];
    
    chkPieceType = promote != PieceType::Empty ?
        promote : PieceAt(from).Type();
    switch (chkPieceType)
    {
    case PieceType::Knight:
        chk = NIGHTCHK(to, ekcoord);
        break;
    case PieceType::Queen:
        chk = QUEENCHK(to, from, ekcoord);
        break;
    case PieceType::Bishop:
        chk = BISHOPCHK(to, from, ekcoord);
        break;
    case PieceType::Rook:
        chk = ROOKCHK(to, from, ekcoord);
        break;
    case PieceType::Pawn:
        chk = PAWNCHK(to, ekcoord, Turn());
        break;
    default:
        chk = FLAG; // Kings cannot give check.
        break;
    }
    MoveT move = ToMove(from, to, promote, mergeChk(chk, dc));
    mvlist.AddMove(move, *this);
}

// generate all the moves for a promoting pawn.
void PrivBoard::promo(MoveList &mvlist, cell_t from, cell_t to, cell_t dc) const
{
    cell_t ekcoord = PieceCoords(Piece(Turn() ^ 1, PieceType::King))[0];
    MoveT move = ToMove(from, to, PieceType::Queen,
                        mergeChk(dc, QUEENCHK(to, from, ekcoord)));
    mvlist.AddMove(move, *this);

    move.promote = PieceType::Knight;
    move.chk = mergeChk(dc, NIGHTCHK(to, ekcoord));
    mvlist.AddMove(move, *this);

    move.promote = PieceType::Rook;
    move.chk = mergeChk(dc, ROOKCHK(to, from, ekcoord));
    mvlist.AddMove(move, *this);

    move.promote = PieceType::Bishop;
    move.chk = mergeChk(dc, BISHOPCHK(to, from, ekcoord));
    mvlist.AddMove(move, *this);
}

void PrivBoard::cappose(MoveList &mvlist, cell_t attcoord,
                        const PinsT &pinlist, cell_t kcoord,
                        const PinsT &dclist) const
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
        attacked(&attList, attcoord, Turn(), Turn() ^ 1);
        // have to add possible moves right now.
        // so search the pinlist for possible pins on the attackers.

        for (i = 0; i < attList.lgh; i++)
        {
            src = attList.coords[i];
            dest = attcoord;
            enpassPieceType = PieceType::Empty;
            if (PieceAt(src).IsPawn() &&
                Rank(src) == Rank(attcoord))  // special case: en passant
            {
                // It is worth noting that with a pawn-push discovered
                // check, we can never use en passant to get out of it.
                // So there is never occasion to need enpassLegal().
                assert(dest == EnPassantCoord());
                enpassPieceType = PieceType::Pawn;
                dest += (-2 * Turn() + 1) << 3;
            }

            /// LOG_DEBUG("Trying %c%c%c%c ",
            //            AsciiFile(src), AsciiRank(src),
            //            AsciiFile(dest), AsciiRank(dest));
            pintype = pinlist.c[src];
            if (pintype == FLAG ||
                // pinned knights simply cannot move.
                (!PieceAt(src).IsKnight() &&
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
                    dc = enpassdc(src);

                if (PieceAt(src).IsPawn() && (dest < 8 || dest > 55))
                    promo(mvlist, src, dest, dc);
                else
                    addMoveCalcChk(mvlist, src, dest, enpassPieceType, dc);
            }
        }
        if (PieceAt(attcoord).IsKnight())
            break; // cannot attack interposing squares in n's case.
        attcoord = *j;
        j++;  // get next interposing place.
    }
}

// probes sliding moves.  Piece should either be pinned in this direction,
// or not pinned.
void PrivBoard::probe(MoveList &mvlist, const cell_t *moves,
                      cell_t from, cell_t dc, Piece myPiece,
                      cell_t ekcoord, int capOnly, MoveT move) const
{
    cell_t to;
    PieceRelationship relationship;

    for (; (to = *moves) != FLAG; moves++)
    {
        relationship = PieceAt(to).Relationship(Turn());
        if (int(relationship) > capOnly)
        {
            move.dst = to;
            move.chk = mergeChk(dc,
                                (myPiece.IsQueen() ?
                                 QUEENCHK(to, from, ekcoord) :
                                 myPiece.IsBishop() ?
                                 BISHOPCHK(to, from, ekcoord) :
                                 ROOKCHK(to, from, ekcoord)));
            mvlist.AddMoveFast(move, *this);
        }
        if (relationship != PieceRelationship::Empty)
            break; // Occupied.  Can't probe further.
    }
}

void PrivBoard::generateBishopRookMoves(MoveList &mvlist, cell_t from,
                                        uint8 pintype, const int *dirs,
                                        cell_t dc, cell_t ekcoord,
                                        int capOnly) const
{
    Piece myPiece(PieceAt(from));
    MoveT move;

    move.src = from;
    move.promote = PieceType::Empty;
    
    do
    {
        if (pintype == FLAG || pintype == ((*dirs) & 3))
        {
            // piece either pinned in this direction, or not pinned
            probe(mvlist, gPreCalc.moves[*dirs] [from], from, dc,
                  myPiece, ekcoord, capOnly, move);
        }
    } while (*(++dirs) != FLAG);
}

void PrivBoard::generatePawnMoves(MoveList &mvlist, cell_t from,
                                  uint8 pintype, cell_t dc, cell_t ekcoord,
                                  int capOnly) const
{
    int pindir;
    cell_t to, to2;
    cell_t dc1, dc2, pawnchk;
    uint8 turn = Turn();
    const uint8 *moves = gPreCalc.moves[10 + turn] [from];
    int promote;

    MoveT move;
    move.src = from;
    move.promote = PieceType::Empty;
    
    // generate captures (if any).
    for (pindir = 2; pindir >= 0; pindir -= 2)
    {
        to = *(moves++);
        if (to != FLAG &&
            (pintype == FLAG || pintype == (pindir ^ (turn << 1))))
        {
            // enemy on diag?
            if (PieceAt(to).IsEnemy(turn))
            {
                // can we promote?
                if (to > 55 || to < 8)
                    promo(mvlist, from, to, CALCDC(dc, from, to));
                else
                {
                    // normal capture.
                    move.dst = to;
                    move.chk = mergeChk(CALCDC(dc, from, to),
                                        PAWNCHK(to, ekcoord, turn));
                    mvlist.AddMoveFast(move, *this);
                }
            }

            // en passant?
            else if (to - 8 + (turn << 4) == EnPassantCoord() &&
                     enpassLegal(from))
            {
                // yes.
                dc1 = CALCDC(dc, from, to);
                dc2 = enpassdc(from);
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

                move.dst = to;
                move.promote = PieceType::Pawn;
                move.chk = mergeChk(dc1, pawnchk);
                mvlist.AddMove(move, *this);
                move.promote = PieceType::Empty;
            }
        }
    }

    // Generate pawn pushes.
    to = *moves;
    promote = (to > 55 || to < 8);
    if (promote >= capOnly &&
        PieceAt(to).IsEmpty() &&
        (pintype == FLAG || pintype == 1))
        // space ahead
    {   // can we promote?
        if (promote)
            promo(mvlist, from, to, CALCDC(dc, from, to));
        else
        {
            // check e2e4-like moves
            if ((from > 47 || from < 16) &&
                PieceAt((to2 = *(++moves))).IsEmpty())
            {
                move.dst = to2;
                move.chk = mergeChk(CALCDC(dc, from, to2),
                                    PAWNCHK(to2, ekcoord, turn));
                mvlist.AddMoveFast(move, *this);
            }
            // add e2e3-like moves.
            move.dst = to;
            move.chk = mergeChk(CALCDC(dc, from, to),
                                PAWNCHK(to, ekcoord, turn));
            mvlist.AddMoveFast(move, *this);
        }
    }
}

void PrivBoard::checkCastle(MoveList &mvlist, cell_t kSrc, cell_t kDst,
                            cell_t rSrc, cell_t rDst, bool isCastleOO,
                            cell_t ekcoord) const
{
    // 'src' assumed to == castling->start.king.

    // Chess 960 castling rules (from wikipedia):
    //  "All squares between the king's initial and final squares
    //   (including the final square), and all squares between the
    //   rook's initial and final squares (including the final square),
    //   must be vacant except for the king and castling rook."
    if (
        // Check if rook can move.
        (rSrc == rDst ||
         ((PieceAt(rDst).IsEmpty() || rDst == kSrc) &&
          nopose(rSrc, rDst, kSrc))) &&
        // Check if king can move.
        (kSrc == kDst ||
         ((PieceAt(kDst).IsEmpty() || kDst == rSrc) &&
          nopose(kSrc, kDst, rSrc) &&
          !castleAttacked(kSrc, kDst))))
    {
        cell_t sq =
            isCastleOO ? Turn() : (1 << NUM_PLAYERS_BITS) | Turn();
        MoveT move;
        move.src = sq;
        move.dst = sq;
        move.promote = PieceType::Empty;
        move.chk = ROOKCHK(rDst, kSrc, ekcoord);
        mvlist.AddMove(move, *this);
    }
}

void PrivBoard::generateKingCastleMoves(MoveList &mvlist, cell_t src,
                                        cell_t ekcoord, int capOnly) const
{
    uint8 turn = Turn();

    // 'src' assumed to == castling->start.king.
    if (!capOnly) // assumed true: && board.ncheck == FLAG
    {
        CastleCoordsT castling = Variant::Current()->Castling(turn);

        // check for kingside castle
        if (CanCastleOO(turn))
        {
            checkCastle(mvlist,
                        src, castling.endOO.king,
                        castling.start.rookOO, castling.endOO.rook,
                        true, ekcoord);
        }

        // check for queenside castle.
        if (CanCastleOOO(turn))
        {
            checkCastle(mvlist,
                        src, castling.endOOO.king,
                        castling.start.rookOOO, castling.endOOO.rook,
                        false, ekcoord);
        }
    }
}

void PrivBoard::generateKingMoves(MoveList &mvlist, cell_t from,
                                  cell_t dc, int capOnly) const
{
    const int *idx;
    cell_t to;
    uint8 turn = Turn();
    MoveT move;
    move.src = from;
    move.promote = PieceType::Empty;
    
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
            int(PieceAt(to).Relationship(turn)) > capOnly &&
            /* I could optimize a few of these calls out if I already
               did this while figuring out the castling moves. ... but I doubt
               it's a win. */
            !attacked(NULL, to, turn, turn))
        {
            move.dst = to;
            move.chk = CALCDC(dc, from, to);
            mvlist.AddMoveFast(move, *this);
        }
    }
}

void PrivBoard::generateKnightMoves(MoveList &mvlist, cell_t from, cell_t dc,
                                    cell_t ekcoord, int capOnly) const
{
    uint8 turn = Turn();
    cell_t *moves = gPreCalc.moves[8 + turn] [from];
    MoveT move;
    move.src = from;
    move.promote = PieceType::Empty;
    
    for (; *moves != FLAG; moves++)
    {
        if (int(PieceAt(*moves).Relationship(turn)) > capOnly)
        {
            move.dst = *moves;
            move.chk = mergeChk(dc, NIGHTCHK(*moves, ekcoord));
            mvlist.AddMoveFast(move, *this);
        }
    }
}

void PrivBoard::GenerateLegalMoves(MoveList &mvlist,
                                   bool generateCapturesOnly) const
{
    uint8 turn = Turn();
    PinsT dclist, pinlist;
    cell_t kcoord = PieceCoords(Piece(turn, PieceType::King))[0];
    cell_t ekcoord = PieceCoords(Piece(turn ^ 1, PieceType::King))[0];
    int capOnly = generateCapturesOnly;
    
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

    mvlist.DeleteAllMoves();

    /* generate list of pieces that can potentially give
       discovered check. A very short list. Non-sorted.*/
    gendclist(dclist, ekcoord, turn);

    // find all king pins (yay puns :)
    findpins(pinlist, kcoord, turn);

    if (!IsInCheck())
    {
        // Not in check.

        // Generate king castling moves.
        generateKingCastleMoves(mvlist, kcoord, ekcoord, capOnly);

        // Generate pawn moves.
        for (cell_t coord : PieceCoords(Piece(turn, PieceType::Pawn)))
        {
            generatePawnMoves(mvlist, coord, pinlist.c[coord], dclist.c[coord],
                              ekcoord, capOnly);
        }

        // Generate queen moves.
        // Note: it is never possible for qmove to result in discovered
        //  check.  We optimize for this.
        for (cell_t coord : PieceCoords(Piece(turn, PieceType::Queen)))
        {
            generateBishopRookMoves(mvlist, coord, pinlist.c[coord],
                                    preferredQDirs[turn], FLAG,
                                    ekcoord, capOnly);
        }

        // Generate bishop moves.
        for (cell_t coord : PieceCoords(Piece(turn, PieceType::Bishop)))
        {
            generateBishopRookMoves(mvlist, coord, pinlist.c[coord],
                                    preferredBDirs[turn], dclist.c[coord],
                                    ekcoord, capOnly);
        }

        // Generate night moves.
        for (cell_t coord : PieceCoords(Piece(turn, PieceType::Knight)))
        {
            // A pinned knight cannot move w/out checking its king.
            if (pinlist.c[coord] == FLAG)
            {
                generateKnightMoves(mvlist, coord, dclist.c[coord], ekcoord,
                                    capOnly);
            }
        }
        
        // Generate rook moves.
        for (cell_t coord : PieceCoords(Piece(turn, PieceType::Rook)))
        {
            generateBishopRookMoves(mvlist, coord, pinlist.c[coord],
                                    preferredRDirs[turn], dclist.c[coord],
                                    ekcoord, capOnly);
        }
    }
    else if (CheckingCoord() != DOUBLE_CHECK)
    {
        // In check by 1 piece (only), so capture or interpose.
        cappose(mvlist, CheckingCoord(), pinlist, kcoord,
                dclist);
    }

    // generate king (non-castling) moves.
    generateKingMoves(mvlist, kcoord, dclist.c[kcoord], capOnly);

    // Selection Sorting the captures does no good, empirically.
    // But, probably will do good when we extend captures.
}
    
cell_t Board::calcNCheck(const char *context) const
{
    CoordListT attList;
    Piece myPiece;
    auto &kingVec = PieceCoords(Piece(Turn(), PieceType::King));
    const PrivBoard *priv = static_cast<const PrivBoard *>(this);

    if (kingVec.size() != 1)
    {
        // We do not know how to calculate check for a non-standard board.
        assert(0);
    }

    cell_t kcoord = kingVec[0];

    // Minor sanity-check of board.
    myPiece = PieceAt(kcoord);
    if (!myPiece.IsKing())
    {
        LOG_EMERG("calcNCheck (%s): bad king kcoord %d, piece %d\n",
                  context, kcoord, myPiece.ToIndex());
        assert(0);
    }

    attList.lgh = 0;
    priv->attacked(&attList, kcoord, Turn(), Turn());
    return
        attList.lgh >= 2 ? DOUBLE_CHECK :
        attList.lgh == 1 ? attList.coords[0] :
        FLAG;
}

void Board::GenerateLegalMoves(MoveList &mvlist,
                               bool generateCapturesOnly) const
{
    const PrivBoard *priv = static_cast<const PrivBoard *>(this);
    priv->GenerateLegalMoves(mvlist, generateCapturesOnly);
}
