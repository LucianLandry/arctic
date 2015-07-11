//--------------------------------------------------------------------------
//                      log.h - debugging log support.
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
#include <stdarg.h>
#include <stdio.h>

#include "log.h"
#include "uiUtil.h"

int gLogLevel = eLogNormal;
static FILE *gLogFile = NULL;
static const MoveStyleT gMoveStyleLog = { mnCAN, csK2, true};

void LogInit(void)
{
    gLogFile = fopen("errlog", "w");
    assert(gLogFile != NULL);
    // turn off buffering -- useful when we're crashing, but slow.
    setbuf(gLogFile, NULL);
    // LogSetLevel(eLogDebug);
}

void LogSetLevel(int level)
{
    gLogLevel = level;
}

void LogFlush(void)
{
    if (gLogFile) fflush(gLogFile);
}

int LogPrint(int level, const char *format, ...)
{
    va_list ap;
    int rv = 0;

    if (level <= gLogLevel)
    {
	va_start(ap, format);
	rv = vfprintf(gLogFile ? gLogFile : stderr, format, ap);
	va_end(ap);
    }

    return rv;
}

// debugging funcs.
void LogMoveList(int level, MoveListT *mvlist)
{
    char tmpStr[MOVE_STRING_MAX];
    int i;
    if (level > gLogLevel)
    {
	return; /* no-op. */
    }

    LogPrint(level, "{mvlist lgh %d insrt %d co %d ",
	     mvlist->lgh, mvlist->insrt, mvlist->capOnly);
    for (i = 0; i < mvlist->lgh; i++)
    {
	LogPrint(level, "%s ", MoveToString(tmpStr, mvlist->moves[i], &gMoveStyleLog, NULL));
    }
    LogPrint(level, "}\n");
}


void LogMove(int level, BoardT *board, MoveT *move)
{
    int moveDepth;
    int cappiece;
    char capstr[6];
    char promostr[6];
    char chkstr[10];
    char levelstr[160];
    char *myLevelstr;
    char tmpStr[MOVE_STRING_MAX];

    if (level > gLogLevel)
    {
	return; /* no-op */
    }

    /* optimization: do all initialization after gLogLevel check. */
    myLevelstr = levelstr;
    cappiece = board->coord[move->dst];
    capstr[0] = '\0';
    promostr[0] = '\0';
    chkstr[0] = '\0';

    myLevelstr += sprintf(myLevelstr, "D%02d", board->depth);
    for (moveDepth = MIN(board->depth, 20); moveDepth > 0; moveDepth--)
    {
	myLevelstr += sprintf(myLevelstr, "    ");
    }
    if (cappiece)
    {
	sprintf(capstr, "(x%c)", nativeToAscii(cappiece));
    }
    if (move->promote && !ISPAWN(move->promote))
    {
	sprintf(promostr, "(->%c)", nativeToAscii(move->promote));
    }
    if (move->chk != FLAG)
    {
	sprintf(chkstr, "(chk-%c%c)",
		AsciiFile(move->chk), AsciiRank(move->chk));
    }
    LogPrint(level, "%s%s%s%s%s\n",
	     levelstr,
	     MoveToString(tmpStr, *move, &gMoveStyleLog, NULL),
	     capstr, promostr, chkstr);
}


// A very simple "log-this-board" routine.
void LogBoard(int level, BoardT *board)
{
    int rank, file, chr;

    if (level > gLogLevel)
    {
	return; // no-op
    }

    LogPrint(level, "LogBoard:\n");
    for (rank = 7; rank >= 0; rank--)
    {
	for (file = 0; file < 8; file++)
	{
	    chr = nativeToAscii(board->coord[toCoord(rank, file)]);
	    LogPrint(level, "%c", chr == ' ' ? '.' : chr);
	}
	LogPrint(level, "\n");
    }
}


void LogMoveShow(int level, BoardT *board, MoveT *move, const char *caption)
{
    int ascii, i, j;
    char tmpStr[MOVE_STRING_MAX];

    LogPrint(level, "%s:\nMove was %s\n", caption, MoveToString(tmpStr, *move, &gMoveStyleLog, NULL));

    for (i = 7; i >= 0; i--)
    {
	for (j = 0; j < 8; j++)
	{
	    ascii = nativeToAscii(board->coord[(i * 8) + j]);
	    LogPrint(level, "%c", ascii == ' ' ? '.' : ascii);
	}
	LogPrint(level, "\n");
    }
}


/* Only meant to be called in an emergency situation (program is fixing to
   bail). */
void LogPieceList(BoardT *board)
{
    int i, j;
    for (i = 0; i < NUM_PIECE_TYPES; i++)
    {
	if (board->pieceList[i].lgh)
	{
	    LOG_EMERG("%d:", i);
	    for (j = 0; j < board->pieceList[i].lgh; j++)
	    {
		LOG_EMERG("%c%c",
			  AsciiFile(board->pieceList[i].coords[j]),
			  AsciiRank(board->pieceList[i].coords[j]));
	    }
	    LOG_EMERG(".\n");
	}
    }
    LOG_EMERG("pieceList results.\n");
}
