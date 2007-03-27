/***************************************************************************
              init.c - game initialization (gPreCalc, hash, board)
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

#include <string.h> /* memcpy(), memset() */
#include <stdlib.h> /* qsort(3), random(3), malloc(3) */
#include <assert.h>
#include "ref.h"

GPreCalcT gPreCalc;

HashPositionT *gHash = NULL;

static char gAllNormalMoves[512 /* yes, this is the exact size needed */] =  {
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
static char gAllNightMoves[800];


static int whiteGoodNightMove(uint8 *el1, uint8 *el2)
{
    int rankDiff = Rank(*el1) - Rank(*el2);
    return rankDiff != 0 ? -rankDiff : /* higher rank comes first for White */
	/* .. But if both moves are on the same rank, we want the one closest
	   to center. */
	Abs((3 + 4) - (int) (File(*el1) * 2)) -
	Abs((3 + 4) - (int) (File(*el2) * 2));
}


static int blackGoodNightMove(uint8 *el1, uint8 *el2)
{
    int rankDiff = Rank(*el1) - Rank(*el2);
    return rankDiff != 0 ? rankDiff : /* lower rank comes first for Black */
	/* .. But if both moves are on the same rank, we want the one closest
	   to center. */
	Abs((3 + 4) - (int) (File(*el1) * 2)) -
	Abs((3 + 4) - (int) (File(*el2) * 2));
}


typedef int (*QSORTFUNC)(const void *, const void *);

/* Calculates night moves for 'coord' and 'turn' (in preferred order).
   Returns number of moves (+ FLAG) copied into 'moveArray'. */
static int calcNightMoves(uint8 *moveArray, int coord, int turn)
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
	  (QSORTFUNC) (turn ? blackGoodNightMove : whiteGoodNightMove));

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
    if (!rdiff)
        res = 3;                                /* - move */
    else if (!fdiff)
        res = 1;                                /* | move */
    else if (rdiff == fdiff)
        res = 2;                                /* / move */
    else if (rdiff == -fdiff)
        res = 0;                                /* \ move */
    else if (Abs(rdiff) + Abs(fdiff) == 3)
        return 8;                               /* night move */
    else return DIRFLAG;                        /* no direction whatsoever. */
    return from < to ? res : res + 4;
}


/* returns FRIEND, ENEMY, or UNOCCD */
/* White's turn = 0. Black's is 1. */
static int checkf(char piece, int turn)
{
    return piece < KING ? UNOCCD : ((piece & 1) ^ turn) << 1;
}


static int worthf(char piece)
{
    switch(piece | 1)
    {
    case BPAWN:   return EVAL_PAWN;
    case BBISHOP:
    case BNIGHT:  return EVAL_BISHOP;
    case BROOK:   return EVAL_ROOK;
    case BQUEEN:  return EVAL_QUEEN;
    case BKING:   return -1; /*error condition. */
    default:      break;
    }
    return 0;
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


static void rowinit(int d, int start, int finc, int sinc, uint8 *moves[] [16],
		    char *ptr)
{
    int temp, i;
    int row = 0;
    char *oldptr = ptr;
    for (i = temp = start; row < 8;)
    {
	moves[i] [d] = ptr++;
	if (*moves[i] [d] == FLAG)
	{
	    i = (temp += sinc);
	    row++;
	}
	else i += finc;
    }
    i = ((int) ptr) - ((int) oldptr);
}


static void diaginit(int d, int start, int finc, int sinc, uint8 *moves[] [16],
		     char *ptr)
{
    int temp, i, j;
    char *oldptr = ptr;
    for (i = start; Abs(i - start) < 8; i += finc)
	for (j = i; ; j += sinc - finc)
	{
	    moves[j] [d] = ptr++;
	    if (*moves[j] [d] == FLAG)
		break;
	}
    for (temp = (i = start + sinc + finc * 7); Abs(i - temp) < 49; i += sinc)
	for (j = i; ; j += sinc - finc)
	{
	    moves[j] [d] = ptr++;
	    if (*moves[j] [d] == FLAG)
		break;
	}
    j = ((int) ptr) - ((int) oldptr);
}


/* initialize gPreCalc. */
void preCalcInit(int numHashEntries)
{
    int i, d, j, num;
    char *ptr = gAllNormalMoves;

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
        ptr += 64;
    }

    /* Calculate knight-move arrays.  Can reuse ptr. */
    ptr = gAllNightMoves;
    for (i = 0; i < 2; i++)
    {
	for (j = 0; j < 64; j++)
	{
	    gPreCalc.moves[j] [8 + i] = ptr;
	    ptr += calcNightMoves(ptr, j, i);
	}
    }
    assert(ptr = gAllNightMoves + sizeof(gAllNightMoves));

    /* initialize direction, distance, and centerDistance arrays. */
    for (i = 0; i < 64; i++)
    {
        for (j = 0; j < 64; j++)
	{
            gPreCalc.dir[i] [j] = dirf(i, j);
	    gPreCalc.distance[i] [j] = distancef(i, j);
	}
	gPreCalc.centerDistance[i] = centerDistancef(i);
    }


    /* initialize check array. */
    for (i = 0; i < BQUEEN + 1; i++)
    {
	for (j = 0; j < 2; j++)
	{
	    gPreCalc.check[i] [j] = checkf(i, j);
	}
    }

    /* initialize worth array. */
    for (i = 0; i < sizeof(gPreCalc.worth); i++)
    {
	gPreCalc.worth[i] = worthf(i);
    }

    /* initialize attacks array. */
    for (i = 0; i < DIRFLAG + 1; i++)
    {
	/* (the other elements are static and therefore already '0') */
	for (j = BISHOP; j < BQUEEN + 1; j++)
	{
	    switch(j | 1)
	    {
	    case BBISHOP:
		gPreCalc.attacks[i] [j] = !(i & 0x9);
		break;
	    case BROOK:
		gPreCalc.attacks[i] [j] = i & 1;
		break;
	    case BQUEEN:
		gPreCalc.attacks[i] [j] = i < 8;
		break;
	    default:
		assert(0);
		break;
	    }
	}
    }


    /* initialize zobrist hashing. */
    for (i = 0; i < 63; i++)
    {
	for (j = 0; j < BQUEEN + 1; j++)
	{
	    gPreCalc.zobrist.coord[j] [i] = random();
	}

	num = random();
	if (i >= 24 && i < 40)
	{
	    /* every (useful) ebyte zobrist needs 5 unique bits.  The least
	       significant is hardwired to '1' to distinguish this from the
	       "no enpassant" case. */
	    num &= ~0x1f0;
	    num |= (((24 - i) << 5) | 0x10);
	}
	gPreCalc.zobrist.ebyte[i] = num;
	
	if (i < 16)
	{
	    num = random();
	    /* make sure every cbyte zobrist has 4 unique bits. */
	    num &= ~0xf;
	    num |= i;
	    gPreCalc.zobrist.cbyte[i] = num;
	}
    }
    num = random();
    /* 'turn' also needs a unique bit. */
    num |= 0x200;
    gPreCalc.zobrist.turn = num;

    /* Note: Having 10 unique bits means we need a transposition table
       at least 2^10 (1024k) in size for proper hashing, if we do not want
       to store castle bytes, ebytes, and turn as part of the position. */

    /* remember transposition table size. */
    gPreCalc.numHashEntries = numHashEntries;
    gPreCalc.hashMask = numHashEntries - 1;
}


/* This is useful for generating a hash for the initial board position, or
   (slow) validating the incrementally-updated hash. */
int calcZobrist(BoardT *board)
{
    int retVal = 0;
    int i;
    for (i = 0; i < 63; i++)
    {
	retVal ^= gPreCalc.zobrist.coord[board->coord[i]] [i];
    }
    retVal ^= gPreCalc.zobrist.cbyte[board->cbyte];
    if (board->ply & 1)
	retVal ^= gPreCalc.zobrist.turn;
    if (board->ebyte != FLAG)
	retVal ^= gPreCalc.zobrist.ebyte[board->ebyte];
    return retVal;
}


static void hashInit(void)
{
    int size = gPreCalc.numHashEntries * sizeof(HashPositionT);
    int i = 0;

    if (size == 0)
	return; /* degenerate case. */

    if (gHash == NULL /* should only be true once */ &&
	(gHash = malloc(size)) == NULL)
    {
	LOG_EMERG("Failed to init hash (numEntries %d, size %d)\n",
		  gPreCalc.numHashEntries, size);
	exit(0);
    }

    for (i = 0; i < gPreCalc.numHashEntries; i++)
    {
	gHash[i].depth = HASH_NOENTRY;
    }
}


void newgameEx(BoardT *board, uint8 pieces[], int cbyte, int ebyte, int ply)
{
    int x, y, i;
    int saveLevel = board->maxLevel, saveHiswin = board->hiswin;

    /* blank everything. */
    memset(board, 0, sizeof(BoardT));

    /* restore saved variables. */
    board->maxLevel = saveLevel;
    board->hiswin = saveHiswin;

    /* copy all of the pieces over. */
    memcpy(board->coord, pieces, 64);

    /* init playlist/playptr. */
    for (i = 0; i < 64; i++)
	if (board->coord[i])
	    addpiece(board, board->coord[i], i);

    /* reset history table. */
    for (i = 0; i < 2; i++)
	for (x = 0; x < 63; x++)
	    for (y = 0; y < 63; y++)
	    {
		/* -50, not -1, because -1 might trigger accidentally if
		   we expand the history window beyond killer moves. */
		board->hist[i] [x] [y] = -50;
	    }

    /* cbyte handling. */
    board->cbyte = cbyte;
    newcbyte(board);

    /* ebyte handling. */
    assert(ebyte == FLAG || ISPAWN(board->coord[ebyte]));
    board->ebyte = ebyte;

    /* ncheck handling. */
    for (i = 0; i < 2; i++)
    {
	/* must be 1 K of each kind. */
	assert(board->playlist[KING | i].lgh == 1);
	calcNCheck(board, board->playlist[KING | i].list[0], "newgameEx");
    }

    /* ply handling. */
    assert(ply == 0 /* the normal situation */ ||
	   ply == 1 /* hopefully setting up a position */);
    board->ply = ply;

    /* compensate for kings' "worth". */
    board->totalStrgh += 2;
    board->playerStrgh[0]++;
    board->playerStrgh[1]++;

    for (i = 0; i < 64; i++)
    {
        /* abuse this macro to setup board->hashcoord */
	COORDUPDATE(board, i, board->coord[i]);
    }
    board->zobrist = calcZobrist(board);
    hashInit();
    gUI->boardRefresh(board);
}


void newgame(BoardT *board)
{
    uint8 pieces[64] =
	{ROOK, NIGHT, BISHOP, QUEEN, KING, BISHOP, NIGHT, ROOK,
	 PAWN, PAWN, PAWN, PAWN, PAWN, PAWN, PAWN, PAWN,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0,
	 BPAWN, BPAWN, BPAWN, BPAWN, BPAWN, BPAWN, BPAWN, BPAWN,
	 BROOK, BNIGHT, BBISHOP, BQUEEN, BKING, BBISHOP, BNIGHT, BROOK};
    newgameEx(board, pieces, ALLCASTLE, FLAG, 0);
}
