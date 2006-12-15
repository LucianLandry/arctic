#define SYSTEMCOL 	GREEN
#define TICKCOL	BLUE
#define BOARDCOL	BLUE
#define FLAG		127 /* note: chars are signed. */
#define Rank(x)	(x >> 3)
#define File(x)     (x & 7)
#define Abs(x)		(x < 0 ? -(x) : x)

union plist {						/* stores pin info. */
	long l[16];
	char c[64];
};

struct mlist {
	int lgh; 								/* length o' list */
	char list [150] [4];			/* coords, promo, and check. */
	int insrt;					/* spot to insert 'preferred' move. */
};

struct srclist {
	int lgh;
	char list [16];				/* src coord. */
};

struct brd {
	char coord[64];
	char *playptr[64];
	struct srclist playlist['r' + 1];
	char *moves[64] [9];
	char cbyte, ebyte;
	char hist[2] [64] [64];		/* 8k right now.  Will barf after 64 moves
							(127 plies).  Will probly change to int. */
	int hiswin;				/* tells us how many plies we can check
							   backwards or forwards and still be a
							   valid 'history' entry. */
	int move;					/* actually, ply. */
	int depth;				/* depth we're currently searching at.
							   (Searching from root = 0) */
	int level;				/* depth we're authorized to search at.
							   (May be able to break this w/
							   extensions... not implemented yet) */
	char dir[64] [64];			/* pre-calc'd dir fr. one sq to another. */
	int ncheck[2];				/* says if either side currently ncheck. */
};

/* Function prototypes. */
int nopose(struct brd *board, int src, int dest, int hole);
int discheck(struct brd *board, int src, int turn, struct srclist *poplist);

int dirf(int from, int to);
void init(char *moves[] [9], char mvarray[], char dir[] [64]);
void rowinit(int d, int start, int finc, int sinc, char *moves[] [9], char *ptr);
void diaginit(int d, int start, int finc, int sinc, char *moves[] [9], char *ptr);
void new(struct brd *board, int col[]);
void addpiece(struct brd *board, char piece, int coord);

/* comp.c */
int worth(char a);
int eval(char coord[], int turn);
void computermove(struct brd *board, int *show, int col[]);
int minimax(struct brd *board, char comstr[], int turn, int alpha, int strgh,
	long *nodes, long *ack, int *show, int col[], char *pv);

void playermove(struct brd *board, int *show, int control[],
	int col[]);

void ncopy(char a[], char b[], int n);

void newcbyte(char coord[], char *cbyte);
void unmakemove(struct brd *board, char comstr[], char cappiece);
void makemove(struct brd *board, char comstr[], char cappiece);

int concheck(struct brd *board);

/* boardif.c */
void printpv(char *moves, int howmany);
void getopt();
void drawcurs(int coord, int blink, int undo);
void drawoptions();
void prettyprint(char *, int y);
void printstatus(struct brd *board, int timetaken);
void drawticks();
void drawboard();
void printlist(struct mlist *mvlist);
char *searchlist(struct mlist *mvlist, char *comstr);
int barf(char *message);
void update(char board[], int col[]);
void printplaylist(struct brd *board);

void genslide(struct brd *board, struct srclist *dirlist, int turn, int from);
void gendclist(struct brd *board, struct srclist *dclist, int ekcoord,
	int turn);
int findcoord(struct srclist *attlist, int from);
void findpins(struct brd *board, char coord[], char *moves[],
	union plist *pinlist, int turn);
void cappose(struct mlist *mvlist, struct brd *board, char *moves[] [9],
	char attcoord,	union plist *pinlist, int turn, char kcoord,
	struct srclist *dclist);
void genmlist(struct mlist *mvlist, struct brd *board, int turn);
void addsrccoord(struct srclist *attlist, int from);
int attacked(struct srclist *attlist, struct brd *board, char *moves[],
	int from, int turn, int onwho, int stp);
void nightmove(struct mlist *mvlist, struct brd *board, char *moves, int from,
	int turn, int pintype, int dc);
void brmove(struct mlist *mvlist, struct brd *board, char *moves[], int from,
	int turn,	int pintype, int start, int dc);
void probe(struct mlist *mvlist, struct brd *board, char *moves, int from,
	int turn, int dc);
void kingmove(struct mlist *mvlist, struct brd *board, char *moves[] [9],
	int from, int turn, int dc);
void pawnmove(struct mlist *mvlist, struct brd *board, char *moves[], int from,
	int turn, int pintype, int dc);
void promo(struct mlist *mvlist, struct brd *board, int from, int to,
	int turn, int dc);
void add(struct mlist *mvlist, struct brd *board, int from, int to,
	int promote, int dc);
int check(char piece, int turn);
