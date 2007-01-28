#include <string.h> /* memcpy() */
#include <stdlib.h> /* qsort(3), random(3) */
#include <assert.h>
#include "ref.h"

GPreCalcT gPreCalc;


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


/* returns 0 if friend, 1 if enemy, 2 if unoccupied */
/* White's turn = 0. Black's is 1. */
static int checkf(char piece, int turn)
{
    return piece < KING ? 2 : (piece & 1) ^ turn;
}


static int worthf(char piece)
{
    switch(piece | 1)
    {
    case BPAWN:   return 1;
    case BBISHOP:
    case BNIGHT:  return 3;
    case BROOK:   return 5;
    case BQUEEN:  return 9;
    case BKING:   return -1; /*error condition. */
    default:      break;
    }
    return 0;
}


static void rowinit(int d, int start, int finc, int sinc, uint8 *moves[] [10],
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


static void diaginit(int d, int start, int finc, int sinc, uint8 *moves[] [10],
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
void initPreCalc(void)
{
    int i, d, j;
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

    /* initialize direction array. */
    for(i = 0; i < 64; i++)
        for (j = 0; j < 64; j++)
            gPreCalc.dir[i] [j] = dirf(i, j);

    /* initialize check array. */
    for (i = 0; i < 2; i++)
    {
	for (j = 0; j < BQUEEN + 1; j++)
	{
	    gPreCalc.check[i] [j] = checkf(j, i);
	}
    }

    /* initialize worth array. */
    for (i = 0; i < sizeof(gPreCalc.worth); i++)
    {
	gPreCalc.worth[i] = worthf(i);
    }

    /* initialize zobrist hashing. */
    for (i = 0; i < 63; i++)
    {
	for (j = 0; j < BQUEEN + 1; j++)
	{
	    gPreCalc.zobrist.coord[i] [j] = random();
	}
	gPreCalc.zobrist.ebyte[i] = random();
	if (i < 16)
	{
	    gPreCalc.zobrist.cbyte[i] = random();
	}
    }
    gPreCalc.zobrist.turn = random();
}


/* This is useful for generating a hash for the initial board position, or
   (slow) validating the incrementally-updated hash. */
int calcZobrist(BoardT *board)
{
    int retVal = 0;
    int i;
    for (i = 0; i < 63; i++)
    {
	retVal ^= gPreCalc.zobrist.coord[i] [board->coord[i]];
    }
    retVal ^= gPreCalc.zobrist.cbyte[board->cbyte];
    if (board->ply & 1)
	retVal ^= gPreCalc.zobrist.turn;
    if (board->ebyte != FLAG)
	retVal ^= gPreCalc.zobrist.ebyte[board->ebyte];
    return retVal;
}


void newgame(BoardT *board)
{
    int x, y, i;
    uint8 whitePieces[16] =
	{ROOK, NIGHT, BISHOP, QUEEN, KING, BISHOP, NIGHT, ROOK,
	 PAWN, PAWN, PAWN, PAWN, PAWN, PAWN, PAWN, PAWN};
    uint8 blackPieces[16] =
	{BPAWN, BPAWN, BPAWN, BPAWN, BPAWN, BPAWN, BPAWN, BPAWN,
	 BROOK, BNIGHT, BBISHOP, BQUEEN, BKING, BBISHOP, BNIGHT, BROOK};
    int saveLevel = board->level, saveHiswin = board->hiswin;

    /* blank everything. */
    memset(board, 0, sizeof(BoardT));

    /* restore saved variables. */
    board->level = saveLevel;
    board->hiswin = saveHiswin;

    memcpy(board->coord, whitePieces, 16);	  /* white */
    memcpy(&(board->coord[48]), blackPieces, 16); /* black */

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

    board->cbyte = ALLCASTLE;		       /* All can castle */
    board->ebyte = FLAG;		       /* no enpassant */
    board->ncheck[0] = FLAG;		       /* no one's in check. */
    board->ncheck[1] = FLAG;
    board->totalStrgh += 2; /* compensate for kings' "worth" */
    for (i = 0; i < 63; i++)
    {
        /* abuse this macro to setup board->hashcoord */
	COORDUPDATE(board, i, board->coord[i]);
    }
    board->zobrist = calcZobrist(board);
    UIBoardUpdate(board);
}


void addpiece(BoardT *board, uint8 piece, int coord)
{
    board->playptr[coord] =
	&board->playlist[piece].list[board->playlist[piece].lgh++];
    *board->playptr[coord] = coord;
    board->totalStrgh += WORTH(piece);
}


void delpiece(BoardT *board, uint8 piece, int coord)
{
    /* change coord in playlist and dec playlist lgh. */
    board->totalStrgh -= WORTH(piece);
    *board->playptr[coord] = board->playlist[piece].list
	[--board->playlist[piece].lgh];
    /* set the end playptr. */
    board->playptr[*board->playptr[coord]] = board->playptr[coord];
}
