#ifndef REF_H
#define REF_H

#define SYSTEMCOL       GREEN
#define TICKCOL BLUE
#define BOARDCOL        BLUE
#define FLAG            127
#define DIRFLAG         127 /* could be same as FLAG, but they are used
			       for different things.  I'd hate to change
			       one, and break the other. */
#define DISCHKFLAG      255 /* cannot be the same as FLAG. */

#define Rank(x) ((x) >> 3)
#define File(x) ((x) & 7)
#define Abs(x)  ((x) < 0 ? -(x) : (x))

typedef unsigned char uint8;

union plist {                                           /* stores pin info. */
    long l[16];
    char c[64];
};

struct mlist {
    int lgh;                            /* length o' list */
    uint8 list [150] [4];               /* coords, promo, and check. */
    int insrt;                          /* spot to insert 'preferred' move. */
};

struct srclist {
    int lgh;
    uint8 list [16];                    /* src coord. */
};

typedef struct {
    int funcCallCount;
    int moveCount;
} CompStatsT;

typedef struct {
    char coord[64]; /* all the squares on the board. */

    char cbyte;     /* castling byte.  Format (msbit to lsbit):
		       7 - white queen can castle
		       6 - white king can castle
		       5 - black queen can castle
		       4 - black king can castle
		       3-0: reserved (0)
		     */
    uint8 ebyte;    /* en passant byte. */

    int ply;	    /* (aka 1/2-move.) White's 1st move is '0'. */
    int ncheck[2];  /* says if either side currently ncheck. */

    /* This is a way to quickly look up the number and location of any
       type of piece on the board. */
    struct srclist playlist['r' + 1];

    uint8 *playptr[64];      /* Given a coordinate, this points back to the
				exact spot in the playlist that refers to this
				coord.  Basically a reverse lookup for
				playlist. */

    short hist[2] [64] [64]; /* History table.  16k right now. */
    int hiswin;		     /* Tells us how many plies we can check
				backwards or forwards and still be a
				valid 'history' entry. */

    /* Note: we do not attempt to save/restore the below variables.
       saverestore.c also assumes 'depth' is the first non-saved/restored
       variable.
    */
    int depth; /* depth we're currently searching at (Searching from
		  root = 0) */
    int level; /* depth we're authorized to search at.  (May be able to break
		  this w/extensions... not implemented yet) */

    uint8 *moves[64] [9];       /* pre-calculated list of moves from any given
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
				   Each 'list' is terminated with a FLAG. */

    uint8 dir[64] [64];		/* pre-calc'd direction from one square to
				   another. */
} BoardT;


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
int LogPrint(int level, const char *format, ...);
void LogMoveList(int level, struct mlist *mvlist);
void LogMove(int level, int moveDepth, uint8 *comstr);

/* incheck.c */
int nopose(BoardT *board, int src, int dest, int hole);

/* init.c */
void init(uint8 *moves[] [9], uint8 dir[] [64]);
void newgame(BoardT *board);
void addpiece(BoardT *board, uint8 piece, int coord);
void delpiece(BoardT *board, uint8 piece, int coord);

/* comp.c */
void computermove(BoardT *board, int *show);

/* playmov.c */
void playermove(BoardT *board, int *show, int control[]);

/* makemov.c */
/* comstr[] consists of:
   -- src coord (0-63)
   -- dst coord (0-63)
   -- piece to promote to, or [Pp] (after makemove, for en passant), or nada
   -- ncheck, after this move
   (see movgen.c:add() for details)
*/
void makemove(BoardT *board, uint8 comstr[], char cappiece);
void unmakemove(BoardT *board, uint8 comstr[], char cappiece);

/* debug.c */
int concheck(BoardT *board);
void printlist(struct mlist *mvlist);
void printplaylist(BoardT *board);

/* ui.c */
void UIPVDraw(uint8 *moves, int eval, int howmany);
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
void UIMovelistShow(struct mlist *mvlist);
void UINotifyThinking(void);
void UINotifyComputerStats(CompStatsT *stats);
void UISetDebugLoggingLevel(void);

/* movgen.c */
void genmlist(struct mlist *mvlist, BoardT *board, int turn);
int check(char piece, int turn);

/* saverestore.c */
int GameSave(BoardT *board);
int GameRestore(BoardT *board);

#endif /* REF_H */
