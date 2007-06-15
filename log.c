/***************************************************************************
                        log.c - debugging log support.
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

#include <stdarg.h>
#include <stdio.h>
#include "ref.h"

static int gLogLevel = eLogEmerg;
static FILE *gLogFile = NULL;

void LogSetLevel(int level)
{
    gLogLevel = level;
}


void LogSetFile(FILE *logFile)
{
    gLogFile = logFile;
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


/* debugging funcs. */
void LogMoveList(int level, MoveListT *mvlist)
{
    int i;
    if (level > gLogLevel)
    {
	return; /* no-op. */
    }

    LogPrint(eLogDebug, "{mvlist lgh %d insrt %d co %d ",
	     mvlist->lgh, mvlist->insrt, mvlist->capOnly);
    for (i = 0; i < mvlist->lgh; i++)
    {
	LogPrint(level,
 "%c%c%c%c ",
		 File(mvlist->list[i] [0]) + 'a',
		 Rank(mvlist->list[i] [0]) + '1',
		 File(mvlist->list[i] [1]) + 'a',
		 Rank(mvlist->list[i] [1]) + '1');
    }
    LogPrint(level, "}\n");
}


void LogMove(int level, BoardT *board, uint8 *comstr)
{
    int moveDepth;
    int cappiece;
    char capstr[6];
    char promostr[6];
    char chkstr[10];

    if (level > gLogLevel)
    {
	return; /* no-op */
    }

    /* optimization: do all initialization after gLogLevel check. */
    cappiece = board->coord[comstr[1]];
    capstr[0] = '\0';
    promostr[0] = '\0';
    chkstr[0] = '\0';

    LogPrint(level, "d%02d", board->depth);
    for (moveDepth = MIN(board->depth, 20); moveDepth > 0; moveDepth--)
    {
	LogPrint(level, "    ");
    }
    if (cappiece)
    {
	sprintf(capstr, "(x%c)", nativeToAscii(cappiece));
    }
    if (comstr[2] && !ISPAWN(comstr[2]))
    {
	sprintf(promostr, "(->%c)", nativeToAscii(comstr[2]));
    }
    if (comstr[3] != FLAG)
    {
	sprintf(chkstr, "(chk-%c%c)",
		File(comstr[3]) + 'a', Rank(comstr[3]) + '1');
    }
    LogPrint(level, "%c%c%c%c%s%s%s\n",
	     File(comstr[0]) + 'a',
	     Rank(comstr[0]) + '1',
	     File(comstr[1]) + 'a',
	     Rank(comstr[1]) + '1',
	     capstr, promostr, chkstr);
}


void LogMoveShow(int level, BoardT *board, uint8 *comstr, char *caption)
{
    int ascii, i, j;

    LogPrint(level, "%s:\nMove was %c%c%c%c\n", caption,
	     File(comstr[0]) + 'a', Rank(comstr[0]) + '1',
	     File(comstr[1]) + 'a', Rank(comstr[1]) + '1');

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
