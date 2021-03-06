//--------------------------------------------------------------------------
//            gPreCalc.cpp - all constant (or init-time) globals.
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//--------------------------------------------------------------------------

#include <stdlib.h>   // qsort()
#include <assert.h>

#include "aSystem.h"
#include "Eval.h"
#include "gPreCalc.h"
#include "Variant.h"

using arctic::File;
using arctic::Rank;

GPreCalcT gPreCalc;

static Piece gNormalStartingPieces[NUM_SQUARES] = 
{
    // 1st row
    Piece(0, PieceType::Rook),   Piece(0, PieceType::Knight),
    Piece(0, PieceType::Bishop), Piece(0, PieceType::Queen),
    Piece(0, PieceType::King),   Piece(0, PieceType::Bishop),
    Piece(0, PieceType::Knight), Piece(0, PieceType::Rook),
    // 2nd row
    Piece(0, PieceType::Pawn), Piece(0, PieceType::Pawn),
    Piece(0, PieceType::Pawn), Piece(0, PieceType::Pawn),
    Piece(0, PieceType::Pawn), Piece(0, PieceType::Pawn),
    Piece(0, PieceType::Pawn), Piece(0, PieceType::Pawn),
    // 3rd row
    Piece(), Piece(), Piece(), Piece(), Piece(), Piece(), Piece(), Piece(),
    // 4th row    
    Piece(), Piece(), Piece(), Piece(), Piece(), Piece(), Piece(), Piece(),
    // 5th row
    Piece(), Piece(), Piece(), Piece(), Piece(), Piece(), Piece(), Piece(),
    // 6th row
    Piece(), Piece(), Piece(), Piece(), Piece(), Piece(), Piece(), Piece(),
    // 7th row
    Piece(1, PieceType::Pawn), Piece(1, PieceType::Pawn),
    Piece(1, PieceType::Pawn), Piece(1, PieceType::Pawn),
    Piece(1, PieceType::Pawn), Piece(1, PieceType::Pawn),
    Piece(1, PieceType::Pawn), Piece(1, PieceType::Pawn),
    // 8th row
    Piece(1, PieceType::Rook),   Piece(1, PieceType::Knight),
    Piece(1, PieceType::Bishop), Piece(1, PieceType::Queen),
    Piece(1, PieceType::King),   Piece(1, PieceType::Bishop),
    Piece(1, PieceType::Knight), Piece(1, PieceType::Rook)
};


static cell_t gAllNormalMoves[512 /* yes, this is the exact size needed */] =  {
    /* 0 (northwest) direction */
    FLAG,
    8, FLAG,
    9, 16, FLAG,
    10, 17, 24, FLAG,
    11, 18, 25, 32, FLAG,
    12, 19, 26, 33, 40, FLAG,
    13, 20, 27, 34, 41, 48, FLAG,
    14, 21, 28, 35, 42, 49, 56, FLAG,
    22, 29, 36, 43, 50, 57, FLAG,
    30, 37, 44, 51, 58, FLAG,
    38, 45, 52, 59, FLAG,
    46, 53, 60, FLAG,
    54, 61, FLAG,
    62, FLAG,
    FLAG,

    /* 1 (north) direction */
    8, 16, 24, 32, 40, 48, 56, FLAG,
    9, 17, 25, 33, 41, 49, 57, FLAG,
    10, 18, 26, 34, 42, 50, 58, FLAG,
    11, 19, 27, 35, 43, 51, 59, FLAG,
    12, 20, 28, 36, 44, 52, 60, FLAG,
    13, 21, 29, 37, 45, 53, 61, FLAG,
    14, 22, 30, 38, 46, 54, 62, FLAG,
    15, 23, 31, 39, 47, 55, 63, FLAG,
        
    /* 2 (northeast) direction */
    FLAG,
    15, FLAG,
    14, 23, FLAG,
    13, 22, 31, FLAG,
    12, 21, 30, 39, FLAG,
    11, 20, 29, 38, 47, FLAG,
    10, 19, 28, 37, 46, 55, FLAG,
    9, 18, 27, 36, 45, 54, 63, FLAG,
    17, 26, 35, 44, 53, 62, FLAG,
    25, 34, 43, 52, 61, FLAG,
    33, 42, 51, 60, FLAG,
    41, 50, 59, FLAG,
    49, 58, FLAG,
    57, FLAG,
    FLAG,
        
    /* 3 (east) direction */
    1,  2,  3,  4,  5,  6,  7, FLAG,
    9, 10, 11, 12, 13, 14, 15, FLAG,
    17, 18, 19, 20, 21, 22, 23, FLAG,
    25, 26, 27, 28, 29, 30, 31, FLAG,
    33, 34, 35, 36, 37, 38, 39, FLAG,
    41, 42, 43, 44, 45, 46, 47, FLAG,
    49, 50, 51, 52, 53, 54, 55, FLAG,
    57, 58, 59, 60, 61, 62, 63, FLAG,
        
    /* 4 (southeast) direction */
    FLAG,
    55, FLAG,
    54, 47, FLAG,
    53, 46, 39, FLAG,
    52, 45, 38, 31, FLAG,
    51, 44, 37, 30, 23, FLAG,
    50, 43, 36, 29, 22, 15, FLAG,
    49, 42, 35, 28, 21, 14,  7, FLAG,
    41, 34, 27, 20, 13,  6, FLAG,
    33, 26, 19, 12,  5, FLAG,
    25, 18, 11,  4, FLAG,
    17, 10,  3, FLAG,
    9,  2, FLAG,
    1, FLAG,
    FLAG,

    /* 5 (south) direction */
    48, 40, 32, 24, 16,  8,  0, FLAG,
    49, 41, 33, 25, 17,  9,  1, FLAG,
    50, 42, 34, 26, 18, 10,  2, FLAG,
    51, 43, 35, 27, 19, 11,  3, FLAG,
    52, 44, 36, 28, 20, 12,  4, FLAG,
    53, 45, 37, 29, 21, 13,  5, FLAG,
    54, 46, 38, 30, 22, 14,  6, FLAG,
    55, 47, 39, 31, 23, 15,  7, FLAG,

    /* 6 (southwest) direction */
    FLAG,
    48, FLAG,
    49, 40, FLAG,
    50, 41, 32, FLAG,
    51, 42, 33, 24, FLAG,
    52, 43, 34, 25, 16, FLAG,
    53, 44, 35, 26, 17,  8, FLAG,
    54, 45, 36, 27, 18,  9,  0, FLAG,
    46, 37, 28, 19, 10,  1, FLAG,
    38, 29, 20, 11,  2, FLAG,
    30, 21, 12,  3, FLAG,
    22, 13,  4, FLAG,
    14,  5, FLAG,
    6, FLAG,
    FLAG,

    /* 7 (west) direction */
    6,  5,  4,  3,  2,  1,  0, FLAG,
    14, 13, 12, 11, 10,  9,  8, FLAG,
    22, 21, 20, 19, 18, 17, 16, FLAG,
    30, 29, 28, 27, 26, 25, 24, FLAG,
    38, 37, 36, 35, 34, 33, 32, FLAG,
    46, 45, 44, 43, 42, 41, 40, FLAG,
    54, 53, 52, 51, 50, 49, 48, FLAG,
    62, 61, 60, 59, 58, 57, 56, FLAG,
};


/* This is split equally between best night moves for white from a given
   coord, and best moves for black. */
static cell_t gAllKnightMoves[800];


/* contains (up to) 4 valid squares of advancement: 2 capture squares (if
   valid), then any valid e2e4-like move, then the appropriate e2e3 move.
   Each side uses a different set of squares. */
static cell_t gAllPawnMoves[4 * NUM_SQUARES * 2];


/* Note how we already need to have calculated rook and bishop moves. */
static int calcPawnMoves(uint8 *idx, int coord, int turn)
{
    /* indexed by turn and direction, respectively. */
    int toind[2] [3] = {{2, 0, 1}, {4, 6, 5}};
    int i;

    /* was degenerate, but if we are using in attacked(), we actually need
       to fill this out. */
#if 0
    /* degenerate case(s). */
    if (Rank(coord) == 0 || Rank(coord) == 7)
    {
        for (i = 0; i < 4; i++)
        {
            *(idx++) = FLAG;
        }
        return 4;
    }
#endif

    /* calculate capture squares and e2e3 move. */
    for (i = 0; i < 3; i++)
    {
        *(idx++) = *(gPreCalc.moves[toind[turn] [i]] [coord]);
    }
    /* calculate e2e4 moves.  Even the ones that "do not exist" since we use
       this in attacked(). */
    *idx =
        (coord < 56 /* 16 */ && !turn) || (coord > 15 /* 47 */ && turn) ?
        *(gPreCalc.moves[toind[turn] [2]] [coord] + 1) :
        FLAG;
    return 4;
}


static int whiteGoodKnightMove(uint8 *el1, uint8 *el2)
{
    int rankDiff = Rank(*el1) - Rank(*el2);
    return rankDiff != 0 ? -rankDiff : /* higher rank comes first for White */
        /* .. But if both moves are on the same rank, we want the one closest
           to center. */
        abs((3 + 4) - (int) (File(*el1) * 2)) -
        abs((3 + 4) - (int) (File(*el2) * 2));
}


static int blackGoodKnightMove(uint8 *el1, uint8 *el2)
{
    int rankDiff = Rank(*el1) - Rank(*el2);
    return rankDiff != 0 ? rankDiff : /* lower rank comes first for Black */
        /* .. But if both moves are on the same rank, we want the one closest
           to center. */
        abs((3 + 4) - (int) (File(*el1) * 2)) -
        abs((3 + 4) - (int) (File(*el2) * 2));
}


typedef int (*QSORTFUNC)(const void *, const void *);


/* Calculates night moves for 'coord' and 'turn' (in preferred order).
   Returns number of moves (+ FLAG) copied into 'moveArray'. */
static int calcKnightMoves(uint8 *moveArray, int coord, int turn)
{
    uint8 myMoves[9];
    uint8 *ptr = myMoves;

    if (Rank(coord) < 6 && File(coord) > 0)
        *(ptr++) = coord + 15; /* b1-a3 type moves */
    if (Rank(coord) < 6 && File(coord) < 7)
        *(ptr++) = coord + 17; /* a1-b3 type moves */
    if (Rank(coord) < 7 && File(coord) > 1)
        *(ptr++) = coord + 6;  /* c1-a2 type moves */
    if (Rank(coord) < 7 && File(coord) < 6)
        *(ptr++) = coord + 10; /* a1-c2 type moves */
    if (Rank(coord) > 0 && File(coord) > 1)
        *(ptr++) = coord - 10; /* c2-a1 type moves */
    if (Rank(coord) > 0 && File(coord) < 6)
        *(ptr++) = coord - 6;  /* a2-c1 type moves */
    if (Rank(coord) > 1 && File(coord) > 0)
        *(ptr++) = coord - 17; /* b3-a1 type moves */
    if (Rank(coord) > 1 && File(coord) < 7)
        *(ptr++) = coord - 15; /* a3-b1 type moves */

    /* sort moves according to what will probably be best. */
    qsort(myMoves, ptr - myMoves, sizeof(uint8),
          (QSORTFUNC) (turn ? blackGoodKnightMove : whiteGoodKnightMove));

    *(ptr++) = FLAG; /* terminate 'myMoves'. */

    /* ... and copy it over. */
    for (ptr = myMoves; (*(moveArray++) = *(ptr++)) != FLAG; )
        ; /* no-op */

    return ptr - myMoves;
}


static int dirf(int from, int to)
{
    int res;
    int rdiff = Rank(to) - Rank(from);
    int fdiff = File(to) - File(from);
    if (from == to)
        return DIRFLAG;                         /* This is undefined. */
    if (!rdiff)
        res = 3;                                /* - move */
    else if (!fdiff)
        res = 1;                                /* | move */
    else if (rdiff == fdiff)
        res = 2;                                /* / move */
    else if (rdiff == -fdiff)
        res = 0;                                /* \ move */
    else if (abs(rdiff) + abs(fdiff) == 3)
        return 8;                               /* night move */
    else return DIRFLAG;                        /* no direction whatsoever. */
    return from < to ? res : res + 4;
}


static int distancef(uint8 coord1, uint8 coord2)
{
    return abs(Rank(coord1) - Rank(coord2)) +
        abs(File(coord1) - File(coord2));
}


static int centerDistancef(uint8 coord1)
{
    int dist = distancef(coord1, 27);
    dist = MIN(dist, distancef(coord1, 28));
    dist = MIN(dist, distancef(coord1, 35));
    dist = MIN(dist, distancef(coord1, 36));
    return dist;
}


static void rowinit(int d, int start, int finc, int sinc, uint8 *moves[] [NUM_SQUARES],
                    uint8 *ptr)
{
    int temp, i;
    int row = 0;
    for (i = temp = start; row < 8;)
    {
        moves[d] [i] = ptr++;
        if (*moves[d] [i] == FLAG)
        {
            i = (temp += sinc);
            row++;
        }
        else i += finc;
    }
}


static void diaginit(int d, int start, int finc, int sinc, uint8 *moves[] [NUM_SQUARES],
                     uint8 *ptr)
{
    int temp, i, j;
    for (i = start; abs(i - start) < 8; i += finc)
        for (j = i; ; j += sinc - finc)
        {
            moves[d] [j] = ptr++;
            if (*moves[d] [j] == FLAG)
                break;
        }
    for (temp = (i = start + sinc + finc * 7); abs(i - temp) < 49; i += sinc)
        for (j = i; ; j += sinc - finc)
        {
            moves[d] [j] = ptr++;
            if (*moves[d] [j] == FLAG)
                break;
        }
}

static void castleMaskInit(void)
{
    int i;
    CastleStartCoordsT castleStart;

    for (i = 0; i < NUM_SQUARES; i++)
    {
        gPreCalc.castleMask[i] = ~0x0;
    }
    for (i = 0; i < NUM_PLAYERS; i++)
    {
        castleStart = Variant::Current()->Castling(i).start;
        gPreCalc.castleMask[castleStart.king] &= ~(CASTLEBOTH << i);
        gPreCalc.castleMask[castleStart.rookOO] &= ~(CASTLEOO << i);
        gPreCalc.castleMask[castleStart.rookOOO] &= ~(CASTLEOOO << i);
    }
}

uint64 random64(void)
{
    return
        // Even on a 64-bit platform, RAND_MAX may only be 32 bits.
        (((uint64) random()) << 32) ^ ((uint64) random());
}


// initialize gPreCalc.
void gPreCalcInit(int64 userSpecifiedHashSize, int userSpecifiedNumThreads)
{
    int i, d, j;
    cell_t *ptr = gAllNormalMoves;

    /* initialize moves array. */
    for (d = 0; d < 8; d++) /* d signifies direction */
    {
        switch(d)
        {
        case 0: diaginit(d,  0,  1,  8, gPreCalc.moves, ptr); break;
        case 2: diaginit(d,  7, -1,  8, gPreCalc.moves, ptr); break;
        case 4: diaginit(d, 63, -1, -8, gPreCalc.moves, ptr); break;
        case 6: diaginit(d, 56,  1, -8, gPreCalc.moves, ptr); break;
        case 1: rowinit(d,  0,  8, 1, gPreCalc.moves, ptr); break;
        case 3: rowinit(d,  0,  1, 8, gPreCalc.moves, ptr); break;
        case 5: rowinit(d, 56, -8, 1, gPreCalc.moves, ptr); break;
        case 7: rowinit(d,  7, -1, 8, gPreCalc.moves, ptr); break;
        default: break;
        };
        ptr += NUM_SQUARES;
    }

    /* Calculate knight-move arrays.  Can reuse ptr. */
    ptr = gAllKnightMoves;
    for (i = 0; i < NUM_PLAYERS; i++)
    {
        for (j = 0; j < NUM_SQUARES; j++)
        {
            gPreCalc.moves[8 + i] [j] = ptr;
            ptr += calcKnightMoves(ptr, j, i);
        }
    }
    assert(ptr = gAllKnightMoves + sizeof(gAllKnightMoves));

    /* Calculate pawn-move arrays.  Can reuse ptr. */
    ptr = gAllPawnMoves;
    for (i = 0; i < NUM_PLAYERS; i++)
    {
        for (j = 0; j < NUM_SQUARES; j++)
        {
            gPreCalc.moves[10 + i] [j] = ptr;
            ptr += calcPawnMoves(ptr, j, i);
        }
    }
    assert(ptr = gAllPawnMoves + sizeof(gAllPawnMoves));

    /* initialize direction, distance, and centerDistance arrays. */
    for (i = 0; i < NUM_SQUARES; i++)
    {
        for (j = 0; j < NUM_SQUARES; j++)
        {
            gPreCalc.dir[i] [j] = dirf(i, j);
            gPreCalc.distance[i] [j] = distancef(i, j);
        }
        gPreCalc.centerDistance[i] = centerDistancef(i);
    }

    Piece::Init();
    
    // Initialize 'worth' values.
    Piece::SetWorth(PieceType::King,   Eval::King);
    Piece::SetWorth(PieceType::Pawn,   Eval::Pawn);
    Piece::SetWorth(PieceType::Knight, Eval::Knight);
    Piece::SetWorth(PieceType::Bishop, Eval::Bishop);
    Piece::SetWorth(PieceType::Rook,   Eval::Rook);
    Piece::SetWorth(PieceType::Queen,  Eval::Queen);

    // initialize zobrist hashing.
    for (i = 0; i < NUM_SQUARES; i++)
    {
        for (j = 0; j < kMaxPieces; j++)
        {
            gPreCalc.zobrist.coord[j] [i] =
                // Using 0 for empty squares simplifies zobrist calculation
                // when making moves later (do not have to XOR in empty squares,
                // but it is not an error to do so).
                (j >> NUM_PLAYERS_BITS) ? random64() : 0;
        }

        gPreCalc.zobrist.ebyte[i] = random64();
        
        if (i < 16)
        {
            // We also use 0 for the cbyte where no one can castle.
            gPreCalc.zobrist.cbyte[i] = (i > 0) ? random64() : 0;
        }
    }
    gPreCalc.zobrist.turn = random64();

    // We could clamp these to the limits of the local engine; but eventually
    //  we might support interfacing to remote engines, and then that would be
    //  the wrong thing to do.
    gPreCalc.userSpecifiedHashSize = userSpecifiedHashSize;
    gPreCalc.userSpecifiedNumThreads = userSpecifiedNumThreads;

    gPreCalc.normalStartingPieces = gNormalStartingPieces;

    castleMaskInit();
}
