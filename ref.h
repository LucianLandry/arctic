#ifndef REF_H
#define REF_H

#include <string.h> /* memcpy() */

#define FLAG            127
#define FLAG64          0x7f7f7f7f7f7f7f7fLL; /* 8 FLAGs in a row */
#define DIRFLAG         127 /* could be same as FLAG, but they are used
			       for different things.  I'd hate to change
			       one, and break the other. */
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

typedef unsigned char uint8;
typedef signed char   int8;
typedef unsigned int  uint32;
typedef unsigned long long uint64;

typedef union {    /* stores pin info. */
    uint64 ll[8];
    uint8 c[64];
} PListT;

typedef struct {
    int lgh;                            /* length o' list.  Should be 1st,
					   for alignment purposes. */
    uint8 list [150] [4];               /* [0, 1]: coords
					   [2]: promo (or enpass, signified by
					   'p' of opposite color, and
					   [3] check. */
    int insrt;                          /* spot to insert 'preferred' move. */
    int ekcoord;                        /* scratch for genmlist(), for
					   performance reasons. */
    int capOnly;                        /* used for quiescing. */
} MoveListT;

#define PV_DEPTH 15
typedef struct {
    int depth;
    uint8 pv[30]; /* this is enough for 15 moves. */
} PvT; /* preferred variation structure. */

struct srclist {
    int lgh;
    uint8 list [16];                    /* src coord. */
};

typedef struct {
    int funcCallCount;
    int moveCount;
} CompStatsT;


typedef struct {
    uint8 *moves[64] [10];      /* pre-calculated list of moves from any given
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
    char check[2] [BQUEEN + 1]; /* pre-calculated identification of friend (0),
				   enemy (1), or unoccupied (2). */
    int8 worth[BQUEEN + 1];     /* pre-calculated worth of pieces.  Needs to be
				   signed (Kk is -1). */

    /* (pre-calculated) hashing support. */
    struct
    {
	int coord[64] [BQUEEN + 1];
	int turn;
	int cbyte[16];
	int ebyte[64];
    } zobrist;
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
    uint8 cbyte;     /* castling byte. */
    uint8 ebyte;     /* en passant byte. */
} PositionT;

typedef struct {
    uint8 coord[64]; /* all the squares on the board. */

    /* The following 4 variables should be kept together, in this order.
       It is used by drawThreefoldRepitition() and SavePosition().
    */
    int zobrist;       /* zobrist hash.  Incrementally updated w/each move. */
    uint8 hashcoord[32]; /* this is a 4-bit version of the above.  Hopefully
			    useful for hashing, or possibly speeding up move
			    generation (probably not). */
    uint8 cbyte;     /* castling byte. */
    uint8 ebyte;     /* en passant byte. */

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
    struct srclist playlist[BQUEEN + 1];

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
    int level;     /* depth we're authorized to search at (can break this w/
		      quiescing). */
    int qstrgh[2]; /* used for quiescing. */
} BoardT;


/* Incremental update.  To be used everytime when board->coord[i] is updated.
   It is the hashing equivalent of 'board->coord[i] = newVal'.
   Optimization: on undo, I really ought to be able to pop 'zobrist'...
   except during castling.  Ugh.  Need to fix that.
 */
#define COORDUPDATEZ(board, i, newVal) \
    ((board)->zobrist ^= gPreCalc.zobrist.coord[i] [board->coord[i]], \
     board->coord[i] = (newVal), \
     (board)->hashcoord[(i) >> 1] = \
        ((board)->coord[(i) & ~1] << 4) + \
        ((board)->coord[((i) & ~1) + 1]), \
     (board)->zobrist ^= gPreCalc.zobrist.coord[i] [newVal])

#define COORDUPDATE(board, i, newVal) \
    (board->coord[i] = (newVal), \
     (board)->hashcoord[(i) >> 1] = \
        ((board)->coord[(i) & ~1] << 4) + \
        ((board)->coord[((i) & ~1) + 1]))


/* returns 0 if friend, 1 if enemy, 2 if unoccupied */
#define CHECK(piece, turn)  (gPreCalc.check[(turn)] [(piece)])
#define WORTH(piece) (gPreCalc.worth[piece])

/* Fast (but non error-checking) version of tolower().  ASCII-dependent.
   Anybody using this is probably broken now.
   #define TOLOWER(c) ((c) | 0x20) */

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

/* init.c */
void initPreCalc(void);
void newgame(BoardT *board);
void addpiece(BoardT *board, uint8 piece, int coord);
void delpiece(BoardT *board, uint8 piece, int coord);
int calcZobrist(BoardT *board);

/* comp.c */
void computermove(BoardT *board);
static inline int drawFiftyMove(BoardT *board)
{
    return board->ncpPlies >= 100;
}
static inline void SavePosition(BoardT *board)
{
    memcpy(&board->positions[board->ply & 127],
	   &board->zobrist,
	   sizeof(PositionT));
}
int drawInsufficientMaterial(BoardT *board);
int drawThreefoldRepetition(BoardT *board);
int calcCapWorth(BoardT *board, uint8 *comstr);

/* playmov.c */
void playermove(BoardT *board, int *autopass, int control[]);

typedef struct {
    uint8 cappiece; /* any captured piece.. does not include en passant */
    uint8 cbyte;    /* castling, en passant bytes */
    uint8 ebyte;
    uint8 ncpPlies;
    uint32 zobrist; /* saved-off hash. */
} UnMakeT;

/* makemov.c */
/* comstr[] consists of:
   -- src coord (0-63)
   -- dst coord (0-63)
   -- piece to promote to, or [Pp] (for en passant), or nada
   -- ncheck, after this move
   (see movgen.c:add() for details)

   'unmake' is filled in by makemove() and used by unmakemove().
*/
void makemove(BoardT *board, uint8 comstr[], UnMakeT *unmake);
void unmakemove(BoardT *board, uint8 comstr[], UnMakeT *unmake);

/* debug.c */
int concheck(BoardT *board, char *failString);
void printlist(MoveListT *mvlist);
void printplaylist(BoardT *board);

/* ui.c */
void UIPVDraw(PvT *pv, int eval);
void getopt(uint8 command[]);
void printstatus(BoardT *board, int timetaken);
void UITicksDraw(void);
void UIBoardDraw(void);
int barf(char *message);
void UIBoardFlip(BoardT *board);
void UIBoardUpdate(BoardT *board);
void UIPlayerColorChange(void);
void UIInit(void);
void UIExit(void);
void UIMoveShow(BoardT *board, uint8 *comstr, char *caption);
void UIMovelistShow(MoveListT *mvlist);
void UINotifyThinking(void);
void UINotifyComputerStats(CompStatsT *stats);
void UISetDebugLoggingLevel(void);

/* movgen.c */
void mlistGenerate(MoveListT *mvlist, BoardT *board, int capOnly);
void mlistFirstMove(MoveListT *mvlist, BoardT *board, uint8 *comstr);
void mlistSortByCap(MoveListT *mvlist, BoardT *board);

/* saverestore.c */
int GameSave(BoardT *board);
int GameRestore(BoardT *board);

#endif /* REF_H */
