//--------------------------------------------------------------------------
//                 uiUtil.c - UI-oriented utility functions.
//                           -------------------
//  copyright            : (C) 2007 by Lucian Landry
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
#include <ctype.h>  // tolower()
#include <errno.h>  // errno
#include <stdarg.h> // va_list
#include <stdio.h>  // sprintf()
#include <stdlib.h> // abs()

#include "aThread.h"
#include "clockUtil.h"
#include "gDynamic.h"
#include "log.h"
#include "moveList.h"
#include "ui.h"
#include "uiUtil.h"

UIFuncTableT *gUI;

/* (one extra space for \0.) */
static char gPieceUITable[NUM_PIECE_TYPES + 1] = "  KkPpNnBbRrQq";

int nativeToAscii(uint8 piece)
{
    return piece >= NUM_PIECE_TYPES ? ' ' : gPieceUITable[piece];
}


int nativeToBoardAscii(uint8 piece)
{
    int ascii = nativeToAscii(piece);
    return ISPAWN(piece) ? tolower(ascii) :
	(piece & 1) ? toupper(ascii) : ascii;
}


int asciiToNative(uint8 ascii)
{
    char *mychr = strchr(gPieceUITable, ascii);
    return mychr != NULL ? mychr - gPieceUITable : 0;
}


int asciiToCoord(char *inputStr)
{
    return (inputStr[0] >= 'a' && inputStr[0] <= 'h' &&
	    inputStr[1] >= '1' && inputStr[1] <= '8') ?
	inputStr[0] - 'a' + ((inputStr[1] - '1') * 8) :
	FLAG;
}


static bool matchHelper(char *str, char *needle, bool caseSensitive)
{
    int len = strlen(needle);
    return
	str == NULL ? 0 :
	!(caseSensitive ? strncmp(str, needle, len) :
	  strncasecmp(str, needle, len)) &&
	(isspace(str[len]) || str[len] == '\0');
}

// Pattern matchers for tokens embedded at the start of a larger string.
bool matches(char *str, char *needle)
{
    return matchHelper(str, needle, true);
}

bool matchesNoCase(char *str, char *needle)
{
    return matchHelper(str, needle, false);
}

// Direct a report to the user or the error log, whichever is more
// appropriate.  Always returns -1 (as a convenience).
int reportError(int silent, char *errorFormatStr, ...)
{
    char tmpBuf[160];
    va_list ap;

    va_start(ap, errorFormatStr);
    vsnprintf(tmpBuf, sizeof(tmpBuf), errorFormatStr, ap);
    va_end(ap);

    if (!silent)
    {
	gUI->notifyError(tmpBuf);
    }
    LOG_DEBUG("%s\n", tmpBuf);
    return -1;
}


// Simple helper function.  Given a FEN fullmove and turn, return
// the appropriate ply.
static int fenFullmoveToPly(int fullmove, int turn)
{
    return (fullmove - 1) * 2 + turn;
}


// Sets 'board' to the FEN position described by 'fenString'.
// Returns -1 if invalid board detected (and board is left unaltered);
// 0 otherwise.
//
// We only accept standard FEN for an 8x8 board at this point.
int fenToBoard(char *fenString, BoardT *result)
{
    int i, rank = 7, file = 0; // counters.
    int chr, spaces, piece, res;
    BoardT tmpBoard;

    uint8 coord[NUM_SQUARES] = {0}; // initialize to empty board.
    // Setting some defaults:
    // -- no one can castle
    // -- no en passant
    // -- white's turn
    int cbyte = 0, ebyte = FLAG, turn = 0, halfmove, fullmove;
    char coordStr[80], turnStr[6], cbyteStr[6], ebyteStr[6];

    if (fenString == NULL)
    {
	return reportError
	    (0,	"Error: fenToBoard: NULL fenString (missing arg?)");
    }

    // Read in everything.  This is insensitive to whitespace between fields,
    // which is what we want since we want to work w/UCI.
    if ((res = sscanf(fenString, " %80s %6s %6s %6s %d %d",
		      coordStr, turnStr, cbyteStr, ebyteStr,
		      &halfmove, &fullmove)) < 6)
    {
	return reportError
	    (0,	"Error: fenToBoard: not enough arguments (%d)", res);
    }

    // Read in 'coord'.
    for (i = 0;
	 (chr = coordStr[i]) != '\0';
	 i++)
    {
	if (isdigit(chr))
	{
	    spaces = chr - '0';
	    if (file + spaces > 8)
	    {
		return reportError
		    (0,	"Error: fenToBoard: (%d,%d) too many spaces (%d)",
		     rank, file, spaces);
	    }
	    file += spaces;
	}
	else if (chr == '/')
	{
	    if (file < 8 || rank <= 0)
	    {
		return reportError
		    (0, "Error: fenToBoard: (%d,%d) bad separator",
		     rank, file);
	    }
	    rank--;
	    file = 0;
	}
	else if ((piece = asciiToNative(chr)) != 0)
	{
	    if (file >= 8)
	    {
		return reportError
		    (0, "Error: fenToBoard: (%d,%d) too many pieces",
		     rank, file);
	    }
	    coord[toCoord(rank, file++)] = piece;
	}
	else
	{
	    // Unknown token.  Assume it's an unknown piece.
	    return reportError
		(0, "Error: fenToBoard: (%d,%d) unknown piece %c",
		 rank, file, chr);
	}
    }
    if (file != 8 || rank != 0)
    {
	return reportError
	    (0, "Error: fenToBoard: (%d,%d) bad terminator",
	     rank, file);
    }

    // Read in turn.
    if (!strcmp(turnStr, "b"))
    {
	turn = 1;
    }
    else if (strcmp(turnStr, "w"))
    {
	return reportError(0, "Error: fenToBoard: unknown turn %s", turnStr);
    }

    // Read in cbyte.
    if (strcmp(cbyteStr, "-")) // do nothing, if nothing to castle.
    {
	for (i = 0;
	     i < 4 && (chr = cbyteStr[i]) != '\0';
	     i++)
	{
	    switch (chr)
	    {
	    case 'K':
		cbyte |= CASTLEOO;
		break;
	    case 'Q':
		cbyte |= CASTLEOOO;
		break;
	    case 'k':
		cbyte |= CASTLEOO << 1;
		break;
	    case 'q':
		cbyte |= CASTLEOOO << 1;
		break;
	    default:
		return reportError
		    (0, "Error: fenToBoard: unknown cbyte token '%c'", chr);
	    }
	}
	if (i == 4 && cbyteStr[i] != '\0')
	{
	    return reportError(0, "Error: fenToBoard: cbyteStr too long (%c)",
			       cbyteStr[i]);
	}
    }

    // Read in ebyte.
    if (strcmp(ebyteStr, "-"))
    {
	if ((ebyte = asciiToCoord(ebyteStr)) == FLAG)
	{
	    return reportError(0, "Error: fenToBoard: bad ebyte");
	}
	if (ebyteStr[2] != '\0')
	{
	    return reportError(0, "Error: fenToBoard: ebyteStr too long (%c)",
			       ebyteStr[2]);
	}
    }

    // (halfmove and fullmove are already read in.)
    // At this point we think we have something worth checking.
    BoardSet(&tmpBoard, coord, cbyte, ebyte, turn,
	     fenFullmoveToPly(fullmove, turn), halfmove);

    if (BoardSanityCheck(&tmpBoard, 0) < 0)
    {
	return -1;
    }

    BoardCopy(result, &tmpBoard);

    // LogBoard(eLogEmerg, result);
    return 0;
}


// Stop everything (including clocks) and wait for further input, basically.
void setForceMode(ThinkContextT *th, GameT *game)
{
    int i;

    ThinkerCmdBail(th);
    ClocksStop(game);
    for (i = 0; i < NUM_PLAYERS; i++)
    {
	game->control[i] = 0;
    }
}

char *findNextNonWhiteSpace(char *pStr)
{
    if (pStr == NULL)
    {
	return NULL;
    }
    while (isspace(*pStr) && *pStr != '\0')
    {
	pStr++;
    }
    return *pStr != '\0' ? pStr : NULL;
}

char *findNextWhiteSpace(char *pStr)
{
    if (pStr == NULL)
    {
	return NULL;
    }
    while (!isspace(*pStr) && *pStr != '\0')
    {
	pStr++;
    }
    return *pStr != '\0' ? pStr : NULL;
}

char *findNextWhiteSpaceOrNull(char *pStr)
{
    if (pStr == NULL)
    {
	return NULL;
    }
    while (!isspace(*pStr) && *pStr != '\0')
    {
	pStr++;
    }
    return pStr;
}

// Copies (possibly non-NULL-terminated) token 'src' to NULL-terminated 'dst'.
// Returns 'dst' iff a full copy could be performed, otherwise NULL (and in that
//  case, does not clobber 'dst', just to be nice)
static char *copyToken(char *dst, int dstLen, char *src)
{
    int srcLen;

    if (src == NULL ||
	(srcLen = (findNextWhiteSpaceOrNull(src) - src)) >= dstLen)
    {
	return NULL;
    }
    memcpy(dst, src, srcLen);
    dst[srcLen] = '\0';
    return dst;
}

// Return whether or not 'inputStr' looks like a move.
// NULL "inputStr"s are not moves.
// Side effect: fills in 'resultMove'.
// Currently we can only handle algebraic notation (and also O-O-style
//  castling).
bool isMove(char *inputStr, MoveT *resultMove, BoardT *board)
{
    char moveStr[MOVE_STRING_MAX];

    memset(resultMove, 0, sizeof(MoveT));

    if (copyToken(moveStr, sizeof(moveStr), inputStr) == NULL)
    {
	return false;
    }

    if (!strcasecmp(moveStr, "O-O") || !strcasecmp(moveStr, "0-0"))
    {
	MoveCreateFromCastle(resultMove, true, board->turn);
	return true;
    }
    if (!strcasecmp(moveStr, "O-O-O") || !strcasecmp(moveStr, "0-0-0"))
    {
	MoveCreateFromCastle(resultMove, false, board->turn);
	return true;
    }
    else if (asciiToCoord(moveStr) != FLAG && asciiToCoord(&moveStr[2]) != FLAG)
    {
	resultMove->src = asciiToCoord(moveStr);
	resultMove->dst = asciiToCoord(&moveStr[2]);
	MoveUnmangleCastle(resultMove, board);
	return true;
    }
    return false;
}


// Return whether or not 'inputStr' looks like a legal move.
// NULL "inputStr"s are not legal moves.
// Side effect: fills in 'resultMove'.
// Currently we can only handle algebraic notation.
bool isLegalMove(char *inputStr, MoveT *resultMove, BoardT *board)
{
    MoveListT moveList;
    MoveT *foundMove;
    uint8 chr;

    if (!isMove(inputStr, resultMove, board))
    {
	return false;
    }

    mlistGenerate(&moveList, board, 0);

    // Search moveList for move.
    if ((foundMove = mlistSearch(&moveList, resultMove, 2)) == NULL)
    {
	return false;
    }

    // Do we need to promote?
    if (MoveIsPromote(*resultMove, board))
    {
	chr = inputStr[4];
	if (chr != 'q' && chr != 'r' && chr != 'n' && chr != 'b')
	{
	    return false;
	}

	chr = asciiToNative(chr);
	resultMove->promote = (chr & ~1) | (board->turn);
	    
	foundMove = mlistSearch(&moveList, resultMove, 3);
	assert(foundMove != NULL);
    }
    else
    {
	resultMove->promote = foundMove->promote;
    }
    resultMove->chk = foundMove->chk;

    return true;
}

static bool isNewLineChar(char c)
{
    return c == '\n' || c == '\r';
}


char *ChopBeforeNewLine(char *s)
{
    char *origStr = s;

    for (; *s != '\0'; s++)
    {
	if (isNewLineChar(*s))
	{
	    *s = '\0';
	    return origStr;
	}
    }
    return origStr;
}


// Like fgets(), but returns on any newline char, not just '\n'.
// The semantics of what an error is ("size" too small) might be unique, though.
static char *myFgets(char *s, int size, FILE *stream)
{
    int i = 0;
    int chr = EOF; // cannot assign directly to s[i] since EOF may be -1 and
                   // "char" may be unsigned; 255 != -1
    if (size < 1)
    {
	return NULL;
    }
    while (i < size - 1)
    {
	if ((chr = fgetc(stream)) == EOF ||
	    isNewLineChar((s[i++] = chr)))
	{
	    break;
	}
    }
    s[i] = '\0';
    return (chr == EOF && i == 0) ? NULL : s;
}

char *getStdinLine(int maxLen, SwitcherContextT *sw)
{
    // Polyglot likes to send long "position" commands (the startpos and all
    // the moves, not just the FEN position after the last capture/pawn push.
    // You can theoretically play a large number of moves in even a normal
    // chess game, especially considering the fact that the 50-move draw is
    // claimed, not automatic.
    // So, playing it safe here w/an expandable buffer.  We could go even more
    // complicated and read/parse only bits of input at a time, if desired.
    static char *buf = NULL;
    static int bufLen = 0;

    int bytesRead = 0; // for the current line

    while (bytesRead == 0 ||
	   // expect line to be terminated w/a newline.  Otherwise, fgets()
	   // ran out of room.
	   !isNewLineChar(buf[bytesRead - 1]))
    {
	if (maxLen > 0 && maxLen < bytesRead)
	{
	    reportError(0, "%s: maxLen exceeded, buffer was '%s'",
			__func__, buf);
	    exit(0);
	}
	if (bytesRead >= bufLen - 1 &&
	    (buf = realloc(buf, (bufLen += 100))) == NULL)
	{
	    reportError(0, "%s: could not alloc %d bytes\n",
			__func__, bufLen + 100);
	    exit(0);
	}
	if (myFgets(&buf[bytesRead], bufLen - bytesRead, stdin) == NULL)
	{
	    reportError(0, "%s: fgets error %d, bailing\n", __func__, errno);
	    exit(0);
	}
	bytesRead += strlen(&buf[bytesRead]);
	if (bytesRead == 1 && isNewLineChar(buf[0]))
	{
	    // read stand-alone newline.  Just discard it, switch off, and
	    // come back when we have more input.
	    // The reason for these shenanigans is, if a windows program sends
	    // us CRLF, we will hit this.  We could avoid this with fgets(),
	    // but that would screw up when handling a single Mac-style CR
	    // (0x0d) which is explicitly allowed by the UCI spec.
	    bytesRead = 0;
	    SwitcherSwitch(sw);
	}
    }

    return buf;
}


typedef struct {
    ThreadArgsT args;
    ThinkContextT *th;
    GameT *game;
} UiArgsT;


static void uiThread(UiArgsT *args)
{
    UiArgsT myArgs = *args; // struct copy

    gUI->init(myArgs.game);

    // Prevent main thread from continuing until UI has initialized.
    ThreadNotifyCreated("uiThread", args);

    SwitcherRegister(&myArgs.game->sw);
    while(1)
    {
	gUI->playerMove(myArgs.th, myArgs.game);
    }    
}

void uiThreadInit(ThinkContextT *th, GameT *game)
{
    UiArgsT args = {gThreadDummyArgs, th, game};
    ThreadCreate(uiThread, &args);
}
