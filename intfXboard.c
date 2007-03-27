/***************************************************************************
                        intfXboard.c - Xboard interface.
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

/* Note: I am deliberately trying to avoid reliance on version 2 of the xboard
   protocol, in order to interop w/other chess GUIs that might only utilize
   version 1.
*/

#include <stdio.h>
#include <signal.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "ref.h"


#define MAXBUFLEN 160

static struct {
    int post;
    int badPosition;
} gXboardState;


static int matches(char *str, char *needle)
{
    int len = strlen(needle);
    return !strncmp(str, needle, len) && isspace(str[len]);
}


/* Given input like 'a1', returns something like '0'
   (or FLAG, if not a sensible coord) */
static int toCoord(char *inputStr)
{
    return (inputStr[0] >= 'a' && inputStr[0] <= 'h' &&
	    inputStr[1] >= '1' && inputStr[1] <= '8') ?
	inputStr[0] - 'a' + ((inputStr[1] - '1') * 8) :
	FLAG;
}


static int isMove(char *inputStr, char *comstr)
{
    
    if (toCoord(inputStr) != FLAG && toCoord(&inputStr[2]) != FLAG)
    {
	comstr[0] = toCoord(inputStr);
	comstr[1] = toCoord(&inputStr[2]);
	return 1;
    }
    return 0;
}


static void xboardGetInput(char *buffer, int buflen)
{
    /* xboard wants to tell us something.  Go ahead and wait for it all to
       come in. */
    fgets(buffer, buflen, stdin);

    /* ... and it guarantees every complete command will be terminated by
       a newline. */
    if (!strchr(buffer, '\n'))
    {
	assert(0);
    }
}


static void xboardNotifyError(char *reason)
{
    printf("tellusererror Illegal position: %s\n", reason);
}


static void xboardEditPosition(BoardT *board)
{
    char inputStr[MAXBUFLEN];

    board->ebyte = FLAG; /* assumed, for 'edit' command. */
    board->cbyte = ALLCASTLE; /* 'edit' command is optimistic. */
    board->ply &= 1; /* erase move history. */

    int i = 0;
    int myColor = 0;
    int coord;
    int piece;

    while(1)
    {
	xboardGetInput(inputStr, MAXBUFLEN);	

	if (matches(inputStr, "#"))
	{
	    /* Wipe board. */
	    for (i = 0; i < 64; i++)
	    {
		if (board->coord[i])
		{
		    delpiece(board, board->coord[i], i);
		    board->coord[i] = 0;
		}
	    }
	}

	else if (matches(inputStr, "c"))
	{
	    /* Change current color. */
	    myColor ^= 1;
	}

	else if (matches(inputStr, "."))
	{
	    /* Leave edit mode. */
	    newcbyte(board);
	    return;
	}

	else switch(inputStr[0])
	{
	case 'x': /* delete the piece @ inputStr[1]. */
	case 'P': /* Add one of these pieces @ inputStr[1]. */
	case 'R':
	case 'N':
	case 'B':
	case 'Q':
	case 'K':
	    if ((coord = toCoord(&inputStr[1])) == FLAG)
	    {
		printf("Error (edit: %c: bad coord): %s",
		       inputStr[0], &inputStr[1]);
		break;
	    }

	    /* Clear any existing piece. */
	    if (board->coord[coord])
	    {
		delpiece(board, board->coord[coord], coord);
		board->coord[coord] = 0;
	    }

	    if (inputStr[0] == 'x') break;

	    /* Add the new piece. */
	    piece = asciiToNative(inputStr[0]);
	    piece |= myColor;
	    addpiece(board, piece, coord);
	    board->coord[coord] = piece;
	    break;
	default:
	    printf("Error (edit: unknown command): %s", inputStr);	    
	    break;
	}
    }
}


/* This runs as a coroutine with the main thread, and can switch off to it
   at any time.  If it exits it will immediately be called again. */
static void xboardPlayerMove(BoardT *board, ThinkContextT *th,
			     SwitcherContextT *sw, GameStateT *gameState)
{
    char inputStr[MAXBUFLEN];
    int protoVersion, myLevel, turn;
    static int firstCall = 1;

    /* Move-related stuff. */
    uint8 chr;
    uint8 comstr[4];
    uint8 myPieces[64];
    char *ptr;
    MoveListT movelist;

    if (firstCall)
    {
	board->maxLevel = 5; /* Use a sensible default for xboard. */
	firstCall = 0;
    }

    xboardGetInput(inputStr, MAXBUFLEN);

    /* Ignore certain commands... */
    if (matches(inputStr, "xboard") ||
	matches(inputStr, "accepted") ||
	matches(inputStr, "rejected") ||
	matches(inputStr, "random") ||
	matches(inputStr, "hint") ||
	matches(inputStr, "hard") ||
	matches(inputStr, "easy") ||
	matches(inputStr, "name") ||
	matches(inputStr, "rating") || /* This could be useful to implement */
	matches(inputStr, "ics") ||
	matches(inputStr, "computer") ||

        /* (we don't accept draw offers yet) */
	matches(inputStr, "draw") ||

        /* (these are time commands.) */
	matches(inputStr, "level") ||
	matches(inputStr, "st") ||
	matches(inputStr, "time") ||
	matches(inputStr, "otim"))
    {
	LOG_DEBUG("ignoring cmd: %s", inputStr);
    }

    /* Return others as unimplemented... */
    else if (matches(inputStr, "variant") ||
	     matches(inputStr, "playother") ||
	     matches(inputStr, "ping") ||
	     matches(inputStr, "setboard") ||
	     matches(inputStr, "usermove") ||
	     matches(inputStr, "bk") ||
	     matches(inputStr, "undo") ||
	     matches(inputStr, "remove") ||
	     matches(inputStr, "analyze") ||
	     matches(inputStr, "pause") ||
	     matches(inputStr, "resume") ||

             /* (bughouse commands.) */
	     matches(inputStr, "partner") ||
	     matches(inputStr, "ptell") ||
	     matches(inputStr, "holding"))
    {
	printf("Error (unimplemented command): %s", inputStr);
    }

    else if (matches(inputStr, "edit"))
    {
	gXboardState.badPosition = 0; /* hope for the best. */
	ThinkerBail(th);
	xboardEditPosition(board);
	if (BoardSanityCheck(board) == 0)
	{
	    memcpy(myPieces, board->coord, sizeof(myPieces));
	    newgameEx(board, myPieces, board->cbyte, board->ebyte, board->ply);
	    commitmove(board, NULL, th, gameState, 0);
	}
	else
	{
	    /* (BoardSanityCheck() notifies xboard of the defails of the
	       bad position all by itself.) */
	    gXboardState.badPosition = 1;
	}
    }

    else if (matches(inputStr, "new"))
    {
	/* New game, computer is Black.  Use sensible search depth. */
	gXboardState.badPosition = 0; /* hope for the best. */
	ThinkerBail(th);
	newgame(board);
	commitmove(board, NULL, th, gameState, 0);
	gameState->control[0] = 0;
	gameState->control[1] = 1;
    }

    else if (sscanf(inputStr, "protover %d", &protoVersion) == 1 &&
	     protoVersion >= 2)
    {
	/* Note: we do not care if these features are accepted or rejected.
	   We try to handle all input as best as possible. */
	printf("feature analyze=0 myname=arctic0.8 variants=normal "
	       "colors=0 done=1\n");
    }

    else if (matches(inputStr, "quit"))
    {
	ThinkerBail(th);
	exit(0);
    }

    else if (matches(inputStr, "force"))
    {
	/* Stop everything. */
	ThinkerBail(th);
	gameState->control[0] = 0;
	gameState->control[1] = 0;
    }

    else if (matches(inputStr, "white") ||
	     matches(inputStr, "black"))
    {
	/* Stop everything.  Engine plays the other color.  This is not
	   exactly as specified.  Too bad, I'm not going to change whose
	   turn it is to move! */
	ThinkerBail(th);
	turn = matches(inputStr, "white");
	gameState->control[turn] = 0;
	gameState->control[turn ^ 1] = 1;
    }

    else if (sscanf(inputStr, "sd %d", &myLevel) == 1)
    {
	/* Set depth. */
	if (myLevel >= 0 && myLevel <= 9)
	{
	    if (board->level > (board->maxLevel = myLevel))
	    {
		ThinkerMoveNow(th);
	    }
	}
	else
	{
	    printf("Error (bad parameter %d): %s", myLevel, inputStr);
	}
    }

    else if (matches(inputStr, "?"))
    {
	/* Move now. */
	ThinkerMoveNow(th);
    }

    else if (matches(inputStr, "result"))
    {
	/* We don't care if we won, lost, or drew.  Just stop thinking. */
	ThinkerBail(th);
    }

    else if (matches(inputStr, "post"))
    {
	gXboardState.post = 1;
    }

    else if (matches(inputStr, "nopost"))
    {
	gXboardState.post = 0;
    }

    /* (Anything below this case needs a decent position.) */
    else if (gXboardState.badPosition)
    {
	printf("Illegal move (bad position): %s", inputStr);
	return;	
    }

    else if (matches(inputStr, "go"))
    {
	/* Play the color on move, and start thinking. */
	ThinkerBail(th); /* Just in case. */
	turn = board->ply & 1;
	gameState->control[turn] = 1;
	gameState->control[turn ^ 1] = 0;
	ThinkerThink(th);
    }

    else if (isMove(inputStr, comstr))
    {
	/* Move processing.
	   Currently we can only handle algebraic notation. */
	mlistGenerate(&movelist, &gameState->boardCopy, 0);

	/* search movelist for comstr */
	ptr = searchlist(&movelist, comstr, 2);
	if (ptr == NULL)
	{
	    printf("Illegal move: %s", inputStr);
	    return;
	}

	/* Do we need to promote? */
	if (ISPAWN(gameState->boardCopy.coord[comstr[0]]) &&
	    (comstr[1] > 55 || comstr[1] < 8))
	{
	    chr = inputStr[4];
	    if (chr != 'q' && chr != 'r' && chr != 'n' && chr != 'b')
	    {
		printf("Illegal move: %s", inputStr);
		return;
	    }

	    chr = asciiToNative(chr);
	    comstr[2] = (chr & ~1) | (gameState->boardCopy.ply & 1);
	    
	    ptr = searchlist(&movelist, comstr, 3);
	    assert(ptr != NULL);
	}
	else
	{
	    comstr[2] = ptr[2];
	}
	comstr[3] = ptr[3];

	/* At this point, we must have a valid move. */
	ThinkerBail(th);
	commitmove(board, comstr, th, gameState, 0);
    }

    else
    {
	/* Default case. */
	printf("Error (unknown command): %s", inputStr);
    }

    /* Wait for more input. */
    SwitcherSwitch(sw, gameState->playCookie);
}


void xboardNotifyMove(uint8 *comstr)
{
    char outputStr[MAXBUFLEN];
    char *myStr = outputStr;

    myStr += sprintf(myStr, "move %c%c%c%c",
		     File(comstr[0]) + 'a', Rank(comstr[0]) + '1',
		     File(comstr[1]) + 'a', Rank(comstr[1]) + '1');
    if (comstr[2] && !ISPAWN(comstr[2]))
	sprintf(myStr, "%c", tolower(nativeToAscii(comstr[2])));
    printf("%s\n", outputStr);
}


void xboardNotifyDraw(char *reason)
{
    printf("1/2-1/2 {%s}\n", reason);
}


void xboardNotifyResign(int turn)
{
    printf("%d-%d {%s resigns}\n",
	   turn, turn ^ 1, turn ? "Black" : "White");
}


void xboardNotifyCheckmated(int turn)
{
    printf("%d-%d {%s mates}\n",
	   turn, turn ^ 1, turn ? "White" : "Black");
}


void xboardNotifyPV(PvT *pv)
{
    char outputStr[MAXBUFLEN];
    char *myStr = outputStr;
    int howmany = pv->depth + 1;
    uint8 *moves = pv->pv;

    if (!gXboardState.post)
	return;

    myStr += sprintf(myStr, "%d %d 0 0", pv->level, pv->eval);
    for (; howmany > 0; howmany--)
    {
	myStr += sprintf(myStr, " %c%c%c%c",
			 File(moves[0]) + 'a', Rank(moves[0]) + '1',
			 File(moves[1]) + 'a', Rank(moves[1]) + '1');
	moves += 2;
    }
    printf("%s.\n", outputStr);
}


/* xboard (and UCI) wants stats to be moved into notifyPV,
   but I'm not sure I want to do that. */
void xboardNotifyComputerStats(CompStatsT *stats) { }
void xboardBoardRefresh(BoardT *board) { }
void xboardNoop(void) { }
void xboardStatusDraw(BoardT *board, int timeTaken) { }

static UIFuncTableT xboardUIFuncTable =
{
    .playerMove = xboardPlayerMove,
    .boardRefresh = xboardBoardRefresh,
    .statusDraw = xboardStatusDraw,
    .exit = xboardNoop,
    .notifyMove = xboardNotifyMove,
    .notifyError = xboardNotifyError,
    .notifyPV = xboardNotifyPV,
    .notifyThinking = xboardNoop,
    .notifyReady = xboardNoop,
    .notifyComputerStats = xboardNotifyComputerStats,
    .notifyDraw = xboardNotifyDraw,
    .notifyCheckmated = xboardNotifyCheckmated,
    .notifyResign = xboardNotifyResign
};


UIFuncTableT *xboardInit(void)
{
    int err;
    struct sigaction ignore;

    /* Ignore SIGINT. */
    memset(&ignore, 0, sizeof(struct sigaction));
    ignore.sa_flags = SA_RESTART;
    ignore.sa_handler = SIG_IGN;
    err = sigaction(SIGINT, &ignore, NULL);
    assert(err == 0);

    /* Set unbuffered I/O (obviously necessary for output, also necessary for
       input if we want to poll() correctly.) */
    setbuf(stdout, NULL);
    setbuf(stdin, NULL);

    return &xboardUIFuncTable;
}
