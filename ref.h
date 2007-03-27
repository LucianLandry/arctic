/***************************************************************************
                    ref.h - all-inclusive include for Arctic.
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


#ifndef REF_H
#define REF_H

#include <string.h> /* memcpy() */
#include <semaphore.h> /* sem_t */

#define FLAG            127
#define FLAG64          0x7f7f7f7f7f7f7f7fLL; /* 8 FLAGs in a row */
#define DIRFLAG         10  /* This is even, in order to optimize rook attack
			       checks, and it is low, so I can define the
			       precalculated 'attacks' array. */
#define DISCHKFLAG      255 /* cannot be the same as FLAG. */

#define Rank(x) ((x) >> 3)
#define File(x) ((x) & 7)
#define Abs(x)  ((x) < 0 ? -(x) : (x))

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define KING   0x2 /*  010b */
#define PAWN   0x4 /*  100b */
#define NIGHT  0x6 /*  110b */
#define BISHOP 0x8 /* 1000b */
#define ROOK   0xa /* 1010b */
#define QUEEN  0xc /* 1100b */

/* These are the corresponding black pieces. */
#define BKING   (KING | 1)
#define BPAWN   (PAWN | 1)
#define BNIGHT  (NIGHT | 1)
#define BBISHOP (BISHOP | 1)
#define BROOK   (ROOK | 1)
#define BQUEEN  (QUEEN | 1)

/* Is the piece capable of attacking like a rook or bishop. */
#define ATTACKROOK(piece)   ((piece) >= ROOK)
#define ATTACKBISHOP(piece) (((piece) ^ 0x2 /* 0010b */) >= ROOK)

#define ISKING(piece)   (((piece) | 1) == BKING)
#define ISPAWN(piece)   (((piece) | 1) == BPAWN)
#define ISNIGHT(piece)  (((piece) | 1) == BNIGHT)
#define ISBISHOP(piece) (((piece) | 1) == BBISHOP)
#define ISROOK(piece)   (((piece) | 1) == BROOK)
#define ISQUEEN(piece)  (((piece) | 1) == BQUEEN)

#define EVAL_PAWN      100
#define EVAL_BISHOP    300
#define EVAL_KNIGHT    300
#define EVAL_ROOK      500
#define EVAL_QUEEN     900
#define EVAL_CHECKMATE 100000

typedef unsigned char  uint8;
typedef signed char    int8;
typedef unsigned short uint16;
typedef unsigned int   uint32;
typedef unsigned long long uint64;

typedef union {    /* stores pin info. */
    uint64 ll[8];
    uint8 c[64];
} PListT;

typedef struct {
    int lgh;                            /* length o' list.  Should be 1st,
					   for alignment purposes. */
    uint8 list [150] [4];               /* [0, 1]: src, dest coords
					   [2]: promo (or enpass, signified by
					   'p' of opposite color, and
					   [3] check. */
    int insrt;                          /* spot to insert 'preferred' move. */
    int ekcoord;                        /* scratch for genmlist(), for
					   performance reasons. */
    int capOnly;                        /* used for quiescing. */
} MoveListT;

#define MAX_PV_DEPTH 13 /* max PV moves we can fit in an 80-char line */
typedef struct {
    int eval;     /* evaluation of the position. */
    int level;    /* nominal search depth. */
    int depth;    /* including quiescing. */
    uint8 pv[30]; /* this is enough for 15 moves. */
} PvT; /* preferred variation structure. */

typedef struct {
    int lgh;
    uint8 list [64]; /* src coord.  Not usually larger than 16, but with
		        edit-position (or bughouse) we might get extra
			pieces ... */
} CoordListT;

typedef struct {
    int funcCallCount;
    int moveCount;
    int hashHitGood; /* hash hits that returned immediately. */
} CompStatsT;


typedef struct {
    /* 'moves' only needs to be [64] [10], but for alignment purposes... */
    uint8 *moves[64] [16];      /* pre-calculated list of moves from any given
				   square in any given direction.  The
				   directions (from White's perspective) are:
				   0 - northwest
				   1 - north
				   2 - northeast
				   3 - east
				   4 - southeast
				   5 - south
				   6 - southwest
				   7 - west
				   8 - night move.
				   9 - night move (special, used only for
				   calculating black night moves).
				   Each 'list' is terminated with a FLAG. */

    uint8 dir[64] [64];	        /* pre-calculated direction from one square to
				   another. */
    char check[BQUEEN + 1] [2]; /* pre-calculated identification of friend,
				   enemy, or unoccupied. */
    int worth[BQUEEN + 1];      /* pre-calculated worth of pieces.  Needs to be
				   signed (Kk is -1). */

    /* (pre-calculated) bool table that tells us if a piece can attack in a
       certain direction.  Currently only defined for bishop, rook, and
       queen, but I could extend it. */
    /* 'attacks' could be [DIRFLAG + 1] [BQUEEN + 1], but optimized for
       alignment. */
    uint8 attacks[DIRFLAG + 1] [16];

    /* pre-calculated distance from one square to another.  Does not take
       diagonal moves into account. */
    uint8 distance[64] [64];

    /* pre-calculated distance from one square to center of board.  Does not
       take diagonal moves into account. */
    uint8 centerDistance[64];

    /* (pre-calculated) hashing support. */
    struct
    {
	int coord[BQUEEN + 1] [64];
	int turn;
	int cbyte[16];
	int ebyte[64];
    } zobrist;

    /* transposition table mask. */
    int hashMask;
    int numHashEntries;
} GPreCalcT;
extern GPreCalcT gPreCalc;


/* bits which define ability to castle (other bits in 'cbyte' are reserved). */
#define WHITEKCASTLE 0x1
#define BLACKKCASTLE 0x2
#define WHITEQCASTLE 0x4
#define BLACKQCASTLE 0x8
#define WHITECASTLE (WHITEQCASTLE | WHITEKCASTLE)
#define BLACKCASTLE (BLACKQCASTLE | BLACKKCASTLE)
#define ALLCASTLE   (WHITECASTLE | BLACKCASTLE)

typedef struct {
    int zobrist;       /* zobrist hash.  Incrementally updated w/each move. */
    uint8 hashcoord[32]; /* this is a 4-bit version of the above.  Hopefully
			    useful for hashing, or possibly speeding up move
			    generation (probably not). */
} PositionT;

/* This is beyond the depth we can quiesce. */
#define HASH_NOENTRY -64

typedef struct {
    PositionT p;
    int alpha;
    int eval;
    char comstr[4];  /* stores preferred move for this position. */
    uint16 ply;      /* lets us evaluate if this entry is 'too old'. */
    int8 depth;      /* needs to be plys from quiescing, due to incremental
			search. */
} HashPositionT;

extern HashPositionT *gHash;

typedef struct {
    uint8 coord[64]; /* all the squares on the board. */

    /* The following 4 variables should be kept together, in this order.
       It is used by drawThreefoldRepitition() and PositionSave().
    */
    int zobrist;         /* zobrist hash.  Incrementally updated w/each move.
			  */
    uint8 hashcoord[32]; /* this is a 4-bit version of the above.  Hopefully
			    useful for hashing, or possibly speeding up move
			    generation (probably not). */
    uint8 cbyte;         /* castling byte. */
    uint8 ebyte;         /* en passant byte. */

    uint8 ncpPlies;  /* how many plies has it been since last capture or
			pawn-move.  If 100 plies passed, the game can be drawn
			by the fifty-move rule.

			More specifically, (also,) if the next move will
			trigger the fifty-move rule, one side can announce its
			intention to draw and then play the move.

			We ignore that, currently -- two players can decide
			whether to draw, and the computer will check before and
			after its move if the fifty-move draw rule applies. */
    int ply;	     /* (aka 1/2-move.) White's 1st move is '0'. */
    int ncheck[2];   /* says if either side currently ncheck. */

    /* This is a way to quickly look up the number and location of any
       type of piece on the board. */
    CoordListT playlist[BQUEEN + 1];

    uint8 *playptr[64];      /* Given a coordinate, this points back to the
				exact spot in the playlist that refers to this
				coord.  Basically a reverse lookup for
				playlist. */

    short hist[2] [64] [64]; /* History table.  16k right now. */
    int hiswin;		     /* Tells us how many plies we can check
				backwards or forwards and still be a
				valid 'history' entry. */

    int totalStrgh; /* strgh of all pieces combined.  Used for checking
		       draws. */
    int playerStrgh[2]; /* strgh (material, not position) of a given side. */

    /* Saved positions.  I need at least 100 to account for the fifty-move
       rule, and 128 is the next power-of-2 which makes calculating the
       appropriate position for a given ply easy. */
    PositionT positions[128];

    /* Note: we do not attempt to save/restore the below variables.
       saverestore.c also assumes 'depth' is the first non-saved/restored
       variable.
    */
    int depth;     /* depth we're currently searching at (Searching from
		      root = 0) */
    int level;     /* depth we're currently authorized to search at (can break
		      this w/quiescing). */
    int maxLevel;  /* max depth we are authorized to search at.  Set by user.
		    */
    int qstrgh[2]; /* used for quiescing. */
} BoardT;


typedef struct {
    int control[2]; /* 0 if player controls; 1 if computer */
    int bDone[2]; /* boolean: true if:
		     this side has drawn,
		     this side was checkmated, or
		     computer resigned the position. */
    void *mainCookie; /* for switcher. */   
    void *playCookie;
    long lastTime;  /* time it took to make last move. */
    BoardT boardCopy;
} GameStateT;


/* Incremental update.  To be used everytime when board->coord[i] is updated.
   It is the hashing equivalent of 'board->coord[i] = newVal'.
 */
#define COORDUPDATEZ(board, i, newVal) \
    ((board)->zobrist ^= gPreCalc.zobrist.coord[board->coord[i]] [i], \
     board->coord[i] = (newVal), \
     (board)->hashcoord[(i) >> 1] = \
        ((board)->coord[(i) & ~1] << 4) + \
        ((board)->coord[((i) & ~1) + 1]), \
     (board)->zobrist ^= gPreCalc.zobrist.coord[newVal] [i])

#define COORDUPDATE(board, i, newVal) \
    (board->coord[i] = (newVal), \
     (board)->hashcoord[(i) >> 1] = \
        ((board)->coord[(i) & ~1] << 4) + \
        ((board)->coord[((i) & ~1) + 1]))

#define FRIEND 0
#define UNOCCD 1
#define ENEMY 2
/* returns FRIEND, ENEMY, or UNOCCD */
#define CHECK(piece, turn)  (gPreCalc.check[(piece)] [(turn)])

#define WORTH(piece) (gPreCalc.worth[piece])

/* External function prototypes: */

/* log.c */
enum {
    eLogEmerg,
    eLogNormal,
    eLogDebug
} LogLevelT;
#define LOG_EMERG(format, ...) LogPrint(eLogEmerg, (format), ##__VA_ARGS__)
#define LOG_NORMAL(format, ...) LogPrint(eLogNormal, (format), ##__VA_ARGS__)
#define LOG_DEBUG(format, ...) LogPrint(eLogDebug, (format), ##__VA_ARGS__)
void LogSetLevel(int level);
int LogPrint(int level, const char *format, ...)
    __attribute__ ((format (printf, 2, 3)));
void LogMoveList(int level, MoveListT *mvlist);
void LogMove(int level, BoardT *board, uint8 *comstr);
void LogMoveShow(int level, BoardT *board, uint8 *comstr, char *caption);

/* init.c */
void preCalcInit(int numHashEntries);
void newgameEx(BoardT *board, uint8 pieces[], int cbyte, int ebyte, int ply);
void newgame(BoardT *board);
int calcZobrist(BoardT *board);

typedef struct {
    /* The main thread uses the mainSock, and the computer thread uses
       the compSock. */
    int mainSock, compSock;
    volatile uint8 moveNow;
    uint8 isThinking;  /* bool. */
} ThinkContextT;

/* comp.c */
void compThreadInit(BoardT *board, ThinkContextT *th);
static inline int drawFiftyMove(BoardT *board)
{
    return board->ncpPlies >= 100;
}
static inline void PositionSave(BoardT *board)
{
    memcpy(&board->positions[board->ply & 127],
	   &board->zobrist,
	   sizeof(PositionT));
}
int drawInsufficientMaterial(BoardT *board);
int drawThreefoldRepetition(BoardT *board);
int calcCapWorth(BoardT *board, uint8 *comstr);

/* switcher.c */
typedef struct {
    sem_t sem1, sem2;
    void *cookie;
} SwitcherContextT;
void SwitcherInit(SwitcherContextT *sw);
void *SwitcherGetCookie(SwitcherContextT *sw);
void SwitcherSwitch(SwitcherContextT *sw, void *cookie);

/* playmov.c */
int asciiToNative(uint8 ascii);
int nativeToAscii(uint8 piece);
void playerThreadInit(BoardT *board, ThinkContextT *th, SwitcherContextT *sw,
		      GameStateT *gameState);
char *searchlist(MoveListT *mvlist, char *comstr, int howmany);
int BoardSanityCheck(BoardT *board);


typedef struct {
    uint8 cappiece; /* any captured piece.. does not include en passant */
    uint8 cbyte;    /* castling, en passant bytes */
    uint8 ebyte;
    uint8 ncpPlies;
    uint32 zobrist; /* saved-off hash. */
} UnMakeT;

/* makemov.c */
/* comstr[] follows (exactly) the format of 'MoveListT's list above.
   (see movgen.c:addmove() for details)

   'unmake' is filled in by makemove() and used by unmakemove().
*/
void addpiece(BoardT *board, uint8 piece, int coord);
void delpiece(BoardT *board, uint8 piece, int coord);
void makemove(BoardT *board, uint8 comstr[], UnMakeT *unmake);
void unmakemove(BoardT *board, uint8 comstr[], UnMakeT *unmake);
void commitmove(BoardT *board, uint8 *comstr, ThinkContextT *th,
		GameStateT *gameState, int declaredDraw);
void newcbyte(BoardT *board);

/* debug.c */
int concheck(BoardT *board, char *failString, int checkz);
void printlist(MoveListT *mvlist);
void printplaylist(BoardT *board);

typedef struct {
    void (*playerMove)(BoardT *board, ThinkContextT *th, SwitcherContextT *sw,
		       GameStateT *gameState);
    void (*boardRefresh)(BoardT *board);
    void (*exit)(void);
    void (*statusDraw)(BoardT *board, int timeTaken);
    void (*notifyMove)(uint8 *comstr);
    void (*notifyError)(char *reason);
    void (*notifyPV)(PvT *pv);
    void (*notifyThinking)(void);
    void (*notifyReady)(void);
    void (*notifyComputerStats)(CompStatsT *stats);
    void (*notifyDraw)(char *reason);
    void (*notifyCheckmated)(int turn);
    void (*notifyResign)(int turn);
} UIFuncTableT;
extern UIFuncTableT *gUI;

/* ui.c */
UIFuncTableT *UIInit(void);

/* intfXboard.c */
UIFuncTableT *xboardInit(void);

/* movgen.c */
void mlistGenerate(MoveListT *mvlist, BoardT *board, int capOnly);
void mlistFirstMove(MoveListT *mvlist, BoardT *board, uint8 *comstr);
void mlistSortByCap(MoveListT *mvlist, BoardT *board);
int calcNCheck(BoardT *board, int kcoord, char *context);

/* saverestore.c */
void CopyBoard(BoardT *dest, BoardT *src);
int GameSave(BoardT *board);
int GameRestore(BoardT *board);

/* thinker.c */
typedef enum {
    eCmdThink,
    eCmdMoveNow,

    eRspDraw,
    eRspMove,
    eRspResign,
    eRspStats,
    eRspPv
} eThinkMsgT;

void ThinkerInit(ThinkContextT *th);
eThinkMsgT ThinkerRecvRsp(ThinkContextT *th, void *buffer, int bufLen);
void ThinkerThink(ThinkContextT *th);
void ThinkerMoveNow(ThinkContextT *th);
void ThinkerBail(ThinkContextT *th);

static inline int ThinkerCompNeedsToMove(ThinkContextT *th)
{
    return th->moveNow;
}
void ThinkerCompDraw(ThinkContextT *th, uint8 *comstr);
void ThinkerCompMove(ThinkContextT *th, uint8 *comstr);
void ThinkerCompResign(ThinkContextT *th);
void ThinkerCompNotifyStats(ThinkContextT *th, CompStatsT *stats);
void ThinkerCompNotifyPv(ThinkContextT *th, PvT *Pv);
void ThinkerCompWaitThink(ThinkContextT *th);

typedef void *(*PTHREAD_FUNC)(void *);

#endif /* REF_H */
