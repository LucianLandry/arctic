#include <stdarg.h>
#include <stdio.h>
#include "ref.h"

static int gLogLevel = eLogEmerg;


void LogSetLevel(int level)
{
    gLogLevel = level;
}


int LogPrint(int level, const char *format, ...)
{
    va_list ap;
    int rv = 0;

    if (level <= gLogLevel)
    {
	va_start(ap, format);
	rv = vfprintf(stderr, format, ap);
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

    for (moveDepth = MIN(board->depth, 20); moveDepth > 0; moveDepth--)
    {
	LogPrint(level, "    ");
    }
    if (cappiece)
    {
	sprintf(capstr, "(x%c)", cappiece);
    }
    if (comstr[2] && !ISPAWN(comstr[2]))
    {
	sprintf(promostr, "(->%c)", comstr[2]);
    }
    if (comstr[3] != FLAG)
    {
	sprintf(chkstr, "(chk-%d)", comstr[3]);
    }
    LogPrint(level, "%c%c%c%c%s%s%s\n",
	     File(comstr[0]) + 'a',
	     Rank(comstr[0]) + '1',
	     File(comstr[1]) + 'a',
	     Rank(comstr[1]) + '1',
	     capstr, promostr, chkstr);
}
